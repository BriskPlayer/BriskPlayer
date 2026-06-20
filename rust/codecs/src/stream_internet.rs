/*
 * BriskPlayer — internet stream module (Rust port of CPI_Stream_Internet.c)
 *
 * Safety improvements over the C original:
 *   - Vec<u8>  replaces the realloc/overflow-check loop for playlist download
 *   - Arc<AtomicBool> replaces the unsynchronised BOOL terminate flag
 *   - std::thread replaces _beginthreadex; JoinHandle replaces WaitForSingleObject
 *   - Rust slice/str ops replace strtok_s and strstr for PLS/M3U parsing
 *   - Bounded Vec<u8> replaces CALLOC_TYPE for Icecast metadata blocks
 *   - CPs_InStream vtable reproduced as #[repr(C)] — ownership via Box::into_raw
 */

#![allow(non_snake_case)]

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_uint, c_void};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use windows_sys::Win32::Foundation::{BOOL, FALSE, HWND, LPARAM};

use super::ffi::CpsInStream;
use windows_sys::Win32::Networking::WinInet::{
    HttpQueryInfoA, InternetCloseHandle, InternetOpenA, InternetOpenUrlA, InternetReadFile,
    InternetSetOptionA, HTTP_QUERY_CUSTOM, INTERNET_FLAG_NO_CACHE_WRITE,
    INTERNET_FLAG_PRAGMA_NOCACHE, INTERNET_FLAG_RELOAD, INTERNET_OPEN_TYPE_PRECONFIG,
    INTERNET_OPTION_CONNECT_TIMEOUT, INTERNET_OPTION_RECEIVE_TIMEOUT,
    INTERNET_OPTION_SEND_TIMEOUT,
};
use windows_sys::Win32::System::Threading::Sleep;
use windows_sys::Win32::UI::WindowsAndMessaging::{PeekMessageA, PostMessageA, MSG, PM_NOREMOVE};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const WM_APP: u32 = 0x8000;
const CPNM_SETSTREAMINGSTATE: u32 = WM_APP + 0x107;
const CPTM_STOP: u32 = WM_APP + 0x005;

const STREAM_BUFFER_SIZE: u32 = 0x40000; // 256 KB
const PREBUFFER_AMOUNT: u32 = 0x8000;   // 32 KB
const READ_CHUNK_SIZE: usize = 0x1000;  // 4 KB
const MAX_METADATA_SIZE: u32 = 4096;
const MAX_PLAYLIST_SIZE: usize = 10 * 1024 * 1024;

// ---------------------------------------------------------------------------
// C-side circle-buffer accessors  (shims in CPI_Stream_Internet.c)
// The CPs_CircleBuffer struct has two layouts depending on C23 threading
// support; Rust cannot safely mirror it, so we go through the C vtable.
// ---------------------------------------------------------------------------

extern "C" {
    fn CP_CreateCircleBuffer(size: u32) -> *mut c_void;
    fn CPCB_Write(cb: *mut c_void, src: *const c_void, n: u32);
    fn CPCB_Read(cb: *mut c_void, dst: *mut c_void, n: usize, out: *mut usize) -> BOOL;
    fn CPCB_GetUsedSize(cb: *mut c_void) -> u32;
    fn CPCB_GetFreeSize(cb: *mut c_void) -> u32;
    fn CPCB_SetComplete(cb: *mut c_void);
    fn CPCB_IsComplete(cb: *mut c_void) -> BOOL;
    fn CPCB_Uninitialise(cb: *mut c_void);
}

// CpsInStream vtable is defined in ffi.rs and imported above.

// ---------------------------------------------------------------------------
// Runtime context
// ---------------------------------------------------------------------------

struct StreamContext {
    circle_buffer: *mut c_void,
    terminate: Arc<AtomicBool>,
    filler_thread: Option<std::thread::JoinHandle<()>>,
}

unsafe impl Send for StreamContext {}

struct FillerContext {
    circle_buffer: *mut c_void,
    terminate: Arc<AtomicBool>,
    hwnd_notify: HWND,
    url: String,
    icy_meta_int: u32,
    audio_bytes_read: u32,
}

unsafe impl Send for FillerContext {}

// ---------------------------------------------------------------------------
// Icecast metadata-aware read
//
// Returns Some(n) with audio bytes placed in buf[..n], or None on error.
// Metadata blocks are consumed and discarded (title is logged in debug builds).
// ---------------------------------------------------------------------------

