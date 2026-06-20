/*
 * tags.rs — lofty-based metadata implementation replacing TagLib.
 *
 * Implements all CPTL_* metadata I/O functions.  Functions that need Windows
 * GDI+/WIC (HBITMAP, album-art cache, CPTL_CreateBitmapFromImageData, etc.)
 * stay in CPI_TagLib.c.
 *
 * String ownership: all `*mut c_char` fields in CPs_AllMetadata/CPs_AlbumArt
 * are allocated by Rust via CString::into_raw() and freed by Rust via
 * CString::from_raw() inside CPTL_FreeMetadata / CPTL_FreeAlbumArt.
 * On Windows/MinGW both Rust and C use the same process heap, so callers
 * that reach for free() directly will not crash, but the canonical path is
 * through the provided free functions.
 */

use super::ffi::{BOOL, FALSE, TRUE};
use lofty::config::WriteOptions;
use lofty::file::FileType;
use lofty::picture::{MimeType, Picture, PictureType};
use lofty::prelude::*;
use lofty::tag::{ItemKey, Tag};
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_uint, c_uchar, c_ushort};
use std::ptr;

// ---------------------------------------------------------------------------
// C-compatible struct mirrors
// ---------------------------------------------------------------------------

/// Mirror of CPs_AllMetadata from CPI_TagLib.h.
/// Field order and types must match the C declaration exactly so that
/// #[repr(C)] produces an identical memory layout.
#[repr(C)]
pub struct CPs_AllMetadata {
    // Basic tags
    pub m_pcTitle:       *mut c_char,
    pub m_pcArtist:      *mut c_char,
    pub m_pcAlbum:       *mut c_char,
    pub m_pcYear:        *mut c_char,
    pub m_pcComment:     *mut c_char,
    pub m_pcGenre:       *mut c_char,
    pub m_iTrackNum:     c_uint,
    pub m_iLength:       c_uint,
    pub m_iTagType:      c_int,
    // Extended metadata
    pub m_pcComposer:    *mut c_char,
    pub m_pcAlbumArtist: *mut c_char,
    pub m_pcGrouping:    *mut c_char,
    pub m_pcCopyright:   *mut c_char,
    pub m_pcLyrics:      *mut c_char,
    pub m_iDiscNumber:   c_ushort,
    pub m_iBPM:          c_ushort,
    // ReplayGain
    pub m_fTrackGain:    f32,
    pub m_fTrackPeak:    f32,
    pub m_fAlbumGain:    f32,
    pub m_fAlbumPeak:    f32,
    // Audio properties
    pub m_iBitrate:      c_uint,
    pub m_iSampleRate:   c_uint,
    pub m_iBitDepth:     c_ushort,
    pub m_cChannels:     c_uchar,
    pub m_pcCodec:       *mut c_char,
    pub m_pcBitrateMode: *mut c_char,
    pub m_iFileSize:     c_uint,
    // Multiple artists
    pub m_pcArtists:        *mut c_char,
    pub m_pcFeaturedArtist: *mut c_char,
    pub m_pcRemixer:        *mut c_char,
    // MusicBrainz IDs
    pub m_pcMB_TrackID:          *mut c_char,
    pub m_pcMB_ReleaseID:        *mut c_char,
    pub m_pcMB_ArtistID:         *mut c_char,
    pub m_pcMB_AlbumArtistID:    *mut c_char,
    pub m_pcMB_ReleaseGroupID:   *mut c_char,
    // Flags
    pub m_bHasBasicTags:       BOOL,
    pub m_bHasExtendedTags:    BOOL,
    pub m_bHasReplayGain:      BOOL,
    pub m_bHasAudioProperties: BOOL,
    pub m_bHasMultipleArtists: BOOL,
    pub m_bHasMusicBrainzIDs:  BOOL,
}

