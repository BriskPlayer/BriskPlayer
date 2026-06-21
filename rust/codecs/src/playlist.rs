/*
 * playlist.rs — Rust implementation of the BriskPlayer playlist data model.
 *
 * Replaces the data-model layer of CPI_Playlist.c.  All Win32 I/O (worker
 * thread, FindFirstFile, DragQueryFile, M3U/PLS parsing) stays in C.
 *
 * The public C API is preserved exactly — every #[no_mangle] symbol here
 * shadows the old C implementation that was removed from CPI_Playlist.c.
 *
 * Memory model:
 *   CPPL_AllocPlaylist() → Box::into_raw(Box<Playlist>) cast to *mut c_void.
 *   CPPL_FreePlaylist()  → Box::from_raw + drop.
 *   All CP_HPLAYLISTITEM values are opaque *mut c_void owned by C (CPI_PlaylistItem.c).
 */

#![allow(non_snake_case, non_camel_case_types, clippy::missing_safety_doc)]

use std::collections::HashSet;
use std::ffi::CStr;
use std::os::raw::{c_char, c_int, c_void};
use super::ffi::{BOOL, FALSE, TRUE};

// ---------------------------------------------------------------------------
// Type aliases
// ---------------------------------------------------------------------------

type HPlaylist = *mut c_void;
type HItem    = *mut c_void;

// ---------------------------------------------------------------------------
// Playlist data object (Rust-owned, replaces CPs_Playlist)
// ---------------------------------------------------------------------------

struct Playlist {
    // Doubly-linked list anchors (opaque C item handles)
    first:   HItem,
    last:    HItem,
    current: HItem,

    // Play order / shuffle stack (replaces realloc array)
    stack:        Vec<HItem>,
    stack_cursor: usize,

    // O(1) duplicate detection (replaces manual hash table)
    path_hash: HashSet<String>,

    // Worker thread handles — set by C after thread creation
    worker_thread:    usize, // HANDLE as usize
    worker_thread_id: u32,   // DWORD
    host_thread_id:   u32,   // for PostThreadMessage back to main

    // Batch ID: main thread increments to cancel in-flight tag reads
    batch_id: u32,

    // Flags
    sync_load_next_file: bool,
    auto_activate_initial: bool,
}

impl Playlist {
    fn new() -> Self {
        Playlist {
            first:   std::ptr::null_mut(),
            last:    std::ptr::null_mut(),
            current: std::ptr::null_mut(),
            stack:        Vec::new(),
            stack_cursor: 0,
            path_hash: HashSet::new(),
            worker_thread:    0,
            worker_thread_id: 0,
            host_thread_id:   0,
            batch_id: 0,
            sync_load_next_file:   false,
            auto_activate_initial: false,
        }
    }
}

// ---------------------------------------------------------------------------
// C item API — declared so Rust can call into CPI_PlaylistItem.c
// ---------------------------------------------------------------------------

