/*
 * MPEG/MP3 codec — pure-Rust implementation using the nanomp3 crate.
 *
 * Replaces CPI_Player_CoDec_MPEG.c (which depends on libmad via vcpkg).
 * nanomp3 is a c2rust translation of minimp3 with no_std support.
 *
 * Registers "MP3", "MP2", and "MP1" extensions.
 * Always delivers 16-bit signed interleaved PCM to the engine.
 *
 * Duration and seek use a CBR bitrate estimate derived from the first decoded
 * frame; VBR files report approximate values, which is acceptable for a UI.
 */

use super::ffi::*;
use nanomp3::{Decoder, MAX_SAMPLES_PER_FRAME};
use std::ffi::CStr;
use std::fs;
use std::io::{Read, Seek, SeekFrom};
use std::os::raw::{c_char, c_int, c_void};

// How many compressed bytes we try to keep in the input buffer at once.
const INPUT_BUF_TARGET: usize = 16 * 1024;

// ---------------------------------------------------------------------------
// Internal codec state
// ---------------------------------------------------------------------------

struct Mp3Context {
    file:             Option<std::fs::File>,
    file_size:        u64,
    /// File offset of first audio byte (past any leading ID3v2 tag).
    stream_start:     u64,
    /// File read-head — next byte to be read into input_buf.
    file_read_pos:    u64,

    /// Raw compressed MP3 data not yet fed to the decoder.
    /// Unconsumed bytes are always stored from index 0.
    input_buf:        Vec<u8>,

    decoder:          Decoder,

    /// Decoded 16-bit PCM samples waiting to be delivered to the caller.
    pcm_queue:        Vec<i16>,

    // Metadata (populated after the first successfully decoded frame)
    sample_rate:      u32,
    channels:         u8,
    bitrate_kbps:     u32,
    total_duration_secs: u32,

    /// Compressed bytes consumed by the decoder since stream_start.
    /// Used to approximate the current playback position.
    bytes_consumed:   u64,
}

