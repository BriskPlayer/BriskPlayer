/*
 * test_stubs.rs — link-only stand-ins for the C-side functions this crate's
 * modules call into via `extern "C"`.
 *
 * `cargo test` builds one test-harness binary containing the whole crate
 * (every module, not just the one under test), and links it as an ordinary
 * executable. The real implementations of these functions live in the C
 * project (CPI_Playlist*.c, CPI_Stream_Internet.c, options.c, ...) and are
 * only linked in by the CMake/Corrosion build — never by `cargo test`. Every
 * signature here must match its real `extern "C"` declaration in ffi.rs /
 * playlist.rs / stream_internet.rs exactly (same parameter and return
 * types); the linker only checks the symbol name, not the signature, so a
 * mismatch here would not fail to link — it would silently miscompile any
 * test that actually calls through it.
 *
 * None of the tests in this crate as written (see wav.rs's #[cfg(test)] mod
 * tests) exercise any code path that calls these — they only exist to
 * satisfy the linker so the test binary can be produced at all. Bodies are
 * intentionally minimal/inert (nulls, FALSE, no-ops) rather than trying to
 * emulate real behavior; if a future test needs one of these to behave
 * meaningfully, give it a real fake implementation at that point rather
 * than trusting these placeholders.
 */

#![allow(unused_variables)]

use super::ffi::{CPs_CoDecModule, CpsInStream, BOOL, FALSE};
use std::os::raw::{c_char, c_int, c_void};

// ---------------------------------------------------------------------------
// CPI_Player_FileAssoc.c
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPFA_InitialiseFileAssociations(pCoDec: *mut CPs_CoDecModule) {}

#[no_mangle]
pub unsafe extern "C" fn CPFA_EmptyFileAssociations(pCoDec: *mut CPs_CoDecModule) {}

#[no_mangle]
pub unsafe extern "C" fn CPFA_AddFileAssociation(
    pCoDec:      *mut CPs_CoDecModule,
    pcExtension: *const c_char,
    dwCookie:    usize,
) {
}

// ---------------------------------------------------------------------------
// CP_CreateInStream (CPI_Stream.c) — dispatches to a real file/internet
// stream in the C build; here just report "could not open".
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CP_CreateInStream(
    path: *const c_char,
    hwnd: *mut c_void,
) -> *mut CpsInStream {
    std::ptr::null_mut()
}

// ---------------------------------------------------------------------------
// CPI_Stream_Internet.c circle-buffer shims
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CP_CreateCircleBuffer(size: u32) -> *mut c_void {
    std::ptr::null_mut()
}

#[no_mangle]
pub unsafe extern "C" fn CPCB_Write(cb: *mut c_void, src: *const c_void, n: u32) {}

#[no_mangle]
pub unsafe extern "C" fn CPCB_Read(
    cb: *mut c_void,
    dst: *mut c_void,
    n: usize,
    out: *mut usize,
) -> BOOL {
    if !out.is_null() {
        *out = 0;
    }
    FALSE
}

#[no_mangle]
pub unsafe extern "C" fn CPCB_GetUsedSize(cb: *mut c_void) -> u32 { 0 }

#[no_mangle]
pub unsafe extern "C" fn CPCB_GetFreeSize(cb: *mut c_void) -> u32 { 0 }

#[no_mangle]
pub unsafe extern "C" fn CPCB_SetComplete(cb: *mut c_void) {}

#[no_mangle]
pub unsafe extern "C" fn CPCB_IsComplete(cb: *mut c_void) -> BOOL { FALSE }

#[no_mangle]
pub unsafe extern "C" fn CPCB_Uninitialise(cb: *mut c_void) {}

// ---------------------------------------------------------------------------
// CPI_PlaylistItem.c — per-item accessors (HItem is an opaque *mut c_void)
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPLI_Next(hItem: *mut c_void) -> *mut c_void { std::ptr::null_mut() }

#[no_mangle]
pub unsafe extern "C" fn CPLI_Prev(hItem: *mut c_void) -> *mut c_void { std::ptr::null_mut() }

#[no_mangle]
pub unsafe extern "C" fn CPLI_SetNext(hItem: *mut c_void, hNext: *mut c_void) {}

#[no_mangle]
pub unsafe extern "C" fn CPLI_SetPrev(hItem: *mut c_void, hPrev: *mut c_void) {}