extern "C" {
    fn CPLI_Next(hItem: HItem) -> HItem;
    fn CPLI_Prev(hItem: HItem) -> HItem;
    fn CPLI_SetNext(hItem: HItem, hNext: HItem);
    fn CPLI_SetPrev(hItem: HItem, hPrev: HItem);
    fn CPLI_DestroyItem(hItem: HItem);
    fn CPLI_GetPath(hItem: HItem) -> *const c_char;
    fn CPLI_GetTrackName(hItem: HItem) -> *const c_char;
    fn CPLI_SetTrackName(hItem: HItem, name: *const c_char);
    fn CPLI_GetArtist(hItem: HItem) -> *const c_char;
    fn CPLI_GetAlbum(hItem: HItem) -> *const c_char;
    fn CPLI_GetYear(hItem: HItem) -> *const c_char;
    fn CPLI_GetGenre(hItem: HItem) -> *const c_char;
    fn CPLI_GetComment(hItem: HItem) -> *const c_char;
    fn CPLI_GetTrackNum(hItem: HItem) -> u8;
    fn CPLI_GetTrackLength(hItem: HItem) -> c_int;
    fn CPLI_SetTrackStackPos(hItem: HItem, pos: c_int);
    fn CPLI_IsDestroyOnDeactivate(hItem: HItem) -> BOOL;
    fn CPLI_SetDestroyOnDeactivate(hItem: HItem, val: BOOL);
    fn CPLI_SetCookie(hItem: HItem, cookie: c_int);
    fn CPLI_ReadTag(hItem: HItem);
    fn CP_IsURL(pcPath: *const c_char) -> BOOL;

    // UI callbacks implemented in CPI_PlaylistWindow.c
    fn CPL_cb_OnPlaylistAppend(hItem: HItem);
    fn CPL_cb_OnPlaylistItemDelete(hItem: HItem);
    fn CPL_cb_OnPlaylistEmpty();
    fn CPL_cb_OnPlaylistActivationChange(hItem: HItem, bNewActiveState: BOOL);
    fn CPL_cb_OnPlaylistActivationEmpty();
    fn CPL_cb_SetWindowToReflectList();
    fn CPL_cb_TrackStackChanged();

    // Player engine — still in C
    fn CPL_PlayActiveItem(hPlaylist: HPlaylist, bStopFirst: BOOL);

    // Options (accessor functions in CPI_Playlist.c)
    fn CP_opt_allow_file_once() -> BOOL;
    fn CP_opt_read_id3_tag_of_selected() -> BOOL;
    fn CP_opt_shuffle_play() -> BOOL;
    fn CP_opt_repeat_playlist() -> BOOL;
    fn CP_opt_set_initial_file(path: *const c_char);

    // libc
    fn rand() -> c_int;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

#[inline]
unsafe fn pl(h: HPlaylist) -> &'static mut Playlist {
    &mut *(h as *mut Playlist)
}

#[inline]
unsafe fn cstr_opt(ptr: *const c_char) -> Option<&'static str> {
    if ptr.is_null() { None } else { CStr::from_ptr(ptr).to_str().ok() }
}

fn stri_cmp(a: Option<&str>, b: Option<&str>) -> std::cmp::Ordering {
    let a = a.unwrap_or("").bytes();
    let b = b.unwrap_or("").bytes();
    a.map(|c| c.to_ascii_lowercase())
     .cmp(b.map(|c| c.to_ascii_lowercase()))
}

// CIC_TRACKSTACK_UNSTACKED (0xEFFFFFFF as signed i32)
const TRACKSTACK_UNSTACKED: c_int = 0xEFFFFFFFu32 as i32;
// CPC_INVALIDITEM from CLV_ListView.h
const INVALID_ITEM: c_int = -1;

// ---------------------------------------------------------------------------
// Allocation / teardown (called from C's CPL_CreatePlaylist/DestroyPlaylist)
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPPL_AllocPlaylist() -> HPlaylist {
    Box::into_raw(Box::new(Playlist::new())) as HPlaylist
}

#[no_mangle]
pub unsafe extern "C" fn CPPL_FreePlaylist(h: HPlaylist) {
    if !h.is_null() {
        drop(Box::from_raw(h as *mut Playlist));
    }
}

// ---------------------------------------------------------------------------
// Worker-thread field accessors (called from C shell)
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPPL_SetWorkerThread(h: HPlaylist, handle: usize, thread_id: u32) {
    let p = pl(h);
    p.worker_thread    = handle;
    p.worker_thread_id = thread_id;
}

#[no_mangle]
pub unsafe extern "C" fn CPPL_SetHostThreadID(h: HPlaylist, host_id: u32) {
    pl(h).host_thread_id = host_id;
}

#[no_mangle]
pub unsafe extern "C" fn CPPL_GetWorkerThread(h: HPlaylist) -> usize {
    pl(h).worker_thread
}

#[no_mangle]
pub unsafe extern "C" fn CPPL_GetWorkerThreadID(h: HPlaylist) -> u32 {
    pl(h).worker_thread_id
}

#[no_mangle]
pub unsafe extern "C" fn CPPL_GetHostThreadID(h: HPlaylist) -> u32 {
    pl(h).host_thread_id
}

#[no_mangle]
pub unsafe extern "C" fn CPPL_GetBatchID(h: HPlaylist) -> u32 {
    pl(h).batch_id
}

#[no_mangle]
pub unsafe extern "C" fn CPPL_IncrBatchID(h: HPlaylist) {
    pl(h).batch_id = pl(h).batch_id.wrapping_add(1);
}

#[no_mangle]
pub unsafe extern "C" fn CPPL_GetSyncLoadNextFile(h: HPlaylist) -> BOOL {
    if pl(h).sync_load_next_file { TRUE } else { FALSE }
}

