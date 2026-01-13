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




////////////////////////////////////////////////////////////////////////////////
//
// TagLib-based metadata handling
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _CPI_TAGLIB_H_
#define _CPI_TAGLIB_H_

#include <windows.h>
// #include <taglib/tag_c.h>  // Temporarily commented for compilation

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Genre definitions - compatible with ID3v1 genres
//
////////////////////////////////////////////////////////////////////////////////

#define CIC_NUMGENRES 149
extern const char* glb_pcGenres[];

////////////////////////////////////////////////////////////////////////////////
//
// TagLib wrapper functions  
//
////////////////////////////////////////////////////////////////////////////////

// Initialize/cleanup
void CPTL_Initialize(void);
void CPTL_Cleanup(void);

////////////////////////////////////////////////////////////////////////////////
//
// Consolidated metadata structure - holds all metadata in one struct
// This allows reading all metadata with a single file open for efficiency
//
////////////////////////////////////////////////////////////////////////////////

typedef struct _CPs_AllMetadata
{
    // Basic tags
    char* m_pcTitle;
    char* m_pcArtist;
    char* m_pcAlbum;
    char* m_pcYear;
    char* m_pcComment;
    char* m_pcGenre;
    unsigned int m_iTrackNum;
    unsigned int m_iLength;
    int m_iTagType;
    
    // Extended metadata
    char* m_pcComposer;
    char* m_pcAlbumArtist;
    char* m_pcGrouping;
    char* m_pcCopyright;
    char* m_pcLyrics;
    unsigned short m_iDiscNumber;
    unsigned short m_iBPM;
    
    // ReplayGain
    float m_fTrackGain;
    float m_fTrackPeak;
    float m_fAlbumGain;
    float m_fAlbumPeak;
    
    // Audio properties
    unsigned int m_iBitrate;
    unsigned int m_iSampleRate;
    unsigned short m_iBitDepth;
    unsigned char m_cChannels;
    char* m_pcCodec;
    char* m_pcBitrateMode;
    unsigned int m_iFileSize;
    
    // Multiple artists
    char* m_pcArtists;
    char* m_pcFeaturedArtist;
    char* m_pcRemixer;
    
    // MusicBrainz IDs
    char* m_pcMB_TrackID;
    char* m_pcMB_ReleaseID;
    char* m_pcMB_ArtistID;
    char* m_pcMB_AlbumArtistID;
    char* m_pcMB_ReleaseGroupID;
    
    // Flags indicating which sections were read successfully
    BOOL m_bHasBasicTags;
    BOOL m_bHasExtendedTags;
    BOOL m_bHasReplayGain;
    BOOL m_bHasAudioProperties;
    BOOL m_bHasMultipleArtists;
    BOOL m_bHasMusicBrainzIDs;
} CPs_AllMetadata;

// Initialize metadata structure (zeros all fields)
void CPTL_InitMetadata(CPs_AllMetadata* pMetadata);

// Free all allocated strings in metadata structure
void CPTL_FreeMetadata(CPs_AllMetadata* pMetadata);

// Read ALL metadata in a single file open - most efficient for bulk loading
BOOL CPTL_ReadAllMetadata(const char* pcFilePath, CPs_AllMetadata* pMetadata);

// Read BASIC metadata only (title, artist, album, etc.) - faster for initial playlist load
// Extended metadata can be loaded later with CPTL_ReadExtendedMetadataOnly
BOOL CPTL_ReadBasicMetadataOnly(const char* pcFilePath, CPs_AllMetadata* pMetadata);

// Read EXTENDED metadata only (composer, lyrics, replaygain, musicbrainz, etc.)
// Use after CPTL_ReadBasicMetadataOnly for lazy loading
BOOL CPTL_ReadExtendedMetadataOnly(const char* pcFilePath, CPs_AllMetadata* pMetadata);

// Read tag information from file (legacy - opens file separately)
BOOL CPTL_ReadTags(const char* pcFilePath, 
                   char** ppcTitle, 
                   char** ppcArtist, 
                   char** ppcAlbum, 
                   char** ppcYear, 
                   char** ppcComment, 
                   char** ppcGenre,
                   unsigned int* piTrackNum,
                   unsigned int* piLength,
                   int* piTagType);

// Write tag information to file
BOOL CPTL_WriteTags(const char* pcFilePath,
                    const char* pcTitle,
                    const char* pcArtist,
                    const char* pcAlbum,
                    const char* pcYear,
                    const char* pcComment,
                    const char* pcGenre,
                    unsigned int iTrackNum,
                    unsigned int iLength);

// Check if file can be written to
BOOL CPTL_CanWriteToFile(const char* pcFilePath);

// Get genre index from genre string
int CPTL_GetGenreIndex(const char* pcGenre);

// Get genre string from index
const char* CPTL_GetGenreString(int iGenreIndex);

// Extended metadata reading (returns allocated strings that must be freed)
BOOL CPTL_ReadExtendedTags(const char* pcFilePath,
                           char** ppcComposer,
                           char** ppcAlbumArtist,
                           char** ppcGrouping,
                           char** ppcCopyright,
                           char** ppcLyrics,
                           unsigned short* piDiscNumber,
                           unsigned short* piBPM);

