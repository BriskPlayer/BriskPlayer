/*
 * MPEG/MP3 codec — pure-Rust implementation using the nanomp3 crate.
 *
 * Replaces CPI_Player_CoDec_MPEG.c (which depends on libmad via vcpkg).
 * nanomp3 is a c2rust translation of minimp3 with no_std support.
 *
 * Registers "MP3", "MP2", and "MP1" extensions.
 * Always delivers 16-bit signed interleaved PCM to the engine.
 *
 * I/O goes through CP_CreateInStream (CPI_Stream.c) which transparently
 * handles both local files and internet streams — same pattern as the
 * original C libmad codec.
 *
 * Unsafe usage is confined to three patterns:
 *   1. Extracting the typed context from m_pModuleCookie (raw pointer from C).
 *   2. Calling InStream::open (wraps CP_CreateInStream, a C function).
 *   3. Box::from_raw in the uninitialise callback.
 * All decode/seek/query logic is safe Rust.
 */

use super::ffi::*;
use nanomp3::{Decoder, MAX_SAMPLES_PER_FRAME};
use std::io::{Read, Seek, SeekFrom};
use std::os::raw::{c_char, c_int, c_void};

const INPUT_BUF_TARGET: usize = 16 * 1024;
const INTERNET_STREAM_LEN: u64 = 0xFFFF_FFFF;

// ---------------------------------------------------------------------------
// Internal codec state
// ---------------------------------------------------------------------------

struct Mp3Context {
    /// Open stream; None when no file is open.  Drop calls uninitialise.
    stream:           Option<InStream>,
    /// Cached from stream.length() at open time.
    stream_length:    u64,
    /// Byte offset of first audio frame (past any leading ID3v2 tag).
    stream_start:     u64,
    seekable:         bool,

    input_buf:        Vec<u8>,
    decoder:          Decoder,
    pcm_queue:        Vec<i16>,

    sample_rate:         u32,
    channels:            u8,
    bitrate_kbps:        u32,
    total_duration_secs: u32,
    bytes_consumed:      u64,
}

impl Mp3Context {
    fn new() -> Self {
        Mp3Context {
            stream:              None,
            stream_length:       0,
            stream_start:        0,
            seekable:            false,
            input_buf:           Vec::with_capacity(INPUT_BUF_TARGET * 2),
            decoder:             Decoder::new(),
            pcm_queue:           Vec::new(),
            sample_rate:         0,
            channels:            0,
            bitrate_kbps:        0,
            total_duration_secs: 0,
            bytes_consumed:      0,
        }
    }

    /// Pull more compressed data from the stream into input_buf.
    fn refill_input(&mut self) {
        let stream = match self.stream.as_mut() { Some(s) => s, None => return };
        let have   = self.input_buf.len();
        let needed = INPUT_BUF_TARGET.saturating_sub(have);
        if needed == 0 { return; }

        let old_len = self.input_buf.len();
        self.input_buf.resize(old_len + needed, 0);
        // Read::read is safe — vtable call is inside InStream's impl.
        let got = stream.read(&mut self.input_buf[old_len..]).unwrap_or(0);
        self.input_buf.truncate(old_len + got);
    }

    /// Decode one MP3 frame and append i16 PCM to pcm_queue.
    fn decode_one(&mut self) -> bool {
        if self.input_buf.len() < INPUT_BUF_TARGET / 2 {
            self.refill_input();
        }
        if self.input_buf.is_empty() { return false; }

        let mut pcm_scratch = vec![0f32; MAX_SAMPLES_PER_FRAME];
        let (consumed, maybe_info) = self.decoder.decode(&self.input_buf, &mut pcm_scratch);

        if consumed == 0 {
            self.refill_input();
            return false;
        }

        self.input_buf.drain(..consumed);
        self.bytes_consumed += consumed as u64;

        if let Some(info) = maybe_info {
            if self.sample_rate == 0 {
                self.sample_rate  = info.sample_rate;
                self.channels     = info.channels.num();
                self.bitrate_kbps = info.bitrate;
                if self.seekable
                    && self.stream_length < INTERNET_STREAM_LEN
                    && self.bitrate_kbps > 0
                {
                    let audio_bytes = self.stream_length.saturating_sub(self.stream_start);
                    self.total_duration_secs =
                        (audio_bytes * 8 / (self.bitrate_kbps as u64 * 1000)) as u32;
                }
            }
            let n = info.samples_produced * info.channels.num() as usize;
            for &s in &pcm_scratch[..n] {
                let i = (s * 32767.0).clamp(-32768.0, 32767.0) as i16;
                self.pcm_queue.push(i);
            }
            true
        } else {
            false
        }
    }

