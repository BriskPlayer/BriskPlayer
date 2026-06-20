/*
 * BriskPlayer — WAV codec, Rust implementation.
 * Implements the same CPs_CoDecModule interface as CPI_Player_CoDec_WAV.c.
 *
 * The C side calls CP_InitialiseCodec_WAV (exported via #[no_mangle])
 * which fills in the function-pointer table. All other entry points are
 * not exported by name; they are stored as function pointers in the module
 * struct and called through that table by the player engine.
 *
 * CPFA_* file-association helpers remain in C and are declared extern here;
 * the linker resolves them when this static library is merged into the
 * final executable alongside the C object files.
 */

#![allow(non_snake_case, non_camel_case_types)]

use std::ffi::CStr;
use std::fs::File;
use std::io::{self, Read, Seek, SeekFrom};
use std::os::raw::{c_char, c_int, c_void};

// ---------------------------------------------------------------------------
// Windows primitive type aliases matching the C headers
// ---------------------------------------------------------------------------

type BOOL = c_int;
type DWORD = u32;
type UINT = u32;

const TRUE: BOOL = 1;
const FALSE: BOOL = 0;

// ---------------------------------------------------------------------------
// Mirror of CPs_FileInfo (globals.h)
// Layout must be byte-for-byte identical to the C struct.
// ---------------------------------------------------------------------------

#[repr(C)]
pub struct CPs_FileInfo {
    pub m_iFileLength_Secs: UINT,
    pub m_iBitRate_Kbs: UINT,
    pub m_iFreq_Hz: UINT,
    pub m_bStereo: BOOL,
    pub m_b16bit: BOOL,
}

// ---------------------------------------------------------------------------
// Mirror of CPs_CoDecModule (CPI_Player_CoDec.h)
// ---------------------------------------------------------------------------

#[repr(C)]
pub struct CPs_CoDecModule {
    pub Uninitialise: Option<unsafe extern "C" fn(*mut CPs_CoDecModule)>,
    pub OpenFile:
        Option<unsafe extern "C" fn(*mut CPs_CoDecModule, *const c_char, usize, *mut c_void) -> BOOL>,
    pub CloseFile: Option<unsafe extern "C" fn(*mut CPs_CoDecModule)>,
    pub Seek: Option<unsafe extern "C" fn(*mut CPs_CoDecModule, c_int, c_int)>,
    pub GetFileInfo: Option<unsafe extern "C" fn(*mut CPs_CoDecModule, *mut CPs_FileInfo)>,
    pub GetPCMBlock:
        Option<unsafe extern "C" fn(*mut CPs_CoDecModule, *mut c_void, *mut DWORD) -> BOOL>,
    pub GetCurrentPos_secs: Option<unsafe extern "C" fn(*mut CPs_CoDecModule) -> c_int>,
    pub m_pModuleCookie: *mut c_void,
    pub m_pFileAssociationCookie: *mut c_void,
}

// SAFETY: The module struct is pinned behind a raw pointer and only accessed
// from the single-threaded player engine, matching the original C guarantees.
unsafe impl Send for CPs_CoDecModule {}

// ---------------------------------------------------------------------------
// File-association helpers — defined in CPI_Player_FileAssoc.c, resolved at
// final link time when this static library is merged into the executable.
// ---------------------------------------------------------------------------

extern "C" {
    fn CPFA_InitialiseFileAssociations(pCoDec: *mut CPs_CoDecModule);
    fn CPFA_EmptyFileAssociations(pCoDec: *mut CPs_CoDecModule);
    fn CPFA_AddFileAssociation(
        pCoDec: *mut CPs_CoDecModule,
        pcExtension: *const c_char,
        dwCookie: usize,
    );
}

// ---------------------------------------------------------------------------
// Internal codec state
// ---------------------------------------------------------------------------

struct WavContext {
    file: Option<File>,
    start_of_wav_data: u64,
    length_of_wav_data: u32,
    file_info: CPs_FileInfo,
    bytes_per_second: i32,
    current_offset_secs: i32,
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
                m_iBitRate_Kbs: 0,
                m_iFreq_Hz: 0,
                m_bStereo: FALSE,
                m_b16bit: FALSE,
            },
            bytes_per_second: 0,
            current_offset_secs: 0,
            current_offset_fraction_bytes: 0,
        }
    }
}

// ---------------------------------------------------------------------------
// Low-level RIFF parsing helpers
// ---------------------------------------------------------------------------

fn read_u16_le(file: &mut File) -> io::Result<u16> {
    let mut buf = [0u8; 2];
    file.read_exact(&mut buf)?;
    Ok(u16::from_le_bytes(buf))
}

fn read_u32_le(file: &mut File) -> io::Result<u32> {
    let mut buf = [0u8; 4];
    file.read_exact(&mut buf)?;
    Ok(u32::from_le_bytes(buf))
}