// Extended metadata writing
BOOL CPTL_WriteExtendedTags(const char* pcFilePath,
                            const char* pcComposer,
                            const char* pcAlbumArtist,
                            const char* pcGrouping,
                            const char* pcCopyright,
                            const char* pcLyrics,
                            unsigned short iDiscNumber,
                            unsigned short iBPM);

// ReplayGain reading
BOOL CPTL_ReadReplayGain(const char* pcFilePath,
                         float* pfTrackGain,
                         float* pfTrackPeak,
                         float* pfAlbumGain,
                         float* pfAlbumPeak);

// ReplayGain writing
BOOL CPTL_WriteReplayGain(const char* pcFilePath,
                          float fTrackGain,
                          float fTrackPeak,
                          float fAlbumGain,
                          float fAlbumPeak);

// Read audio properties from file
BOOL CPTL_ReadAudioProperties(const char* pcFilePath,
                              unsigned int* pBitrate,
                              unsigned int* pSampleRate,
                              unsigned short* pBitDepth,
                              unsigned char* pChannels,
                              char** ppcCodec,
                              char** ppcBitrateMode,
                              unsigned int* pFileSize);

// Read multiple artists metadata
BOOL CPTL_ReadMultipleArtists(const char* pcFilePath,
                              char** ppcArtists,
                              char** ppcFeaturedArtist,
                              char** ppcRemixer);

// Write multiple artists metadata
BOOL CPTL_WriteMultipleArtists(const char* pcFilePath,
                               const char* pcArtists,
                               const char* pcFeaturedArtist,
                               const char* pcRemixer);

// Read MusicBrainz IDs
BOOL CPTL_ReadMusicBrainzIDs(const char* pcFilePath,
                             char** ppcTrackID,
                             char** ppcReleaseID,
                             char** ppcArtistID,
                             char** ppcAlbumArtistID,
                             char** ppcReleaseGroupID);

// Write MusicBrainz IDs
BOOL CPTL_WriteMusicBrainzIDs(const char* pcFilePath,
                              const char* pcTrackID,
                              const char* pcReleaseID,
                              const char* pcArtistID,
                              const char* pcAlbumArtistID,
                              const char* pcReleaseGroupID);

// Skip ID3v2 tag for codec parsers (maintains compatibility)
unsigned int CPTL_SkipID3v2Tag(const void* pBuffer, unsigned int iBufferSize);

// Utility functions for backwards compatibility
char* DecodeID3String(const char* pcSource, const int iLength);

////////////////////////////////////////////////////////////////////////////////
//
// Album Art / Cover Art support
//
////////////////////////////////////////////////////////////////////////////////

// Album art image data structure
typedef struct _CPs_AlbumArt
{
	BYTE* m_pImageData;           // Raw image data (JPEG/PNG)
	unsigned int m_iImageSize;    // Size in bytes
	char* m_pcMimeType;           // MIME type ("image/jpeg" or "image/png")
	unsigned int m_iWidth;        // Image width (if decoded)
	unsigned int m_iHeight;       // Image height (if decoded)
} CPs_AlbumArt;

// Read album art from file
BOOL CPTL_ReadAlbumArt(const char* pcFilePath, CPs_AlbumArt* pAlbumArt);

// Free album art structure
void CPTL_FreeAlbumArt(CPs_AlbumArt* pAlbumArt);

// Write album art to file
BOOL CPTL_WriteAlbumArt(const char* pcFilePath, 
                        const BYTE* pImageData, 
                        unsigned int iImageSize, 
                        const char* pcMimeType);

// Check if file has album art
BOOL CPTL_HasAlbumArt(const char* pcFilePath);

// Convert image data to HBITMAP for display
HBITMAP CPTL_CreateBitmapFromImageData(const BYTE* pImageData, 
                                       unsigned int iImageSize,
                                       unsigned int iMaxWidth,
                                       unsigned int iMaxHeight,
                                       unsigned int* piActualWidth,
                                       unsigned int* piActualHeight);

////////////////////////////////////////////////////////////////////////////////
//
// Album Art Cache Management
//
////////////////////////////////////////////////////////////////////////////////

#define CPC_ALBUMART_CACHE_SIZE 20
#define CPC_ALBUMART_MAX_MEMORY_MB 10

// Initialize album art cache
void CPTL_InitAlbumArtCache(void);

// Cleanup album art cache
void CPTL_CleanupAlbumArtCache(void);

// Get album art from cache (or load if not cached)
HBITMAP CPTL_GetAlbumArtBitmap(const char* pcFilePath,
                                unsigned int iMaxWidth,
                                unsigned int iMaxHeight,
                                unsigned int* piActualWidth,
                                unsigned int* piActualHeight);

// Load album art at specific size (bypasses cache, caller must free HBITMAP)
HBITMAP CPTL_LoadAlbumArtBitmap(const char* pcFilePath,
                                unsigned int iTargetWidth,
                                unsigned int iTargetHeight,
                                unsigned int* piActualWidth,
                                unsigned int* piActualHeight);

// Clear album art cache
void CPTL_ClearAlbumArtCache(void);

// Remove specific item from cache
void CPTL_RemoveFromAlbumArtCache(const char* pcFilePath);

#ifdef __cplusplus
}
#endif

#endif // _CPI_TAGLIB_H_