impl Mp3Context {
    fn new() -> Self {
        Mp3Context {
            file:                None,
            file_size:           0,
            stream_start:        0,
            file_read_pos:       0,
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

    /// Pull more compressed data from the file into input_buf.
    fn refill_input(&mut self) {
        let f = match self.file.as_mut() {
            Some(f) => f,
            None    => return,
        };
        let have      = self.input_buf.len();
        let needed    = INPUT_BUF_TARGET.saturating_sub(have);
        if needed == 0 { return; }
        let avail_in_file = self.file_size.saturating_sub(self.file_read_pos) as usize;
        let to_read   = needed.min(avail_in_file);
        if to_read == 0 { return; }

        let old_len = self.input_buf.len();
        self.input_buf.resize(old_len + to_read, 0);
        match f.read(&mut self.input_buf[old_len..]) {
            Ok(n) if n > 0 => {
                self.input_buf.truncate(old_len + n);
                self.file_read_pos += n as u64;
            }
            _ => {
                self.input_buf.truncate(old_len);
            }
        }
    }

    /// Decode one MP3 frame and append the resulting i16 PCM to pcm_queue.
    /// Returns true if a frame was produced.
    fn decode_one(&mut self) -> bool {
        if self.input_buf.len() < INPUT_BUF_TARGET / 2 {
            self.refill_input();
        }
        if self.input_buf.is_empty() {
            return false;
        }

        let mut pcm_scratch = vec![0f32; MAX_SAMPLES_PER_FRAME];
        let (consumed, maybe_info) = self.decoder.decode(&self.input_buf, &mut pcm_scratch);

        if consumed == 0 {
            // Decoder needs more input; try once more after a refill.
            self.refill_input();
            return false;
        }

        self.input_buf.drain(..consumed);
        self.bytes_consumed += consumed as u64;

        if let Some(info) = maybe_info {
            // Cache metadata from the first decoded frame.
            if self.sample_rate == 0 {
                self.sample_rate  = info.sample_rate;
                self.channels     = info.channels.num();
                self.bitrate_kbps = info.bitrate;
                let audio_bytes   = self.file_size.saturating_sub(self.stream_start);
                if self.bitrate_kbps > 0 {
                    self.total_duration_secs =
                        (audio_bytes * 8 / (self.bitrate_kbps as u64 * 1000)) as u32;
                }
            }

            // samples_produced is per-channel; total interleaved samples = produced × channels.
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

    /// How many i16 samples are waiting in pcm_queue.
    fn pcm_available(&self) -> usize {
        self.pcm_queue.len()
    }
}

// ---------------------------------------------------------------------------
// Exported initialiser
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CP_InitialiseCodec_MPEG(pCoDec: *mut CPs_CoDecModule) {
    if pCoDec.is_null() {
        return;
    }
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
    if pModule.is_null() {
        return;
    }
    let m = &mut *pModule;
    if !m.m_pModuleCookie.is_null() {
        drop(Box::from_raw(m.m_pModuleCookie as *mut Mp3Context));
        m.m_pModuleCookie = std::ptr::null_mut();
    }
    CPFA_EmptyFileAssociations(pModule);
}

unsafe extern "C" fn mp3_open_file(
    pModule:    *mut CPs_CoDecModule,
    pcFilename: *const c_char,
    _dwCookie:  usize,
    _hWndOwner: *mut c_void,
) -> BOOL {
    let m   = &mut *pModule;
    let ctx = &mut *(m.m_pModuleCookie as *mut Mp3Context);

    // Reset all mutable state.
    ctx.file = None;
    ctx.input_buf.clear();
    ctx.pcm_queue.clear();
    ctx.decoder          = Decoder::new();
    ctx.sample_rate      = 0;
    ctx.channels         = 0;
    ctx.bitrate_kbps     = 0;
    ctx.total_duration_secs = 0;
    ctx.bytes_consumed   = 0;
    ctx.file_read_pos    = 0;

    let path = match CStr::from_ptr(pcFilename).to_str() {
        Ok(s)  => s,
        Err(_) => return FALSE,
    };

    ctx.file_size = fs::metadata(path).map(|md| md.len()).unwrap_or(0);
    if ctx.file_size == 0 {
        return FALSE;
    }

    let mut f = match std::fs::File::open(path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };

    // Skip a leading ID3v2 tag so our bitrate-based duration estimate uses
    // the audio payload size rather than the full file size.
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

    ctx.stream_start  = stream_start;
    ctx.file_read_pos = stream_start;
    ctx.file = Some(f);

    // Probe up to 32 frames to populate metadata.
    for _ in 0..32 {
        if ctx.sample_rate != 0 {
            break;
        }
        ctx.decode_one();
    }

    if ctx.sample_rate == 0 {
        ctx.file = None;
        return FALSE;
    }

    TRUE
}

unsafe extern "C" fn mp3_close_file(pModule: *mut CPs_CoDecModule) {
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut Mp3Context);
    ctx.file = None;
    ctx.input_buf.clear();
    ctx.pcm_queue.clear();
}

unsafe extern "C" fn mp3_seek(
    pModule:      *mut CPs_CoDecModule,
    iNumerator:   c_int,
    iDenominator: c_int,
) {
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut Mp3Context);
    let f   = match ctx.file.as_mut() { Some(f) => f, None => return };

    if iDenominator == 0 || ctx.file_size == 0 {
        return;
    }

    let audio_len     = ctx.file_size.saturating_sub(ctx.stream_start);
    let ratio         = iNumerator as f64 / iDenominator as f64;
    let target_offset = ((ratio * audio_len as f64) as u64).min(audio_len.saturating_sub(1));
    let file_pos      = ctx.stream_start + target_offset;

    if f.seek(SeekFrom::Start(file_pos)).is_err() {
        return;
    }

    ctx.file_read_pos  = file_pos;
    ctx.bytes_consumed = target_offset;
    ctx.input_buf.clear();
    ctx.pcm_queue.clear();
    // Reset the decoder so it resynchronises cleanly at the new position.
    ctx.decoder = Decoder::new();
}

unsafe extern "C" fn mp3_get_file_info(
    pModule: *mut CPs_CoDecModule,
    pInfo:   *mut CPs_FileInfo,
) {
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
    let ctx = &mut *((*pModule).m_pModuleCookie as *mut Mp3Context);

    if ctx.file.is_none() {
        *pdwBlockSize = 0;
        return FALSE;
    }

    let requested_bytes   = *pdwBlockSize as usize;
    let requested_samples = requested_bytes / 2; // i16 = 2 bytes

    // Decode frames until we have enough samples or no progress is possible.
    // We track ALL forms of progress, not just PCM output: after a seek, minimp3
    // may consume several frames worth of bytes to resync before producing PCM.
    // Breaking only on "no input consumed AND no new file data AND no PCM" avoids
    // the false-EOF that would advance to the next song right after a seek.
    loop {
        if ctx.pcm_available() >= requested_samples {
            break;
        }
        let pcm_before   = ctx.pcm_queue.len();
        let input_before = ctx.input_buf.len();
        let file_before  = ctx.file_read_pos;
        ctx.decode_one();
        let any_progress = ctx.pcm_queue.len()  != pcm_before
            || ctx.input_buf.len()  != input_before
            || ctx.file_read_pos    != file_before;
        if !any_progress {
            break;
        }
    }

    let available = ctx.pcm_available();
    if available == 0 {
        *pdwBlockSize = 0;
        return FALSE;
    }

    let to_deliver = available.min(requested_samples);
    let dst = std::slice::from_raw_parts_mut(pBlock as *mut i16, to_deliver);
    dst.copy_from_slice(&ctx.pcm_queue[..to_deliver]);
    ctx.pcm_queue.drain(..to_deliver);

    *pdwBlockSize = (to_deliver * 2) as DWORD;
    TRUE
}

unsafe extern "C" fn mp3_get_current_pos_secs(pModule: *mut CPs_CoDecModule) -> c_int {
    let ctx = &*((*pModule).m_pModuleCookie as *const Mp3Context);
    if ctx.bitrate_kbps == 0 {
        return 0;
    }
    // Approximate: compressed bytes consumed ÷ (bitrate in bytes/sec)
    (ctx.bytes_consumed * 8 / (ctx.bitrate_kbps as u64 * 1000)) as c_int
}
