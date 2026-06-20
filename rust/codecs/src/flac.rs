/*
 * FLAC codec — pure-Rust port using the flac-codec crate.
 *
 * Replaces CPI_Player_CoDec_FLAC.c (which depends on libFLAC via vcpkg).
 *
 * I/O goes through CP_CreateInStream via InStream (see ffi.rs), so both local
 * files and internet FLAC streams work transparently.
 *
 * Unsafe usage is confined to:
 *   1. Extracting the typed context from m_pModuleCookie.
 *   2. Calling InStream::open (wraps CP_CreateInStream).
 *   3. Box::from_raw / slice::from_raw_parts_mut in the uninitialise and
 *      get_pcm_block callbacks.
 * The stream cleanup is RAII: dropping FlacByteReader<InStream> automatically
 * drops InStream, which calls uninitialise on the C stream.
 */

use super::ffi::*;
use flac_codec::byteorder::LittleEndian;
use flac_codec::decode::FlacByteReader;
use std::io::{Read, Seek, SeekFrom};
use std::os::raw::{c_char, c_int, c_void};

// ---------------------------------------------------------------------------
// Type alias
// ---------------------------------------------------------------------------

type FlacReader = FlacByteReader<InStream, LittleEndian>;

// ---------------------------------------------------------------------------
// Internal codec state
// ---------------------------------------------------------------------------

struct FlacContext {
    seekable:                bool,
    /// InStream is owned by FlacByteReader.  Setting reader = None drops it,
    /// which drops InStream and calls uninitialise — no manual cleanup needed.
    reader:                  Option<FlacReader>,

    sample_rate:             u32,
    channels:                u8,
    bits_per_sample:         u8,
    bytes_per_native_sample: u32,
    bytes_per_native_frame:  u64,
    total_samples:           u64,
    file_size_bytes:         u64,
    total_duration_secs:     u32,
}

impl FlacContext {
    fn new() -> Self {
        FlacContext {
            seekable:                false,
            reader:                  None,
            sample_rate:             0,
            channels:                0,
            bits_per_sample:         0,
            bytes_per_native_sample: 0,
            bytes_per_native_frame:  0,
            total_samples:           0,
            file_size_bytes:         0,
            total_duration_secs:     0,
        }
    }
}

// ---------------------------------------------------------------------------
// Exported initialiser
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CP_InitialiseCodec_FLAC(pCoDec: *mut CPs_CoDecModule) {
    if pCoDec.is_null() { return; }
    // SAFETY: pCoDec is non-null, checked above.
    let m = &mut *pCoDec;

    m.Uninitialise       = Some(flac_uninitialise);
    m.OpenFile           = Some(flac_open_file);
    m.CloseFile          = Some(flac_close_file);
    m.Seek               = Some(flac_seek);
    m.GetFileInfo        = Some(flac_get_file_info);
    m.GetPCMBlock        = Some(flac_get_pcm_block);
    m.GetCurrentPos_secs = Some(flac_get_current_pos_secs);

    let ctx = Box::new(FlacContext::new());
    m.m_pModuleCookie = Box::into_raw(ctx) as *mut c_void;

    CPFA_InitialiseFileAssociations(pCoDec);
    CPFA_AddFileAssociation(pCoDec, b"FLAC\0".as_ptr() as *const c_char, 0);
    CPFA_AddFileAssociation(pCoDec, b"FLA\0".as_ptr() as *const c_char, 0);
}

// ---------------------------------------------------------------------------
// Codec entry points
// ---------------------------------------------------------------------------

/// Drop the reader (which owns the InStream) to trigger cleanup.  Safe Rust.
fn close_stream(ctx: &mut FlacContext) {
    ctx.reader = None; // Drop FlacByteReader → Drop InStream → calls uninitialise
}

unsafe extern "C" fn flac_uninitialise(pModule: *mut CPs_CoDecModule) {
    if pModule.is_null() { return; }
    // SAFETY: pModule is valid when called from C via the vtable.
    let m = &mut *pModule;
    if !m.m_pModuleCookie.is_null() {
        {
            // SAFETY: m_pModuleCookie was set to Box::into_raw(FlacContext) in CP_InitialiseCodec_FLAC.
            let ctx = &mut *(m.m_pModuleCookie as *mut FlacContext);
            close_stream(ctx); // safe
        }
        // SAFETY: same Box::into_raw provenance.
        drop(Box::from_raw(m.m_pModuleCookie as *mut FlacContext));
        m.m_pModuleCookie = std::ptr::null_mut();
    }
    CPFA_EmptyFileAssociations(pModule);
}