fn read_fourcc(file: &mut File) -> io::Result<[u8; 4]> {
    let mut buf = [0u8; 4];
    file.read_exact(&mut buf)?;
    Ok(buf)
}

// Advance the file past chunks until one with `target` ID is found.
// Returns the chunk's data length; the file position is left at the first
// byte of that chunk's data (matching SkipToChunk in CPI_Player_CoDec_WAV.c).
fn skip_to_chunk(file: &mut File, target: &[u8; 4]) -> io::Result<u32> {
    for _ in 0..4096 {
        let id = read_fourcc(file)?;
        let len = read_u32_le(file)?;
        if &id == target {
            return Ok(len);
        }
        // Reject zero-length or oversized chunks — same guard as the C code.
        if len == 0 || len > 0x7FFF_FFFF {
            return Err(io::Error::new(io::ErrorKind::InvalidData, "bad chunk length"));
        }
        file.seek(SeekFrom::Current(len as i64))?;
    }
    Err(io::Error::new(io::ErrorKind::NotFound, "chunk not found"))
}

// ---------------------------------------------------------------------------
// Exported initialiser — called by CPI_Player_Engine.c
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CP_InitialiseCodec_WAV(pCoDec: *mut CPs_CoDecModule) {
    if pCoDec.is_null() {
        return;
    }
    let module = &mut *pCoDec;

    module.Uninitialise = Some(wav_uninitialise);
    module.OpenFile = Some(wav_open_file);
    module.CloseFile = Some(wav_close_file);
    module.Seek = Some(wav_seek);
    module.GetFileInfo = Some(wav_get_file_info);
    module.GetPCMBlock = Some(wav_get_pcm_block);
    module.GetCurrentPos_secs = Some(wav_get_current_pos_secs);

    let ctx = Box::new(WavContext::new());
    module.m_pModuleCookie = Box::into_raw(ctx) as *mut c_void;

    CPFA_InitialiseFileAssociations(pCoDec);
    CPFA_AddFileAssociation(pCoDec, b"WAV\0".as_ptr() as *const c_char, 0);
}

// ---------------------------------------------------------------------------
// Codec entry points (stored as function pointers, not exported by name)
// ---------------------------------------------------------------------------

unsafe extern "C" fn wav_uninitialise(pModule: *mut CPs_CoDecModule) {
    if pModule.is_null() {
        return;
    }
    let module = &mut *pModule;
    if !module.m_pModuleCookie.is_null() {
        // Dropping the Box closes any open File via Drop.
        drop(Box::from_raw(module.m_pModuleCookie as *mut WavContext));
        module.m_pModuleCookie = std::ptr::null_mut();
    }
    CPFA_EmptyFileAssociations(pModule);
}

unsafe extern "C" fn wav_open_file(
    pModule: *mut CPs_CoDecModule,
    pcFilename: *const c_char,
    _dwCookie: usize,
    _hWndOwner: *mut c_void,
) -> BOOL {
    let module = &mut *pModule;
    let ctx = &mut *(module.m_pModuleCookie as *mut WavContext);

    ctx.file = None;

    let filename = match CStr::from_ptr(pcFilename).to_str() {
        Ok(s) => s,
        Err(_) => return FALSE,
    };

    let mut file = match File::open(filename) {
        Ok(f) => f,
        Err(_) => return FALSE,
    };

    // Skip any leading ID3v2 tag (same logic as the C implementation).
    let mut hdr10 = [0u8; 10];
    let stream_start = if file.read_exact(&mut hdr10).is_ok() && hdr10.starts_with(b"ID3") {
        let tag_size = ((hdr10[6] as u64 & 0x7F) << 21)
            | ((hdr10[7] as u64 & 0x7F) << 14)
            | ((hdr10[8] as u64 & 0x7F) << 7)
            | (hdr10[9] as u64 & 0x7F);
        10 + tag_size
    } else {
        0u64
    };

    if file.seek(SeekFrom::Start(stream_start)).is_err() {
        return FALSE;
    }

    // Validate the RIFF/WAVE header (12 bytes: "RIFF" + size + "WAVE").
    let mut riff = [0u8; 12];
    if file.read_exact(&mut riff).is_err()
        || &riff[0..4] != b"RIFF"
        || &riff[8..12] != b"WAVE"
    {
        return FALSE;
    }

    // Locate and parse the fmt chunk.
    let fmt_len = match skip_to_chunk(&mut file, b"fmt ") {
        Ok(l) if l >= 16 => l,
        _ => return FALSE,
    };

    let format_tag = match read_u16_le(&mut file) {
        Ok(v) => v,
        Err(_) => return FALSE,
    };
    let n_channels = match read_u16_le(&mut file) {
        Ok(v) => v,
        Err(_) => return FALSE,
    };
    let n_samples_per_sec = match read_u32_le(&mut file) {
        Ok(v) => v,
        Err(_) => return FALSE,
    };
    // Skip nAvgBytesPerSec (4) and nBlockAlign (2).
    if file.seek(SeekFrom::Current(6)).is_err() {
        return FALSE;
    }
    let bits_per_sample = match read_u16_le(&mut file) {
        Ok(v) => v,
        Err(_) => return FALSE,
    };

    // Only PCM (format tag 1) is supported.
    if format_tag != 1 {
        return FALSE;
    }

    // Skip any extra fmt bytes (e.g. WAVEFORMATEX cbSize field).
    let consumed = 16u32;
    if fmt_len > consumed {
        if file.seek(SeekFrom::Current((fmt_len - consumed) as i64)).is_err() {
            return FALSE;
        }
    }

    ctx.file_info.m_iFreq_Hz = n_samples_per_sec;
    ctx.file_info.m_bStereo = if n_channels == 2 { TRUE } else { FALSE };
    ctx.file_info.m_b16bit = if bits_per_sample == 16 { TRUE } else { FALSE };
    ctx.file_info.m_iBitRate_Kbs =
        (bits_per_sample as u32 * n_channels as u32 * n_samples_per_sec) / 1000;

    ctx.bytes_per_second = n_samples_per_sec as i32
        * if n_channels == 2 { 2 } else { 1 }
        * if bits_per_sample == 16 { 2 } else { 1 };

    // Locate the data chunk and record its position.
    let data_len = match skip_to_chunk(&mut file, b"data") {
        Ok(l) => l,
        Err(_) => return FALSE,
    };

    ctx.length_of_wav_data = data_len;
    ctx.start_of_wav_data = match file.stream_position() {
        Ok(p) => p,
        Err(_) => return FALSE,
    };

    if ctx.bytes_per_second > 0 {
        ctx.file_info.m_iFileLength_Secs = data_len / ctx.bytes_per_second as u32;
    }
    ctx.current_offset_secs = 0;
    ctx.current_offset_fraction_bytes = 0;
    ctx.file = Some(file);

    TRUE
}

