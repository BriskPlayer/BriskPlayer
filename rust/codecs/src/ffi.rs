/*
 * C type mirrors shared by all codec modules.
 *
 * Every struct must be byte-for-byte compatible with the corresponding C
 * declaration (CPI_Player_CoDec.h, globals.h).  Any change to the C side
 * must be reflected here — and vice-versa.
 */

use std::io::{self, Read, Seek, SeekFrom};
use std::os::raw::{c_char, c_int, c_void};
use std::ptr::NonNull;

// ---------------------------------------------------------------------------
// Windows primitive aliases
// ---------------------------------------------------------------------------

pub type BOOL = c_int;
pub type DWORD = u32;
pub type UINT  = u32;

pub const TRUE:  BOOL = 1;
pub const FALSE: BOOL = 0;

// ---------------------------------------------------------------------------
// CPs_FileInfo  (globals.h)
// ---------------------------------------------------------------------------

#[repr(C)]
pub struct CPs_FileInfo {
    pub m_iFileLength_Secs: UINT,
    pub m_iBitRate_Kbs:     UINT,
    pub m_iFreq_Hz:         UINT,
    pub m_bStereo:          BOOL,
    pub m_b16bit:           BOOL,
}

// ---------------------------------------------------------------------------
// CPs_CoDecModule  (CPI_Player_CoDec.h)
// ---------------------------------------------------------------------------

#[repr(C)]
pub struct CPs_CoDecModule {
    pub Uninitialise:         Option<unsafe extern "C" fn(*mut CPs_CoDecModule)>,
    pub OpenFile:             Option<unsafe extern "C" fn(*mut CPs_CoDecModule, *const c_char, usize, *mut c_void) -> BOOL>,
    pub CloseFile:            Option<unsafe extern "C" fn(*mut CPs_CoDecModule)>,
    pub Seek:                 Option<unsafe extern "C" fn(*mut CPs_CoDecModule, c_int, c_int)>,
    pub GetFileInfo:          Option<unsafe extern "C" fn(*mut CPs_CoDecModule, *mut CPs_FileInfo)>,
    pub GetPCMBlock:          Option<unsafe extern "C" fn(*mut CPs_CoDecModule, *mut c_void, *mut DWORD) -> BOOL>,
    pub GetCurrentPos_secs:   Option<unsafe extern "C" fn(*mut CPs_CoDecModule) -> c_int>,
    pub m_pModuleCookie:       *mut c_void,
    pub m_pFileAssociationCookie: *mut c_void,
}

// SAFETY: All access is from the single-threaded player engine; this matches
// the guarantees made by the original C code.
unsafe impl Send for CPs_CoDecModule {}

// ---------------------------------------------------------------------------
// File-association helpers — defined in CPI_Player_FileAssoc.c
// ---------------------------------------------------------------------------

extern "C" {
    pub fn CPFA_InitialiseFileAssociations(pCoDec: *mut CPs_CoDecModule);
    pub fn CPFA_EmptyFileAssociations(pCoDec: *mut CPs_CoDecModule);
    pub fn CPFA_AddFileAssociation(
        pCoDec:      *mut CPs_CoDecModule,
        pcExtension: *const c_char,
        dwCookie:    usize,
    );
}

// ---------------------------------------------------------------------------
// CPs_InStream  (CPI_Stream.h)
//
// Abstract byte-stream returned by CP_CreateInStream.  Lifetime is managed
// through the vtable: call uninitialise(ptr) when done — it frees both the
// context and the struct itself.  Never call Box::from_raw on this pointer.
// ---------------------------------------------------------------------------

#[repr(C)]
pub struct CpsInStream {
    pub uninitialise: unsafe extern "C" fn(*mut CpsInStream),
    pub read:         unsafe extern "C" fn(*mut CpsInStream, *mut c_void, usize, *mut usize) -> BOOL,
    pub seek:         unsafe extern "C" fn(*mut CpsInStream, usize),
    pub tell:         Option<unsafe extern "C" fn(*mut CpsInStream) -> UINT>,
    pub get_length:   unsafe extern "C" fn(*mut CpsInStream) -> UINT,
    pub is_seekable:  unsafe extern "C" fn(*mut CpsInStream) -> BOOL,
    pub cookie:       *mut c_void,
}