unsafe extern "C" fn flac_open_file(
    pModule:    *mut CPs_CoDecModule,
    pcFilename: *const c_char,
    _dwCookie:  usize,
    hwnd_owner: *mut c_void,
) -> BOOL {
    // SAFETY: pModule and m_pModuleCookie are valid.
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut FlacContext);

    close_stream(ctx); // safe
    ctx.sample_rate             = 0;
    ctx.channels                = 0;
    ctx.bits_per_sample         = 0;
    ctx.bytes_per_native_sample = 0;
    ctx.bytes_per_native_frame  = 0;
    ctx.total_samples           = 0;
    ctx.file_size_bytes         = 0;
    ctx.total_duration_secs     = 0;

    // SAFETY: pcFilename is a valid null-terminated C string; hwnd_owner is a valid HWND or null.
    let stream = match InStream::open(pcFilename, hwnd_owner) {
        Some(s) => s,
        None    => return FALSE,
    };

    let raw_len  = stream.length(); // safe method
    let seekable = stream.seekable; // safe field
    ctx.seekable = seekable;
    ctx.file_size_bytes = if raw_len < 0xFFFF_FFFF { raw_len } else { 0 };

    // Pass ownership of stream into FlacByteReader.  If construction fails,
    // stream is dropped automatically → uninitialise called.
    let reader: FlacReader = if seekable {
        match FlacByteReader::new_seekable(stream) {
            Ok(r)  => r,
            Err(_) => return FALSE,
        }
    } else {
        match FlacByteReader::new(stream) {
            Ok(r)  => r,
            Err(_) => return FALSE,
        }
    };

    let info = reader.metadata().streaminfo();

    let sample_rate   = info.sample_rate;
    let channels: u8  = info.channels.get();
    let bps: u32      = info.bits_per_sample.into();
    let total_samples = info.total_samples.map(|n| n.get()).unwrap_or(0);

    if sample_rate == 0 || channels == 0 || bps == 0 {
        // reader (and stream inside it) dropped here automatically
        return FALSE;
    }

    let bytes_per_native_sample = (bps + 7) / 8;
    let bytes_per_native_frame  = bytes_per_native_sample as u64 * channels as u64;
    let total_duration_secs     = if sample_rate > 0 && total_samples > 0 {
        (total_samples / sample_rate as u64) as u32
    } else {
        0
    };

    ctx.sample_rate             = sample_rate;
    ctx.channels                = channels;
    ctx.bits_per_sample         = bps.min(255) as u8;
    ctx.bytes_per_native_sample = bytes_per_native_sample;
    ctx.bytes_per_native_frame  = bytes_per_native_frame;
    ctx.total_samples           = total_samples;
    ctx.total_duration_secs     = total_duration_secs;
    ctx.reader = Some(reader);

    TRUE
}

unsafe extern "C" fn flac_close_file(pModule: *mut CPs_CoDecModule) {
    // SAFETY: m_pModuleCookie is valid.
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut FlacContext);
    close_stream(ctx); // safe
}

unsafe extern "C" fn flac_seek(
    pModule:      *mut CPs_CoDecModule,
    iNumerator:   c_int,
    iDenominator: c_int,
) {
    // SAFETY: m_pModuleCookie is valid.
    let ctx    = &mut *((*pModule).m_pModuleCookie as *mut FlacContext);
    let reader = match ctx.reader.as_mut() { Some(r) => r, None => return };

    if !ctx.seekable { return; }
    if iDenominator == 0 || ctx.total_samples == 0 || ctx.bytes_per_native_frame == 0 {
        return;
    }

    let ratio         = iNumerator as f64 / iDenominator as f64;
    let target_sample = (ratio * ctx.total_samples as f64) as u64;
    let target_sample = target_sample.min(ctx.total_samples.saturating_sub(1));
    let seek_byte     = target_sample * ctx.bytes_per_native_frame;

    let _ = reader.seek(SeekFrom::Start(seek_byte)); // Seek::seek on FlacByteReader — safe
}