unsafe extern "C" fn wav_close_file(pModule: *mut CPs_CoDecModule) {
    let module = &mut *pModule;
    let ctx = &mut *(module.m_pModuleCookie as *mut WavContext);
    ctx.file = None;
}

unsafe extern "C" fn wav_seek(
    pModule: *mut CPs_CoDecModule,
    iNumerator: c_int,
    iDenominator: c_int,
) {
    let module = &mut *pModule;
    let ctx = &mut *(module.m_pModuleCookie as *mut WavContext);

    let file = match ctx.file.as_mut() {
        Some(f) => f,
        None => return,
    };

    let mut seek_pos =
        ((iNumerator as f32 / iDenominator as f32) * ctx.length_of_wav_data as f32) as u32;

    // Round down to nearest 4-byte sample boundary (matches the C ~0x3 mask).
    seek_pos &= !0x3u32;

    ctx.current_offset_secs = seek_pos as i32 / ctx.bytes_per_second;
    ctx.current_offset_fraction_bytes = seek_pos as i32 % ctx.bytes_per_second;

    let _ = file.seek(SeekFrom::Start(ctx.start_of_wav_data + seek_pos as u64));
}

unsafe extern "C" fn wav_get_file_info(
    pModule: *mut CPs_CoDecModule,
    pInfo: *mut CPs_FileInfo,
) {
    let module = &mut *pModule;
    let ctx = &*(module.m_pModuleCookie as *const WavContext);
    std::ptr::copy_nonoverlapping(&ctx.file_info, pInfo, 1);
}

unsafe extern "C" fn wav_get_pcm_block(
    pModule: *mut CPs_CoDecModule,
    pBlock: *mut c_void,
    pdwBlockSize: *mut DWORD,
) -> BOOL {
    let module = &mut *pModule;
    let ctx = &mut *(module.m_pModuleCookie as *mut WavContext);

    let file = match ctx.file.as_mut() {
        Some(f) => f,
        None => return FALSE,
    };

    let requested = *pdwBlockSize as usize;
    let buf = std::slice::from_raw_parts_mut(pBlock as *mut u8, requested);

    let bytes_read = match file.read(buf) {
        Ok(n) if n > 0 => n,
        _ => {
            *pdwBlockSize = 0;
            return FALSE;
        }
    };

    *pdwBlockSize = bytes_read as DWORD;

    ctx.current_offset_fraction_bytes += bytes_read as i32;
    if ctx.current_offset_fraction_bytes > ctx.bytes_per_second {
        ctx.current_offset_secs += 1;
        ctx.current_offset_fraction_bytes -= ctx.bytes_per_second;
    }

    TRUE
}

unsafe extern "C" fn wav_get_current_pos_secs(pModule: *mut CPs_CoDecModule) -> c_int {
    let module = &mut *pModule;
    let ctx = &*(module.m_pModuleCookie as *const WavContext);
    ctx.current_offset_secs
}
