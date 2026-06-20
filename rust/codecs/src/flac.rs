/*
 * FLAC codec — pure-Rust port using the flac-codec crate.
 *
 * Replaces CPI_Player_CoDec_FLAC.c (which depends on libFLAC via vcpkg).
 * The flac-codec crate is a pure-Rust FLAC decoder, so no additional C
 * library is needed.
 *
 * Behaviour notes
 * ───────────────
 * • GetPCMBlock always delivers 16-bit little-endian interleaved PCM
 *   regardless of the file's native bit depth, matching what the C codec
 *   did via flac_convert_samples().
 * • GetFileInfo always reports m_b16bit = TRUE for the same reason.
 * • Seeking uses FlacByteReader::seek(SeekFrom::Start(byte_offset)) where
 *   the byte offset is in the *native* PCM output stream.  GetCurrentPos_secs
 *   queries the current offset with seek(SeekFrom::Current(0)).
 */

use super::ffi::*;
use flac_codec::byteorder::LittleEndian;
use flac_codec::decode::FlacByteReader;
use std::ffi::CStr;
use std::fs::{self, File};
use std::io::{BufReader, Read, Seek, SeekFrom};
use std::os::raw::{c_char, c_int, c_void};

// ---------------------------------------------------------------------------
// Type alias for the seekable, file-backed, little-endian FLAC reader
// ---------------------------------------------------------------------------

type FlacReader = FlacByteReader<BufReader<File>, LittleEndian>;

// ---------------------------------------------------------------------------
// Internal codec state
// ---------------------------------------------------------------------------

struct FlacContext {
    reader: Option<FlacReader>,

    // Cached from STREAMINFO — used for seek and position calculations.
    sample_rate:            u32,
    channels:               u8,
    bits_per_sample:        u8,
    /// Bytes per sample in the *native* PCM output (ceil(bits_per_sample / 8)).
    bytes_per_native_sample: u32,
    /// Bytes per interleaved frame (channels × bytes_per_native_sample).
    bytes_per_native_frame:  u64,
    total_samples:          u64, // 0 when unknown
    file_size_bytes:        u64,
    total_duration_secs:    u32,
}