#[no_mangle]
pub unsafe extern "C" fn CPPL_SetSyncLoadNextFile(h: HPlaylist, val: BOOL) {
    pl(h).sync_load_next_file = val != FALSE;
}

#[no_mangle]
pub unsafe extern "C" fn CPPL_GetAutoActivateInitial(h: HPlaylist) -> BOOL {
    if pl(h).auto_activate_initial { TRUE } else { FALSE }
}

#[no_mangle]
pub unsafe extern "C" fn CPPL_SetAutoActivateInitial(h: HPlaylist, val: BOOL) {
    pl(h).auto_activate_initial = val != FALSE;
}

// ---------------------------------------------------------------------------
// Core list: empty, unlink, find, accessors
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPL_Empty(h: HPlaylist) {
    let p = pl(h);
    p.batch_id = p.batch_id.wrapping_add(1);

    // If current item is still live in the list, unlink and mark for deferred destroy
    if !p.current.is_null() && CPLI_IsDestroyOnDeactivate(p.current) == FALSE {
        CPL_UnlinkItem(h, p.current);
        CPLI_SetNext(p.current, std::ptr::null_mut());
        CPLI_SetPrev(p.current, std::ptr::null_mut());
        CPLI_SetDestroyOnDeactivate(p.current, TRUE);
        CPL_cb_OnPlaylistActivationChange(p.current, FALSE);
        CPLI_SetCookie(p.current, INVALID_ITEM);
    }

    CPL_cb_OnPlaylistEmpty();

    // Destroy all remaining items in the list
    let mut cursor = p.first;
    while !cursor.is_null() {
        let next = CPLI_Next(cursor);
        CPLI_DestroyItem(cursor);
        cursor = next;
    }

    p.first = std::ptr::null_mut();
    p.last  = std::ptr::null_mut();
    p.path_hash.clear();
    p.stack.clear();
    p.stack_cursor = 0;
}

#[no_mangle]
pub unsafe extern "C" fn CPL_UnlinkItem(h: HPlaylist, hItem: HItem) {
    let p = pl(h);
    let prev = CPLI_Prev(hItem);
    let next = CPLI_Next(hItem);

    if !prev.is_null() {
        CPLI_SetNext(prev, next);
    } else {
        // hItem was the first item
        p.first = next;
    }

    if !next.is_null() {
        CPLI_SetPrev(next, prev);
    } else {
        // hItem was the last item
        p.last = prev;
    }
}

#[no_mangle]
pub unsafe extern "C" fn CPL_GetFirstItem(h: HPlaylist) -> HItem {
    pl(h).first
}

#[no_mangle]
pub unsafe extern "C" fn CPL_GetLastItem(h: HPlaylist) -> HItem {
    pl(h).last
}

#[no_mangle]
pub unsafe extern "C" fn CPL_GetActiveItem(h: HPlaylist) -> HItem {
    pl(h).current
}

#[no_mangle]
pub unsafe extern "C" fn CPL_FindPlaylistItem(h: HPlaylist, pcPath: *const c_char) -> HItem {
    if pcPath.is_null() { return std::ptr::null_mut(); }
    let target = match CStr::from_ptr(pcPath).to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let p = pl(h);
    let mut cursor = p.first;
    while !cursor.is_null() {
        let path_ptr = CPLI_GetPath(cursor);
        if !path_ptr.is_null() {
            if let Ok(s) = CStr::from_ptr(path_ptr).to_str() {
                if stri_cmp(Some(s), Some(target)).is_eq() {
                    return cursor;
                }
            }
        }
        cursor = CPLI_Next(cursor);
    }
    std::ptr::null_mut()
}

