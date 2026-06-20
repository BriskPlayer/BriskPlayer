/*
 * OGG/Vorbis codec — pure-Rust port using the lewton crate.
 *
 * Replaces CPI_Player_CoDec_OGG.c (which depends on libvorbis/libogg via vcpkg).
 * lewton is a pure-Rust Vorbis decoder; no C libraries are required.
 *
 * I/O goes through CP_CreateInStream via InStream (see ffi.rs), so both local
 * files and Icecast OGG internet streams work transparently.
 *
 * Unsafe usage is confined to:
 *   1. Extracting the typed context from m_pModuleCookie.
 *   2. Calling InStream::open (wraps CP_CreateInStream).
 *   3. Box::from_raw / slice::from_raw_parts_mut in uninitialise / get_pcm_block.
 * Stream cleanup is RAII: dropping OggStreamReader<OggAdapter> drops OggAdapter,
 * which drops InStream, which calls uninitialise on the C stream.
 */

use super::ffi::*;
use lewton::inside_ogg::OggStreamReader;
use std::io::{self, Read, Seek, SeekFrom};
use std::os::raw::{c_char, c_int, c_void};

const INTERNET_STREAM_LEN: u64 = 0xFFFF_FFFF;

// ---------------------------------------------------------------------------
// OggAdapter — thin wrapper around InStream with OGG-specific seek behaviour
//
// lewton's OggStreamReader::new calls seek(Start(0)) during header parsing.
// For non-seekable internet streams that call would normally return Err and
// abort initialisation.  We special-case it as a no-op (the stream IS at
// position 0 at that moment), so internet streams can be opened by lewton.
// ---------------------------------------------------------------------------

struct OggAdapter(InStream);

impl OggAdapter {
    fn new(stream: InStream) -> Self { OggAdapter(stream) }
}

impl Read for OggAdapter {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.0.read(buf) // delegates to InStream::read — safe
    }
}

impl Seek for OggAdapter {
    fn seek(&mut self, from: SeekFrom) -> io::Result<u64> {
        if !self.0.seekable {
            return match from {
                // lewton calls seek(Start(0)) at init; allow as a no-op so
                // internet streams open correctly.
                SeekFrom::Start(0) if self.0.pos() == 0 => Ok(0),
                SeekFrom::Current(0) => Ok(self.0.pos()),
                _ => Err(io::Error::new(io::ErrorKind::Unsupported, "stream not seekable")),
            };
        }
        self.0.seek(from) // delegates to InStream::seek — safe
    }
}

// SAFETY: all access is from the single-threaded player engine.
unsafe impl Send for OggAdapter {}

// ---------------------------------------------------------------------------
// Internal codec state
// ---------------------------------------------------------------------------

struct OggContext {
    seekable: bool,
    /// InStream owned through OggAdapter inside OggStreamReader.
    /// Setting reader = None drops the whole chain → uninitialise called.
    reader:   Option<OggStreamReader<OggAdapter>>,

    pcm_queue:           Vec<i16>,
    sample_rate:         u32,
    channels:            u8,
    bitrate_kbps:        u32,
    total_samples:       u64, // 0 when unknown (internet streams)
    total_duration_secs: u32,
    current_sample:      u64, // samples per channel decoded since last seek
    at_eos:              bool,
}

impl OggContext {
    fn new() -> Self {
        OggContext {
            seekable:            false,
            reader:              None,
            pcm_queue:           Vec::new(),
            sample_rate:         0,
            channels:            0,
            bitrate_kbps:        0,
            total_samples:       0,
            total_duration_secs: 0,
            current_sample:      0,
            at_eos:              false,
        }
    }