impl FlacContext {
    fn new() -> Self {
        FlacContext {
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
    if pCoDec.is_null() {
        return;
    }
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

unsafe extern "C" fn flac_uninitialise(pModule: *mut CPs_CoDecModule) {
    if pModule.is_null() {
        return;
    }
    let m = &mut *pModule;
    if !m.m_pModuleCookie.is_null() {
        drop(Box::from_raw(m.m_pModuleCookie as *mut FlacContext));
        m.m_pModuleCookie = std::ptr::null_mut();
    }
    CPFA_EmptyFileAssociations(pModule);
}

unsafe extern "C" fn flac_open_file(
    pModule:    *mut CPs_CoDecModule,
    pcFilename: *const c_char,
    _dwCookie:  usize,
    _hWndOwner: *mut c_void,
) -> BOOL {
    let m   = &mut *pModule;
    let ctx = &mut *(m.m_pModuleCookie as *mut FlacContext);

    ctx.reader = None;

    let path = match CStr::from_ptr(pcFilename).to_str() {
        Ok(s)  => s,
        Err(_) => return FALSE,
    };

    // File size for bitrate estimation.
    ctx.file_size_bytes = fs::metadata(path).map(|md| md.len()).unwrap_or(0);

    // Open via a seekable BufReader so FlacByteReader can seek during decoding.
    let file = match File::open(path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    let buf_reader = BufReader::new(file);

    let reader: FlacReader = match FlacByteReader::new_seekable(buf_reader) {
        Ok(r)  => r,
        Err(_) => return FALSE,
    };

    // Extract STREAMINFO metadata.
    let info = reader.metadata().streaminfo();

    let sample_rate: u32 = info.sample_rate;
    let channels:    u8  = info.channels.get();
    let bps: u32         = info.bits_per_sample.into();
    let bps_u8           = bps.min(255) as u8;
    let total_samples: u64 = info
        .total_samples
        .map(|n| n.get())
        .unwrap_or(0);

    if sample_rate == 0 || channels == 0 || bps == 0 {
        return FALSE;
    }

    let bytes_per_native_sample = (bps + 7) / 8;
    let bytes_per_native_frame  = bytes_per_native_sample as u64 * channels as u64;

    let total_duration_secs = if sample_rate > 0 && total_samples > 0 {
        (total_samples / sample_rate as u64) as u32
    } else {
        0
    };

    ctx.sample_rate             = sample_rate;
    ctx.channels                = channels;
    ctx.bits_per_sample         = bps_u8;
    ctx.bytes_per_native_sample = bytes_per_native_sample;
    ctx.bytes_per_native_frame  = bytes_per_native_frame;
    ctx.total_samples           = total_samples;
    ctx.total_duration_secs     = total_duration_secs;
    ctx.reader = Some(reader);

    TRUE
}

unsafe extern "C" fn flac_close_file(pModule: *mut CPs_CoDecModule) {
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut FlacContext);
    ctx.reader = None;
}

unsafe extern "C" fn flac_seek(
    pModule:      *mut CPs_CoDecModule,
    iNumerator:   c_int,
    iDenominator: c_int,
) {
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut FlacContext);
    let reader = match ctx.reader.as_mut() { Some(r) => r, None => return };

    if iDenominator == 0 || ctx.total_samples == 0 || ctx.bytes_per_native_frame == 0 {
        return;
    }

    let ratio         = iNumerator as f64 / iDenominator as f64;
    let target_sample = (ratio * ctx.total_samples as f64) as u64;
    let target_sample = target_sample.min(ctx.total_samples.saturating_sub(1));
    let seek_byte     = target_sample * ctx.bytes_per_native_frame;

    let _ = reader.seek(SeekFrom::Start(seek_byte));
}

unsafe extern "C" fn flac_get_file_info(
    pModule: *mut CPs_CoDecModule,
    pInfo:   *mut CPs_FileInfo,
) {
    let ctx = &*((*pModule).m_pModuleCookie as *const FlacContext);
    let info = &mut *pInfo;

    info.m_iFreq_Hz         = ctx.sample_rate;
    info.m_bStereo          = if ctx.channels == 2 { TRUE } else { FALSE };
    info.m_b16bit           = TRUE; // we always deliver 16-bit output
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
    let ctx    = &mut *((*pModule).m_pModuleCookie as *mut FlacContext);
    let reader = match ctx.reader.as_mut() { Some(r) => r, None => { *pdwBlockSize = 0; return FALSE; } };

    let requested_out_bytes = *pdwBlockSize as usize;

    if ctx.bytes_per_native_sample == 2 {
        // 16-bit native — read directly into the caller's buffer, no conversion needed.
        let buf = std::slice::from_raw_parts_mut(pBlock as *mut u8, requested_out_bytes);
        match reader.read(buf) {
            Ok(n) if n > 0 => { *pdwBlockSize = n as DWORD; TRUE }
            _              => { *pdwBlockSize = 0; FALSE }
        }
    } else {
        // Non-16-bit native — read native bytes into a temporary buffer and
        // convert each sample to i16 LE before writing to the caller's buffer.
        let bps            = ctx.bits_per_sample as usize;
        let native_bps     = ctx.bytes_per_native_sample as usize;
        let out_samples    = requested_out_bytes / 2; // i16 samples to produce
        let native_needed  = out_samples * native_bps;

        let mut native_buf = vec![0u8; native_needed];
        let n_native = match reader.read(&mut native_buf) {
            Ok(0) | Err(_) => { *pdwBlockSize = 0; return FALSE; }
            Ok(n)          => n,
        };

        let n_samples = n_native / native_bps;
        let out_i16   = std::slice::from_raw_parts_mut(pBlock as *mut i16, n_samples);
        let shift     = bps as i32 - 16;

        for i in 0..n_samples {
            let start = i * native_bps;

            // Assemble little-endian bytes into an i32 (unsigned accumulation first).
            let mut raw = 0u32;
            for b in 0..native_bps {
                raw |= (native_buf[start + b] as u32) << (b * 8);
            }

            // Sign-extend from bps bits to full i32 via arithmetic shift trick.
            let signed: i32 = if bps < 32 {
                let up = 32 - bps;
                ((raw << up) as i32) >> up
            } else {
                raw as i32
            };

            // Scale to 16-bit range.
            let s16 = if shift > 0 {
                signed >> shift
            } else {
                signed << (-shift)
            };
            out_i16[i] = s16.clamp(-32768, 32767) as i16;
        }

        *pdwBlockSize = (n_samples * 2) as DWORD;
        TRUE
    }
}

unsafe extern "C" fn flac_get_current_pos_secs(pModule: *mut CPs_CoDecModule) -> c_int {
    let ctx    = &mut *((*pModule).m_pModuleCookie as *mut FlacContext);
    let reader = match ctx.reader.as_mut() { Some(r) => r, None => return 0 };

    if ctx.bytes_per_native_frame == 0 || ctx.sample_rate == 0 {
        return 0;
    }

    // Query current byte position in the native PCM output stream.
    let current_byte = match reader.seek(SeekFrom::Current(0)) {
        Ok(pos) => pos,
        Err(_)  => return 0,
    };

    let current_sample = current_byte / ctx.bytes_per_native_frame;
    (current_sample / ctx.sample_rate as u64) as c_int
}