/// Mirror of CPs_AlbumArt from CPI_TagLib.h.
#[repr(C)]
pub struct CPs_AlbumArt {
    pub m_pImageData:  *mut u8,
    pub m_iImageSize:  c_uint,
    pub m_pcMimeType:  *mut c_char,
    pub m_iWidth:      c_uint,
    pub m_iHeight:     c_uint,
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

unsafe fn alloc_str(s: &str) -> *mut c_char {
    match CString::new(s) {
        Ok(cs) => cs.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

unsafe fn free_str(ptr: *mut c_char) {
    if !ptr.is_null() {
        drop(CString::from_raw(ptr));
    }
}

fn parse_gain(s: &str) -> f32 {
    s.split_whitespace().next().and_then(|w| w.parse::<f32>().ok()).unwrap_or(0.0)
}

fn file_type_codec(ft: FileType) -> &'static str {
    match ft {
        FileType::Mpeg    => "MP3",
        FileType::Flac    => "FLAC",
        FileType::Vorbis  => "Vorbis",
        FileType::Mp4     => "AAC",
        FileType::Wav     => "WAV/PCM",
        FileType::Aiff    => "AIFF",
        FileType::Ape     => "APE",
        FileType::WavPack => "WavPack",
        FileType::Opus    => "Opus",
        FileType::Aac     => "AAC",
        FileType::Mpc     => "Musepack",
        FileType::Speex   => "Speex",
        _                 => "Unknown",
    }
}

fn file_type_bitrate_mode(ft: FileType) -> &'static str {
    match ft {
        FileType::Mpeg => "CBR",
        FileType::Wav  => "CBR",
        FileType::Aiff => "CBR",
        _              => "VBR",
    }
}

// ---------------------------------------------------------------------------
// Tag-fill helpers (operate on a lofty Tag reference)
// ---------------------------------------------------------------------------

unsafe fn fill_basic(tag: &Tag, m: &mut CPs_AllMetadata) {
    m.m_pcTitle   = tag.title().as_deref().map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcArtist  = tag.artist().as_deref().map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcAlbum   = tag.album().as_deref().map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcComment = tag.comment().as_deref().map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcGenre   = tag.genre().as_deref().map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcYear    = tag.get_string(ItemKey::Year).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_iTrackNum = tag.track().unwrap_or(0);

    if !m.m_pcTitle.is_null() || !m.m_pcArtist.is_null() || !m.m_pcAlbum.is_null()
        || !m.m_pcYear.is_null() || !m.m_pcComment.is_null() || !m.m_pcGenre.is_null()
        || m.m_iTrackNum > 0
    {
        m.m_iTagType = 2; // ttID3v2 — generic "has tags"
    }
    m.m_bHasBasicTags = TRUE;
}

unsafe fn fill_extended(tag: &Tag, m: &mut CPs_AllMetadata) {
    m.m_pcComposer    = tag.get_string(ItemKey::Composer).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcAlbumArtist = tag.get_string(ItemKey::AlbumArtist).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcGrouping    = tag.get_string(ItemKey::ContentGroup).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcCopyright   = tag.get_string(ItemKey::CopyrightMessage).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcLyrics      = tag.get_string(ItemKey::Lyrics).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());

    m.m_iDiscNumber = tag.get_string(ItemKey::DiscNumber)
        .and_then(|s| s.split('/').next())
        .and_then(|s| s.parse::<u16>().ok())
        .unwrap_or(0);

    m.m_iBPM = tag.get_string(ItemKey::Bpm)
        .and_then(|s| s.split('.').next())
        .and_then(|s| s.parse::<u16>().ok())
        .unwrap_or(0);

    if !m.m_pcComposer.is_null() || !m.m_pcAlbumArtist.is_null() || !m.m_pcGrouping.is_null()
        || !m.m_pcCopyright.is_null() || !m.m_pcLyrics.is_null()
        || m.m_iDiscNumber > 0 || m.m_iBPM > 0
    {
        m.m_bHasExtendedTags = TRUE;
    }
}

unsafe fn fill_replaygain(tag: &Tag, m: &mut CPs_AllMetadata) {
    if let Some(s) = tag.get_string(ItemKey::ReplayGainTrackGain) {
        m.m_fTrackGain = parse_gain(s);
        m.m_bHasReplayGain = TRUE;
    }
    if let Some(s) = tag.get_string(ItemKey::ReplayGainTrackPeak) {
        m.m_fTrackPeak = parse_gain(s);
        m.m_bHasReplayGain = TRUE;
    }
    if let Some(s) = tag.get_string(ItemKey::ReplayGainAlbumGain) {
        m.m_fAlbumGain = parse_gain(s);
        m.m_bHasReplayGain = TRUE;
    }
    if let Some(s) = tag.get_string(ItemKey::ReplayGainAlbumPeak) {
        m.m_fAlbumPeak = parse_gain(s);
        m.m_bHasReplayGain = TRUE;
    }
}

unsafe fn fill_multiple_artists(tag: &Tag, m: &mut CPs_AllMetadata) {
    // lofty doesn't have a dedicated multi-ARTISTS key; skip m_pcArtists for now.

    if let Some(s) = tag.get_string(ItemKey::Performer) {
        m.m_pcFeaturedArtist = alloc_str(s);
        m.m_bHasMultipleArtists = TRUE;
    }
    if let Some(s) = tag.get_string(ItemKey::Remixer) {
        m.m_pcRemixer = alloc_str(s);
        m.m_bHasMultipleArtists = TRUE;
    }
}