// Access is always from the player thread, matching the C threading model.
unsafe impl Send for CpsInStream {}

extern "C" {
    /// Dispatches to CP_CreateInStream_LocalFile or CP_CreateInStream_Internet
    /// depending on whether `path` is a filesystem path or a URL.
    pub fn CP_CreateInStream(path: *const c_char, hwnd: *mut c_void) -> *mut CpsInStream;
}

// ---------------------------------------------------------------------------
// InStream — safe RAII wrapper around CpsInStream
//
// Calling code deals only with this type; the raw CpsInStream pointer never
// leaks into codec logic.  Drop calls uninitialise automatically, so there is
// no need for manual cleanup anywhere.
// ---------------------------------------------------------------------------

/// Owns a `CpsInStream` and calls `uninitialise` when dropped.
///
/// Implements `Read` and `Seek` so it can be passed directly to pure-Rust
/// libraries such as `FlacByteReader` and `OggStreamReader`.
pub struct InStream {
    ptr:          NonNull<CpsInStream>,
    pub seekable: bool,
    pos:          u64, // tracked locally; needed for SeekFrom::Current(0)
}

impl InStream {
    /// Open a stream for `path` (filesystem path or URL).
    ///
    /// Returns `None` if `CP_CreateInStream` returns null.
    ///
    /// # Safety
    /// `path` must be a valid null-terminated C string.
    /// `hwnd` must be a valid `HWND` or null.
    pub unsafe fn open(path: *const c_char, hwnd: *mut c_void) -> Option<Self> {
        let raw = CP_CreateInStream(path, hwnd);
        let ptr = NonNull::new(raw)?;
        // SAFETY: ptr is non-null and freshly returned from CP_CreateInStream.
        let seekable = (ptr.as_ref().is_seekable)(ptr.as_ptr()) != FALSE;
        Some(InStream { ptr, seekable, pos: 0 })
    }

    /// Length of the underlying data.  Returns `0xFFFF_FFFF` for internet streams.
    pub fn length(&self) -> u64 {
        // SAFETY: ptr invariant holds for the lifetime of self.
        unsafe { (self.ptr.as_ref().get_length)(self.ptr.as_ptr()) as u64 }
    }

    /// Current byte position (updated by `Read::read`).
    pub fn pos(&self) -> u64 { self.pos }
}

impl Drop for InStream {
    fn drop(&mut self) {
        // SAFETY: ptr invariant; called exactly once.
        unsafe { (self.ptr.as_ref().uninitialise)(self.ptr.as_ptr()) }
    }
}

impl Read for InStream {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        if buf.is_empty() { return Ok(0); }
        let mut got: usize = 0;
        // SAFETY: ptr invariant; buf slice is valid for buf.len() bytes.
        let ok = unsafe {
            (self.ptr.as_ref().read)(
                self.ptr.as_ptr(),
                buf.as_mut_ptr() as *mut c_void,
                buf.len(),
                &mut got,
            )
        };
        if ok != FALSE { self.pos += got as u64; Ok(got) } else { Ok(0) }
    }
}

impl Seek for InStream {
    fn seek(&mut self, from: SeekFrom) -> io::Result<u64> {
        if !self.seekable {
            return match from {
                SeekFrom::Current(0) => Ok(self.pos),
                _ => Err(io::Error::new(io::ErrorKind::Unsupported, "stream not seekable")),
            };
        }
        let length = self.length();
        let new_pos = match from {
            SeekFrom::Start(p)   => p,
            SeekFrom::Current(d) => (self.pos as i64 + d).max(0) as u64,
            SeekFrom::End(d)     => (length as i64 + d).max(0) as u64,
        };
        // SAFETY: ptr invariant.
        unsafe { (self.ptr.as_ref().seek)(self.ptr.as_ptr(), new_pos as usize) }
        self.pos = new_pos;
        Ok(new_pos)
    }
}

// SAFETY: all access is from the single-threaded player engine.
unsafe impl Send for InStream {}