unsafe extern "C" fn flac_get_file_info(
    pModule: *mut CPs_CoDecModule,
    pInfo:   *mut CPs_FileInfo,
) {
    // SAFETY: both pointers are valid when called from C.
    let ctx  = &*((*pModule).m_pModuleCookie as *const FlacContext);
    let info = &mut *pInfo;

    info.m_iFreq_Hz         = ctx.sample_rate;
    info.m_bStereo          = if ctx.channels == 2 { TRUE } else { FALSE };
    info.m_b16bit           = TRUE;
    info.m_iFileLength_Secs = ctx.total_duration_secs;
    info.m_iBitRate_Kbs     = if ctx.total_duration_secs > 0 && ctx.file_size_bytes > 0 {
        ((ctx.file_size_bytes * 8) / (ctx.total_duration_secs as u64 * 1000)) as UINT
    } else {
        0
    };
}

unsafe extern "C" fn flac_get_pcm_block(
    pModule:      *mut CPs_CoDecModule,
    pBlock:       *mut c_void,
    pdwBlockSize: *mut DWORD,
) -> BOOL {
    // SAFETY: all pointers are valid when called from C.
    let ctx    = &mut *((*pModule).m_pModuleCookie as *mut FlacContext);
    let reader = match ctx.reader.as_mut() {
        Some(r) => r,
        None    => { *pdwBlockSize = 0; return FALSE; }
    };

    let requested_out_bytes = *pdwBlockSize as usize;

    if ctx.bytes_per_native_sample == 2 {
        // SAFETY: pBlock points to a buffer of at least requested_out_bytes bytes.
        let buf = std::slice::from_raw_parts_mut(pBlock as *mut u8, requested_out_bytes);
        match reader.read(buf) { // Read::read on FlacByteReader — safe
            Ok(n) if n > 0 => { *pdwBlockSize = n as DWORD; TRUE }
            _              => { *pdwBlockSize = 0; FALSE }
        }
    } else {
        let bps           = ctx.bits_per_sample as usize;
        let native_bps    = ctx.bytes_per_native_sample as usize;
        let out_samples   = requested_out_bytes / 2;
        let native_needed = out_samples * native_bps;

        let mut native_buf = vec![0u8; native_needed];
        let n_native = match reader.read(&mut native_buf) { // safe
            Ok(0) | Err(_) => { *pdwBlockSize = 0; return FALSE; }
            Ok(n)          => n,
        };

        let n_samples = n_native / native_bps;
        // SAFETY: pBlock points to a buffer of at least requested_out_bytes bytes.
        let out_i16   = std::slice::from_raw_parts_mut(pBlock as *mut i16, n_samples);
        let shift     = bps as i32 - 16;

        for i in 0..n_samples {
            let start = i * native_bps;
            let mut raw = 0u32;
            for b in 0..native_bps {
                raw |= (native_buf[start + b] as u32) << (b * 8);
            }
            let signed: i32 = if bps < 32 {
                let up = 32 - bps;
                ((raw << up) as i32) >> up
            } else {
                raw as i32
            };
            let s16 = if shift > 0 { signed >> shift } else { signed << (-shift) };
            out_i16[i] = s16.clamp(-32768, 32767) as i16;
        }

        *pdwBlockSize = (n_samples * 2) as DWORD;
        TRUE
    }
}

unsafe extern "C" fn flac_get_current_pos_secs(pModule: *mut CPs_CoDecModule) -> c_int {
    // SAFETY: m_pModuleCookie is valid.
    let ctx    = &mut *((*pModule).m_pModuleCookie as *mut FlacContext);
    let reader = match ctx.reader.as_mut() { Some(r) => r, None => return 0 };

    if ctx.bytes_per_native_frame == 0 || ctx.sample_rate == 0 { return 0; }

    // SeekFrom::Current(0) returns the tracked position for both seekable and
    // non-seekable streams (InStream counts bytes read).  Seek::seek is safe.
    let current_byte = match reader.seek(SeekFrom::Current(0)) {
        Ok(pos) => pos,
        Err(_)  => return 0,
    };

    let current_sample = current_byte / ctx.bytes_per_native_frame;
    (current_sample / ctx.sample_rate as u64) as c_int
}