// ---------------------------------------------------------------------------
// Item insertion (pt2 = after worker thread has read the tag)
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPL_AddSingleFile_pt2(h: HPlaylist, hNewFile: HItem, dwBatchID: u32) {
    let p = pl(h);

    // Discard if our batch has moved on (playlist was cleared during load)
    if dwBatchID != p.batch_id {
        CPLI_DestroyItem(hNewFile);
        return;
    }

    // Duplicate check
    let path_ptr = CPLI_GetPath(hNewFile);
    if path_ptr.is_null() { CPLI_DestroyItem(hNewFile); return; }
    let path_str = match CStr::from_ptr(path_ptr).to_str() {
        Ok(s) => s,
        Err(_) => { CPLI_DestroyItem(hNewFile); return; }
    };
    let path_lc = path_str.to_ascii_lowercase();

    if CP_opt_allow_file_once() != FALSE && p.path_hash.contains(&path_lc) {
        CPLI_DestroyItem(hNewFile);
        return;
    }

    // Append to linked list tail
    CPLI_SetPrev(hNewFile, p.last);
    CPLI_SetNext(hNewFile, std::ptr::null_mut());

    if !p.last.is_null() {
        CPLI_SetNext(p.last, hNewFile);
    }
    p.last = hNewFile;
    if p.first.is_null() {
        p.first = hNewFile;
    }

    // Record path for future duplicate checks
    p.path_hash.insert(path_lc);

    // If no track name was read from tags, derive one from the filename
    if CPLI_GetTrackName(hNewFile).is_null() {
        let name = derive_track_name(path_str, CP_IsURL(path_ptr) != FALSE);
        if let Ok(cs) = std::ffi::CString::new(name) {
            CPLI_SetTrackName(hNewFile, cs.as_ptr());
        }
    }

    CPL_Stack_Append(h, hNewFile);
    CPL_cb_OnPlaylistAppend(hNewFile);
}

unsafe fn derive_track_name(path: &str, is_url: bool) -> String {
    if is_url {
        return path.to_owned();
    }

    // Find last slash (backslash or forward slash)
    let after_slash = path.rfind(|c| c == '\\' || c == '/')
        .map(|i| &path[i + 1..])
        .unwrap_or(path);

    // Strip extension: find last dot in the filename part
    let name = after_slash.rfind('.')
        .map(|i| &after_slash[..i])
        .unwrap_or(after_slash);

    if name.is_empty() { path.to_owned() } else { name.to_owned() }
}

// ---------------------------------------------------------------------------
// Remove duplicates (O(n) single-pass using a local HashSet)
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPL_RemoveDuplicates(h: HPlaylist) {
    let mut seen: HashSet<String> = HashSet::new();
    let mut cursor = pl(h).first;
    while !cursor.is_null() {
        let next = CPLI_Next(cursor);
        let path_ptr = CPLI_GetPath(cursor);
        let key = if path_ptr.is_null() {
            String::new()
        } else {
            CStr::from_ptr(path_ptr).to_str().unwrap_or("").to_ascii_lowercase()
        };
        if seen.contains(&key) {
            CPL_RemoveItem(h, cursor);
        } else {
            seen.insert(key);
        }
        cursor = next;
    }
}

// ---------------------------------------------------------------------------
// Remove / activate
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPL_RemoveItem(h: HPlaylist, hItem: HItem) {
    let p = pl(h);

    // Remove from path hash before unlinking
    let path_ptr = CPLI_GetPath(hItem);
    if !path_ptr.is_null() {
        if let Ok(s) = CStr::from_ptr(path_ptr).to_str() {
            p.path_hash.remove(&s.to_ascii_lowercase());
        }
    }

    CPL_cb_OnPlaylistItemDelete(hItem);
    CPL_UnlinkItem(h, hItem);
    CPL_Stack_Remove(h, hItem);

    if hItem == p.current {
        // Active item: defer destruction until activation changes
        CPLI_SetNext(hItem, std::ptr::null_mut());
        CPLI_SetPrev(hItem, std::ptr::null_mut());
        CPLI_SetDestroyOnDeactivate(hItem, TRUE);
        CPL_cb_OnPlaylistActivationChange(hItem, FALSE);
        CPLI_SetCookie(hItem, INVALID_ITEM);
    } else {
        CPLI_DestroyItem(hItem);
    }
}

#[no_mangle]
pub unsafe extern "C" fn CPL_SetActiveItem(h: HPlaylist, hItem: HItem) {
    let p = pl(h);

    if p.current == hItem { return; }

    // Deactivate old current
    if !p.current.is_null() {
        if CPLI_IsDestroyOnDeactivate(p.current) != FALSE {
            CPLI_DestroyItem(p.current);
        } else {
            CPL_cb_OnPlaylistActivationChange(p.current, FALSE);
        }
    }

    p.current = hItem;

    if !p.current.is_null() {
        CPL_cb_OnPlaylistActivationChange(p.current, TRUE);
        if CP_opt_read_id3_tag_of_selected() != FALSE {
            CPLI_ReadTag(hItem);
        }
    } else {
        CPL_cb_OnPlaylistActivationEmpty();
    }

    CPL_Stack_SetCursor(h, p.current);

    if !p.current.is_null() {
        let path = CPLI_GetPath(hItem);
        if !path.is_null() {
            CP_opt_set_initial_file(path);
        }
    }
}