#[no_mangle]
pub unsafe extern "C" fn CPLI_DestroyItem(hItem: *mut c_void) {}

#[no_mangle]
pub unsafe extern "C" fn CPLI_GetPath(hItem: *mut c_void) -> *const c_char { std::ptr::null() }

#[no_mangle]
pub unsafe extern "C" fn CPLI_GetTrackName(hItem: *mut c_void) -> *const c_char { std::ptr::null() }

#[no_mangle]
pub unsafe extern "C" fn CPLI_SetTrackName(hItem: *mut c_void, name: *const c_char) {}

#[no_mangle]
pub unsafe extern "C" fn CPLI_GetArtist(hItem: *mut c_void) -> *const c_char { std::ptr::null() }

#[no_mangle]
pub unsafe extern "C" fn CPLI_GetAlbum(hItem: *mut c_void) -> *const c_char { std::ptr::null() }

#[no_mangle]
pub unsafe extern "C" fn CPLI_GetYear(hItem: *mut c_void) -> *const c_char { std::ptr::null() }

#[no_mangle]
pub unsafe extern "C" fn CPLI_GetGenre(hItem: *mut c_void) -> *const c_char { std::ptr::null() }

#[no_mangle]
pub unsafe extern "C" fn CPLI_GetComment(hItem: *mut c_void) -> *const c_char { std::ptr::null() }

#[no_mangle]
pub unsafe extern "C" fn CPLI_GetTrackNum(hItem: *mut c_void) -> u8 { 0 }

#[no_mangle]
pub unsafe extern "C" fn CPLI_GetTrackLength(hItem: *mut c_void) -> c_int { 0 }

#[no_mangle]
pub unsafe extern "C" fn CPLI_SetTrackStackPos(hItem: *mut c_void, pos: c_int) {}

#[no_mangle]
pub unsafe extern "C" fn CPLI_GetTrackStackPos(hItem: *mut c_void) -> c_int { 0 }

#[no_mangle]
pub unsafe extern "C" fn CPLI_IsDestroyOnDeactivate(hItem: *mut c_void) -> BOOL { FALSE }

#[no_mangle]
pub unsafe extern "C" fn CPLI_SetDestroyOnDeactivate(hItem: *mut c_void, val: BOOL) {}

#[no_mangle]
pub unsafe extern "C" fn CPLI_SetCookie(hItem: *mut c_void, cookie: c_int) {}

#[no_mangle]
pub unsafe extern "C" fn CPLI_ReadTag(hItem: *mut c_void) {}

// ---------------------------------------------------------------------------
// CPI_Playlist.c — options accessors and UI callbacks
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CP_IsURL(pcPath: *const c_char) -> BOOL { FALSE }

#[no_mangle]
pub unsafe extern "C" fn CPL_cb_OnPlaylistAppend(hItem: *mut c_void) {}

#[no_mangle]
pub unsafe extern "C" fn CPL_cb_OnPlaylistItemDelete(hItem: *mut c_void) {}

#[no_mangle]
pub unsafe extern "C" fn CPL_cb_OnPlaylistEmpty() {}

#[no_mangle]
pub unsafe extern "C" fn CPL_cb_OnPlaylistActivationChange(hItem: *mut c_void, bNewActiveState: BOOL) {}

#[no_mangle]
pub unsafe extern "C" fn CPL_cb_OnPlaylistActivationEmpty() {}

#[no_mangle]
pub unsafe extern "C" fn CPL_cb_SetWindowToReflectList() {}

#[no_mangle]
pub unsafe extern "C" fn CPL_cb_TrackStackChanged() {}

#[no_mangle]
pub unsafe extern "C" fn CPL_PlayActiveItem(hPlaylist: *mut c_void, bStopFirst: BOOL) {}

#[no_mangle]
pub unsafe extern "C" fn CP_opt_allow_file_once() -> BOOL { FALSE }

#[no_mangle]
pub unsafe extern "C" fn CP_opt_read_id3_tag_of_selected() -> BOOL { FALSE }

#[no_mangle]
pub unsafe extern "C" fn CP_opt_shuffle_play() -> BOOL { FALSE }

#[no_mangle]
pub unsafe extern "C" fn CP_opt_repeat_playlist() -> BOOL { FALSE }

#[no_mangle]
pub unsafe extern "C" fn CP_opt_set_initial_file(path: *const c_char) {}
