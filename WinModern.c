/*
 * BriskPlayer - Blazing fast audio player.
 * Copyright (C) 2000-2001 Niek Albers
 * Copyright (C) 2025 Zach Bacon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

////////////////////////////////////////////////////////////////////////////////
//
// Modern Windows API Implementation
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "WinModern.h"

#include <process.h>  // For _beginthreadex

////////////////////////////////////////////////////////////////////////////////
//
// WIC Image Loading Implementation
//
// We dynamically load windowscodecs.dll to support systems without WIC
//
////////////////////////////////////////////////////////////////////////////////

// WIC GUIDs - defined here to avoid header issues
static const GUID CP_CLSID_WICImagingFactory = 
    { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
static const GUID CP_IID_IWICImagingFactory = 
    { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
static const GUID CP_GUID_WICPixelFormat32bppBGRA = 
    { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f } };

// Minimal WIC interface definitions for C
// We only define what we need to avoid header conflicts

typedef interface IWICBitmapSource IWICBitmapSource;
typedef interface IWICBitmapDecoder IWICBitmapDecoder;
typedef interface IWICBitmapFrameDecode IWICBitmapFrameDecode;
typedef interface IWICFormatConverter IWICFormatConverter;
typedef interface IWICImagingFactory IWICImagingFactory;
typedef interface IWICStream IWICStream;

typedef enum WICDecodeOptions {
    WICDecodeMetadataCacheOnDemand = 0,
    WICDecodeMetadataCacheOnLoad = 1
} WICDecodeOptions;

typedef enum WICBitmapDitherType {
    WICBitmapDitherTypeNone = 0
} WICBitmapDitherType;

typedef enum WICBitmapPaletteType {
    WICBitmapPaletteTypeCustom = 0
} WICBitmapPaletteType;

// IWICBitmapSource vtable
typedef struct IWICBitmapSourceVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IWICBitmapSource*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IWICBitmapSource*);
    ULONG (STDMETHODCALLTYPE *Release)(IWICBitmapSource*);
    HRESULT (STDMETHODCALLTYPE *GetSize)(IWICBitmapSource*, UINT*, UINT*);
    HRESULT (STDMETHODCALLTYPE *GetPixelFormat)(IWICBitmapSource*, GUID*);
    HRESULT (STDMETHODCALLTYPE *GetResolution)(IWICBitmapSource*, double*, double*);
    HRESULT (STDMETHODCALLTYPE *CopyPalette)(IWICBitmapSource*, void*);
    HRESULT (STDMETHODCALLTYPE *CopyPixels)(IWICBitmapSource*, const void*, UINT, UINT, BYTE*);
} IWICBitmapSourceVtbl;

struct IWICBitmapSource { const IWICBitmapSourceVtbl* lpVtbl; };

// IWICBitmapDecoder vtable - must match exact COM interface order
typedef struct IWICBitmapDecoderVtbl {
    // IUnknown (0-2)
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IWICBitmapDecoder*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IWICBitmapDecoder*);
    ULONG (STDMETHODCALLTYPE *Release)(IWICBitmapDecoder*);
    // IWICBitmapDecoder methods (3-11)
    HRESULT (STDMETHODCALLTYPE *QueryCapability)(IWICBitmapDecoder*, void*, DWORD*);  // 3
    HRESULT (STDMETHODCALLTYPE *Initialize)(IWICBitmapDecoder*, void*, WICDecodeOptions);  // 4
    HRESULT (STDMETHODCALLTYPE *GetContainerFormat)(IWICBitmapDecoder*, GUID*);  // 5
    HRESULT (STDMETHODCALLTYPE *GetDecoderInfo)(IWICBitmapDecoder*, void**);  // 6
    HRESULT (STDMETHODCALLTYPE *CopyPalette)(IWICBitmapDecoder*, void*);  // 7
    HRESULT (STDMETHODCALLTYPE *GetMetadataQueryReader)(IWICBitmapDecoder*, void**);  // 8
    HRESULT (STDMETHODCALLTYPE *GetPreview)(IWICBitmapDecoder*, void**);  // 9
    HRESULT (STDMETHODCALLTYPE *GetColorContexts)(IWICBitmapDecoder*, UINT, void**, UINT*);  // 10
    HRESULT (STDMETHODCALLTYPE *GetThumbnail)(IWICBitmapDecoder*, void**);  // 11
    HRESULT (STDMETHODCALLTYPE *GetFrameCount)(IWICBitmapDecoder*, UINT*);  // 12
    HRESULT (STDMETHODCALLTYPE *GetFrame)(IWICBitmapDecoder*, UINT, IWICBitmapFrameDecode**);  // 13
} IWICBitmapDecoderVtbl;

struct IWICBitmapDecoder { const IWICBitmapDecoderVtbl* lpVtbl; };

// IWICBitmapFrameDecode vtable (inherits from IWICBitmapSource)
typedef struct IWICBitmapFrameDecodeVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IWICBitmapFrameDecode*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IWICBitmapFrameDecode*);
    ULONG (STDMETHODCALLTYPE *Release)(IWICBitmapFrameDecode*);
    HRESULT (STDMETHODCALLTYPE *GetSize)(IWICBitmapFrameDecode*, UINT*, UINT*);
    HRESULT (STDMETHODCALLTYPE *GetPixelFormat)(IWICBitmapFrameDecode*, GUID*);
    HRESULT (STDMETHODCALLTYPE *GetResolution)(IWICBitmapFrameDecode*, double*, double*);
    HRESULT (STDMETHODCALLTYPE *CopyPalette)(IWICBitmapFrameDecode*, void*);
    HRESULT (STDMETHODCALLTYPE *CopyPixels)(IWICBitmapFrameDecode*, const void*, UINT, UINT, BYTE*);
    // Frame-specific methods follow...
} IWICBitmapFrameDecodeVtbl;

struct IWICBitmapFrameDecode { const IWICBitmapFrameDecodeVtbl* lpVtbl; };

// IWICFormatConverter vtable
typedef struct IWICFormatConverterVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IWICFormatConverter*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IWICFormatConverter*);
    ULONG (STDMETHODCALLTYPE *Release)(IWICFormatConverter*);
    HRESULT (STDMETHODCALLTYPE *GetSize)(IWICFormatConverter*, UINT*, UINT*);
    HRESULT (STDMETHODCALLTYPE *GetPixelFormat)(IWICFormatConverter*, GUID*);
    HRESULT (STDMETHODCALLTYPE *GetResolution)(IWICFormatConverter*, double*, double*);
    HRESULT (STDMETHODCALLTYPE *CopyPalette)(IWICFormatConverter*, void*);
    HRESULT (STDMETHODCALLTYPE *CopyPixels)(IWICFormatConverter*, const void*, UINT, UINT, BYTE*);
    HRESULT (STDMETHODCALLTYPE *Initialize)(IWICFormatConverter*, IWICBitmapSource*, const GUID*, 
              WICBitmapDitherType, void*, double, WICBitmapPaletteType);
    HRESULT (STDMETHODCALLTYPE *CanConvert)(IWICFormatConverter*, const GUID*, const GUID*, BOOL*);
} IWICFormatConverterVtbl;

struct IWICFormatConverter { const IWICFormatConverterVtbl* lpVtbl; };

// IWICStream vtable
typedef struct IWICStreamVtbl {
    // IUnknown methods
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IWICStream*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IWICStream*);
    ULONG (STDMETHODCALLTYPE *Release)(IWICStream*);
    // ISequentialStream methods
    HRESULT (STDMETHODCALLTYPE *Read)(IWICStream*, void*, ULONG, ULONG*);
    HRESULT (STDMETHODCALLTYPE *Write)(IWICStream*, const void*, ULONG, ULONG*);
    // IStream methods
    HRESULT (STDMETHODCALLTYPE *Seek)(IWICStream*, LARGE_INTEGER, DWORD, ULARGE_INTEGER*);
    HRESULT (STDMETHODCALLTYPE *SetSize)(IWICStream*, ULARGE_INTEGER);
    HRESULT (STDMETHODCALLTYPE *CopyTo)(IWICStream*, void*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*);
    HRESULT (STDMETHODCALLTYPE *Commit)(IWICStream*, DWORD);
    HRESULT (STDMETHODCALLTYPE *Revert)(IWICStream*);
    HRESULT (STDMETHODCALLTYPE *LockRegion)(IWICStream*, ULARGE_INTEGER, ULARGE_INTEGER, DWORD);
    HRESULT (STDMETHODCALLTYPE *UnlockRegion)(IWICStream*, ULARGE_INTEGER, ULARGE_INTEGER, DWORD);
    HRESULT (STDMETHODCALLTYPE *Stat)(IWICStream*, void*, DWORD);
    HRESULT (STDMETHODCALLTYPE *Clone)(IWICStream*, void**);
    // IWICStream methods
    HRESULT (STDMETHODCALLTYPE *InitializeFromIStream)(IWICStream*, void*);
    HRESULT (STDMETHODCALLTYPE *InitializeFromFilename)(IWICStream*, LPCWSTR, DWORD);
    HRESULT (STDMETHODCALLTYPE *InitializeFromMemory)(IWICStream*, BYTE*, DWORD);
    HRESULT (STDMETHODCALLTYPE *InitializeFromIStreamRegion)(IWICStream*, void*, ULARGE_INTEGER, ULARGE_INTEGER);
} IWICStreamVtbl;

struct IWICStream { const IWICStreamVtbl* lpVtbl; };

// IWICImagingFactory vtable - must match exact COM interface order
typedef struct IWICImagingFactoryVtbl {
    // IUnknown methods (0-2)
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IWICImagingFactory*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IWICImagingFactory*);
    ULONG (STDMETHODCALLTYPE *Release)(IWICImagingFactory*);
    // IWICImagingFactory methods (3-19)
    HRESULT (STDMETHODCALLTYPE *CreateDecoderFromFilename)(IWICImagingFactory*, LPCWSTR, const GUID*, 
              DWORD, WICDecodeOptions, IWICBitmapDecoder**);  // 3
    HRESULT (STDMETHODCALLTYPE *CreateDecoderFromStream)(IWICImagingFactory*, void*, const GUID*,
              WICDecodeOptions, IWICBitmapDecoder**);  // 4
    HRESULT (STDMETHODCALLTYPE *CreateDecoderFromFileHandle)(IWICImagingFactory*, ULONG_PTR, const GUID*,
              WICDecodeOptions, IWICBitmapDecoder**);  // 5
    HRESULT (STDMETHODCALLTYPE *CreateComponentInfo)(IWICImagingFactory*, REFGUID, void**);  // 6
    HRESULT (STDMETHODCALLTYPE *CreateDecoder)(IWICImagingFactory*, REFGUID, const GUID*, IWICBitmapDecoder**);  // 7
    HRESULT (STDMETHODCALLTYPE *CreateEncoder)(IWICImagingFactory*, REFGUID, const GUID*, void**);  // 8
    HRESULT (STDMETHODCALLTYPE *CreatePalette)(IWICImagingFactory*, void**);  // 9
    HRESULT (STDMETHODCALLTYPE *CreateFormatConverter)(IWICImagingFactory*, IWICFormatConverter**);  // 10
    HRESULT (STDMETHODCALLTYPE *CreateBitmapScaler)(IWICImagingFactory*, void**);  // 11
    HRESULT (STDMETHODCALLTYPE *CreateBitmapClipper)(IWICImagingFactory*, void**);  // 12
    HRESULT (STDMETHODCALLTYPE *CreateBitmapFlipRotator)(IWICImagingFactory*, void**);  // 13
    HRESULT (STDMETHODCALLTYPE *CreateStream)(IWICImagingFactory*, IWICStream**);  // 14
    HRESULT (STDMETHODCALLTYPE *CreateColorContext)(IWICImagingFactory*, void**);  // 15
    HRESULT (STDMETHODCALLTYPE *CreateColorTransformer)(IWICImagingFactory*, void**);  // 16
    HRESULT (STDMETHODCALLTYPE *CreateBitmap)(IWICImagingFactory*, UINT, UINT, REFGUID, DWORD, void**);  // 17
    HRESULT (STDMETHODCALLTYPE *CreateBitmapFromSource)(IWICImagingFactory*, void*, DWORD, void**);  // 18
    HRESULT (STDMETHODCALLTYPE *CreateBitmapFromSourceRect)(IWICImagingFactory*, void*, UINT, UINT, UINT, UINT, void**);  // 19
} IWICImagingFactoryVtbl;

struct IWICImagingFactory { const IWICImagingFactoryVtbl* lpVtbl; };

// Global WIC state
static BOOL g_bWICChecked = FALSE;
static BOOL g_bWICAvailable = FALSE;
static IWICImagingFactory* g_pWICFactory = NULL;

static BOOL WIC_Initialize(void)
{
    if (g_bWICChecked)
        return g_bWICAvailable;
    
    g_bWICChecked = TRUE;
    
    // Initialize COM if needed
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return FALSE;
    
    // Create WIC factory
    hr = CoCreateInstance(
        &CP_CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        &CP_IID_IWICImagingFactory,
        (void**)&g_pWICFactory
    );
    
    g_bWICAvailable = SUCCEEDED(hr) && g_pWICFactory != NULL;
    return g_bWICAvailable;
}

BOOL WIC_IsAvailable(void)
{
    return WIC_Initialize();
}

void WIC_Cleanup(void)
{
    if (g_pWICFactory)
    {
        g_pWICFactory->lpVtbl->Release(g_pWICFactory);
        g_pWICFactory = NULL;
    }
    g_bWICChecked = FALSE;
    g_bWICAvailable = FALSE;
}

HBITMAP WIC_LoadImageFromFile(const wchar_t* pwcFilePath, int* pWidth, int* pHeight)
{
    if (!WIC_Initialize() || !pwcFilePath)
        return NULL;
    
    HBITMAP hBitmap = NULL;
    IWICBitmapDecoder* pDecoder = NULL;
    IWICBitmapFrameDecode* pFrame = NULL;
    IWICFormatConverter* pConverter = NULL;
    
    // Create decoder from file
    HRESULT hr = g_pWICFactory->lpVtbl->CreateDecoderFromFilename(
        g_pWICFactory,
        pwcFilePath,
        NULL,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &pDecoder
    );
    
    if (FAILED(hr))
        goto cleanup;
    
    // Get first frame
    hr = pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame);
    if (FAILED(hr))
        goto cleanup;
    
    // Create format converter to 32bpp BGRA
    hr = g_pWICFactory->lpVtbl->CreateFormatConverter(g_pWICFactory, &pConverter);
    if (FAILED(hr))
        goto cleanup;
    
    hr = pConverter->lpVtbl->Initialize(
        pConverter,
        (IWICBitmapSource*)pFrame,
        &CP_GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        NULL,
        0.0,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr))
        goto cleanup;
    
    // Get dimensions
    UINT width = 0, height = 0;
    hr = pConverter->lpVtbl->GetSize(pConverter, &width, &height);
    if (FAILED(hr))
        goto cleanup;
    
    if (pWidth) *pWidth = (int)width;
    if (pHeight) *pHeight = (int)height;
    
    // Create DIB section
    {
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = (LONG)width;
        bmi.bmiHeader.biHeight = -(LONG)height;  // Top-down DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        void* pBits = NULL;
        HDC hdcScreen = GetDC(NULL);
        hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
        ReleaseDC(NULL, hdcScreen);
        
        if (hBitmap && pBits)
        {
            UINT stride = width * 4;
            hr = pConverter->lpVtbl->CopyPixels(pConverter, NULL, stride, stride * height, (BYTE*)pBits);
            if (FAILED(hr))
            {
                DeleteObject(hBitmap);
                hBitmap = NULL;
            }
        }
    }
    
cleanup:
    if (pConverter) pConverter->lpVtbl->Release(pConverter);
    if (pFrame) pFrame->lpVtbl->Release(pFrame);
    if (pDecoder) pDecoder->lpVtbl->Release(pDecoder);
    
    return hBitmap;
}

HBITMAP WIC_LoadImageFromMemory(const void* pData, size_t dataSize, int* pWidth, int* pHeight)
{
    if (!WIC_Initialize() || !pData || dataSize == 0)
        return NULL;
    
    HBITMAP hBitmap = NULL;
    IWICStream* pStream = NULL;
    IWICBitmapDecoder* pDecoder = NULL;
    IWICBitmapFrameDecode* pFrame = NULL;
    IWICFormatConverter* pConverter = NULL;
    
    // Create WIC stream
    HRESULT hr = g_pWICFactory->lpVtbl->CreateStream(g_pWICFactory, &pStream);
    if (FAILED(hr))
        goto cleanup;
    
    hr = pStream->lpVtbl->InitializeFromMemory(pStream, (BYTE*)pData, (DWORD)dataSize);
    if (FAILED(hr))
        goto cleanup;
    
    // Create decoder from stream
    hr = g_pWICFactory->lpVtbl->CreateDecoderFromStream(
        g_pWICFactory,
        pStream,
        NULL,
        WICDecodeMetadataCacheOnDemand,
        &pDecoder
    );
    if (FAILED(hr))
        goto cleanup;
    
    // Get first frame
    hr = pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame);
    if (FAILED(hr))
        goto cleanup;
    
    // Create format converter
    hr = g_pWICFactory->lpVtbl->CreateFormatConverter(g_pWICFactory, &pConverter);
    if (FAILED(hr))
        goto cleanup;
    
    hr = pConverter->lpVtbl->Initialize(
        pConverter,
        (IWICBitmapSource*)pFrame,
        &CP_GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        NULL,
        0.0,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr))
        goto cleanup;
    
    // Get dimensions and create bitmap
    UINT width = 0, height = 0;
    hr = pConverter->lpVtbl->GetSize(pConverter, &width, &height);
    if (FAILED(hr))
        goto cleanup;
    
    if (pWidth) *pWidth = (int)width;
    if (pHeight) *pHeight = (int)height;
    
    {
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = (LONG)width;
        bmi.bmiHeader.biHeight = -(LONG)height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        void* pBits = NULL;
        HDC hdcScreen = GetDC(NULL);
        hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
        ReleaseDC(NULL, hdcScreen);
        
        if (hBitmap && pBits)
        {
            UINT stride = width * 4;
            hr = pConverter->lpVtbl->CopyPixels(pConverter, NULL, stride, stride * height, (BYTE*)pBits);
            if (FAILED(hr))
            {
                DeleteObject(hBitmap);
                hBitmap = NULL;
            }
        }
    }
    
cleanup:
    if (pConverter) pConverter->lpVtbl->Release(pConverter);
    if (pFrame) pFrame->lpVtbl->Release(pFrame);
    if (pDecoder) pDecoder->lpVtbl->Release(pDecoder);
    if (pStream) pStream->lpVtbl->Release(pStream);
    
    return hBitmap;
}

HBITMAP WIC_LoadImageFromResource(UINT uiResourceID, int* pWidth, int* pHeight)
{
    HRSRC hRes = FindResource(GetModuleHandle(NULL), MAKEINTRESOURCE(uiResourceID), RT_RCDATA);
    if (!hRes) return NULL;

    HGLOBAL hMem = LoadResource(GetModuleHandle(NULL), hRes);
    if (!hMem) return NULL;

    DWORD dwSize = SizeofResource(GetModuleHandle(NULL), hRes);
    void* pData = LockResource(hMem);
    if (!pData || dwSize == 0) return NULL;

    return WIC_LoadImageFromMemory(pData, (size_t)dwSize, pWidth, pHeight);
}

////////////////////////////////////////////////////////////////////////////////
//
// Background Worker Implementation
//
////////////////////////////////////////////////////////////////////////////////

typedef struct _CP_BackgroundWorker {
    HANDLE hThread;
    CP_WorkCallback callback;
    void* pContext;
    volatile LONG bRunning;
} CP_BackgroundWorker;

static unsigned __stdcall BackgroundWorker_ThreadProc(void* pParam)
{
    CP_BackgroundWorker* pWorker = (CP_BackgroundWorker*)pParam;
    if (pWorker && pWorker->callback)
    {
        pWorker->callback(pWorker->pContext);
    }
    InterlockedExchange(&pWorker->bRunning, FALSE);
    return 0;
}

CP_HWORKER BackgroundWorker_Start(CP_WorkCallback callback, void* pContext)
{
    if (!callback)
        return NULL;
    
    CP_BackgroundWorker* pWorker = (CP_BackgroundWorker*)malloc(sizeof(CP_BackgroundWorker));
    if (!pWorker)
        return NULL;
    
    pWorker->callback = callback;
    pWorker->pContext = pContext;
    pWorker->bRunning = TRUE;
    
    pWorker->hThread = (HANDLE)_beginthreadex(
        NULL, 0, BackgroundWorker_ThreadProc, pWorker, 0, NULL
    );
    
    if (!pWorker->hThread)
    {
        free(pWorker);
        return NULL;
    }
    
    return pWorker;
}

void BackgroundWorker_Wait(CP_HWORKER hWorker)
{
    CP_BackgroundWorker* pWorker = (CP_BackgroundWorker*)hWorker;
    if (pWorker && pWorker->hThread)
    {
        WaitForSingleObject(pWorker->hThread, INFINITE);
    }
}

BOOL BackgroundWorker_IsRunning(CP_HWORKER hWorker)
{
    CP_BackgroundWorker* pWorker = (CP_BackgroundWorker*)hWorker;
    if (!pWorker)
        return FALSE;
    return InterlockedCompareExchange(&pWorker->bRunning, 0, 0) != 0;
}

void BackgroundWorker_Close(CP_HWORKER hWorker)
{
    CP_BackgroundWorker* pWorker = (CP_BackgroundWorker*)hWorker;
    if (pWorker)
    {
        if (pWorker->hThread)
        {
            CloseHandle(pWorker->hThread);
        }
        free(pWorker);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Shell Balloon Notifications Implementation
//
////////////////////////////////////////////////////////////////////////////////

BOOL ShellBalloon_Show(HWND hWnd, UINT uID,
                       const wchar_t* pwcTitle,
                       const wchar_t* pwcMessage,
                       DWORD dwFlags, DWORD dwTimeout)
{
    NOTIFYICONDATAW nid;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hWnd;
    nid.uID = uID;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = dwFlags;
    nid.uTimeout = dwTimeout;
    
    if (pwcTitle)
    {
        wcsncpy(nid.szInfoTitle, pwcTitle, 
                sizeof(nid.szInfoTitle) / sizeof(wchar_t) - 1);
    }
    
    if (pwcMessage)
    {
        wcsncpy(nid.szInfo, pwcMessage,
                sizeof(nid.szInfo) / sizeof(wchar_t) - 1);
    }
    
    return Shell_NotifyIconW(NIM_MODIFY, &nid);
}

BOOL ShellBalloon_Hide(HWND hWnd, UINT uID)
{
    NOTIFYICONDATAW nid;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hWnd;
    nid.uID = uID;
    nid.uFlags = NIF_INFO;
    // Empty strings hide the balloon
    nid.szInfoTitle[0] = L'\0';
    nid.szInfo[0] = L'\0';
    
    return Shell_NotifyIconW(NIM_MODIFY, &nid);
}

////////////////////////////////////////////////////////////////////////////////
//
// Enhanced File Dialogs Implementation
//
////////////////////////////////////////////////////////////////////////////////

wchar_t* FileDialog_BuildFilterString(const CP_FileDialogFilter* pFilters, int filterCount)
{
    if (!pFilters || filterCount <= 0)
        return NULL;
    
    // Calculate required size
    size_t totalLen = 1;  // Final null terminator
    for (int i = 0; i < filterCount; i++)
    {
        if (pFilters[i].pwcDescription)
            totalLen += wcslen(pFilters[i].pwcDescription) + 1;
        else
            totalLen += 1;
        
        if (pFilters[i].pwcPattern)
            totalLen += wcslen(pFilters[i].pwcPattern) + 1;
        else
            totalLen += 1;
    }
    
    wchar_t* pResult = (wchar_t*)malloc(totalLen * sizeof(wchar_t));
    if (!pResult)
        return NULL;
    
    wchar_t* pCurrent = pResult;
    for (int i = 0; i < filterCount; i++)
    {
        // Copy description
        if (pFilters[i].pwcDescription)
        {
            size_t len = wcslen(pFilters[i].pwcDescription);
            wcscpy(pCurrent, pFilters[i].pwcDescription);
            pCurrent += len + 1;
        }
        else
        {
            *pCurrent++ = L'\0';
        }
        
        // Copy pattern
        if (pFilters[i].pwcPattern)
        {
            size_t len = wcslen(pFilters[i].pwcPattern);
            wcscpy(pCurrent, pFilters[i].pwcPattern);
            pCurrent += len + 1;
        }
        else
        {
            *pCurrent++ = L'\0';
        }
    }
    *pCurrent = L'\0';  // Double null terminator
    
    return pResult;
}

wchar_t* FileDialog_OpenFile(HWND hWndOwner,
                             const wchar_t* pwcTitle,
                             const wchar_t* pwcInitialDir,
                             const CP_FileDialogFilter* pFilters,
                             int filterCount,
                             CP_FileDialogOptions options)
{
    // Allocate buffer for file path(s)
    // For multiselect, buffer may contain directory + multiple null-separated filenames
    size_t bufferSize = 65536;  // 64KB should be plenty
    wchar_t* pBuffer = (wchar_t*)malloc(bufferSize * sizeof(wchar_t));
    if (!pBuffer)
        return NULL;
    
    ZeroMemory(pBuffer, bufferSize * sizeof(wchar_t));
    
    // Build filter string
    wchar_t* pFilter = FileDialog_BuildFilterString(pFilters, filterCount);
    
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hWndOwner;
    ofn.lpstrFile = pBuffer;
    ofn.nMaxFile = (DWORD)bufferSize;
    ofn.lpstrFilter = pFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = pwcTitle;
    ofn.lpstrInitialDir = pwcInitialDir;
    
    // Set flags
    ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;
    if (options & CP_FD_MULTISELECT)
        ofn.Flags |= OFN_ALLOWMULTISELECT;
    if (options & CP_FD_MUST_EXIST)
        ofn.Flags |= OFN_FILEMUSTEXIST;
    if (options & CP_FD_PATH_MUST_EXIST)
        ofn.Flags |= OFN_PATHMUSTEXIST;
    if (options & CP_FD_NO_READONLY)
        ofn.Flags |= OFN_NOREADONLYRETURN;
    
    BOOL bResult = GetOpenFileNameW(&ofn);
    
    free(pFilter);
    
    if (!bResult)
    {
        free(pBuffer);
        return NULL;
    }
    
    // For multiselect, convert null-separated list to pipe-separated
    if (options & CP_FD_MULTISELECT)
    {
        // Check if it's actually multiple files (directory in first part)
        wchar_t* pSecond = pBuffer + wcslen(pBuffer) + 1;
        if (*pSecond != L'\0')
        {
            // Multiple files selected - format: dir\0file1\0file2\0\0
            // Convert to: dir\file1|dir\file2
            size_t dirLen = wcslen(pBuffer);
            wchar_t* pResult = (wchar_t*)malloc(bufferSize * sizeof(wchar_t));
            if (!pResult)
            {
                free(pBuffer);
                return NULL;
            }
            
            wchar_t* pOut = pResult;
            wchar_t* pFile = pSecond;
            BOOL bFirst = TRUE;
            
            while (*pFile)
            {
                if (!bFirst)
                    *pOut++ = L'|';
                bFirst = FALSE;
                
                // Copy directory
                wcscpy(pOut, pBuffer);
                pOut += dirLen;
                
                // Add backslash if needed
                if (pBuffer[dirLen - 1] != L'\\')
                    *pOut++ = L'\\';
                
                // Copy filename
                size_t fileLen = wcslen(pFile);
                wcscpy(pOut, pFile);
                pOut += fileLen;
                
                pFile += fileLen + 1;
            }
            *pOut = L'\0';
            
            free(pBuffer);
            return pResult;
        }
    }
    
    // Single file - reallocate to exact size
    size_t len = wcslen(pBuffer) + 1;
    wchar_t* pResult = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (pResult)
        wcscpy(pResult, pBuffer);
    
    free(pBuffer);
    return pResult;
}

wchar_t* FileDialog_SaveFile(HWND hWndOwner,
                             const wchar_t* pwcTitle,
                             const wchar_t* pwcDefaultName,
                             const wchar_t* pwcInitialDir,
                             const CP_FileDialogFilter* pFilters,
                             int filterCount,
                             const wchar_t* pwcDefaultExt,
                             CP_FileDialogOptions options)
{
    wchar_t* pBuffer = (wchar_t*)malloc(MAX_PATH * sizeof(wchar_t));
    if (!pBuffer)
        return NULL;
    
    ZeroMemory(pBuffer, MAX_PATH * sizeof(wchar_t));
    
    // Copy default name if provided
    if (pwcDefaultName)
        wcsncpy(pBuffer, pwcDefaultName, MAX_PATH - 1);
    
    // Build filter string
    wchar_t* pFilter = FileDialog_BuildFilterString(pFilters, filterCount);
    
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hWndOwner;
    ofn.lpstrFile = pBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = pFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = pwcTitle;
    ofn.lpstrInitialDir = pwcInitialDir;
    ofn.lpstrDefExt = pwcDefaultExt;
    
    // Set flags
    ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;
    if (options & CP_FD_OVERWRITE_PROMPT)
        ofn.Flags |= OFN_OVERWRITEPROMPT;
    if (options & CP_FD_PATH_MUST_EXIST)
        ofn.Flags |= OFN_PATHMUSTEXIST;
    
    BOOL bResult = GetSaveFileNameW(&ofn);
    
    free(pFilter);
    
    if (!bResult)
    {
        free(pBuffer);
        return NULL;
    }
    
    return pBuffer;
}

// Callback for SHBrowseForFolder - set initial directory
static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    (void)lParam;
    if (uMsg == BFFM_INITIALIZED && lpData)
    {
        SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, lpData);
    }
    return 0;
}

wchar_t* FileDialog_BrowseFolder(HWND hWndOwner, 
                                 const wchar_t* pwcTitle,
                                 const wchar_t* pwcInitialDir)
{
    wchar_t* pBuffer = (wchar_t*)malloc(MAX_PATH * sizeof(wchar_t));
    if (!pBuffer)
        return NULL;
    
    BROWSEINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = hWndOwner;
    bi.pszDisplayName = pBuffer;
    bi.lpszTitle = pwcTitle;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI | BIF_NONEWFOLDERBUTTON;
    bi.lpfn = BrowseCallbackProc;
    bi.lParam = (LPARAM)pwcInitialDir;
    
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl)
    {
        free(pBuffer);
        return NULL;
    }
    
    if (!SHGetPathFromIDListW(pidl, pBuffer))
    {
        CoTaskMemFree(pidl);
        free(pBuffer);
        return NULL;
    }
    
    CoTaskMemFree(pidl);
    return pBuffer;
}

wchar_t** FileDialog_ParseMultiSelect(const wchar_t* pwcResult)
{
    if (!pwcResult || !*pwcResult)
        return NULL;
    
    // Count pipes to determine number of files
    int count = 1;
    const wchar_t* p = pwcResult;
    while (*p)
    {
        if (*p == L'|')
            count++;
        p++;
    }
    
    // Allocate array
    wchar_t** ppResult = (wchar_t**)malloc((count + 1) * sizeof(wchar_t*));
    if (!ppResult)
        return NULL;
    
    ZeroMemory(ppResult, (count + 1) * sizeof(wchar_t*));
    
    // Parse paths
    const wchar_t* pStart = pwcResult;
    int idx = 0;
    
    p = pwcResult;
    while (idx < count)
    {
        if (*p == L'|' || *p == L'\0')
        {
            size_t len = p - pStart;
            ppResult[idx] = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
            if (ppResult[idx])
            {
                wcsncpy(ppResult[idx], pStart, len);
                ppResult[idx][len] = L'\0';
            }
            idx++;
            pStart = p + 1;
        }
        if (*p == L'\0')
            break;
        p++;
    }
    
    return ppResult;
}

void FileDialog_FreeMultiSelect(wchar_t** ppPaths)
{
    if (!ppPaths)
        return;
    
    for (int i = 0; ppPaths[i] != NULL; i++)
    {
        free(ppPaths[i]);
    }
    free(ppPaths);
}