// ---------------------------------------------------------------------------
// Insert before / after (used by drag-and-drop reordering)
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPL_InsertItemBefore(h: HPlaylist, hAnchor: HItem, hToMove: HItem) {
    CPL_UnlinkItem(h, hToMove);

    let p = pl(h);
    let anchor_prev = CPLI_Prev(hAnchor);

    if !anchor_prev.is_null() {
        CPLI_SetNext(anchor_prev, hToMove);
        CPLI_SetPrev(hToMove, anchor_prev);
    } else {
        p.first = hToMove;
        CPLI_SetPrev(hToMove, std::ptr::null_mut());
    }

    CPLI_SetPrev(hAnchor, hToMove);
    CPLI_SetNext(hToMove, hAnchor);
}

#[no_mangle]
pub unsafe extern "C" fn CPL_InsertItemAfter(h: HPlaylist, hAnchor: HItem, hToMove: HItem) {
    CPL_UnlinkItem(h, hToMove);

    let p = pl(h);
    let anchor_next = CPLI_Next(hAnchor);

    if !anchor_next.is_null() {
        CPLI_SetPrev(anchor_next, hToMove);
        CPLI_SetNext(hToMove, anchor_next);
    } else {
        p.last = hToMove;
        CPLI_SetNext(hToMove, std::ptr::null_mut());
    }

    CPLI_SetNext(hAnchor, hToMove);
    CPLI_SetPrev(hToMove, hAnchor);
}

// ---------------------------------------------------------------------------
// Sort  (replaces qsort + CPs_PlaylistItem** flat array)
// ---------------------------------------------------------------------------

// CPe_PlayItemSortElement enum values
const PISE_TRACK_STACK_POS: c_int = 0;
const PISE_ARTIST:          c_int = 1;
const PISE_ALBUM:           c_int = 2;
const PISE_TRACK_NAME:      c_int = 3;
const PISE_YEAR:            c_int = 4;
const PISE_COMMENT:         c_int = 5;
const PISE_TRACK_NUM:       c_int = 6;
const PISE_GENRE:           c_int = 7;
const PISE_PATH:            c_int = 8;
const PISE_FILENAME:        c_int = 9;
const PISE_LENGTH:          c_int = 10;

#[no_mangle]
pub unsafe extern "C" fn CPL_SortList(h: HPlaylist, enElement: c_int, bDesc: BOOL) {
    let p = pl(h);
    if p.first.is_null() { return; }

    // Collect into a Vec
    let mut items: Vec<HItem> = Vec::new();
    let mut cursor = p.first;
    while !cursor.is_null() {
        items.push(cursor);
        cursor = CPLI_Next(cursor);
    }

    // Sort using the requested comparator
    items.sort_by(|&a, &b| {
        let ord = match enElement {
            PISE_TRACK_STACK_POS => {
                let pa = CPLI_GetTrackStackPos_as_int(a);
                let pb = CPLI_GetTrackStackPos_as_int(b);
                pa.cmp(&pb)
            }
            PISE_TRACK_NAME => stri_cmp(cstr_opt(CPLI_GetTrackName(a)), cstr_opt(CPLI_GetTrackName(b))),
            PISE_ARTIST     => stri_cmp(cstr_opt(CPLI_GetArtist(a)), cstr_opt(CPLI_GetArtist(b))),
            PISE_ALBUM      => stri_cmp(cstr_opt(CPLI_GetAlbum(a)), cstr_opt(CPLI_GetAlbum(b))),
            PISE_YEAR       => stri_cmp(cstr_opt(CPLI_GetYear(a)), cstr_opt(CPLI_GetYear(b))),
            PISE_COMMENT    => stri_cmp(cstr_opt(CPLI_GetComment(a)), cstr_opt(CPLI_GetComment(b))),
            PISE_GENRE      => stri_cmp(cstr_opt(CPLI_GetGenre(a)), cstr_opt(CPLI_GetGenre(b))),
            PISE_TRACK_NUM  => {
                let na = CPLI_GetTrackNum(a);
                let nb = CPLI_GetTrackNum(b);
                if na != nb { na.cmp(&nb) }
                else { stri_cmp(cstr_opt(CPLI_GetPath(a)), cstr_opt(CPLI_GetPath(b))) }
            }
            PISE_LENGTH => {
                let la = CPLI_GetTrackLength(a);
                let lb = CPLI_GetTrackLength(b);
                la.cmp(&lb)
            }
            PISE_FILENAME | PISE_PATH | _ =>
                stri_cmp(cstr_opt(CPLI_GetPath(a)), cstr_opt(CPLI_GetPath(b))),
        };
        if bDesc != FALSE { ord.reverse() } else { ord }
    });

    // Relink the list
    p.first = std::ptr::null_mut();
    p.last  = std::ptr::null_mut();
    let mut prev: HItem = std::ptr::null_mut();
    for &item in &items {
        CPLI_SetPrev(item, prev);
        if !prev.is_null() {
            CPLI_SetNext(prev, item);
        } else {
            p.first = item;
        }
        prev = item;
    }
    if !prev.is_null() {
        CPLI_SetNext(prev, std::ptr::null_mut());
        p.last = prev;
    }

    CPL_cb_SetWindowToReflectList();
}