unsafe fn fill_musicbrainz(tag: &Tag, m: &mut CPs_AllMetadata) {
    m.m_pcMB_TrackID       = tag.get_string(ItemKey::MusicBrainzRecordingId).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcMB_ReleaseID     = tag.get_string(ItemKey::MusicBrainzReleaseId).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcMB_ArtistID      = tag.get_string(ItemKey::MusicBrainzArtistId).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcMB_AlbumArtistID = tag.get_string(ItemKey::MusicBrainzReleaseArtistId).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    m.m_pcMB_ReleaseGroupID= tag.get_string(ItemKey::MusicBrainzReleaseGroupId).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());

    if !m.m_pcMB_TrackID.is_null() || !m.m_pcMB_ReleaseID.is_null()
        || !m.m_pcMB_ArtistID.is_null() || !m.m_pcMB_AlbumArtistID.is_null()
        || !m.m_pcMB_ReleaseGroupID.is_null()
    {
        m.m_bHasMusicBrainzIDs = TRUE;
    }
}

unsafe fn fill_audio_props(tagged_file: &lofty::file::TaggedFile, path: &str, m: &mut CPs_AllMetadata) {
    let props = tagged_file.properties();
    m.m_iLength     = props.duration().as_secs() as c_uint;
    m.m_iBitrate    = props.audio_bitrate().unwrap_or(0);
    m.m_iSampleRate = props.sample_rate().unwrap_or(0);
    m.m_cChannels   = props.channels().unwrap_or(0) as c_uchar;
    m.m_iBitDepth   = props.bit_depth().unwrap_or(0) as c_ushort;

    let ft = tagged_file.file_type();
    m.m_pcCodec       = alloc_str(file_type_codec(ft));
    m.m_pcBitrateMode = alloc_str(file_type_bitrate_mode(ft));
    m.m_iFileSize     = std::fs::metadata(path).map(|md| md.len() as c_uint).unwrap_or(0);
    m.m_bHasAudioProperties = TRUE;
}

// ---------------------------------------------------------------------------
// Path helper
// ---------------------------------------------------------------------------

unsafe fn path_from_c(pc: *const c_char) -> Option<String> {
    if pc.is_null() {
        return None;
    }
    CStr::from_ptr(pc).to_str().ok().map(|s| s.to_owned())
}

// ---------------------------------------------------------------------------
// Init / Free
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_InitMetadata(pMetadata: *mut CPs_AllMetadata) {
    if !pMetadata.is_null() {
        ptr::write_bytes(pMetadata, 0, 1);
    }
}

#[no_mangle]
pub unsafe extern "C" fn CPTL_FreeMetadata(pMetadata: *mut CPs_AllMetadata) {
    if pMetadata.is_null() {
        return;
    }
    let m = &mut *pMetadata;
    free_str(m.m_pcTitle);
    free_str(m.m_pcArtist);
    free_str(m.m_pcAlbum);
    free_str(m.m_pcYear);
    free_str(m.m_pcComment);
    free_str(m.m_pcGenre);
    free_str(m.m_pcComposer);
    free_str(m.m_pcAlbumArtist);
    free_str(m.m_pcGrouping);
    free_str(m.m_pcCopyright);
    free_str(m.m_pcLyrics);
    free_str(m.m_pcCodec);
    free_str(m.m_pcBitrateMode);
    free_str(m.m_pcArtists);
    free_str(m.m_pcFeaturedArtist);
    free_str(m.m_pcRemixer);
    free_str(m.m_pcMB_TrackID);
    free_str(m.m_pcMB_ReleaseID);
    free_str(m.m_pcMB_ArtistID);
    free_str(m.m_pcMB_AlbumArtistID);
    free_str(m.m_pcMB_ReleaseGroupID);
    ptr::write_bytes(pMetadata, 0, 1);
}

// ---------------------------------------------------------------------------
// ReadAllMetadata — single file open, all fields
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_ReadAllMetadata(
    pcFilePath: *const c_char,
    pMetadata:  *mut CPs_AllMetadata,
) -> BOOL {
    let (path, m) = match (path_from_c(pcFilePath), pMetadata.as_mut()) {
        (Some(p), Some(m)) => (p, m),
        _ => return FALSE,
    };

    CPTL_InitMetadata(pMetadata);

    let tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };

    if let Some(tag) = tagged_file.primary_tag().or_else(|| tagged_file.first_tag()) {
        fill_basic(tag, m);
        fill_extended(tag, m);
        fill_replaygain(tag, m);
        fill_multiple_artists(tag, m);
        fill_musicbrainz(tag, m);
    }

    fill_audio_props(&tagged_file, &path, m);
    TRUE
}