fn read_stream_data(
    h_url: *mut c_void,
    ctx: &mut FillerContext,
    buf: &mut [u8; READ_CHUNK_SIZE],
) -> Option<usize> {
    let mut n: u32 = 0;

    if ctx.icy_meta_int == 0 {
        let ok = unsafe {
            InternetReadFile(h_url, buf.as_mut_ptr() as _, READ_CHUNK_SIZE as u32, &mut n)
        };
        return if ok != 0 { Some(n as usize) } else { None };
    }

    loop {
        let until_meta = ctx.icy_meta_int - ctx.audio_bytes_read;
        let to_read = (READ_CHUNK_SIZE as u32).min(until_meta);

        if to_read > 0 {
            let ok = unsafe {
                InternetReadFile(h_url, buf.as_mut_ptr() as _, to_read, &mut n)
            };
            if ok == 0 {
                return None;
            }
            ctx.audio_bytes_read += n;
            return Some(n as usize);
        }

        // Read the 1-byte metadata-block length prefix (length * 16 = bytes)
        let mut meta_len_byte: u8 = 0;
        let ok = unsafe {
            InternetReadFile(h_url, &mut meta_len_byte as *mut _ as _, 1, &mut n)
        };
        if ok == 0 || n != 1 {
            return None;
        }

        let meta_size = (meta_len_byte as u32) * 16;

        if meta_size > MAX_METADATA_SIZE {
            // Drain byte-by-byte without allocating to avoid memory exhaustion
            let mut dummy: u8 = 0;
            for _ in 0..meta_size {
                if unsafe { InternetReadFile(h_url, &mut dummy as *mut _ as _, 1, &mut n) } == 0 {
                    return None;
                }
            }
        } else if meta_size > 0 {
            let mut meta_buf = vec![0u8; meta_size as usize];
            let mut meta_read: u32 = 0;
            unsafe {
                InternetReadFile(h_url, meta_buf.as_mut_ptr() as _, meta_size, &mut meta_read);
            }
            #[cfg(debug_assertions)]
            if let Some(title) = parse_icy_title(&meta_buf[..meta_read as usize]) {
                let _ = title; // available for attaching a debugger / future logging hook
            }
        }

        ctx.audio_bytes_read = 0;
        // Loop back to deliver audio data after consuming the metadata block
    }
}

fn parse_icy_title(meta: &[u8]) -> Option<String> {
    let s = std::str::from_utf8(meta).ok()?;
    let start = s.find("StreamTitle='")? + "StreamTitle='".len();
    let rest = &s[start..];
    let end = rest.find("';")?;
    Some(rest[..end].to_owned())
}

// ---------------------------------------------------------------------------
// Playlist fetching and parsing
// ---------------------------------------------------------------------------

fn download_playlist(url: &str) -> Option<Vec<u8>> {
    let agent = CString::new("BriskPlayer/3.0").ok()?;
    let url_c = CString::new(url).ok()?;
    let timeout: u32 = 15000;

    let h_internet = unsafe {
        InternetOpenA(
            agent.as_ptr() as _,
            INTERNET_OPEN_TYPE_PRECONFIG,
            std::ptr::null(),
            std::ptr::null(),
            0,
        )
    };
    if h_internet.is_null() {
        return None;
    }

    unsafe {
        InternetSetOptionA(
            h_internet,
            INTERNET_OPTION_CONNECT_TIMEOUT,
            &timeout as *const _ as _,
            4,
        );
        InternetSetOptionA(
            h_internet,
            INTERNET_OPTION_RECEIVE_TIMEOUT,
            &timeout as *const _ as _,
            4,
        );
    }

    let h_url = unsafe {
        InternetOpenUrlA(
            h_internet,
            url_c.as_ptr() as _,
            std::ptr::null(),
            0,
            INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD,
            0,
        )
    };
    if h_url.is_null() {
        unsafe { InternetCloseHandle(h_internet) };
        return None;
    }

    let mut content: Vec<u8> = Vec::new();
    let mut chunk = [0u8; 4096];
    let mut n: u32 = 0;
    loop {
        let ok = unsafe {
            InternetReadFile(h_url, chunk.as_mut_ptr() as _, chunk.len() as u32, &mut n)
        };
        if ok == 0 || n == 0 {
            break;
        }
        if content.len() + n as usize > MAX_PLAYLIST_SIZE {
            content.clear(); // treat oversized playlist as a failure
            break;
        }
        content.extend_from_slice(&chunk[..n as usize]);
    }

    unsafe {
        InternetCloseHandle(h_url);
        InternetCloseHandle(h_internet);
    }

    if content.is_empty() { None } else { Some(content) }
}

fn parse_pls(content: &str) -> Option<String> {
    for line in content.lines() {
        let line = line.trim();
        if line.len() > 8192 {
            continue;
        }
        if line.to_ascii_lowercase().starts_with("file") {
            if let Some(eq) = line.find('=') {
                let url = line[eq + 1..].trim();
                if !url.is_empty() {
                    return Some(url.to_owned());
                }
            }
        }
    }
    None
}