    /// Decode packets until pcm_queue holds at least `target` i16 samples.
    fn fill_pcm(&mut self, target: usize) {
        let mut consecutive_errors: u32 = 0;
        while self.pcm_queue.len() < target && !self.at_eos {
            let reader = match self.reader.as_mut() { Some(r) => r, None => break };
            match reader.read_dec_packet_itl() { // safe method on OggStreamReader
                Ok(Some(samples)) => {
                    consecutive_errors = 0;
                    let n_per_ch = if self.channels > 0 {
                        samples.len() / self.channels as usize
                    } else {
                        0
                    };
                    self.current_sample += n_per_ch as u64;
                    self.pcm_queue.extend_from_slice(&samples);
                }
                Ok(None) => { self.at_eos = true; break; }
                Err(_)   => {
                    consecutive_errors += 1;
                    if consecutive_errors >= 8 { self.at_eos = true; break; }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Exported initialiser
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CP_InitialiseCodec_OGG(pCoDec: *mut CPs_CoDecModule) {
    if pCoDec.is_null() { return; }
    // SAFETY: pCoDec is non-null, checked above.
    let m = &mut *pCoDec;

    m.Uninitialise       = Some(ogg_uninitialise);
    m.OpenFile           = Some(ogg_open_file);
    m.CloseFile          = Some(ogg_close_file);
    m.Seek               = Some(ogg_seek);
    m.GetFileInfo        = Some(ogg_get_file_info);
    m.GetPCMBlock        = Some(ogg_get_pcm_block);
    m.GetCurrentPos_secs = Some(ogg_get_current_pos_secs);

    let ctx = Box::new(OggContext::new());
    m.m_pModuleCookie = Box::into_raw(ctx) as *mut c_void;

    CPFA_InitialiseFileAssociations(pCoDec);
    CPFA_AddFileAssociation(pCoDec, b"OGG\0".as_ptr() as *const c_char, 0);
    CPFA_AddFileAssociation(pCoDec, b"OGA\0".as_ptr() as *const c_char, 0);
    CPFA_AddFileAssociation(pCoDec, b"OGV\0".as_ptr() as *const c_char, 0);
    CPFA_AddFileAssociation(pCoDec, b"OGX\0".as_ptr() as *const c_char, 0);
}

// ---------------------------------------------------------------------------
// Codec entry points
// ---------------------------------------------------------------------------

/// Drop the reader chain to trigger RAII cleanup.  Safe Rust.
fn close_ogg_stream(ctx: &mut OggContext) {
    ctx.reader = None; // Drop OggStreamReader → Drop OggAdapter → Drop InStream → uninitialise
    ctx.pcm_queue.clear();
    ctx.at_eos = false;
}

unsafe extern "C" fn ogg_uninitialise(pModule: *mut CPs_CoDecModule) {
    if pModule.is_null() { return; }
    // SAFETY: pModule is valid when called from C via the vtable.
    let m = &mut *pModule;
    if !m.m_pModuleCookie.is_null() {
        {
            // SAFETY: m_pModuleCookie was set to Box::into_raw(OggContext) in CP_InitialiseCodec_OGG.
            let ctx = &mut *(m.m_pModuleCookie as *mut OggContext);
            close_ogg_stream(ctx); // safe
        }
        // SAFETY: same Box::into_raw provenance.
        drop(Box::from_raw(m.m_pModuleCookie as *mut OggContext));
        m.m_pModuleCookie = std::ptr::null_mut();
    }
    CPFA_EmptyFileAssociations(pModule);
}

unsafe extern "C" fn ogg_open_file(
    pModule:    *mut CPs_CoDecModule,
    pcFilename: *const c_char,
    _dwCookie:  usize,
    hwnd_owner: *mut c_void,
) -> BOOL {
    // SAFETY: pModule and m_pModuleCookie are valid.
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut OggContext);

    close_ogg_stream(ctx); // safe
    ctx.sample_rate         = 0;
    ctx.channels            = 0;
    ctx.bitrate_kbps        = 0;
    ctx.total_samples       = 0;
    ctx.total_duration_secs = 0;
    ctx.current_sample      = 0;

    // SAFETY: pcFilename is a valid null-terminated C string; hwnd_owner is a valid HWND or null.
    let stream = match InStream::open(pcFilename, hwnd_owner) {
        Some(s) => s,
        None    => return FALSE,
    };

    let raw_len  = stream.length();  // safe method
    ctx.seekable = stream.seekable;  // safe field

    // Wrap in OggAdapter (takes ownership of stream) then pass to OggStreamReader.
    // If OggStreamReader::new fails, adapter (and stream inside) is dropped automatically.
    let adapter = OggAdapter::new(stream);
    let reader  = match OggStreamReader::new(adapter) {
        Ok(r)  => r,
        Err(_) => return FALSE,
    };

    let sample_rate = reader.ident_hdr.audio_sample_rate;
    let channels    = reader.ident_hdr.audio_channels;
    let bitrate_nom = reader.ident_hdr.bitrate_nominal; // i32, bits/s

    if sample_rate == 0 || channels == 0 {
        return FALSE; // reader (and stream inside) dropped automatically
    }

    ctx.sample_rate  = sample_rate;
    ctx.channels     = channels;
    ctx.bitrate_kbps = if bitrate_nom > 0 { bitrate_nom as u32 / 1000 } else { 0 };

    // Duration estimate from file size and nominal bitrate (local files only).
    if bitrate_nom > 0 && raw_len > 0 && raw_len < INTERNET_STREAM_LEN {
        let duration_secs       = raw_len * 8 / bitrate_nom as u64;
        ctx.total_duration_secs = duration_secs as u32;
        ctx.total_samples       = duration_secs * sample_rate as u64;
    }

    ctx.reader = Some(reader); // transfer ownership — no raw pointer in context
    TRUE
}

unsafe extern "C" fn ogg_close_file(pModule: *mut CPs_CoDecModule) {
    // SAFETY: m_pModuleCookie is valid.
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut OggContext);
    close_ogg_stream(ctx); // safe
}

unsafe extern "C" fn ogg_seek(
    pModule:      *mut CPs_CoDecModule,
    iNumerator:   c_int,
    iDenominator: c_int,
) {
    // SAFETY: m_pModuleCookie is valid.
    let ctx    = &mut *((*pModule).m_pModuleCookie as *mut OggContext);
    let reader = match ctx.reader.as_mut() { Some(r) => r, None => return };

    if !ctx.seekable || iDenominator == 0 || ctx.total_samples == 0 { return; }

    let ratio         = iNumerator as f64 / iDenominator as f64;
    let target_sample = (ratio * ctx.total_samples as f64) as u64;

    if reader.seek_absgp_pg(target_sample).is_ok() { // safe method
        ctx.current_sample = reader.get_last_absgp().unwrap_or(target_sample);
    }
    ctx.pcm_queue.clear();
    ctx.at_eos = false;
}

unsafe extern "C" fn ogg_get_file_info(
    pModule: *mut CPs_CoDecModule,
    pInfo:   *mut CPs_FileInfo,
) {
    // SAFETY: both pointers are valid when called from C.
    let ctx  = &*((*pModule).m_pModuleCookie as *const OggContext);
    let info = &mut *pInfo;
    info.m_iFreq_Hz         = ctx.sample_rate;
    info.m_bStereo          = if ctx.channels >= 2 { TRUE } else { FALSE };
    info.m_b16bit           = TRUE;
    info.m_iBitRate_Kbs     = ctx.bitrate_kbps;
    info.m_iFileLength_Secs = ctx.total_duration_secs;
}

unsafe extern "C" fn ogg_get_pcm_block(
    pModule:      *mut CPs_CoDecModule,
    pBlock:       *mut c_void,
    pdwBlockSize: *mut DWORD,
) -> BOOL {
    // SAFETY: all pointers are valid when called from C.
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut OggContext);

    if ctx.reader.is_none() {
        *pdwBlockSize = 0;
        return FALSE;
    }

    let requested_bytes   = *pdwBlockSize as usize;
    let requested_samples = requested_bytes / 2; // i16 = 2 bytes

    ctx.fill_pcm(requested_samples); // safe — calls read_dec_packet_itl through trait

    let available = ctx.pcm_queue.len();
    if available == 0 {
        *pdwBlockSize = 0;
        return FALSE;
    }

    let to_deliver = available.min(requested_samples);
    // SAFETY: pBlock points to a caller-owned buffer of at least requested_bytes bytes.
    let dst = std::slice::from_raw_parts_mut(pBlock as *mut i16, to_deliver);
    dst.copy_from_slice(&ctx.pcm_queue[..to_deliver]);
    ctx.pcm_queue.drain(..to_deliver);

    *pdwBlockSize = (to_deliver * 2) as DWORD;
    TRUE
}

unsafe extern "C" fn ogg_get_current_pos_secs(pModule: *mut CPs_CoDecModule) -> c_int {
    // SAFETY: m_pModuleCookie is valid.
    let ctx = &*((*pModule).m_pModuleCookie as *const OggContext);
    if ctx.sample_rate == 0 { return 0; }
    (ctx.current_sample / ctx.sample_rate as u64) as c_int
}