// CPLI_GetTrackStackPos calls into C — declare it here
extern "C" {
    fn CPLI_GetTrackStackPos(hItem: HItem) -> c_int;
}

unsafe fn CPLI_GetTrackStackPos_as_int(hItem: HItem) -> i32 {
    CPLI_GetTrackStackPos(hItem)
}

// ---------------------------------------------------------------------------
// Track stack
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPL_Stack_Append(h: HPlaylist, hItem: HItem) {
    let p = pl(h);

    // Clamp cursor before adding
    if p.stack_cursor > p.stack.len() {
        p.stack_cursor = p.stack.len();
    }

    let item_number = (p.stack.len() as i32) - (p.stack_cursor as i32);
    p.stack.push(hItem);
    CPLI_SetTrackStackPos(hItem, item_number);

    CPL_cb_TrackStackChanged();
}

#[no_mangle]
pub unsafe extern "C" fn CPL_Stack_Renumber(h: HPlaylist) {
    let p = pl(h);
    let cursor = p.stack_cursor as i32;
    for (i, &item) in p.stack.iter().enumerate() {
        CPLI_SetTrackStackPos(item, (i as i32) - cursor);
    }
    CPL_cb_TrackStackChanged();
}

#[no_mangle]
pub unsafe extern "C" fn CPL_Stack_Remove(h: HPlaylist, hItem: HItem) {
    let p = pl(h);
    CPLI_SetTrackStackPos(hItem, TRACKSTACK_UNSTACKED);

    if let Some(idx) = p.stack.iter().position(|&x| x == hItem) {
        p.stack.remove(idx);
        if idx < p.stack_cursor && p.stack_cursor > 0 {
            p.stack_cursor -= 1;
        }
        CPL_Stack_Renumber(h);
        CPL_cb_TrackStackChanged();
    }
}

#[no_mangle]
pub unsafe extern "C" fn CPL_Stack_SetCursor(h: HPlaylist, hItem: HItem) {
    let p = pl(h);
    if hItem.is_null() { return; }

    if let Some(idx) = p.stack.iter().position(|&x| x == hItem) {
        p.stack_cursor = idx;
    } else {
        // Item not in stack — append it and set cursor to it
        CPL_Stack_Append(h, hItem);
        p.stack_cursor = p.stack.len().saturating_sub(1);
    }
}

#[no_mangle]
pub unsafe extern "C" fn CPL_Stack_Clear(h: HPlaylist) {
    let p = pl(h);
    for &item in &p.stack {
        CPLI_SetTrackStackPos(item, TRACKSTACK_UNSTACKED);
    }
    p.stack.clear();
    p.stack_cursor = 0;
    CPL_cb_TrackStackChanged();
}

