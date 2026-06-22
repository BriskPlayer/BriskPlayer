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
 * The stream cleanup is RAII: dropping FlacByteReader<BufReader<InStream>>
 * automatically drops BufReader<InStream> → InStream → calls uninitialise.
 */

use super::ffi::*;
use flac_codec::byteorder::LittleEndian;
use flac_codec::decode::FlacByteReader;
use std::io::{BufReader, Read, Seek, SeekFrom};
use std::os::raw::{c_char, c_int, c_void};

// ---------------------------------------------------------------------------
// Type alias — BufReader batches small FLAC bitstream reads into 64 KB chunks,
// dramatically reducing FFI crossings from the flac-codec crate's many small
// reads down to ~2–3 per second during normal playback.
// ---------------------------------------------------------------------------

type FlacReader = FlacByteReader<BufReader<InStream>, LittleEndian>;

// ---------------------------------------------------------------------------
// Internal codec state
// ---------------------------------------------------------------------------

struct FlacContext {
    seekable:                bool,
    /// InStream is owned by BufReader which is owned by FlacByteReader.
    /// Setting reader = None drops the whole chain and calls uninitialise.
    reader:                  Option<FlacReader>,

    sample_rate:             u32,
    channels:                u8,
    bits_per_sample:         u8,
    bytes_per_native_sample: u32,
    bytes_per_native_frame:  u64,
    total_samples:           u64,
    file_size_bytes:         u64,
    total_duration_secs:     u32,
    /// Running count of decoded native-PCM bytes; updated in get_pcm_block
    /// and reset/adjusted in open/seek.  Used by get_current_pos_secs to
    /// avoid seek(Current(0)) which would flush the BufReader on every poll.
    pcm_byte_pos:            u64,
    /// Reusable scratch buffer for non-16-bit FLAC block conversion.
    native_buf:              Vec<u8>,
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
            pcm_byte_pos:            0,
            native_buf:              Vec::new(),
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

/// Drop the reader (which owns BufReader → InStream) to trigger cleanup.
fn close_stream(ctx: &mut FlacContext) {
    ctx.reader = None;
}

unsafe extern "C" fn flac_uninitialise(pModule: *mut CPs_CoDecModule) {
    if pModule.is_null() { return; }
    // SAFETY: pModule is valid when called from C via the vtable.
    let m = &mut *pModule;
    if !m.m_pModuleCookie.is_null() {
        {
            // SAFETY: m_pModuleCookie was set to Box::into_raw(FlacContext).
            let ctx = &mut *(m.m_pModuleCookie as *mut FlacContext);
            close_stream(ctx);
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

    close_stream(ctx);
    ctx.sample_rate             = 0;
    ctx.channels                = 0;
    ctx.bits_per_sample         = 0;
    ctx.bytes_per_native_sample = 0;
    ctx.bytes_per_native_frame  = 0;
    ctx.total_samples           = 0;
    ctx.file_size_bytes         = 0;
    ctx.total_duration_secs     = 0;
    ctx.pcm_byte_pos            = 0;

    // SAFETY: pcFilename is a valid null-terminated C string; hwnd_owner is a valid HWND or null.
    let stream = match InStream::open(pcFilename, hwnd_owner) {
        Some(s) => s,
        None    => return FALSE,
    };

    let raw_len  = stream.length();
    let seekable = stream.seekable;
    ctx.seekable = seekable;
    ctx.file_size_bytes = if raw_len < 0xFFFF_FFFF { raw_len } else { 0 };

    // Wrap the raw stream in a BufReader so the flac-codec crate's many small
    // bitstream reads are batched into 64 KB chunks — this is the main fix for
    // audio stutters caused by per-byte FFI crossings into the C InStream.
    let buffered = BufReader::with_capacity(65536, stream);

    let reader: FlacReader = if seekable {
        match FlacByteReader::new_seekable(buffered) {
            Ok(r)  => r,
            Err(_) => return FALSE,
        }
    } else {
        match FlacByteReader::new(buffered) {
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
    close_stream(ctx);
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

    // Seeking discards the BufReader's buffer; the next read refills it.
    if reader.seek(SeekFrom::Start(seek_byte)).is_ok() {
        ctx.pcm_byte_pos = seek_byte;
    }
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
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut FlacContext);

    let requested_out_bytes = *pdwBlockSize as usize;

    if ctx.bytes_per_native_sample == 2 {
        let reader = match ctx.reader.as_mut() {
            Some(r) => r,
            None    => { *pdwBlockSize = 0; return FALSE; }
        };
        // SAFETY: pBlock points to a buffer of at least requested_out_bytes bytes.
        let buf = std::slice::from_raw_parts_mut(pBlock as *mut u8, requested_out_bytes);
        match reader.read(buf) {
            Ok(n) if n > 0 => {
                ctx.pcm_byte_pos += n as u64;
                *pdwBlockSize = n as DWORD;
                TRUE
            }
            _ => { *pdwBlockSize = 0; FALSE }
        }
    } else {
        let bps           = ctx.bits_per_sample as usize;
        let native_bps    = ctx.bytes_per_native_sample as usize;
        let out_samples   = requested_out_bytes / 2;
        let native_needed = out_samples * native_bps;

        // Resize the reusable scratch buffer before borrowing the reader.
        ctx.native_buf.resize(native_needed, 0);

        // Borrow reader after native_buf is resized to avoid split-borrow conflict.
        let reader = match ctx.reader.as_mut() {
            Some(r) => r,
            None    => { *pdwBlockSize = 0; return FALSE; }
        };
        let n_native = match reader.read(&mut ctx.native_buf) {
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
                raw |= (ctx.native_buf[start + b] as u32) << (b * 8);
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

        ctx.pcm_byte_pos += n_native as u64;
        *pdwBlockSize = (n_samples * 2) as DWORD;
        TRUE
    }
}

unsafe extern "C" fn flac_get_current_pos_secs(pModule: *mut CPs_CoDecModule) -> c_int {
    // SAFETY: m_pModuleCookie is valid.
    let ctx = &*((*pModule).m_pModuleCookie as *const FlacContext);

    if ctx.bytes_per_native_frame == 0 || ctx.sample_rate == 0 || ctx.reader.is_none() {
        return 0;
    }

    // Use the locally-tracked byte position rather than seek(Current(0)).
    // seek(Current(0)) on a BufReader flushes the read-ahead buffer, causing
    // an extra InStream read on every UI position poll (~10 times/second).
    let current_sample = ctx.pcm_byte_pos / ctx.bytes_per_native_frame;
    (current_sample / ctx.sample_rate as u64) as c_int
}