// ---------------------------------------------------------------------------
// ReadBasicMetadataOnly — faster path for initial playlist load
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_ReadBasicMetadataOnly(
    pcFilePath: *const c_char,
    pMetadata:  *mut CPs_AllMetadata,
) -> BOOL {
    let (path, m) = match (path_from_c(pcFilePath), pMetadata.as_mut()) {
        (Some(p), Some(m)) => (p, m),
        _ => return FALSE,
    };

    CPTL_InitMetadata(pMetadata);

    let tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };

    if let Some(tag) = tagged_file.primary_tag().or_else(|| tagged_file.first_tag()) {
        fill_basic(tag, m);
    }

    fill_audio_props(&tagged_file, &path, m);
    TRUE
}

// ---------------------------------------------------------------------------
// ReadExtendedMetadataOnly — lazy load after basic
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_ReadExtendedMetadataOnly(
    pcFilePath: *const c_char,
    pMetadata:  *mut CPs_AllMetadata,
) -> BOOL {
    let (path, m) = match (path_from_c(pcFilePath), pMetadata.as_mut()) {
        (Some(p), Some(m)) => (p, m),
        _ => return FALSE,
    };

    let tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };

    if let Some(tag) = tagged_file.primary_tag().or_else(|| tagged_file.first_tag()) {
        fill_extended(tag, m);
        fill_replaygain(tag, m);
        fill_multiple_artists(tag, m);
        fill_musicbrainz(tag, m);
    }

    // Extended bit-depth / bitrate mode
    let ft = tagged_file.file_type();
    if m.m_iBitDepth == 0 {
        m.m_iBitDepth = tagged_file.properties().bit_depth().unwrap_or(0) as c_ushort;
    }
    if m.m_pcBitrateMode.is_null() {
        m.m_pcBitrateMode = alloc_str(file_type_bitrate_mode(ft));
    }

    TRUE
}

// ---------------------------------------------------------------------------
// ReadTags — legacy flat API
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_ReadTags(
    pcFilePath: *const c_char,
    ppcTitle:   *mut *mut c_char,
    ppcArtist:  *mut *mut c_char,
    ppcAlbum:   *mut *mut c_char,
    ppcYear:    *mut *mut c_char,
    ppcComment: *mut *mut c_char,
    ppcGenre:   *mut *mut c_char,
    piTrackNum: *mut c_uint,
    piLength:   *mut c_uint,
    piTagType:  *mut c_int,
) -> BOOL {
    // Null-initialise all outputs first.
    macro_rules! init_out {
        ($p:expr, $v:expr) => { if !$p.is_null() { *$p = $v; } };
    }
    init_out!(ppcTitle,   ptr::null_mut());
    init_out!(ppcArtist,  ptr::null_mut());
    init_out!(ppcAlbum,   ptr::null_mut());
    init_out!(ppcYear,    ptr::null_mut());
    init_out!(ppcComment, ptr::null_mut());
    init_out!(ppcGenre,   ptr::null_mut());
    init_out!(piTrackNum, 0);
    init_out!(piLength,   0);
    init_out!(piTagType,  0);

    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };

    let tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };

    let tag = match tagged_file.primary_tag().or_else(|| tagged_file.first_tag()) {
        Some(t) => t,
        None    => return FALSE,
    };

    macro_rules! set_str {
        ($dst:expr, $src:expr) => {
            if !$dst.is_null() {
                if let Some(s) = $src {
                    *$dst = alloc_str(s.as_ref());
                }
            }
        };
    }

    set_str!(ppcTitle,   tag.title());
    set_str!(ppcArtist,  tag.artist());
    set_str!(ppcAlbum,   tag.album());
    set_str!(ppcComment, tag.comment());
    set_str!(ppcGenre,   tag.genre());

    if !ppcYear.is_null() {
        if let Some(y) = tag.get_string(ItemKey::Year) {
            *ppcYear = alloc_str(y);
        }
    }

    if !piTrackNum.is_null() {
        *piTrackNum = tag.track().unwrap_or(0);
    }

    if !piLength.is_null() {
        *piLength = tagged_file.properties().duration().as_secs() as c_uint;
    }

    let has_tags = (!ppcTitle.is_null() && !(*ppcTitle).is_null())
        || (!ppcArtist.is_null() && !(*ppcArtist).is_null())
        || (!ppcAlbum.is_null()  && !(*ppcAlbum).is_null())
        || (!ppcYear.is_null()   && !(*ppcYear).is_null())
        || (!piTrackNum.is_null() && *piTrackNum > 0);
    if !piTagType.is_null() && has_tags {
        *piTagType = 2; // ttID3v2
    }

    TRUE
}