    fn pcm_available(&self) -> usize { self.pcm_queue.len() }
}

// ---------------------------------------------------------------------------
// Exported initialiser
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CP_InitialiseCodec_MPEG(pCoDec: *mut CPs_CoDecModule) {
    if pCoDec.is_null() { return; }
    // SAFETY: pCoDec is non-null, checked above.
    let m = &mut *pCoDec;

    m.Uninitialise       = Some(mp3_uninitialise);
    m.OpenFile           = Some(mp3_open_file);
    m.CloseFile          = Some(mp3_close_file);
    m.Seek               = Some(mp3_seek);
    m.GetFileInfo        = Some(mp3_get_file_info);
    m.GetPCMBlock        = Some(mp3_get_pcm_block);
    m.GetCurrentPos_secs = Some(mp3_get_current_pos_secs);

    let ctx = Box::new(Mp3Context::new());
    m.m_pModuleCookie = Box::into_raw(ctx) as *mut c_void;

    CPFA_InitialiseFileAssociations(pCoDec);
    CPFA_AddFileAssociation(pCoDec, b"MP3\0".as_ptr() as *const c_char, 0);
    CPFA_AddFileAssociation(pCoDec, b"MP2\0".as_ptr() as *const c_char, 0);
    CPFA_AddFileAssociation(pCoDec, b"MP1\0".as_ptr() as *const c_char, 0);
}

// ---------------------------------------------------------------------------
// Codec entry points
// ---------------------------------------------------------------------------

unsafe extern "C" fn mp3_uninitialise(pModule: *mut CPs_CoDecModule) {
    if pModule.is_null() { return; }
    // SAFETY: pModule is valid when called from C via the vtable.
    let m = &mut *pModule;
    if !m.m_pModuleCookie.is_null() {
        {
            // SAFETY: m_pModuleCookie was set to Box::into_raw(Mp3Context) in CP_InitialiseCodec_MPEG.
            let ctx = &mut *(m.m_pModuleCookie as *mut Mp3Context);
            ctx.stream = None; // Drop InStream → calls uninitialise on the C stream
        }
        // SAFETY: same Box::into_raw provenance.
        drop(Box::from_raw(m.m_pModuleCookie as *mut Mp3Context));
        m.m_pModuleCookie = std::ptr::null_mut();
    }
    CPFA_EmptyFileAssociations(pModule);
}

unsafe extern "C" fn mp3_open_file(
    pModule:    *mut CPs_CoDecModule,
    pcFilename: *const c_char,
    _dwCookie:  usize,
    hwnd_owner: *mut c_void,
) -> BOOL {
    // SAFETY: pModule and m_pModuleCookie are valid (set in CP_InitialiseCodec_MPEG).
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut Mp3Context);

    ctx.stream = None; // RAII: calls uninitialise on any previously open stream
    ctx.input_buf.clear();
    ctx.pcm_queue.clear();
    ctx.decoder          = Decoder::new();
    ctx.sample_rate      = 0;
    ctx.channels         = 0;
    ctx.bitrate_kbps     = 0;
    ctx.total_duration_secs = 0;
    ctx.bytes_consumed   = 0;
    ctx.stream_start     = 0;
    ctx.stream_length    = 0;
    ctx.seekable         = false;

    // SAFETY: pcFilename is a valid null-terminated C string; hwnd_owner is a valid HWND or null.
    let mut stream = match InStream::open(pcFilename, hwnd_owner) {
        Some(s) => s,
        None    => return FALSE,
    };

    ctx.stream_length = stream.length(); // safe method
    ctx.seekable      = stream.seekable; // safe field

    // Seekable streams only: detect and skip a leading ID3v2 tag so the
    // bitrate-based duration estimate uses audio bytes, not tag bytes.
    if ctx.seekable {
        let mut hdr = [0u8; 10];
        let n = stream.read(&mut hdr).unwrap_or(0); // Read::read is safe
        let stream_start = if n == 10 && hdr.starts_with(b"ID3") {
            let sz = ((hdr[6] as u64 & 0x7F) << 21)
                   | ((hdr[7] as u64 & 0x7F) << 14)
                   | ((hdr[8] as u64 & 0x7F) <<  7)
                   |  (hdr[9] as u64 & 0x7F);
            10 + sz
        } else {
            0u64
        };
        stream.seek(SeekFrom::Start(stream_start)).ok(); // Seek::seek is safe
        ctx.stream_start = stream_start;
    }

    ctx.stream = Some(stream); // transfer ownership — no raw pointer stored in context

    // Probe up to 32 frames to populate sample_rate / channels / bitrate.
    for _ in 0..32 {
        if ctx.sample_rate != 0 { break; }
        ctx.decode_one(); // all safe Rust
    }

    if ctx.sample_rate == 0 {
        ctx.stream = None; // RAII cleanup
        return FALSE;
    }

    TRUE
}