fn parse_m3u(content: &str) -> Option<String> {
    for line in content.lines() {
        let line = line.trim();
        if line.len() > 8192 {
            continue;
        }
        if !line.is_empty() && !line.starts_with('#') && line.contains("://") {
            return Some(line.to_owned());
        }
    }
    None
}

fn is_remote_url(s: &str) -> bool {
    let l = s.to_ascii_lowercase();
    l.starts_with("http://")
        || l.starts_with("https://")
        || l.starts_with("ftp://")
        || l.starts_with("icy://")
}

fn read_playlist_bytes(url: &str) -> Option<Vec<u8>> {
    if is_remote_url(url) {
        download_playlist(url)
    } else {
        std::fs::read(url).ok()
    }
}

fn extract_stream_url(playlist_url: &str) -> Option<String> {
    let bytes = read_playlist_bytes(playlist_url)?;
    let content = std::str::from_utf8(&bytes).ok()?;
    let lower = playlist_url.to_ascii_lowercase();
    if lower.contains(".pls") {
        parse_pls(content)
    } else if lower.contains(".m3u") {
        parse_m3u(content)
    } else if content.contains("File1=") || content.contains("[playlist]") {
        parse_pls(content)
    } else {
        parse_m3u(content)
    }
}

// ---------------------------------------------------------------------------
// Filler thread body
// ---------------------------------------------------------------------------

fn run_filler(mut ctx: FillerContext) {
    let hwnd = ctx.hwnd_notify;
    unsafe { PostMessageA(hwnd, CPNM_SETSTREAMINGSTATE, 1, 0) };

    // icy:// is a SHOUTcast alias for http://
    let actual_url = if ctx.url.to_ascii_lowercase().starts_with("icy://") {
        format!("http://{}", &ctx.url[6..])
    } else {
        ctx.url.clone()
    };

    let url_c = match CString::new(actual_url.as_str()) {
        Ok(s) => s,
        Err(_) => {
            unsafe { CPCB_SetComplete(ctx.circle_buffer) };
            return;
        }
    };

    let agent = CString::new("BriskPlayer/3.0").unwrap();
    let headers = CString::new(
        "User-Agent: BriskPlayer/3.0\r\n\
         Accept: */*\r\n\
         Icy-MetaData: 1\r\n\
         Connection: close\r\n",
    )
    .unwrap();

    let h_internet = unsafe {
        InternetOpenA(
            agent.as_ptr() as _,
            INTERNET_OPEN_TYPE_PRECONFIG,
            std::ptr::null(),
            std::ptr::null(),
            0,
        )
    };
    if h_internet.is_null() {
        unsafe { CPCB_SetComplete(ctx.circle_buffer) };
        return;
    }

    let timeout: u32 = 10000;
    unsafe {
        InternetSetOptionA(
            h_internet,
            INTERNET_OPTION_CONNECT_TIMEOUT,
            &timeout as *const _ as _,
            4,
        );
        InternetSetOptionA(
            h_internet,
            INTERNET_OPTION_RECEIVE_TIMEOUT,
            &timeout as *const _ as _,
            4,
        );
        InternetSetOptionA(
            h_internet,
            INTERNET_OPTION_SEND_TIMEOUT,
            &timeout as *const _ as _,
            4,
        );
    }

    let h_url = unsafe {
        InternetOpenUrlA(
            h_internet,
            url_c.as_ptr() as _,
            headers.as_ptr() as _,
            headers.to_bytes().len() as u32,
            INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD,
            0,
        )
    };
    if h_url.is_null() {
        unsafe {
            InternetCloseHandle(h_internet);
            CPCB_SetComplete(ctx.circle_buffer);
        }
        return;
    }

    // Read icy-metaint from response headers.
    // HTTP_QUERY_CUSTOM requires the header name pre-loaded in the buffer;
    // HttpQueryInfoA overwrites it with the value on success.
    {
        let mut buf = [0u8; 32];
        let name = b"icy-metaint";
        buf[..name.len()].copy_from_slice(name);
        let mut buf_len = buf.len() as u32;
        let ok = unsafe {
            HttpQueryInfoA(
                h_url,
                HTTP_QUERY_CUSTOM,
                buf.as_mut_ptr() as _,
                &mut buf_len,
                std::ptr::null_mut(),
            )
        };
        if ok != 0 {
            if let Ok(s) = std::str::from_utf8(&buf[..buf_len as usize]) {
                if let Ok(n) = s.trim_end_matches('\0').trim().parse::<i64>() {
                    if (256..=1_048_576).contains(&n) {
                        ctx.icy_meta_int = n as u32;
                    }
                }
            }
        }
    }

    let mut read_buf = [0u8; READ_CHUNK_SIZE];

    loop {
        if ctx.terminate.load(Ordering::Relaxed) {
            break;
        }

        let free = unsafe { CPCB_GetFreeSize(ctx.circle_buffer) };
        if (free as usize) < READ_CHUNK_SIZE {
            unsafe { Sleep(20) };
            continue;
        }

        match read_stream_data(h_url, &mut ctx, &mut read_buf) {
            None => break,
            Some(0) => unsafe { Sleep(50) },
            Some(n) => {
                unsafe { CPCB_Write(ctx.circle_buffer, read_buf.as_ptr() as _, n as u32) };
                let used = unsafe { CPCB_GetUsedSize(ctx.circle_buffer) };
                let pct = (used as u64 * 100 / STREAM_BUFFER_SIZE as u64) as LPARAM;
                unsafe { PostMessageA(hwnd, CPNM_SETSTREAMINGSTATE, 1, pct) };
            }
        }
    }

    unsafe {
        InternetCloseHandle(h_url);
        InternetCloseHandle(h_internet);
        CPCB_SetComplete(ctx.circle_buffer);
        PostMessageA(hwnd, CPNM_SETSTREAMINGSTATE, 0, 0);
    }
}