// ---------------------------------------------------------------------------
// WriteTags
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_WriteTags(
    pcFilePath: *const c_char,
    pcTitle:    *const c_char,
    pcArtist:   *const c_char,
    pcAlbum:    *const c_char,
    pcYear:     *const c_char,
    pcComment:  *const c_char,
    pcGenre:    *const c_char,
    iTrackNum:  c_uint,
    _iLength:   c_uint,
) -> BOOL {
    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };

    let mut tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };

    let tag = if tagged_file.primary_tag().is_some() {
        tagged_file.primary_tag_mut().unwrap()
    } else {
        match tagged_file.first_tag_mut() {
            Some(t) => t,
            None    => return FALSE,
        }
    };

    macro_rules! set_if {
        ($key:expr, $ptr:expr) => {
            if !$ptr.is_null() {
                if let Ok(s) = CStr::from_ptr($ptr).to_str() {
                    if !s.is_empty() {
                        tag.insert_text($key, s.to_owned());
                    }
                }
            }
        };
    }

    set_if!(ItemKey::TrackTitle,  pcTitle);
    set_if!(ItemKey::TrackArtist, pcArtist);
    set_if!(ItemKey::AlbumTitle,  pcAlbum);
    set_if!(ItemKey::Genre,       pcGenre);
    set_if!(ItemKey::Comment,     pcComment);

    if !pcYear.is_null() {
        if let Ok(s) = CStr::from_ptr(pcYear).to_str() {
            if !s.is_empty() {
                tag.insert_text(ItemKey::Year, s.to_owned());
            }
        }
    }

    if iTrackNum > 0 {
        tag.set_track(iTrackNum);
    }

    match tagged_file.save_to_path(&path, WriteOptions::default()) {
        Ok(_)  => TRUE,
        Err(_) => FALSE,
    }
}

// ---------------------------------------------------------------------------
// ReadExtendedTags / WriteExtendedTags
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_ReadExtendedTags(
    pcFilePath:    *const c_char,
    ppcComposer:   *mut *mut c_char,
    ppcAlbumArtist:*mut *mut c_char,
    ppcGrouping:   *mut *mut c_char,
    ppcCopyright:  *mut *mut c_char,
    ppcLyrics:     *mut *mut c_char,
    piDiscNumber:  *mut c_ushort,
    piBPM:         *mut c_ushort,
) -> BOOL {
    macro_rules! init_p { ($p:expr) => { if !$p.is_null() { *$p = ptr::null_mut(); } }; }
    init_p!(ppcComposer); init_p!(ppcAlbumArtist); init_p!(ppcGrouping);
    init_p!(ppcCopyright); init_p!(ppcLyrics);
    if !piDiscNumber.is_null() { *piDiscNumber = 0; }
    if !piBPM.is_null()        { *piBPM = 0; }

    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    let tag = match tagged_file.primary_tag().or_else(|| tagged_file.first_tag()) {
        Some(t) => t,
        None    => return FALSE,
    };

    macro_rules! read_str {
        ($dst:expr, $key:expr) => {
            if !$dst.is_null() {
                if let Some(s) = tag.get_string($key) {
                    *$dst = alloc_str(s);
                }
            }
        };
    }
    read_str!(ppcComposer,    ItemKey::Composer);
    read_str!(ppcAlbumArtist, ItemKey::AlbumArtist);
    read_str!(ppcGrouping,    ItemKey::ContentGroup);
    read_str!(ppcCopyright,   ItemKey::CopyrightMessage);
    read_str!(ppcLyrics,      ItemKey::Lyrics);

    if !piDiscNumber.is_null() {
        *piDiscNumber = tag.get_string(ItemKey::DiscNumber)
            .and_then(|s| s.split('/').next())
            .and_then(|s| s.parse::<u16>().ok())
            .unwrap_or(0);
    }
    if !piBPM.is_null() {
        *piBPM = tag.get_string(ItemKey::Bpm)
            .and_then(|s| s.split('.').next())
            .and_then(|s| s.parse::<u16>().ok())
            .unwrap_or(0);
    }

    TRUE
}

#[no_mangle]
pub unsafe extern "C" fn CPTL_WriteExtendedTags(
    pcFilePath:     *const c_char,
    pcComposer:     *const c_char,
    pcAlbumArtist:  *const c_char,
    pcGrouping:     *const c_char,
    pcCopyright:    *const c_char,
    pcLyrics:       *const c_char,
    iDiscNumber:    c_ushort,
    iBPM:           c_ushort,
) -> BOOL {
    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let mut tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    let tag = if tagged_file.primary_tag().is_some() {
        tagged_file.primary_tag_mut().unwrap()
    } else {
        match tagged_file.first_tag_mut() {
            Some(t) => t,
            None    => return FALSE,
        }
    };

    macro_rules! write_str {
        ($key:expr, $ptr:expr) => {
            if !$ptr.is_null() {
                if let Ok(s) = CStr::from_ptr($ptr).to_str() {
                    if !s.is_empty() { tag.insert_text($key, s.to_owned()); }
                }
            }
        };
    }
    write_str!(ItemKey::Composer,        pcComposer);
    write_str!(ItemKey::AlbumArtist,     pcAlbumArtist);
    write_str!(ItemKey::ContentGroup,    pcGrouping);
    write_str!(ItemKey::CopyrightMessage,pcCopyright);
    write_str!(ItemKey::Lyrics,          pcLyrics);

    if iDiscNumber > 0 {
        tag.insert_text(ItemKey::DiscNumber, iDiscNumber.to_string());
    }
    if iBPM > 0 {
        tag.insert_text(ItemKey::Bpm, iBPM.to_string());
    }

    match tagged_file.save_to_path(&path, WriteOptions::default()) {
        Ok(_)  => TRUE,
        Err(_) => FALSE,
    }
}