unsafe extern "C" fn mp3_close_file(pModule: *mut CPs_CoDecModule) {
    // SAFETY: m_pModuleCookie is valid (set in CP_InitialiseCodec_MPEG).
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut Mp3Context);
    ctx.stream = None; // RAII: calls uninitialise
    ctx.input_buf.clear();
    ctx.pcm_queue.clear();
}

unsafe extern "C" fn mp3_seek(
    pModule:      *mut CPs_CoDecModule,
    iNumerator:   c_int,
    iDenominator: c_int,
) {
    // SAFETY: m_pModuleCookie is valid.
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut Mp3Context);

    let stream = match ctx.stream.as_mut() { Some(s) => s, None => return };
    if !stream.seekable { return; }
    if iDenominator == 0 || ctx.stream_length == 0 { return; }

    let audio_len     = ctx.stream_length.saturating_sub(ctx.stream_start);
    let ratio         = iNumerator as f64 / iDenominator as f64;
    let target_offset = ((ratio * audio_len as f64) as u64).min(audio_len.saturating_sub(1));
    let stream_pos    = ctx.stream_start + target_offset;

    stream.seek(SeekFrom::Start(stream_pos)).ok(); // Seek::seek is safe

    ctx.bytes_consumed = target_offset;
    ctx.input_buf.clear();
    ctx.pcm_queue.clear();
    ctx.decoder = Decoder::new();
}

unsafe extern "C" fn mp3_get_file_info(
    pModule: *mut CPs_CoDecModule,
    pInfo:   *mut CPs_FileInfo,
) {
    // SAFETY: both pointers are valid when called from C.
    let ctx  = &*((*pModule).m_pModuleCookie as *const Mp3Context);
    let info = &mut *pInfo;
    info.m_iFreq_Hz         = ctx.sample_rate;
    info.m_bStereo          = if ctx.channels == 2 { TRUE } else { FALSE };
    info.m_b16bit           = TRUE;
    info.m_iBitRate_Kbs     = ctx.bitrate_kbps;
    info.m_iFileLength_Secs = ctx.total_duration_secs;
}

unsafe extern "C" fn mp3_get_pcm_block(
    pModule:      *mut CPs_CoDecModule,
    pBlock:       *mut c_void,
    pdwBlockSize: *mut DWORD,
) -> BOOL {
    // SAFETY: all pointers are valid when called from C.
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut Mp3Context);

    if ctx.stream.is_none() {
        *pdwBlockSize = 0;
        return FALSE;
    }

    let requested_bytes   = *pdwBlockSize as usize;
    let requested_samples = requested_bytes / 2; // i16 = 2 bytes

    loop {
        if ctx.pcm_available() >= requested_samples { break; }
        let pcm_before   = ctx.pcm_queue.len();
        let input_before = ctx.input_buf.len();
        ctx.decode_one(); // safe Rust
        let any_progress = ctx.pcm_queue.len() != pcm_before
            || ctx.input_buf.len() != input_before;
        if !any_progress { break; }
    }

    let available = ctx.pcm_available();
    if available == 0 {
        *pdwBlockSize = 0;
        return FALSE;
    }

    let to_deliver = available.min(requested_samples);
    // SAFETY: pBlock points to a buffer of at least requested_bytes bytes.
    let dst = std::slice::from_raw_parts_mut(pBlock as *mut i16, to_deliver);
    dst.copy_from_slice(&ctx.pcm_queue[..to_deliver]);
    ctx.pcm_queue.drain(..to_deliver);

    *pdwBlockSize = (to_deliver * 2) as DWORD;
    TRUE
}

unsafe extern "C" fn mp3_get_current_pos_secs(pModule: *mut CPs_CoDecModule) -> c_int {
    // SAFETY: m_pModuleCookie is valid.
    let ctx = &*((*pModule).m_pModuleCookie as *const Mp3Context);
    if ctx.bitrate_kbps == 0 { return 0; }
    (ctx.bytes_consumed * 8 / (ctx.bitrate_kbps as u64 * 1000)) as c_int
}