// ---------------------------------------------------------------------------
// CPs_InStream vtable implementations
// ---------------------------------------------------------------------------

unsafe extern "C" fn inet_uninitialise(stream: *mut CpsInStream) {
    let mut ctx = Box::from_raw((*stream).cookie as *mut StreamContext);
    ctx.terminate.store(true, Ordering::Relaxed);
    if let Some(handle) = ctx.filler_thread.take() {
        let _ = handle.join();
    }
    CPCB_Uninitialise(ctx.circle_buffer);
    // ctx dropped here, then stream itself
    drop(Box::from_raw(stream));
}

unsafe extern "C" fn inet_read(
    stream: *mut CpsInStream,
    dst: *mut c_void,
    n: usize,
    out: *mut usize,
) -> BOOL {
    let ctx = &*((*stream).cookie as *const StreamContext);
    CPCB_Read(ctx.circle_buffer, dst, n, out)
}

unsafe extern "C" fn inet_seek(_stream: *mut CpsInStream, _offset: usize) {
    // Internet streams are not seekable; silently ignore
}

unsafe extern "C" fn inet_get_length(_stream: *mut CpsInStream) -> c_uint {
    0xFFFF_FFFF // unbounded stream
}

unsafe extern "C" fn inet_is_seekable(_stream: *mut CpsInStream) -> BOOL {
    FALSE
}

// ---------------------------------------------------------------------------
// Public entry point — called from CPI_Stream.c
// ---------------------------------------------------------------------------

#[no_mangle]
pub unsafe extern "C" fn CP_CreateInStream_Internet(
    url: *const c_char,
    hwnd: HWND,
) -> *mut CpsInStream {
    let url_str = CStr::from_ptr(url).to_string_lossy().into_owned();

    // Resolve .pls / .m3u containers to the actual stream URL before opening
    let lower = url_str.to_ascii_lowercase();
    let actual_url = if lower.contains(".pls") || lower.contains(".m3u") {
        match extract_stream_url(&url_str) {
            Some(u) => u,
            None => return std::ptr::null_mut(),
        }
    } else {
        url_str
    };

    let cb = CP_CreateCircleBuffer(STREAM_BUFFER_SIZE);
    if cb.is_null() {
        return std::ptr::null_mut();
    }

    let terminate = Arc::new(AtomicBool::new(false));

    let filler = FillerContext {
        circle_buffer: cb,
        terminate: terminate.clone(),
        hwnd_notify: hwnd,
        url: actual_url,
        icy_meta_int: 0,
        audio_bytes_read: 0,
    };

    let handle = std::thread::spawn(move || run_filler(filler));

    let stream_ctx = Box::new(StreamContext {
        circle_buffer: cb,
        terminate,
        filler_thread: Some(handle),
    });

    // Pre-buffer: wait for PREBUFFER_AMOUNT bytes, stream completion,
    // a stop message, or a 10-second timeout — whichever comes first.
    let mut iters = 0i32;
    loop {
        if CPCB_IsComplete(cb) != 0 {
            break;
        }
        Sleep(100);
        iters += 1;
        let used = CPCB_GetUsedSize(cb);
        let mut msg: MSG = std::mem::zeroed();
        if PeekMessageA(&mut msg, 0, CPTM_STOP, CPTM_STOP, PM_NOREMOVE) != 0 {
            break;
        }
        if iters >= 100 || used >= PREBUFFER_AMOUNT {
            break;
        }
    }

    let stream = Box::new(CpsInStream {
        uninitialise: inet_uninitialise,
        read: inet_read,
        seek: inet_seek,
        tell: None,
        get_length: inet_get_length,
        is_seekable: inet_is_seekable,
        cookie: Box::into_raw(stream_ctx) as _,
    });

    Box::into_raw(stream)
}