// ---------------------------------------------------------------------------
// ReplayGain
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_ReadReplayGain(
    pcFilePath:  *const c_char,
    pfTrackGain: *mut f32,
    pfTrackPeak: *mut f32,
    pfAlbumGain: *mut f32,
    pfAlbumPeak: *mut f32,
) -> BOOL {
    if !pfTrackGain.is_null() { *pfTrackGain = 0.0; }
    if !pfTrackPeak.is_null() { *pfTrackPeak = 0.0; }
    if !pfAlbumGain.is_null() { *pfAlbumGain = 0.0; }
    if !pfAlbumPeak.is_null() { *pfAlbumPeak = 0.0; }

    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    let tag = match tagged_file.primary_tag().or_else(|| tagged_file.first_tag()) {
        Some(t) => t,
        None    => return FALSE,
    };

    if !pfTrackGain.is_null() {
        if let Some(s) = tag.get_string(ItemKey::ReplayGainTrackGain) { *pfTrackGain = parse_gain(s); }
    }
    if !pfTrackPeak.is_null() {
        if let Some(s) = tag.get_string(ItemKey::ReplayGainTrackPeak) { *pfTrackPeak = parse_gain(s); }
    }
    if !pfAlbumGain.is_null() {
        if let Some(s) = tag.get_string(ItemKey::ReplayGainAlbumGain) { *pfAlbumGain = parse_gain(s); }
    }
    if !pfAlbumPeak.is_null() {
        if let Some(s) = tag.get_string(ItemKey::ReplayGainAlbumPeak) { *pfAlbumPeak = parse_gain(s); }
    }
    TRUE
}

#[no_mangle]
pub unsafe extern "C" fn CPTL_WriteReplayGain(
    pcFilePath: *const c_char,
    fTrackGain: f32,
    fTrackPeak: f32,
    fAlbumGain: f32,
    fAlbumPeak: f32,
) -> BOOL {
    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let mut tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    let tag = if tagged_file.primary_tag().is_some() {
        tagged_file.primary_tag_mut().unwrap()
    } else {
        match tagged_file.first_tag_mut() {
            Some(t) => t,
            None    => return FALSE,
        }
    };

    if fTrackGain != 0.0 { tag.insert_text(ItemKey::ReplayGainTrackGain, format!("{:.2} dB", fTrackGain)); }
    if fTrackPeak != 0.0 { tag.insert_text(ItemKey::ReplayGainTrackPeak, format!("{:.6}", fTrackPeak)); }
    if fAlbumGain != 0.0 { tag.insert_text(ItemKey::ReplayGainAlbumGain, format!("{:.2} dB", fAlbumGain)); }
    if fAlbumPeak != 0.0 { tag.insert_text(ItemKey::ReplayGainAlbumPeak, format!("{:.6}", fAlbumPeak)); }

    match tagged_file.save_to_path(&path, WriteOptions::default()) {
        Ok(_)  => TRUE,
        Err(_) => FALSE,
    }
}

// ---------------------------------------------------------------------------
// Audio properties
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_ReadAudioProperties(
    pcFilePath:    *const c_char,
    pBitrate:      *mut c_uint,
    pSampleRate:   *mut c_uint,
    pBitDepth:     *mut c_ushort,
    pChannels:     *mut c_uchar,
    ppcCodec:      *mut *mut c_char,
    ppcBitrateMode:*mut *mut c_char,
    pFileSize:     *mut c_uint,
) -> BOOL {
    if pBitrate.is_null() || pSampleRate.is_null() || pBitDepth.is_null()
        || pChannels.is_null() || ppcCodec.is_null() || ppcBitrateMode.is_null() || pFileSize.is_null()
    {
        return FALSE;
    }
    *pBitrate = 0; *pSampleRate = 0; *pBitDepth = 0; *pChannels = 0;
    *ppcCodec = ptr::null_mut(); *ppcBitrateMode = ptr::null_mut(); *pFileSize = 0;

    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };

    let props = tagged_file.properties();
    *pBitrate    = props.audio_bitrate().unwrap_or(0);
    *pSampleRate = props.sample_rate().unwrap_or(0);
    *pChannels   = props.channels().unwrap_or(0) as c_uchar;
    *pBitDepth   = props.bit_depth().unwrap_or(0) as c_ushort;

    let ft = tagged_file.file_type();
    *ppcCodec       = alloc_str(file_type_codec(ft));
    *ppcBitrateMode = alloc_str(file_type_bitrate_mode(ft));
    *pFileSize      = std::fs::metadata(&path).map(|md| md.len() as c_uint).unwrap_or(0);
    TRUE
}

