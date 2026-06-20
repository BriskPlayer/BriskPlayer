/*
 * WAV codec — port of CPI_Player_CoDec_WAV.c
 *
 * I/O goes through CP_CreateInStream via InStream (see ffi.rs), so both local
 * files and internet WAV streams work transparently.
 *
 * Unsafe usage is confined to:
 *   1. Extracting the typed context from m_pModuleCookie.
 *   2. Calling InStream::open (wraps CP_CreateInStream).
 *   3. Box::from_raw / ptr::copy_nonoverlapping / slice::from_raw_parts_mut.
 * All RIFF parsing and PCM delivery is safe Rust calling Read/Seek trait methods.
 */

use super::ffi::*;
use std::io::{self, Read, Seek, SeekFrom};
use std::os::raw::{c_char, c_int, c_void};

// ---------------------------------------------------------------------------
// Internal codec state
// ---------------------------------------------------------------------------

struct WavContext {
    /// Open stream; None when no file is open.  Drop calls uninitialise.
    stream:   Option<InStream>,
    seekable: bool,
    open:     bool,

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
            stream:   None,
            seekable: false,
            open:     false,
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
// RIFF parsing helpers (generic over Read / Read+Seek)
// ---------------------------------------------------------------------------

fn read_u16_le<R: Read>(f: &mut R) -> io::Result<u16> {
    let mut b = [0u8; 2];
    f.read_exact(&mut b)?;
    Ok(u16::from_le_bytes(b))
}

fn read_u32_le<R: Read>(f: &mut R) -> io::Result<u32> {
    let mut b = [0u8; 4];
    f.read_exact(&mut b)?;
    Ok(u32::from_le_bytes(b))
}

fn read_fourcc<R: Read>(f: &mut R) -> io::Result<[u8; 4]> {
    let mut b = [0u8; 4];
    f.read_exact(&mut b)?;
    Ok(b)
}

/// Scan forward through RIFF chunks until `target` is found.
/// Unknown chunks are skipped: seekable streams seek past them;
/// non-seekable streams drain them byte-by-byte.
fn skip_to_chunk<R: Read + Seek>(f: &mut R, target: &[u8; 4]) -> io::Result<u32> {
    for _ in 0..4096 {
        let id  = read_fourcc(f)?;
        let len = read_u32_le(f)?;
        if &id == target {
            return Ok(len);
        }
        if len == 0 || len > 0x7FFF_FFFF {
            return Err(io::Error::new(io::ErrorKind::InvalidData, "bad chunk length"));
        }
        // Try a forward seek; fall back to byte-draining on non-seekable streams.
        if f.seek(SeekFrom::Current(len as i64)).is_err() {
            let mut discard = vec![0u8; len as usize];
            f.read_exact(&mut discard)?;
        }
    }
    Err(io::Error::new(io::ErrorKind::NotFound, "chunk not found"))
}

// ---------------------------------------------------------------------------
// Exported initialiser
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CP_InitialiseCodec_WAV(pCoDec: *mut CPs_CoDecModule) {
    if pCoDec.is_null() { return; }
    // SAFETY: pCoDec is non-null, checked above.
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

/// Drop the stream to trigger RAII cleanup.  Safe Rust.
fn close_wav_stream(ctx: &mut WavContext) {
    ctx.open   = false;
    ctx.stream = None; // Drop InStream → calls uninitialise
}

unsafe extern "C" fn wav_uninitialise(pModule: *mut CPs_CoDecModule) {
    if pModule.is_null() { return; }
    // SAFETY: pModule is valid when called from C via the vtable.
    let m = &mut *pModule;
    if !m.m_pModuleCookie.is_null() {
        {
            // SAFETY: m_pModuleCookie was set to Box::into_raw(WavContext) in CP_InitialiseCodec_WAV.
            let ctx = &mut *(m.m_pModuleCookie as *mut WavContext);
            close_wav_stream(ctx); // safe
        }
        // SAFETY: same Box::into_raw provenance.
        drop(Box::from_raw(m.m_pModuleCookie as *mut WavContext));
        m.m_pModuleCookie = std::ptr::null_mut();
    }
    CPFA_EmptyFileAssociations(pModule);
}

unsafe extern "C" fn wav_open_file(
    pModule:    *mut CPs_CoDecModule,
    pcFilename: *const c_char,
    _dwCookie:  usize,
    hwnd_owner: *mut c_void,
) -> BOOL {
    // SAFETY: pModule and m_pModuleCookie are valid.
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut WavContext);

    close_wav_stream(ctx); // safe

    // SAFETY: pcFilename is a valid null-terminated C string; hwnd_owner is a valid HWND or null.
    let mut stream = match InStream::open(pcFilename, hwnd_owner) {
        Some(s) => s,
        None    => return FALSE,
    };

    let seekable = stream.seekable; // safe field
    ctx.seekable = seekable;

    // Seekable streams only: skip any leading ID3v2 tag.
    if seekable {
        let mut hdr = [0u8; 10];
        let stream_start = if stream.read_exact(&mut hdr).is_ok() && hdr.starts_with(b"ID3") {
            let sz = ((hdr[6] as u64 & 0x7F) << 21)
                   | ((hdr[7] as u64 & 0x7F) << 14)
                   | ((hdr[8] as u64 & 0x7F) <<  7)
                   |  (hdr[9] as u64 & 0x7F);
            10 + sz
        } else {
            0u64
        };
        if stream.seek(SeekFrom::Start(stream_start)).is_err() {
            return FALSE; // stream dropped automatically
        }
    }

    // Validate RIFF/WAVE header.
    let mut riff = [0u8; 12];
    if stream.read_exact(&mut riff).is_err()
        || &riff[0..4] != b"RIFF"
        || &riff[8..12] != b"WAVE"
    {
        return FALSE; // stream dropped automatically
    }

    // Parse fmt chunk.
    let fmt_len = match skip_to_chunk(&mut stream, b"fmt ") { // safe: InStream: Read+Seek
        Ok(l) if l >= 16 => l,
        _ => return FALSE,
    };

    macro_rules! read_u16 {
        () => { match read_u16_le(&mut stream) { Ok(v) => v, Err(_) => return FALSE } }
    }
    macro_rules! read_u32 {
        () => { match read_u32_le(&mut stream) { Ok(v) => v, Err(_) => return FALSE } }
    }

    let format_tag        = read_u16!();
    let n_channels        = read_u16!();
    let n_samples_per_sec = read_u32!();

    // Skip nAvgBytesPerSec + nBlockAlign (6 bytes): try seek, drain as fallback.
    if stream.seek(SeekFrom::Current(6)).is_err() {
        let mut discard = [0u8; 6];
        if stream.read_exact(&mut discard).is_err() { return FALSE; }
    }

    let bits_per_sample = read_u16!();

    if format_tag != 1 {
        return FALSE; // only PCM supported
    }

    if fmt_len > 16 {
        let extra = (fmt_len - 16) as i64;
        if stream.seek(SeekFrom::Current(extra)).is_err() {
            let mut discard = vec![0u8; (fmt_len - 16) as usize];
            if stream.read_exact(&mut discard).is_err() { return FALSE; }
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

    let data_len = match skip_to_chunk(&mut stream, b"data") { // safe
        Ok(l)  => l,
        Err(_) => return FALSE,
    };

    ctx.length_of_wav_data = data_len;
    ctx.start_of_wav_data  = stream.pos(); // current position after chunk header — safe

    if ctx.bytes_per_second > 0 {
        ctx.file_info.m_iFileLength_Secs = data_len / ctx.bytes_per_second as u32;
    }
    ctx.current_offset_secs           = 0;
    ctx.current_offset_fraction_bytes = 0;
    ctx.open   = true;
    ctx.stream = Some(stream); // transfer ownership — no raw pointer in context

    TRUE
}

unsafe extern "C" fn wav_close_file(pModule: *mut CPs_CoDecModule) {
    // SAFETY: m_pModuleCookie is valid.
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut WavContext);
    close_wav_stream(ctx); // safe
}

unsafe extern "C" fn wav_seek(
    pModule:      *mut CPs_CoDecModule,
    iNumerator:   c_int,
    iDenominator: c_int,
) {
    // SAFETY: m_pModuleCookie is valid.
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut WavContext);
    if !ctx.open || !ctx.seekable { return; }

    let stream = match ctx.stream.as_mut() { Some(s) => s, None => return };

    let mut seek_pos =
        ((iNumerator as f32 / iDenominator as f32) * ctx.length_of_wav_data as f32) as u32;
    seek_pos &= !0x3u32; // align to 4-byte boundary

    ctx.current_offset_secs           = seek_pos as i32 / ctx.bytes_per_second;
    ctx.current_offset_fraction_bytes = seek_pos as i32 % ctx.bytes_per_second;

    let target = ctx.start_of_wav_data + seek_pos as u64;
    stream.seek(SeekFrom::Start(target)).ok(); // Seek::seek is safe
}

unsafe extern "C" fn wav_get_file_info(
    pModule: *mut CPs_CoDecModule,
    pInfo:   *mut CPs_FileInfo,
) {
    // SAFETY: both pointers are valid when called from C.
    let ctx = &*((*pModule).m_pModuleCookie as *const WavContext);
    std::ptr::copy_nonoverlapping(&ctx.file_info, pInfo, 1);
}

unsafe extern "C" fn wav_get_pcm_block(
    pModule:      *mut CPs_CoDecModule,
    pBlock:       *mut c_void,
    pdwBlockSize: *mut DWORD,
) -> BOOL {
    // SAFETY: all pointers are valid when called from C.
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut WavContext);
    if !ctx.open {
        *pdwBlockSize = 0;
        return FALSE;
    }

    let stream = match ctx.stream.as_mut() { Some(s) => s, None => { *pdwBlockSize = 0; return FALSE; } };

    let requested = *pdwBlockSize as usize;
    // SAFETY: pBlock points to a caller-owned buffer of at least `requested` bytes.
    let buf = std::slice::from_raw_parts_mut(pBlock as *mut u8, requested);

    let got = stream.read(buf).unwrap_or(0); // Read::read is safe

    if got == 0 {
        *pdwBlockSize = 0;
        return FALSE;
    }

    *pdwBlockSize = got as DWORD;

    ctx.current_offset_fraction_bytes += got as i32;
    if ctx.bytes_per_second > 0
        && ctx.current_offset_fraction_bytes >= ctx.bytes_per_second
    {
        ctx.current_offset_secs           += 1;
        ctx.current_offset_fraction_bytes -= ctx.bytes_per_second;
    }

    TRUE
}

unsafe extern "C" fn wav_get_current_pos_secs(pModule: *mut CPs_CoDecModule) -> c_int {
    // SAFETY: m_pModuleCookie is valid.
    let ctx = &*((*pModule).m_pModuleCookie as *const WavContext);
    ctx.current_offset_secs
}