#[no_mangle]
pub unsafe extern "C" fn CPL_Stack_RestackAll(h: HPlaylist) {
    let p = pl(h);
    p.stack.clear();
    p.stack_cursor = 0;

    let mut cursor = p.first;
    while !cursor.is_null() {
        CPL_Stack_Append(h, cursor);
        cursor = CPLI_Next(cursor);
    }

    if !p.current.is_null() {
        CPL_Stack_SetCursor(h, p.current);
    }
}

// CPe_ItemStackState enum values
const ISS_UNSTACKED:    c_int = 0;
const ISS_PLAYED:       c_int = 1;
const ISS_STACKED_TOP:  c_int = 2;
const ISS_STACKED:      c_int = 3;

#[no_mangle]
pub unsafe extern "C" fn CPL_Stack_GetItemState(h: HPlaylist, hItem: HItem) -> c_int {
    let p = pl(h);
    for (i, &x) in p.stack.iter().enumerate() {
        if x == hItem {
            return if i < p.stack_cursor {
                ISS_PLAYED
            } else if i == p.stack_cursor {
                ISS_STACKED_TOP
            } else {
                ISS_STACKED
            };
        }
    }
    ISS_UNSTACKED
}

#[no_mangle]
pub unsafe extern "C" fn CPL_Stack_Shuffle(h: HPlaylist, bForceCurrentToHead: BOOL) {
    let p = pl(h);
    if p.stack.is_empty() { return; }

    // Fisher-Yates shuffle using C rand()
    let n = p.stack.len();
    for i in (1..n).rev() {
        let j = (rand() as usize).wrapping_rem(i + 1);
        p.stack.swap(i, j);
    }

    p.stack_cursor = 0;

    let current = p.current;
    if !current.is_null() {
        if bForceCurrentToHead != FALSE {
            // Move current to position 0
            if let Some(idx) = p.stack.iter().position(|&x| x == current) {
                p.stack.swap(0, idx);
            }
        } else {
            // If current ended up at [0], swap it to the last slot to avoid double-play
            if p.stack[0] == current {
                let last = n - 1;
                p.stack.swap(0, last);
            }
        }
    }

    CPL_Stack_Renumber(h);
}

#[no_mangle]
pub unsafe extern "C" fn CPL_Stack_ClipFromCurrent(h: HPlaylist) {
    let p = pl(h);
    if p.stack.is_empty() || p.stack_cursor >= p.stack.len() { return; }

    for i in (p.stack_cursor + 1)..p.stack.len() {
        CPLI_SetTrackStackPos(p.stack[i], TRACKSTACK_UNSTACKED);
    }
    p.stack.truncate(p.stack_cursor + 1);

    CPL_cb_TrackStackChanged();
}

#[no_mangle]
pub unsafe extern "C" fn CPL_Stack_ClipFromItem(h: HPlaylist, hItem: HItem) {
    let p = pl(h);
    if p.stack.is_empty() { return; }

    let found_idx = p.stack.iter().position(|&x| x == hItem);
    let clip_start = match found_idx {
        Some(i) => i + 1,
        None    => return,
    };

    for i in clip_start..p.stack.len() {
        CPLI_SetTrackStackPos(p.stack[i], TRACKSTACK_UNSTACKED);
    }
    p.stack.truncate(clip_start);

    if p.stack_cursor >= p.stack.len() && !p.stack.is_empty() {
        p.stack_cursor = p.stack.len() - 1;
    }

    CPL_cb_TrackStackChanged();
}