// ---------------------------------------------------------------------------
// Multiple artists
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_ReadMultipleArtists(
    pcFilePath:      *const c_char,
    ppcArtists:      *mut *mut c_char,
    ppcFeaturedArtist:*mut *mut c_char,
    ppcRemixer:      *mut *mut c_char,
) -> BOOL {
    if ppcArtists.is_null() || ppcFeaturedArtist.is_null() || ppcRemixer.is_null() { return FALSE; }
    *ppcArtists = ptr::null_mut();
    *ppcFeaturedArtist = ptr::null_mut();
    *ppcRemixer = ptr::null_mut();

    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    let tag = match tagged_file.primary_tag().or_else(|| tagged_file.first_tag()) {
        Some(t) => t,
        None    => return TRUE, // no tag, but not an error
    };

    if let Some(s) = tag.get_string(ItemKey::Performer) { *ppcFeaturedArtist = alloc_str(s); }
    if let Some(s) = tag.get_string(ItemKey::Remixer)   { *ppcRemixer        = alloc_str(s); }
    TRUE
}

#[no_mangle]
pub unsafe extern "C" fn CPTL_WriteMultipleArtists(
    pcFilePath:       *const c_char,
    pcArtists:        *const c_char,
    pcFeaturedArtist: *const c_char,
    pcRemixer:        *const c_char,
) -> BOOL {
    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let mut tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    let tag = if tagged_file.primary_tag().is_some() {
        tagged_file.primary_tag_mut().unwrap()
    } else {
        match tagged_file.first_tag_mut() {
            Some(t) => t,
            None    => return FALSE,
        }
    };

    macro_rules! write_str {
        ($key:expr, $ptr:expr) => {
            if !$ptr.is_null() {
                if let Ok(s) = CStr::from_ptr($ptr).to_str() {
                    if !s.is_empty() { tag.insert_text($key, s.to_owned()); }
                }
            }
        };
    }
    write_str!(ItemKey::Performer, pcFeaturedArtist);
    write_str!(ItemKey::Remixer,   pcRemixer);
    // pcArtists — no standard lofty key; skip.
    let _ = pcArtists;

    match tagged_file.save_to_path(&path, WriteOptions::default()) {
        Ok(_)  => TRUE,
        Err(_) => FALSE,
    }
}

// ---------------------------------------------------------------------------
// MusicBrainz IDs
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_ReadMusicBrainzIDs(
    pcFilePath:        *const c_char,
    ppcTrackID:        *mut *mut c_char,
    ppcReleaseID:      *mut *mut c_char,
    ppcArtistID:       *mut *mut c_char,
    ppcAlbumArtistID:  *mut *mut c_char,
    ppcReleaseGroupID: *mut *mut c_char,
) -> BOOL {
    if ppcTrackID.is_null() || ppcReleaseID.is_null() || ppcArtistID.is_null()
        || ppcAlbumArtistID.is_null() || ppcReleaseGroupID.is_null()
    { return FALSE; }
    *ppcTrackID = ptr::null_mut();
    *ppcReleaseID = ptr::null_mut();
    *ppcArtistID = ptr::null_mut();
    *ppcAlbumArtistID = ptr::null_mut();
    *ppcReleaseGroupID = ptr::null_mut();

    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    let tag = match tagged_file.primary_tag().or_else(|| tagged_file.first_tag()) {
        Some(t) => t,
        None    => return TRUE,
    };

    *ppcTrackID        = tag.get_string(ItemKey::MusicBrainzRecordingId).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    *ppcReleaseID      = tag.get_string(ItemKey::MusicBrainzReleaseId).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    *ppcArtistID       = tag.get_string(ItemKey::MusicBrainzArtistId).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    *ppcAlbumArtistID  = tag.get_string(ItemKey::MusicBrainzReleaseArtistId).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    *ppcReleaseGroupID = tag.get_string(ItemKey::MusicBrainzReleaseGroupId).map(|s| alloc_str(s)).unwrap_or(ptr::null_mut());
    TRUE
}

