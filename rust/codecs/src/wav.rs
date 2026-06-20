/*
 * WAV codec — port of CPI_Player_CoDec_WAV.c
 *
 * Implements CP_InitialiseCodec_WAV and the seven function-pointer slots
 * in CPs_CoDecModule.  Supports PCM (format tag 1) WAV files only.
 */

use super::ffi::*;
use std::ffi::CStr;
use std::fs::File;
use std::io::{self, Read, Seek, SeekFrom};
use std::os::raw::{c_char, c_int, c_void};

// ---------------------------------------------------------------------------
// Internal codec state
// ---------------------------------------------------------------------------

struct WavContext {
    file:                          Option<File>,
    start_of_wav_data:             u64,
    length_of_wav_data:            u32,
    file_info:                     CPs_FileInfo,
    bytes_per_second:              i32,
    current_offset_secs:           i32,
    current_offset_fraction_bytes: i32,
}

impl WavContext {
    fn new() -> Self {
        WavContext {
            file: None,
            start_of_wav_data: 0,
            length_of_wav_data: 0,
            file_info: CPs_FileInfo {
                m_iFileLength_Secs: 0,
                m_iBitRate_Kbs:     0,
                m_iFreq_Hz:         0,
                m_bStereo:          FALSE,
                m_b16bit:           FALSE,
            },
            bytes_per_second:              0,
            current_offset_secs:           0,
            current_offset_fraction_bytes: 0,
        }
    }
}

// ---------------------------------------------------------------------------
// RIFF parsing helpers
// ---------------------------------------------------------------------------

fn read_u16_le(f: &mut File) -> io::Result<u16> {
    let mut b = [0u8; 2];
    f.read_exact(&mut b)?;
    Ok(u16::from_le_bytes(b))
}

fn read_u32_le(f: &mut File) -> io::Result<u32> {
    let mut b = [0u8; 4];
    f.read_exact(&mut b)?;
    Ok(u32::from_le_bytes(b))
}

fn read_fourcc(f: &mut File) -> io::Result<[u8; 4]> {
    let mut b = [0u8; 4];
    f.read_exact(&mut b)?;
    Ok(b)
}

// Advance past chunks until one with `target` FourCC is found.
// Returns that chunk's data length; file position is at the first data byte.
fn skip_to_chunk(f: &mut File, target: &[u8; 4]) -> io::Result<u32> {
    for _ in 0..4096 {
        let id  = read_fourcc(f)?;
        let len = read_u32_le(f)?;
        if &id == target {
            return Ok(len);
        }
        if len == 0 || len > 0x7FFF_FFFF {
            return Err(io::Error::new(io::ErrorKind::InvalidData, "bad chunk length"));
        }
        f.seek(SeekFrom::Current(len as i64))?;
    }
    Err(io::Error::new(io::ErrorKind::NotFound, "chunk not found"))
}

// ---------------------------------------------------------------------------
// Exported initialiser
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CP_InitialiseCodec_WAV(pCoDec: *mut CPs_CoDecModule) {
    if pCoDec.is_null() {
        return;
    }
    let m = &mut *pCoDec;

    m.Uninitialise       = Some(wav_uninitialise);
    m.OpenFile           = Some(wav_open_file);
    m.CloseFile          = Some(wav_close_file);
    m.Seek               = Some(wav_seek);
    m.GetFileInfo        = Some(wav_get_file_info);
    m.GetPCMBlock        = Some(wav_get_pcm_block);
    m.GetCurrentPos_secs = Some(wav_get_current_pos_secs);

    let ctx = Box::new(WavContext::new());
    m.m_pModuleCookie = Box::into_raw(ctx) as *mut c_void;

    CPFA_InitialiseFileAssociations(pCoDec);
    CPFA_AddFileAssociation(pCoDec, b"WAV\0".as_ptr() as *const c_char, 0);
}

// ---------------------------------------------------------------------------
// Codec entry points
// ---------------------------------------------------------------------------

unsafe extern "C" fn wav_uninitialise(pModule: *mut CPs_CoDecModule) {
    if pModule.is_null() {
        return;
    }
    let m = &mut *pModule;
    if !m.m_pModuleCookie.is_null() {
        drop(Box::from_raw(m.m_pModuleCookie as *mut WavContext));
        m.m_pModuleCookie = std::ptr::null_mut();
    }
    CPFA_EmptyFileAssociations(pModule);
}