#[no_mangle]
pub unsafe extern "C" fn CPL_Stack_PlayNext(h: HPlaylist, hItem: HItem) {
    CPL_Stack_Remove(h, hItem);

    let p = pl(h);

    // If cursor is past the end or stack is empty, just append
    if p.stack_cursor >= p.stack.len() || p.stack.is_empty() {
        CPL_Stack_Append(h, hItem);
        return;
    }

    // Insert at cursor+1, shifting everything above up
    let insert_at = p.stack_cursor + 1;
    p.stack.insert(insert_at, hItem);

    // Renumber from insert point
    let cursor = p.stack_cursor as i32;
    for (i, &x) in p.stack.iter().enumerate() {
        CPLI_SetTrackStackPos(x, (i as i32) - cursor);
    }

    CPL_cb_TrackStackChanged();
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CPL_PeekNextItem(h: HPlaylist) -> HItem {
    let p = pl(h);
    let current = p.current;
    let cursor  = p.stack_cursor;
    let len     = p.stack.len();

    // If current != stack[cursor], next is stack[cursor]
    if !current.is_null() && cursor < len && p.stack[cursor] != current {
        return p.stack[cursor];
    }

    // Next is stack[cursor+1]
    if cursor + 1 < len {
        return p.stack[cursor + 1];
    }

    // Wrap with repeat
    if CP_opt_repeat_playlist() != FALSE && !p.stack.is_empty() {
        return p.stack[0];
    }

    std::ptr::null_mut()
}

// pmCurrentItem=0, pmNextItem=1, pmPrevItem=2
const PM_CURRENT: c_int = 0;
const PM_NEXT:    c_int = 1;
const PM_PREV:    c_int = 2;

#[no_mangle]
pub unsafe extern "C" fn CPL_PlayItem(h: HPlaylist, bStopFirst: BOOL, enPlayMode: c_int) {
    let p = pl(h);
    let mut hPlay: HItem = std::ptr::null_mut();
    let cursor = p.stack_cursor;
    let len    = p.stack.len();
    let current = p.current;

    match enPlayMode {
        PM_CURRENT => {
            if !current.is_null() {
                if bStopFirst != FALSE || CPLI_IsDestroyOnDeactivate(current) != FALSE {
                    hPlay = if cursor < len { p.stack[cursor] }
                            else if !p.stack.is_empty() { p.stack[0] }
                            else { std::ptr::null_mut() };
                } else {
                    hPlay = current;
                }
            } else if cursor < len {
                hPlay = p.stack[cursor];
            } else {
                if CP_opt_shuffle_play() != FALSE {
                    CPL_Stack_Shuffle(h, FALSE);
                }
                hPlay = if !p.stack.is_empty() { p.stack[0] } else { std::ptr::null_mut() };
            }
        }
        PM_NEXT => {
            // If current != stack[cursor], play stack[cursor]
            if !current.is_null() && cursor < len && p.stack[cursor] != current {
                hPlay = p.stack[cursor];
            }
            if hPlay.is_null() {
                // Advance cursor
                let p2 = pl(h);
                if p2.stack_cursor < p2.stack.len() {
                    p2.stack_cursor += 1;
                }
                let cursor2 = p2.stack_cursor;
                let len2    = p2.stack.len();
                if cursor2 < len2 {
                    hPlay = p2.stack[cursor2];
                }
            }
            if hPlay.is_null() && CP_opt_repeat_playlist() != FALSE {
                if CP_opt_shuffle_play() != FALSE {
                    CPL_Stack_Shuffle(h, FALSE);
                }
                let p3 = pl(h);
                hPlay = if !p3.stack.is_empty() { p3.stack[0] } else { std::ptr::null_mut() };
            }
        }
        PM_PREV => {
            let p = pl(h);
            if p.stack_cursor > 0 {
                hPlay = p.stack[p.stack_cursor - 1];
            } else if CP_opt_repeat_playlist() != FALSE && !p.stack.is_empty() {
                hPlay = p.stack[p.stack.len() - 1];
            } else if !p.stack.is_empty() {
                hPlay = p.stack[0];
            }
        }
        _ => {}
    }

    CPL_SetActiveItem(h, hPlay);
    CPL_PlayActiveItem(h, bStopFirst);
}

#[no_mangle]
pub unsafe extern "C" fn CPL_AdvanceToNextItem(h: HPlaylist) {
    let hNext = CPL_PeekNextItem(h);
    if hNext.is_null() { return; }

    {
        let p = pl(h);
        let current = p.current;
        let cursor  = p.stack_cursor;
        let len     = p.stack.len();

        if !current.is_null() && cursor < len && p.stack[cursor] != current {
            // Current was behind the cursor — cursor stays
        } else {
            if p.stack_cursor < p.stack.len() {
                p.stack_cursor += 1;
            }
            // Wrap around with repeat + optional reshuffle
            if p.stack_cursor >= p.stack.len()
                && CP_opt_repeat_playlist() != FALSE
                && !p.stack.is_empty()
            {
                if CP_opt_shuffle_play() != FALSE {
                    CPL_Stack_Shuffle(h, FALSE);
                }
                p.stack_cursor = 0;
            }
        }
    }

    CPL_SetActiveItem(h, hNext);

    let path = CPLI_GetPath(hNext);
    if !path.is_null() {
        CP_opt_set_initial_file(path);
    }
}