#[no_mangle]
pub unsafe extern "C" fn CPTL_WriteMusicBrainzIDs(
    pcFilePath:        *const c_char,
    pcTrackID:         *const c_char,
    pcReleaseID:       *const c_char,
    pcArtistID:        *const c_char,
    pcAlbumArtistID:   *const c_char,
    pcReleaseGroupID:  *const c_char,
) -> BOOL {
    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let mut tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    let tag = if tagged_file.primary_tag().is_some() {
        tagged_file.primary_tag_mut().unwrap()
    } else {
        match tagged_file.first_tag_mut() {
            Some(t) => t,
            None    => return FALSE,
        }
    };

    macro_rules! write_str {
        ($key:expr, $ptr:expr) => {
            if !$ptr.is_null() {
                if let Ok(s) = CStr::from_ptr($ptr).to_str() {
                    if !s.is_empty() { tag.insert_text($key, s.to_owned()); }
                }
            }
        };
    }
    write_str!(ItemKey::MusicBrainzRecordingId,    pcTrackID);
    write_str!(ItemKey::MusicBrainzReleaseId,      pcReleaseID);
    write_str!(ItemKey::MusicBrainzArtistId,       pcArtistID);
    write_str!(ItemKey::MusicBrainzReleaseArtistId,pcAlbumArtistID);
    write_str!(ItemKey::MusicBrainzReleaseGroupId, pcReleaseGroupID);

    match tagged_file.save_to_path(&path, WriteOptions::default()) {
        Ok(_)  => TRUE,
        Err(_) => FALSE,
    }
}

// ---------------------------------------------------------------------------
// Album Art
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPTL_ReadAlbumArt(
    pcFilePath: *const c_char,
    pAlbumArt:  *mut CPs_AlbumArt,
) -> BOOL {
    let art = match pAlbumArt.as_mut() { Some(a) => a, None => return FALSE };
    ptr::write_bytes(pAlbumArt, 0, 1);

    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    let tag = match tagged_file.primary_tag().or_else(|| tagged_file.first_tag()) {
        Some(t) => t,
        None    => return FALSE,
    };

    let pic = match tag.pictures().first() {
        Some(p) => p,
        None    => return FALSE,
    };

    let raw = pic.data();
    if raw.is_empty() { return FALSE; }

    let mut owned = raw.to_vec();
    owned.shrink_to_fit();
    let len = owned.len();
    art.m_pImageData = owned.as_mut_ptr();
    art.m_iImageSize = len as c_uint;
    std::mem::forget(owned);

    let mime_str = pic.mime_type().map(|m| m.as_str()).unwrap_or("image/jpeg");
    art.m_pcMimeType = alloc_str(mime_str);

    TRUE
}

#[no_mangle]
pub unsafe extern "C" fn CPTL_FreeAlbumArt(pAlbumArt: *mut CPs_AlbumArt) {
    let art = match pAlbumArt.as_mut() { Some(a) => a, None => return };
    if !art.m_pImageData.is_null() {
        let len = art.m_iImageSize as usize;
        drop(Vec::from_raw_parts(art.m_pImageData, len, len));
    }
    free_str(art.m_pcMimeType);
    ptr::write_bytes(pAlbumArt, 0, 1);
}

#[no_mangle]
pub unsafe extern "C" fn CPTL_WriteAlbumArt(
    pcFilePath:  *const c_char,
    pImageData:  *const u8,
    iImageSize:  c_uint,
    pcMimeType:  *const c_char,
) -> BOOL {
    if pImageData.is_null() || iImageSize == 0 { return FALSE; }

    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let mut tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    let tag = if tagged_file.primary_tag().is_some() {
        tagged_file.primary_tag_mut().unwrap()
    } else {
        match tagged_file.first_tag_mut() {
            Some(t) => t,
            None    => return FALSE,
        }
    };

    let data = std::slice::from_raw_parts(pImageData, iImageSize as usize).to_vec();

    let mime = if !pcMimeType.is_null() {
        CStr::from_ptr(pcMimeType).to_str().unwrap_or("image/jpeg").to_owned()
    } else {
        "image/jpeg".to_owned()
    };

    let mime_type = if mime.contains("png") { MimeType::Png } else { MimeType::Jpeg };

    let picture = Picture::unchecked(data)
        .pic_type(PictureType::CoverFront)
        .mime_type(mime_type)
        .build();

    tag.set_picture(0, picture);

    match tagged_file.save_to_path(&path, WriteOptions::default()) {
        Ok(_)  => TRUE,
        Err(_) => FALSE,
    }
}

#[no_mangle]
pub unsafe extern "C" fn CPTL_HasAlbumArt(pcFilePath: *const c_char) -> BOOL {
    let path = match path_from_c(pcFilePath) {
        Some(p) => p,
        None    => return FALSE,
    };
    let tagged_file = match lofty::read_from_path(&path) {
        Ok(f)  => f,
        Err(_) => return FALSE,
    };
    if let Some(tag) = tagged_file.primary_tag().or_else(|| tagged_file.first_tag()) {
        if !tag.pictures().is_empty() { return TRUE; }
    }
    FALSE
}
