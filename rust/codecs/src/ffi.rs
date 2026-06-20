/*
 * C type mirrors shared by all codec modules.
 *
 * Every struct must be byte-for-byte compatible with the corresponding C
 * declaration (CPI_Player_CoDec.h, globals.h).  Any change to the C side
 * must be reflected here — and vice-versa.
 */

use std::os::raw::{c_char, c_int, c_void};

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
