/*
 * CoolPlayer - Blazing fast audio player.
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
// Compiled as C++ because the album art functions use WIC COM interfaces
// which require C++ method-call syntax (->Method()).  All metadata I/O has
// moved to rust/codecs/src/tags.rs (lofty).
//

// Disable NLS for this file to avoid macro conflicts
#ifdef ENABLE_NLS
#undef ENABLE_NLS
#endif

#include "stdafx.h"

extern "C" {
#include "CPI_TagLib.h"
#include "CPI_PlaylistItem.h"
#include "CPString.h"
}

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

// For image decoding (WIC - Windows Imaging Component)
#include <wincodec.h>
#include <objbase.h>

extern "C" {

////////////////////////////////////////////////////////////////////////////////
//
// Genre definitions - compatible with ID3v1 genres
//
////////////////////////////////////////////////////////////////////////////////

const char* glb_pcGenres[CIC_NUMGENRES] =
{
    "Blues",
    "Classic Rock",
    "Country",
    "Dance",
    "Disco",
    "Funk",
    "Grunge",
    "Hip-Hop",
    "Jazz",
    "Metal",
    "New Age",
    "Oldies",
    "Other",
    "Pop",
    "R&B",
    "Rap",
    "Reggae",
    "Rock",
    "Techno",
    "Industrial",
    "Alternative",
    "Ska",
    "Death Metal",
    "Pranks",
    "Soundtrack",
    "Euro-Techno",
    "Ambient",
    "Trip-Hop",
    "Vocal",
    "Jazz+Funk",
    "Fusion",
    "Trance",
    "Classical",
    "Instrumental",
    "Acid",
    "House",
    "Game",
    "Sound Clip",
    "Gospel",
    "Noise",
    "AlternRock",
    "Bass",
    "Soul",
    "Punk",
    "Space",
    "Meditative",
    "Instrumental Pop",
    "Instrumental Rock",
    "Ethnic",
    "Gothic",
    "Darkwave",
    "Techno-Industrial",
    "Electronic",
    "Pop-Folk",
    "Eurodance",
    "Dream",
    "Southern Rock",
    "Comedy",
    "Cult",
    "Gangsta",
    "Top 40",
    "Christian Rap",
    "Pop/Funk",
    "Jungle",
    "Native American",
    "Cabaret",
    "New Wave",
    "Psychadelic",
    "Rave",
    "Showtunes",
    "Trailer",
    "Lo-Fi",
    "Tribal",
    "Acid Punk",
    "Acid Jazz",
    "Polka",
    "Retro",
    "Musical",
    "Rock & Roll",
    "Hard Rock",
    "Folk",
    "Folk/Rock",
    "National Folk",
    "Swing",
    "Fusion",
    "Bebob",
    "Latin",
    "Revival",
    "Celtic",
    "Bluegrass",
    "Avantgarde",
    "Gothic Rock",
    "Progressive Rock",
    "Psychedelic Rock",
    "Symphonic Rock",
    "Slow Rock",
    "Big Band",
    "Chorus",
    "Easy Listening",
    "Accoustic",
    "Humour",
    "Speech",
    "Chanson",
    "Opera",
    "Chamber Music",
    "Sonata",
    "Symphony",
    "Booty Bass",
    "Primus",
    "Porn Groove",
    "Satire",
    "Slow Jam",
    "Club",
    "Tango",
    "Samba",
    "Folklore",
    "Ballad",
    "Power Ballad",
    "Rhytmic Soul",
    "Freestyle",
    "Duet",
    "Punk Rock",
    "Drum Solo",
    "Acapella",
    "Euro-House",
    "Dance Hall",
    "Goa",
    "Drum & Bass",
    "Club-House",
    "Hardcore",
    "Terror",
    "Indie",
    "BritPop",
    "Negerpunk",
    "Polsk Punk",
    "Beat",
    "Christian Gangsta Rap",
    "Heavy Metal",
    "Black Metal",
    "Crossover",
    "Contemporary Christian",
    "Christian Rock",
    "Merengue",
    "Salsa",
    "Trash Metal",
    "Anime",
    "JPop",
    "Synthpop",
    "Unknown"
};

////////////////////////////////////////////////////////////////////////////////
//
// Initialize / Cleanup
//
////////////////////////////////////////////////////////////////////////////////

void CPTL_Initialize(void)
{
    CPTL_InitAlbumArtCache();
}

void CPTL_Cleanup(void)
{
    CPTL_CleanupAlbumArtCache();
}

////////////////////////////////////////////////////////////////////////////////
//
// CPTL_CanWriteToFile — Win32 write-access check
//
////////////////////////////////////////////////////////////////////////////////

BOOL CPTL_CanWriteToFile(const char* pcFilePath)
{
    HANDLE hFile;

    if (!pcFilePath)
        return FALSE;

    WCHAR* pwcFilePath = STR_ConvertToUnicode(pcFilePath);
    if (!pwcFilePath)
        return FALSE;

    hFile = CreateFileW(pwcFilePath, GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                        OPEN_EXISTING, 0, 0);
    free(pwcFilePath);

    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    CloseHandle(hFile);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Genre lookup hash table for O(1) performance
////////////////////////////////////////////////////////////////////////////////

#define GENRE_HASH_BUCKETS 256

typedef struct _CPs_GenreHashEntry
{
    const char* m_pcGenre;
    int m_iIndex;
    struct _CPs_GenreHashEntry* m_pNext;
} CPs_GenreHashEntry;

static CPs_GenreHashEntry* g_pGenreHashTable[GENRE_HASH_BUCKETS] = {0};
static CPs_GenreHashEntry g_GenreEntries[CIC_NUMGENRES];
static BOOL g_bGenreHashInitialized = FALSE;

static unsigned int CPTL_HashGenre(const char* pcGenre)
{
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*pcGenre++) != 0)
    {
        if (c >= 'A' && c <= 'Z') c += 32;
        hash = ((hash << 5) + hash) + c;
    }
    return hash % GENRE_HASH_BUCKETS;
}

static void CPTL_InitGenreHash(void)
{
    int i;
    unsigned int hash;

    if (g_bGenreHashInitialized)
        return;

    memset(g_pGenreHashTable, 0, sizeof(g_pGenreHashTable));

    for (i = 0; i < CIC_NUMGENRES; i++)
    {
        g_GenreEntries[i].m_pcGenre = glb_pcGenres[i];
        g_GenreEntries[i].m_iIndex  = i;

        hash = CPTL_HashGenre(glb_pcGenres[i]);
        g_GenreEntries[i].m_pNext = g_pGenreHashTable[hash];
        g_pGenreHashTable[hash]   = &g_GenreEntries[i];
    }

    g_bGenreHashInitialized = TRUE;
}

int CPTL_GetGenreIndex(const char* pcGenre)
{
    unsigned int hash;
    CPs_GenreHashEntry* pEntry;

    if (!pcGenre)
        return -1;

    if (!g_bGenreHashInitialized)
        CPTL_InitGenreHash();

    hash   = CPTL_HashGenre(pcGenre);
    pEntry = g_pGenreHashTable[hash];

    while (pEntry)
    {
        if (stricmp(pcGenre, pEntry->m_pcGenre) == 0)
            return pEntry->m_iIndex;
        pEntry = pEntry->m_pNext;
    }
    return -1;
}

const char* CPTL_GetGenreString(int iGenreIndex)
{
    if (iGenreIndex < 0 || iGenreIndex >= CIC_NUMGENRES)
        return NULL;
    return glb_pcGenres[iGenreIndex];
}

////////////////////////////////////////////////////////////////////////////////
//
// ID3v2 helpers (used by codec parsers, not by lofty)
//
////////////////////////////////////////////////////////////////////////////////

unsigned int CPTL_SkipID3v2Tag(const void* pBuffer, unsigned int iBufferSize)
{
    const unsigned char* pBytes = (const unsigned char*)pBuffer;
    unsigned int iOffset = 0;

    if (!pBuffer || iBufferSize < 10)
        return 0;

    if (memcmp(pBytes, "ID3", 3) == 0)
    {
        iOffset = 10;
        iOffset += (pBytes[6] << 21) | (pBytes[7] << 14) | (pBytes[8] << 7) | pBytes[9];
    }
    return iOffset;
}

char* DecodeID3String(const char* pcSource, const int iLength)
{
    char* pcDest;
    int i, iDestPos = 0;

    if (!pcSource || iLength <= 0)
        return NULL;

    pcDest = CALLOC_TYPE(char, iLength + 1);
    if (!pcDest)
        return NULL;

    for (i = 0; i < iLength && pcSource[i] != '\0'; i++)
    {
        if (pcSource[i] != ' ' || iDestPos > 0)
            pcDest[iDestPos++] = pcSource[i];
    }

    while (iDestPos > 0 && pcDest[iDestPos - 1] == ' ')
        iDestPos--;

    pcDest[iDestPos] = '\0';

    if (iDestPos == 0)
    {
        free(pcDest);
        return NULL;
    }
    return pcDest;
}

////////////////////////////////////////////////////////////////////////////////
//
// Album Art Cache
//
////////////////////////////////////////////////////////////////////////////////

typedef struct _CPs_AlbumArtCacheEntry
{
    char* m_pcFilePath;
    HBITMAP m_hBitmap;
    unsigned int m_iWidth;
    unsigned int m_iHeight;
    time_t m_tLastAccess;
    unsigned int m_iMemoryUsed;
    struct _CPs_AlbumArtCacheEntry* m_pNext;
    struct _CPs_AlbumArtCacheEntry* m_pLRUNext;
    struct _CPs_AlbumArtCacheEntry* m_pLRUPrev;
} CPs_AlbumArtCacheEntry;

#define ALBUMART_HASH_BUCKETS 64

static CPs_AlbumArtCacheEntry* g_pAlbumArtHashTable[ALBUMART_HASH_BUCKETS] = {0};
static CPs_AlbumArtCacheEntry* g_pLRUHead = NULL;
static CPs_AlbumArtCacheEntry* g_pLRUTail = NULL;
static int g_iCacheEntryCount = 0;
static unsigned int g_iTotalCacheMemory = 0;

static unsigned int CPTL_HashFilePath(const char* pcFilePath)
{
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*pcFilePath++) != 0)
    {
        if (c >= 'A' && c <= 'Z') c += 32;
        hash = ((hash << 5) + hash) + c;
    }
    return hash % ALBUMART_HASH_BUCKETS;
}

static void CPTL_MoveToLRUFront(CPs_AlbumArtCacheEntry* pEntry)
{
    if (!pEntry || pEntry == g_pLRUHead)
        return;

    if (pEntry->m_pLRUPrev)
        pEntry->m_pLRUPrev->m_pLRUNext = pEntry->m_pLRUNext;
    if (pEntry->m_pLRUNext)
        pEntry->m_pLRUNext->m_pLRUPrev = pEntry->m_pLRUPrev;

    if (pEntry == g_pLRUTail)
        g_pLRUTail = pEntry->m_pLRUPrev;

    pEntry->m_pLRUPrev = NULL;
    pEntry->m_pLRUNext = g_pLRUHead;
    if (g_pLRUHead)
        g_pLRUHead->m_pLRUPrev = pEntry;
    g_pLRUHead = pEntry;

    if (!g_pLRUTail)
        g_pLRUTail = pEntry;
}

static void CPTL_AddToLRU(CPs_AlbumArtCacheEntry* pEntry)
{
    pEntry->m_pLRUPrev = NULL;
    pEntry->m_pLRUNext = g_pLRUHead;
    if (g_pLRUHead)
        g_pLRUHead->m_pLRUPrev = pEntry;
    g_pLRUHead = pEntry;
    if (!g_pLRUTail)
        g_pLRUTail = pEntry;
}

static void CPTL_RemoveFromLRU(CPs_AlbumArtCacheEntry* pEntry)
{
    if (pEntry->m_pLRUPrev)
        pEntry->m_pLRUPrev->m_pLRUNext = pEntry->m_pLRUNext;
    else
        g_pLRUHead = pEntry->m_pLRUNext;

    if (pEntry->m_pLRUNext)
        pEntry->m_pLRUNext->m_pLRUPrev = pEntry->m_pLRUPrev;
    else
        g_pLRUTail = pEntry->m_pLRUPrev;
}

void CPTL_InitAlbumArtCache(void)
{
    memset(g_pAlbumArtHashTable, 0, sizeof(g_pAlbumArtHashTable));
    g_pLRUHead = NULL;
    g_pLRUTail = NULL;
    g_iCacheEntryCount = 0;
    g_iTotalCacheMemory = 0;
}

void CPTL_CleanupAlbumArtCache(void)
{
    CPTL_ClearAlbumArtCache();
}

void CPTL_ClearAlbumArtCache(void)
{
    CPs_AlbumArtCacheEntry* pCurrent = g_pLRUHead;
    CPs_AlbumArtCacheEntry* pNext;

    while (pCurrent)
    {
        pNext = pCurrent->m_pLRUNext;
        if (pCurrent->m_pcFilePath)
            free(pCurrent->m_pcFilePath);
        if (pCurrent->m_hBitmap)
            DeleteObject(pCurrent->m_hBitmap);
        free(pCurrent);
        pCurrent = pNext;
    }

    memset(g_pAlbumArtHashTable, 0, sizeof(g_pAlbumArtHashTable));
    g_pLRUHead = NULL;
    g_pLRUTail = NULL;
    g_iCacheEntryCount = 0;
    g_iTotalCacheMemory = 0;
}

static CPs_AlbumArtCacheEntry* CPTL_FindInCache(const char* pcFilePath, unsigned int* pHash)
{
    unsigned int hash = CPTL_HashFilePath(pcFilePath);
    if (pHash) *pHash = hash;

    CPs_AlbumArtCacheEntry* pEntry = g_pAlbumArtHashTable[hash];
    while (pEntry)
    {
        if (pEntry->m_pcFilePath && stricmp(pEntry->m_pcFilePath, pcFilePath) == 0)
            return pEntry;
        pEntry = pEntry->m_pNext;
    }
    return NULL;
}

void CPTL_RemoveFromAlbumArtCache(const char* pcFilePath)
{
    if (!pcFilePath)
        return;

    unsigned int hash = CPTL_HashFilePath(pcFilePath);
    CPs_AlbumArtCacheEntry* pCurrent = g_pAlbumArtHashTable[hash];
    CPs_AlbumArtCacheEntry* pPrev    = NULL;

    while (pCurrent)
    {
        if (pCurrent->m_pcFilePath && stricmp(pCurrent->m_pcFilePath, pcFilePath) == 0)
        {
            if (pPrev)
                pPrev->m_pNext = pCurrent->m_pNext;
            else
                g_pAlbumArtHashTable[hash] = pCurrent->m_pNext;

            CPTL_RemoveFromLRU(pCurrent);
            g_iTotalCacheMemory -= pCurrent->m_iMemoryUsed;
            g_iCacheEntryCount--;

            if (pCurrent->m_pcFilePath)
                free(pCurrent->m_pcFilePath);
            if (pCurrent->m_hBitmap)
                DeleteObject(pCurrent->m_hBitmap);
            free(pCurrent);
            return;
        }
        pPrev    = pCurrent;
        pCurrent = pCurrent->m_pNext;
    }
}

static void CPTL_EvictLRU(void)
{
    CPs_AlbumArtCacheEntry* pOldest = g_pLRUTail;
    if (!pOldest)
        return;

    unsigned int hash = CPTL_HashFilePath(pOldest->m_pcFilePath);
    CPs_AlbumArtCacheEntry* pCurrent = g_pAlbumArtHashTable[hash];
    CPs_AlbumArtCacheEntry* pPrev    = NULL;

    while (pCurrent)
    {
        if (pCurrent == pOldest)
        {
            if (pPrev)
                pPrev->m_pNext = pCurrent->m_pNext;
            else
                g_pAlbumArtHashTable[hash] = pCurrent->m_pNext;
            break;
        }
        pPrev    = pCurrent;
        pCurrent = pCurrent->m_pNext;
    }

    CPTL_RemoveFromLRU(pOldest);
    g_iTotalCacheMemory -= pOldest->m_iMemoryUsed;
    g_iCacheEntryCount--;

    if (pOldest->m_pcFilePath)
        free(pOldest->m_pcFilePath);
    if (pOldest->m_hBitmap)
        DeleteObject(pOldest->m_hBitmap);
    free(pOldest);
}

////////////////////////////////////////////////////////////////////////////////
//
// CPTL_CreateBitmapFromImageData — WIC-based JPEG/PNG → HBITMAP
//
////////////////////////////////////////////////////////////////////////////////

HBITMAP CPTL_CreateBitmapFromImageData(const BYTE* pImageData,
                                       unsigned int iImageSize,
                                       unsigned int iMaxWidth,
                                       unsigned int iMaxHeight,
                                       unsigned int* piActualWidth,
                                       unsigned int* piActualHeight)
{
    if (!pImageData || iImageSize == 0)
        return NULL;

    HBITMAP hBitmap = NULL;
    IWICImagingFactory*  pFactory   = NULL;
    IWICStream*          pStream    = NULL;
    IWICBitmapDecoder*   pDecoder   = NULL;
    IWICBitmapFrameDecode* pFrame   = NULL;
    IWICFormatConverter* pConverter = NULL;
    UINT iWidth = 0, iHeight = 0;
    UINT iScaledWidth = 0, iScaledHeight = 0;
    void* pBits = NULL;
    HDC  hScreenDC = NULL;

    CoInitialize(NULL);

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL,
                                  CLSCTX_INPROC_SERVER,
                                  IID_IWICImagingFactory, (void**)&pFactory);
    if (FAILED(hr) || !pFactory) goto cleanup;

    hr = pFactory->CreateStream(&pStream);
    if (FAILED(hr) || !pStream) goto cleanup;

    hr = pStream->InitializeFromMemory((BYTE*)pImageData, iImageSize);
    if (FAILED(hr)) goto cleanup;

    hr = pFactory->CreateDecoderFromStream((IStream*)pStream, NULL,
                                            WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr) || !pDecoder) goto cleanup;

    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr) || !pFrame) goto cleanup;

    pFrame->GetSize(&iWidth, &iHeight);

    iScaledWidth  = iWidth;
    iScaledHeight = iHeight;

    if (iMaxWidth > 0 && iMaxHeight > 0 && iWidth > 0 && iHeight > 0)
    {
        float fScaleW = (float)iMaxWidth  / iWidth;
        float fScaleH = (float)iMaxHeight / iHeight;
        float fScale  = (fScaleW < fScaleH) ? fScaleW : fScaleH;
        iScaledWidth  = (UINT)(iWidth  * fScale);
        iScaledHeight = (UINT)(iHeight * fScale);
        if (iScaledWidth  < 1) iScaledWidth  = 1;
        if (iScaledHeight < 1) iScaledHeight = 1;
    }

    if (piActualWidth)  *piActualWidth  = iScaledWidth;
    if (piActualHeight) *piActualHeight = iScaledHeight;

    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr) || !pConverter) goto cleanup;

    hr = pConverter->Initialize((IWICBitmapSource*)pFrame,
                                 GUID_WICPixelFormat32bppBGRA,
                                 WICBitmapDitherTypeNone, NULL, 0.0,
                                 WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) goto cleanup;

    {
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(BITMAPINFO));
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = iScaledWidth;
        bmi.bmiHeader.biHeight      = -(LONG)iScaledHeight;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        hScreenDC = GetDC(NULL);
        hBitmap   = CreateDIBSection(hScreenDC, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
        ReleaseDC(NULL, hScreenDC);
        hScreenDC = NULL;

        if (hBitmap && pBits)
        {
            if (iScaledWidth != iWidth || iScaledHeight != iHeight)
            {
                IWICBitmapScaler* pScaler = NULL;
                hr = pFactory->CreateBitmapScaler(&pScaler);
                if (SUCCEEDED(hr) && pScaler)
                {
                    hr = pScaler->Initialize((IWICBitmapSource*)pConverter,
                                              iScaledWidth, iScaledHeight,
                                              WICBitmapInterpolationModeFant);
                    if (SUCCEEDED(hr))
                        pScaler->CopyPixels(NULL, iScaledWidth * 4,
                                            iScaledWidth * iScaledHeight * 4,
                                            (BYTE*)pBits);
                    pScaler->Release();
                }
            }
            else
            {
                pConverter->CopyPixels(NULL, iScaledWidth * 4,
                                       iScaledWidth * iScaledHeight * 4,
                                       (BYTE*)pBits);
            }
        }
    }

cleanup:
    if (pConverter) pConverter->Release();
    if (pFrame)     pFrame->Release();
    if (pDecoder)   pDecoder->Release();
    if (pStream)    pStream->Release();
    if (pFactory)   pFactory->Release();
    return hBitmap;
}

////////////////////////////////////////////////////////////////////////////////
//
// CPTL_LoadAlbumArtBitmap / CPTL_GetAlbumArtBitmap
//
////////////////////////////////////////////////////////////////////////////////

HBITMAP CPTL_LoadAlbumArtBitmap(const char* pcFilePath,
                                unsigned int iTargetWidth,
                                unsigned int iTargetHeight,
                                unsigned int* piActualWidth,
                                unsigned int* piActualHeight)
{
    CPs_AlbumArt art;
    HBITMAP hBitmap;

    if (!pcFilePath || iTargetWidth == 0 || iTargetHeight == 0)
        return NULL;

    if (!CPTL_ReadAlbumArt(pcFilePath, &art))
        return NULL;

    hBitmap = CPTL_CreateBitmapFromImageData(art.m_pImageData, art.m_iImageSize,
                                              iTargetWidth, iTargetHeight,
                                              piActualWidth, piActualHeight);
    CPTL_FreeAlbumArt(&art);
    return hBitmap;
}

HBITMAP CPTL_GetAlbumArtBitmap(const char* pcFilePath,
                                unsigned int iMaxWidth,
                                unsigned int iMaxHeight,
                                unsigned int* piActualWidth,
                                unsigned int* piActualHeight)
{
    CPs_AlbumArtCacheEntry* pEntry;
    CPs_AlbumArt art;
    HBITMAP hBitmap;
    unsigned int iWidth, iHeight;
    unsigned int hash;

    if (!pcFilePath)
        return NULL;

    pEntry = CPTL_FindInCache(pcFilePath, &hash);
    if (pEntry)
    {
        CPTL_MoveToLRUFront(pEntry);
        pEntry->m_tLastAccess = time(NULL);
        if (piActualWidth)  *piActualWidth  = pEntry->m_iWidth;
        if (piActualHeight) *piActualHeight = pEntry->m_iHeight;
        return pEntry->m_hBitmap;
    }

    if (!CPTL_ReadAlbumArt(pcFilePath, &art))
        return NULL;

    hBitmap = CPTL_CreateBitmapFromImageData(art.m_pImageData, art.m_iImageSize,
                                              iMaxWidth, iMaxHeight,
                                              &iWidth, &iHeight);
    CPTL_FreeAlbumArt(&art);

    if (!hBitmap)
        return NULL;

    unsigned int iMemoryUsed = iWidth * iHeight * 4;

    while (g_iCacheEntryCount >= CPC_ALBUMART_CACHE_SIZE ||
           g_iTotalCacheMemory + iMemoryUsed > CPC_ALBUMART_MAX_MEMORY_MB * 1024 * 1024)
    {
        CPTL_EvictLRU();
        if (g_iCacheEntryCount == 0)
            break;
    }

    CPs_AlbumArtCacheEntry* pNew =
        (CPs_AlbumArtCacheEntry*)malloc(sizeof(CPs_AlbumArtCacheEntry));
    if (pNew)
    {
        memset(pNew, 0, sizeof(CPs_AlbumArtCacheEntry));
        pNew->m_pcFilePath   = _strdup(pcFilePath);
        pNew->m_hBitmap      = hBitmap;
        pNew->m_iWidth       = iWidth;
        pNew->m_iHeight      = iHeight;
        pNew->m_tLastAccess  = time(NULL);
        pNew->m_iMemoryUsed  = iMemoryUsed;

        pNew->m_pNext = g_pAlbumArtHashTable[hash];
        g_pAlbumArtHashTable[hash] = pNew;

        CPTL_AddToLRU(pNew);
        g_iCacheEntryCount++;
        g_iTotalCacheMemory += iMemoryUsed;
    }

    if (piActualWidth)  *piActualWidth  = iWidth;
    if (piActualHeight) *piActualHeight = iHeight;
    return hBitmap;
}

} // extern "C"