unsafe extern "C" fn wav_open_file(
    pModule:     *mut CPs_CoDecModule,
    pcFilename:  *const c_char,
    _dwCookie:   usize,
    _hWndOwner:  *mut c_void,
) -> BOOL {
    let m   = &mut *pModule;
    let ctx = &mut *(m.m_pModuleCookie as *mut WavContext);

    ctx.file = None;

    let path = match CStr::from_ptr(pcFilename).to_str() {
        Ok(s)  => s,
        Err(_) => return FALSE,
    };

    let mut f = match File::open(path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };

    // Skip any leading ID3v2 tag.
    let mut hdr = [0u8; 10];
    let stream_start = if f.read_exact(&mut hdr).is_ok() && hdr.starts_with(b"ID3") {
        let sz = ((hdr[6] as u64 & 0x7F) << 21)
               | ((hdr[7] as u64 & 0x7F) << 14)
               | ((hdr[8] as u64 & 0x7F) <<  7)
               |  (hdr[9] as u64 & 0x7F);
        10 + sz
    } else {
        0u64
    };
    if f.seek(SeekFrom::Start(stream_start)).is_err() {
        return FALSE;
    }

    // Validate RIFF/WAVE header (12 bytes: "RIFF" + size + "WAVE").
    let mut riff = [0u8; 12];
    if f.read_exact(&mut riff).is_err()
        || &riff[0..4] != b"RIFF"
        || &riff[8..12] != b"WAVE"
    {
        return FALSE;
    }

    // Parse fmt chunk.
    let fmt_len = match skip_to_chunk(&mut f, b"fmt ") {
        Ok(l) if l >= 16 => l,
        _                => return FALSE,
    };

    let format_tag      = match read_u16_le(&mut f) { Ok(v) => v, Err(_) => return FALSE };
    let n_channels      = match read_u16_le(&mut f) { Ok(v) => v, Err(_) => return FALSE };
    let n_samples_per_sec = match read_u32_le(&mut f) { Ok(v) => v, Err(_) => return FALSE };
    if f.seek(SeekFrom::Current(6)).is_err() { return FALSE; } // skip nAvgBytesPerSec + nBlockAlign
    let bits_per_sample = match read_u16_le(&mut f) { Ok(v) => v, Err(_) => return FALSE };

    if format_tag != 1 {
        return FALSE; // Only PCM supported
    }

    // Skip any extra fmt bytes (e.g. WAVEFORMATEX cbSize).
    if fmt_len > 16 {
        if f.seek(SeekFrom::Current((fmt_len - 16) as i64)).is_err() {
            return FALSE;
        }
    }

    ctx.file_info.m_iFreq_Hz   = n_samples_per_sec;
    ctx.file_info.m_bStereo    = if n_channels      == 2  { TRUE } else { FALSE };
    ctx.file_info.m_b16bit     = if bits_per_sample == 16 { TRUE } else { FALSE };
    ctx.file_info.m_iBitRate_Kbs =
        (bits_per_sample as u32 * n_channels as u32 * n_samples_per_sec) / 1000;

    ctx.bytes_per_second = n_samples_per_sec as i32
        * if n_channels      == 2  { 2 } else { 1 }
        * if bits_per_sample == 16 { 2 } else { 1 };

    // Locate data chunk.
    let data_len = match skip_to_chunk(&mut f, b"data") {
        Ok(l)  => l,
        Err(_) => return FALSE,
    };

    ctx.length_of_wav_data = data_len;
    ctx.start_of_wav_data  = match f.stream_position() {
        Ok(p)  => p,
        Err(_) => return FALSE,
    };

    if ctx.bytes_per_second > 0 {
        ctx.file_info.m_iFileLength_Secs = data_len / ctx.bytes_per_second as u32;
    }
    ctx.current_offset_secs           = 0;
    ctx.current_offset_fraction_bytes = 0;
    ctx.file = Some(f);

    TRUE
}

unsafe extern "C" fn wav_close_file(pModule: *mut CPs_CoDecModule) {
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut WavContext);
    ctx.file = None;
}

unsafe extern "C" fn wav_seek(
    pModule:     *mut CPs_CoDecModule,
    iNumerator:  c_int,
    iDenominator: c_int,
) {
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut WavContext);
    let f   = match ctx.file.as_mut() { Some(f) => f, None => return };

    let mut seek_pos =
        ((iNumerator as f32 / iDenominator as f32) * ctx.length_of_wav_data as f32) as u32;
    seek_pos &= !0x3u32; // align to 4-byte boundary

    ctx.current_offset_secs           = seek_pos as i32 / ctx.bytes_per_second;
    ctx.current_offset_fraction_bytes = seek_pos as i32 % ctx.bytes_per_second;

    let _ = f.seek(SeekFrom::Start(ctx.start_of_wav_data + seek_pos as u64));
}

unsafe extern "C" fn wav_get_file_info(
    pModule: *mut CPs_CoDecModule,
    pInfo:   *mut CPs_FileInfo,
) {
    let ctx = &*((*pModule).m_pModuleCookie as *const WavContext);
    std::ptr::copy_nonoverlapping(&ctx.file_info, pInfo, 1);
}

unsafe extern "C" fn wav_get_pcm_block(
    pModule:      *mut CPs_CoDecModule,
    pBlock:       *mut c_void,
    pdwBlockSize: *mut DWORD,
) -> BOOL {
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut WavContext);
    let f   = match ctx.file.as_mut() { Some(f) => f, None => { *pdwBlockSize = 0; return FALSE; } };

    let requested = *pdwBlockSize as usize;
    let buf       = std::slice::from_raw_parts_mut(pBlock as *mut u8, requested);

    let n = match f.read(buf) {
        Ok(n) if n > 0 => n,
        _ => { *pdwBlockSize = 0; return FALSE; }
    };

    *pdwBlockSize = n as DWORD;

    ctx.current_offset_fraction_bytes += n as i32;
    if ctx.current_offset_fraction_bytes > ctx.bytes_per_second {
        ctx.current_offset_secs           += 1;
        ctx.current_offset_fraction_bytes -= ctx.bytes_per_second;
    }

    TRUE
}

unsafe extern "C" fn wav_get_current_pos_secs(pModule: *mut CPs_CoDecModule) -> c_int {
    let ctx = &*((*pModule).m_pModuleCookie as *const WavContext);
    ctx.current_offset_secs
}
