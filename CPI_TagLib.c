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
// Note: This file is compiled as C++ to use TagLib C++ API for album art extraction
//

// Disable NLS for this file to avoid conflicts with TagLib templates
#ifdef ENABLE_NLS
#undef ENABLE_NLS
#endif

#include "stdafx.h"

// Wrap C includes in extern "C"
extern "C" {
#include "CPI_TagLib.h"
#include "CPI_PlaylistItem.h"
#include "CPString.h"
}

// Undefine ALL gettext macros to prevent conflicts with TagLib
#ifdef _
#undef _
#endif
#ifdef N_
#undef N_
#endif
#ifdef P_
#undef P_
#endif
#ifdef D_
#undef D_
#endif
#ifdef DC_
#undef DC_
#endif
#ifdef C_
#undef C_
#endif
#ifdef CP_
#undef CP_
#endif
#ifdef T
#undef T
#endif

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

// For image decoding (WIC - Windows Imaging Component)
#include <wincodec.h>
#include <objbase.h>

// TagLib C and C++ API
#define TAGLIB_STATIC
#include <tag_c.h>
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/vorbisfile.h>
#include <taglib/xiphcomment.h>

// Wrap all functions in extern "C" for C linkage
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
// TagLib wrapper implementation
//
////////////////////////////////////////////////////////////////////////////////

void CPTL_Initialize(void)
{
    // TagLib C interface is automatically initialized
    
    // Initialize album art cache
    CPTL_InitAlbumArtCache();
}

void CPTL_Cleanup(void)
{
    // TagLib C interface handles cleanup automatically
    
    // Cleanup album art cache
    CPTL_CleanupAlbumArtCache();
}

BOOL CPTL_ReadTags(const char* pcFilePath, 
                   char** ppcTitle, 
                   char** ppcArtist, 
                   char** ppcAlbum, 
                   char** ppcYear, 
                   char** ppcComment, 
                   char** ppcGenre,
                   unsigned int* piTrackNum,
                   unsigned int* piLength,
                   int* piTagType)
{
    TagLib_File* file = NULL;
    TagLib_Tag* tag = NULL;
    const char* str_value = NULL;
    
    if (!pcFilePath || !ppcTitle || !ppcArtist || !ppcAlbum || 
        !ppcYear || !ppcComment || !ppcGenre || !piTrackNum || 
        !piLength || !piTagType)
        return FALSE;
        
    // Initialize output parameters
    *ppcTitle = NULL;
    *ppcArtist = NULL;
    *ppcAlbum = NULL;
    *ppcYear = NULL;
    *ppcComment = NULL;
    *ppcGenre = NULL;
    *piTrackNum = 0;
    *piLength = 0;
    *piTagType = 0; // ttNone
    
    // Open file with TagLib
    file = taglib_file_new(pcFilePath);
    if (!file || !taglib_file_is_valid(file))
    {
        if (file)
            taglib_file_free(file);
        return FALSE;
    }
    
    // Get tag from file
    tag = taglib_file_tag(file);
    if (!tag)
    {
        taglib_file_free(file);
        return FALSE;
    }
    
    // Read title
    str_value = taglib_tag_title(tag);
    if (str_value && *str_value)
    {
        *ppcTitle = CALLOC_TYPE(char, strlen(str_value) + 1);
        if (*ppcTitle)
            strcpy(*ppcTitle, str_value);
    }
    
    // Read artist
    str_value = taglib_tag_artist(tag);
    if (str_value && *str_value)
    {
        *ppcArtist = CALLOC_TYPE(char, strlen(str_value) + 1);
        if (*ppcArtist)
            strcpy(*ppcArtist, str_value);
    }
    
    // Read album
    str_value = taglib_tag_album(tag);
    if (str_value && *str_value)
    {
        *ppcAlbum = CALLOC_TYPE(char, strlen(str_value) + 1);
        if (*ppcAlbum)
            strcpy(*ppcAlbum, str_value);
    }
    
    // Read year
    unsigned int year = taglib_tag_year(tag);
    if (year > 0)
    {
        *ppcYear = CALLOC_TYPE(char, 16);
        if (*ppcYear)
            sprintf_s(*ppcYear, 16, "%u", year);
    }
    
    // Read comment
    str_value = taglib_tag_comment(tag);
    if (str_value && *str_value)
    {
        *ppcComment = CALLOC_TYPE(char, strlen(str_value) + 1);
        if (*ppcComment)
            strcpy(*ppcComment, str_value);
    }
    
    // Read genre
    str_value = taglib_tag_genre(tag);
    if (str_value && *str_value)
    {
        *ppcGenre = CALLOC_TYPE(char, strlen(str_value) + 1);
        if (*ppcGenre)
            strcpy(*ppcGenre, str_value);
    }
    
    // Read track number
    *piTrackNum = taglib_tag_track(tag);
    
    // Get audio properties for length
    const TagLib_AudioProperties* props = taglib_file_audioproperties(file);
    if (props)
    {
        *piLength = taglib_audioproperties_length(props);
    }
    
    // Set tag type - assume ID3v2 if we got any tags
    if (*ppcTitle || *ppcArtist || *ppcAlbum || *ppcYear || 
        *ppcComment || *ppcGenre || *piTrackNum > 0)
    {
        *piTagType = 2; // ttID3v2
    }
    
    taglib_file_free(file);
    return TRUE;
}

BOOL CPTL_WriteTags(const char* pcFilePath,
                    const char* pcTitle,
                    const char* pcArtist,
                    const char* pcAlbum,
                    const char* pcYear,
                    const char* pcComment,
                    const char* pcGenre,
                    unsigned int iTrackNum,
                    unsigned int iLength)
{
    TagLib_File* file = NULL;
    TagLib_Tag* tag = NULL;
    
    (void)iLength;  // Unused parameter
    
    if (!pcFilePath)
        return FALSE;
        
    // Open file with TagLib
    file = taglib_file_new(pcFilePath);
    if (!file || !taglib_file_is_valid(file))
    {
        if (file)
            taglib_file_free(file);
        return FALSE;
    }
    
    // Get tag from file
    tag = taglib_file_tag(file);
    if (!tag)
    {
        taglib_file_free(file);
        return FALSE;
    }
    
    // Write title
    if (pcTitle)
        taglib_tag_set_title(tag, pcTitle);
    
    // Write artist
    if (pcArtist)
        taglib_tag_set_artist(tag, pcArtist);
    
    // Write album
    if (pcAlbum)
        taglib_tag_set_album(tag, pcAlbum);
    
    // Write year
    if (pcYear)
    {
        unsigned int year = atoi(pcYear);
        taglib_tag_set_year(tag, year);
    }
    
    // Write comment
    if (pcComment)
        taglib_tag_set_comment(tag, pcComment);
    
    // Write genre
    if (pcGenre)
        taglib_tag_set_genre(tag, pcGenre);
    
    // Write track number
    if (iTrackNum > 0)
        taglib_tag_set_track(tag, iTrackNum);
    
    // Save the file
    BOOL result = taglib_file_save(file) ? TRUE : FALSE;
    
    taglib_file_free(file);
    return result;
}

BOOL CPTL_CanWriteToFile(const char* pcFilePath)
{
    HANDLE hFile;
    
    if (!pcFilePath)
        return FALSE;
        
    // Convert filename to Unicode for better filename support
    WCHAR* pwcFilePath = STR_ConvertToUnicode(pcFilePath);
    if (!pwcFilePath)
        return FALSE;
        
    // Try to open the file for writing
    hFile = CreateFileW(pwcFilePath, GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                        OPEN_EXISTING, 0, 0);
    free(pwcFilePath);
                       
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;
        
    CloseHandle(hFile);
    return TRUE;
}

int CPTL_GetGenreIndex(const char* pcGenre)
{
    int i;
    
    if (!pcGenre)
        return -1;
        
    for (i = 0; i < CIC_NUMGENRES; i++)
    {
        if (stricmp(pcGenre, glb_pcGenres[i]) == 0)
            return i;
    }
    
    return -1; // Not found
}

const char* CPTL_GetGenreString(int iGenreIndex)
{
    if (iGenreIndex < 0 || iGenreIndex >= CIC_NUMGENRES)
        return NULL;
        
    return glb_pcGenres[iGenreIndex];
}

////////////////////////////////////////////////////////////////////////////////
// Extended Metadata Reading/Writing (C++ TagLib API with C linkage)
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

// Read extended metadata fields from file
BOOL CPTL_ReadExtendedTags(const char* pcFilePath,
                           char** ppcComposer,
                           char** ppcAlbumArtist,
                           char** ppcGrouping,
                           char** ppcCopyright,
                           char** ppcLyrics,
                           unsigned short* piDiscNumber,
                           unsigned short* piBPM)
{
    if (!pcFilePath)
        return FALSE;
        
    // Initialize output parameters
    if (ppcComposer) *ppcComposer = NULL;
    if (ppcAlbumArtist) *ppcAlbumArtist = NULL;
    if (ppcGrouping) *ppcGrouping = NULL;
    if (ppcCopyright) *ppcCopyright = NULL;
    if (ppcLyrics) *ppcLyrics = NULL;
    if (piDiscNumber) *piDiscNumber = 0;
    if (piBPM) *piBPM = 0;
    
    try
    {
        // Open file with TagLib
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull() || !fileRef.tag())
            return FALSE;
            
        TagLib::Tag* tag = fileRef.tag();
        TagLib::PropertyMap properties = fileRef.file()->properties();
        
        // Extract Composer (TCOM in ID3v2, COMPOSER in Vorbis/APE)
        if (ppcComposer && properties.contains("COMPOSER"))
        {
            TagLib::StringList composers = properties["COMPOSER"];
            if (!composers.isEmpty())
            {
                std::string composerStr = composers.front().to8Bit(true);
                *ppcComposer = _strdup(composerStr.c_str());
            }
        }
        
        // Extract Album Artist (TPE2 in ID3v2, ALBUMARTIST in Vorbis/APE)
        if (ppcAlbumArtist && properties.contains("ALBUMARTIST"))
        {
            TagLib::StringList albumArtists = properties["ALBUMARTIST"];
            if (!albumArtists.isEmpty())
            {
                std::string albumArtistStr = albumArtists.front().to8Bit(true);
                *ppcAlbumArtist = _strdup(albumArtistStr.c_str());
            }
        }
        
        // Extract Grouping/Content Group (TIT1 in ID3v2, GROUPING in Vorbis/APE)
        if (ppcGrouping && properties.contains("GROUPING"))
        {
            TagLib::StringList groupings = properties["GROUPING"];
            if (!groupings.isEmpty())
            {
                std::string groupingStr = groupings.front().to8Bit(true);
                *ppcGrouping = _strdup(groupingStr.c_str());
            }
        }
        
        // Extract Copyright (TCOP in ID3v2, COPYRIGHT in Vorbis/APE)
        if (ppcCopyright && properties.contains("COPYRIGHT"))
        {
            TagLib::StringList copyrights = properties["COPYRIGHT"];
            if (!copyrights.isEmpty())
            {
                std::string copyrightStr = copyrights.front().to8Bit(true);
                *ppcCopyright = _strdup(copyrightStr.c_str());
            }
        }
        
        // Extract Lyrics (USLT in ID3v2, LYRICS in Vorbis/APE)
        if (ppcLyrics && properties.contains("LYRICS"))
        {
            TagLib::StringList lyrics = properties["LYRICS"];
            if (!lyrics.isEmpty())
            {
                std::string lyricsStr = lyrics.front().to8Bit(true);
                *ppcLyrics = _strdup(lyricsStr.c_str());
            }
        }
        
        // Extract Disc Number (TPOS in ID3v2, DISCNUMBER in Vorbis/APE)
        if (piDiscNumber && properties.contains("DISCNUMBER"))
        {
            TagLib::StringList discNumbers = properties["DISCNUMBER"];
            if (!discNumbers.isEmpty())
            {
                std::string discStr = discNumbers.front().to8Bit(true);
                int discNum = atoi(discStr.c_str());
                if (discNum > 0 && discNum <= 65535)
                    *piDiscNumber = (unsigned short)discNum;
            }
        }
        
        // Extract BPM (TBPM in ID3v2, BPM in Vorbis/APE)
        if (piBPM && properties.contains("BPM"))
        {
            TagLib::StringList bpms = properties["BPM"];
            if (!bpms.isEmpty())
            {
                std::string bpmStr = bpms.front().to8Bit(true);
                int bpm = atoi(bpmStr.c_str());
                if (bpm > 0 && bpm <= 65535)
                    *piBPM = (unsigned short)bpm;
            }
        }
        
        return TRUE;
    }
    catch (...)
    {
        // Clean up on error
        if (ppcComposer && *ppcComposer) { free(*ppcComposer); *ppcComposer = NULL; }
        if (ppcAlbumArtist && *ppcAlbumArtist) { free(*ppcAlbumArtist); *ppcAlbumArtist = NULL; }
        if (ppcGrouping && *ppcGrouping) { free(*ppcGrouping); *ppcGrouping = NULL; }
        if (ppcCopyright && *ppcCopyright) { free(*ppcCopyright); *ppcCopyright = NULL; }
        if (ppcLyrics && *ppcLyrics) { free(*ppcLyrics); *ppcLyrics = NULL; }
        return FALSE;
    }
}

// Write extended metadata fields to file
BOOL CPTL_WriteExtendedTags(const char* pcFilePath,
                            const char* pcComposer,
                            const char* pcAlbumArtist,
                            const char* pcGrouping,
                            const char* pcCopyright,
                            const char* pcLyrics,
                            unsigned short iDiscNumber,
                            unsigned short iBPM)
{
    if (!pcFilePath)
        return FALSE;
        
    try
    {
        // Open file with TagLib
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull() || !fileRef.file())
            return FALSE;
            
        TagLib::PropertyMap properties = fileRef.file()->properties();
        
        // Set Composer
        if (pcComposer && *pcComposer)
        {
            properties.replace("COMPOSER", TagLib::String(pcComposer, TagLib::String::UTF8));
        }
        
        // Set Album Artist
        if (pcAlbumArtist && *pcAlbumArtist)
        {
            properties.replace("ALBUMARTIST", TagLib::String(pcAlbumArtist, TagLib::String::UTF8));
        }
        
        // Set Grouping
        if (pcGrouping && *pcGrouping)
        {
            properties.replace("GROUPING", TagLib::String(pcGrouping, TagLib::String::UTF8));
        }
        
        // Set Copyright
        if (pcCopyright && *pcCopyright)
        {
            properties.replace("COPYRIGHT", TagLib::String(pcCopyright, TagLib::String::UTF8));
        }
        
        // Set Lyrics
        if (pcLyrics && *pcLyrics)
        {
            properties.replace("LYRICS", TagLib::String(pcLyrics, TagLib::String::UTF8));
        }
        
        // Set Disc Number
        if (iDiscNumber > 0)
        {
            char discStr[16];
            sprintf(discStr, "%u", iDiscNumber);
            properties.replace("DISCNUMBER", TagLib::String(discStr, TagLib::String::UTF8));
        }
        
        // Set BPM
        if (iBPM > 0)
        {
            char bpmStr[16];
            sprintf(bpmStr, "%u", iBPM);
            properties.replace("BPM", TagLib::String(bpmStr, TagLib::String::UTF8));
        }
        
        // Apply properties and save
        fileRef.file()->setProperties(properties);
        return fileRef.save() ? TRUE : FALSE;
    }
    catch (...)
    {
        return FALSE;
    }
}

#ifdef __cplusplus
}
#endif

// ReplayGain reading
BOOL CPTL_ReadReplayGain(const char* pcFilePath,
                         float* pfTrackGain,
                         float* pfTrackPeak,
                         float* pfAlbumGain,
                         float* pfAlbumPeak)
{
    if (!pcFilePath)
        return FALSE;
        
    // Initialize output parameters
    if (pfTrackGain) *pfTrackGain = 0.0f;
    if (pfTrackPeak) *pfTrackPeak = 0.0f;
    if (pfAlbumGain) *pfAlbumGain = 0.0f;
    if (pfAlbumPeak) *pfAlbumPeak = 0.0f;
    
    try
    {
        // Open file with TagLib
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull() || !fileRef.file())
            return FALSE;
            
        TagLib::PropertyMap properties = fileRef.file()->properties();
        
        // Read REPLAYGAIN_TRACK_GAIN (in dB, e.g. "-6.23 dB")
        if (pfTrackGain && properties.contains("REPLAYGAIN_TRACK_GAIN"))
        {
            TagLib::StringList gains = properties["REPLAYGAIN_TRACK_GAIN"];
            if (!gains.isEmpty())
            {
                std::string gainStr = gains.front().to8Bit(true);
                *pfTrackGain = (float)atof(gainStr.c_str());
            }
        }
        
        // Read REPLAYGAIN_TRACK_PEAK (0.0-1.0)
        if (pfTrackPeak && properties.contains("REPLAYGAIN_TRACK_PEAK"))
        {
            TagLib::StringList peaks = properties["REPLAYGAIN_TRACK_PEAK"];
            if (!peaks.isEmpty())
            {
                std::string peakStr = peaks.front().to8Bit(true);
                *pfTrackPeak = (float)atof(peakStr.c_str());
            }
        }
        
        // Read REPLAYGAIN_ALBUM_GAIN (in dB)
        if (pfAlbumGain && properties.contains("REPLAYGAIN_ALBUM_GAIN"))
        {
            TagLib::StringList gains = properties["REPLAYGAIN_ALBUM_GAIN"];
            if (!gains.isEmpty())
            {
                std::string gainStr = gains.front().to8Bit(true);
                *pfAlbumGain = (float)atof(gainStr.c_str());
            }
        }
        
        // Read REPLAYGAIN_ALBUM_PEAK (0.0-1.0)
        if (pfAlbumPeak && properties.contains("REPLAYGAIN_ALBUM_PEAK"))
        {
            TagLib::StringList peaks = properties["REPLAYGAIN_ALBUM_PEAK"];
            if (!peaks.isEmpty())
            {
                std::string peakStr = peaks.front().to8Bit(true);
                *pfAlbumPeak = (float)atof(peakStr.c_str());
            }
        }
        
        return TRUE;
    }
    catch (...)
    {
        return FALSE;
    }
}

// ReplayGain writing
BOOL CPTL_WriteReplayGain(const char* pcFilePath,
                          float fTrackGain,
                          float fTrackPeak,
                          float fAlbumGain,
                          float fAlbumPeak)
{
    if (!pcFilePath)
        return FALSE;
        
    try
    {
        // Open file with TagLib
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull() || !fileRef.file())
            return FALSE;
            
        TagLib::PropertyMap properties = fileRef.file()->properties();
        
        // Set REPLAYGAIN_TRACK_GAIN (format: "+/-X.XX dB")
        if (fTrackGain != 0.0f)
        {
            char gainStr[32];
            sprintf(gainStr, "%.2f dB", fTrackGain);
            properties.replace("REPLAYGAIN_TRACK_GAIN", TagLib::String(gainStr, TagLib::String::UTF8));
        }
        
        // Set REPLAYGAIN_TRACK_PEAK (format: "0.XXXXXX")
        if (fTrackPeak != 0.0f)
        {
            char peakStr[32];
            sprintf(peakStr, "%.6f", fTrackPeak);
            properties.replace("REPLAYGAIN_TRACK_PEAK", TagLib::String(peakStr, TagLib::String::UTF8));
        }
        
        // Set REPLAYGAIN_ALBUM_GAIN
        if (fAlbumGain != 0.0f)
        {
            char gainStr[32];
            sprintf(gainStr, "%.2f dB", fAlbumGain);
            properties.replace("REPLAYGAIN_ALBUM_GAIN", TagLib::String(gainStr, TagLib::String::UTF8));
        }
        
        // Set REPLAYGAIN_ALBUM_PEAK
        if (fAlbumPeak != 0.0f)
        {
            char peakStr[32];
            sprintf(peakStr, "%.6f", fAlbumPeak);
            properties.replace("REPLAYGAIN_ALBUM_PEAK", TagLib::String(peakStr, TagLib::String::UTF8));
        }
        
        // Apply properties and save
        fileRef.file()->setProperties(properties);
        return fileRef.save() ? TRUE : FALSE;
    }
    catch (...)
    {
        return FALSE;
    }
}

BOOL CPTL_ReadAudioProperties(const char* pcFilePath,
                              unsigned int* pBitrate,
                              unsigned int* pSampleRate,
                              unsigned short* pBitDepth,
                              unsigned char* pChannels,
                              char** ppcCodec,
                              char** ppcBitrateMode,
                              unsigned int* pFileSize)
{
    if (!pcFilePath || !pBitrate || !pSampleRate || !pBitDepth || 
        !pChannels || !ppcCodec || !ppcBitrateMode || !pFileSize)
        return FALSE;
        
    // Initialize outputs
    *pBitrate = 0;
    *pSampleRate = 0;
    *pBitDepth = 0;
    *pChannels = 0;
    *ppcCodec = NULL;
    *ppcBitrateMode = NULL;
    *pFileSize = 0;
    
    try
    {
        // Open file with TagLib
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull() || !fileRef.file() || !fileRef.audioProperties())
            return FALSE;
            
        TagLib::AudioProperties* props = fileRef.audioProperties();
        
        // Get basic audio properties
        *pBitrate = props->bitrate();           // in kbps
        *pSampleRate = props->sampleRate();     // in Hz
        *pChannels = props->channels();
        
        // Determine codec from file type
        TagLib::File* file = fileRef.file();
        const char* codecName = "Unknown";
        const char* bitrateMode = "Unknown";
        
        // Check file type and get codec-specific info
        if (dynamic_cast<TagLib::MPEG::File*>(file))
        {
            codecName = "MP3";
            TagLib::MPEG::File* mpegFile = dynamic_cast<TagLib::MPEG::File*>(file);
            if (mpegFile && mpegFile->audioProperties())
            {
                TagLib::MPEG::Properties* mpegProps = mpegFile->audioProperties();
                // Note: TagLib doesn't provide direct VBR detection for MP3
                // We can estimate: if the file has XING/VBRI header, it's VBR
                bitrateMode = "CBR";  // Default assumption
                *pBitDepth = 16;  // MP3 internally uses 16-bit samples
            }
        }
        else if (dynamic_cast<TagLib::FLAC::File*>(file))
        {
            codecName = "FLAC";
            bitrateMode = "VBR";  // FLAC is always variable bitrate
            TagLib::FLAC::File* flacFile = dynamic_cast<TagLib::FLAC::File*>(file);
            if (flacFile && flacFile->audioProperties())
            {
                TagLib::FLAC::Properties* flacProps = flacFile->audioProperties();
                *pBitDepth = flacProps->bitsPerSample();
            }
        }
        else if (dynamic_cast<TagLib::Ogg::Vorbis::File*>(file))
        {
            codecName = "Vorbis";
            bitrateMode = "VBR";  // Vorbis is variable bitrate
            *pBitDepth = 16;  // Vorbis decodes to 16-bit float (represented as 16-bit)
        }
        else if (dynamic_cast<TagLib::MP4::File*>(file))
        {
            codecName = "AAC";
            bitrateMode = "VBR";  // AAC in MP4 is typically VBR
            *pBitDepth = 16;  // AAC typically 16-bit
        }
        else
        {
            // Try to determine from filename extension
            const char* ext = strrchr(pcFilePath, '.');
            if (ext)
            {
                ext++;  // Skip the dot
                if (_stricmp(ext, "wav") == 0 || _stricmp(ext, "wave") == 0)
                {
                    codecName = "WAV/PCM";
                    bitrateMode = "CBR";
                    *pBitDepth = 16;  // Default, actual may vary
                }
                else if (_stricmp(ext, "aiff") == 0 || _stricmp(ext, "aif") == 0)
                {
                    codecName = "AIFF";
                    bitrateMode = "CBR";
                    *pBitDepth = 16;
                }
                else if (_stricmp(ext, "ape") == 0)
                {
                    codecName = "APE";
                    bitrateMode = "VBR";
                    *pBitDepth = 16;
                }
                else if (_stricmp(ext, "wv") == 0)
                {
                    codecName = "WavPack";
                    bitrateMode = "VBR";
                }
            }
        }
        
        // Allocate and copy codec name
        *ppcCodec = _strdup(codecName);
        *ppcBitrateMode = _strdup(bitrateMode);
        
        // Get file size
        struct _stat64 fileStat;
        if (_stat64(pcFilePath, &fileStat) == 0)
        {
            *pFileSize = (unsigned int)fileStat.st_size;
        }
        
        return TRUE;
    }
    catch (...)
    {
        if (*ppcCodec)
        {
            free(*ppcCodec);
            *ppcCodec = NULL;
        }
        if (*ppcBitrateMode)
        {
            free(*ppcBitrateMode);
            *ppcBitrateMode = NULL;
        }
        return FALSE;
    }
}

BOOL CPTL_ReadMultipleArtists(const char* pcFilePath,
                              char** ppcArtists,
                              char** ppcFeaturedArtist,
                              char** ppcRemixer)
{
    if (!pcFilePath || !ppcArtists || !ppcFeaturedArtist || !ppcRemixer)
        return FALSE;
        
    // Initialize outputs
    *ppcArtists = NULL;
    *ppcFeaturedArtist = NULL;
    *ppcRemixer = NULL;
    
    try
    {
        // Open file with TagLib
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull() || !fileRef.file())
            return FALSE;
            
        TagLib::PropertyMap properties = fileRef.file()->properties();
        
        // Read ARTISTS (multiple artists, semicolon-separated typically)
        if (properties.contains("ARTISTS"))
        {
            TagLib::StringList artists = properties["ARTISTS"];
            if (!artists.isEmpty())
            {
                std::string artistsStr = artists.toString("; ").toCString(true);
                *ppcArtists = _strdup(artistsStr.c_str());
            }
        }
        
        // Read featured artist - check multiple possible tags
        // PERFORMER or INVOLVEDPEOPLE in ID3v2
        const char* featuredTags[] = {"PERFORMER", "INVOLVEDPEOPLE", "FEATURED", NULL};
        for (int i = 0; featuredTags[i] != NULL; i++)
        {
            if (properties.contains(featuredTags[i]))
            {
                TagLib::StringList featured = properties[featuredTags[i]];
                if (!featured.isEmpty())
                {
                    std::string featuredStr = featured.toString("; ").toCString(true);
                    *ppcFeaturedArtist = _strdup(featuredStr.c_str());
                    break;
                }
            }
        }
        
        // Read remixer - REMIXER or MIXARTIST
        const char* remixerTags[] = {"REMIXER", "MIXARTIST", "MODIFIEDBY", NULL};
        for (int i = 0; remixerTags[i] != NULL; i++)
        {
            if (properties.contains(remixerTags[i]))
            {
                TagLib::StringList remixer = properties[remixerTags[i]];
                if (!remixer.isEmpty())
                {
                    std::string remixerStr = remixer.toString("; ").toCString(true);
                    *ppcRemixer = _strdup(remixerStr.c_str());
                    break;
                }
            }
        }
        
        return TRUE;
    }
    catch (...)
    {
        if (*ppcArtists)
        {
            free(*ppcArtists);
            *ppcArtists = NULL;
        }
        if (*ppcFeaturedArtist)
        {
            free(*ppcFeaturedArtist);
            *ppcFeaturedArtist = NULL;
        }
        if (*ppcRemixer)
        {
            free(*ppcRemixer);
            *ppcRemixer = NULL;
        }
        return FALSE;
    }
}

BOOL CPTL_WriteMultipleArtists(const char* pcFilePath,
                               const char* pcArtists,
                               const char* pcFeaturedArtist,
                               const char* pcRemixer)
{
    if (!pcFilePath)
        return FALSE;
        
    try
    {
        // Open file with TagLib
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull() || !fileRef.file())
            return FALSE;
            
        TagLib::PropertyMap properties = fileRef.file()->properties();
        
        // Set ARTISTS tag if provided
        if (pcArtists && *pcArtists)
        {
            properties.replace("ARTISTS", TagLib::String(pcArtists, TagLib::String::UTF8));
        }
        
        // Set PERFORMER tag for featured artist
        if (pcFeaturedArtist && *pcFeaturedArtist)
        {
            properties.replace("PERFORMER", TagLib::String(pcFeaturedArtist, TagLib::String::UTF8));
        }
        
        // Set REMIXER tag
        if (pcRemixer && *pcRemixer)
        {
            properties.replace("REMIXER", TagLib::String(pcRemixer, TagLib::String::UTF8));
        }
        
        // Apply properties and save
        fileRef.file()->setProperties(properties);
        return fileRef.save() ? TRUE : FALSE;
    }
    catch (...)
    {
        return FALSE;
    }
}

BOOL CPTL_ReadMusicBrainzIDs(const char* pcFilePath,
                             char** ppcTrackID,
                             char** ppcReleaseID,
                             char** ppcArtistID,
                             char** ppcAlbumArtistID,
                             char** ppcReleaseGroupID)
{
    if (!pcFilePath || !ppcTrackID || !ppcReleaseID || !ppcArtistID || 
        !ppcAlbumArtistID || !ppcReleaseGroupID)
        return FALSE;
        
    // Initialize outputs
    *ppcTrackID = NULL;
    *ppcReleaseID = NULL;
    *ppcArtistID = NULL;
    *ppcAlbumArtistID = NULL;
    *ppcReleaseGroupID = NULL;
    
    try
    {
        // Open file with TagLib
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull() || !fileRef.file())
            return FALSE;
            
        TagLib::PropertyMap properties = fileRef.file()->properties();
        
        // Read MusicBrainz Track ID (Recording ID)
        // Common tags: MUSICBRAINZ_TRACKID, MUSICBRAINZ_RELEASEID
        const char* trackTags[] = {"MUSICBRAINZ_TRACKID", "MUSICBRAINZ TRACK ID", NULL};
        for (int i = 0; trackTags[i] != NULL; i++)
        {
            if (properties.contains(trackTags[i]))
            {
                TagLib::StringList ids = properties[trackTags[i]];
                if (!ids.isEmpty())
                {
                    std::string idStr = ids.front().toCString(true);
                    *ppcTrackID = _strdup(idStr.c_str());
                    break;
                }
            }
        }
        
        // Read MusicBrainz Release ID (Album ID)
        const char* releaseTags[] = {"MUSICBRAINZ_ALBUMID", "MUSICBRAINZ ALBUM ID", NULL};
        for (int i = 0; releaseTags[i] != NULL; i++)
        {
            if (properties.contains(releaseTags[i]))
            {
                TagLib::StringList ids = properties[releaseTags[i]];
                if (!ids.isEmpty())
                {
                    std::string idStr = ids.front().toCString(true);
                    *ppcReleaseID = _strdup(idStr.c_str());
                    break;
                }
            }
        }
        
        // Read MusicBrainz Artist ID
        const char* artistTags[] = {"MUSICBRAINZ_ARTISTID", "MUSICBRAINZ ARTIST ID", NULL};
        for (int i = 0; artistTags[i] != NULL; i++)
        {
            if (properties.contains(artistTags[i]))
            {
                TagLib::StringList ids = properties[artistTags[i]];
                if (!ids.isEmpty())
                {
                    std::string idStr = ids.front().toCString(true);
                    *ppcArtistID = _strdup(idStr.c_str());
                    break;
                }
            }
        }
        
        // Read MusicBrainz Album Artist ID
        const char* albumArtistTags[] = {"MUSICBRAINZ_ALBUMARTISTID", "MUSICBRAINZ ALBUM ARTIST ID", NULL};
        for (int i = 0; albumArtistTags[i] != NULL; i++)
        {
            if (properties.contains(albumArtistTags[i]))
            {
                TagLib::StringList ids = properties[albumArtistTags[i]];
                if (!ids.isEmpty())
                {
                    std::string idStr = ids.front().toCString(true);
                    *ppcAlbumArtistID = _strdup(idStr.c_str());
                    break;
                }
            }
        }
        
        // Read MusicBrainz Release Group ID
        const char* releaseGroupTags[] = {"MUSICBRAINZ_RELEASEGROUPID", "MUSICBRAINZ RELEASE GROUP ID", NULL};
        for (int i = 0; releaseGroupTags[i] != NULL; i++)
        {
            if (properties.contains(releaseGroupTags[i]))
            {
                TagLib::StringList ids = properties[releaseGroupTags[i]];
                if (!ids.isEmpty())
                {
                    std::string idStr = ids.front().toCString(true);
                    *ppcReleaseGroupID = _strdup(idStr.c_str());
                    break;
                }
            }
        }
        
        return TRUE;
    }
    catch (...)
    {
        // Clean up on error
        if (*ppcTrackID)
        {
            free(*ppcTrackID);
            *ppcTrackID = NULL;
        }
        if (*ppcReleaseID)
        {
            free(*ppcReleaseID);
            *ppcReleaseID = NULL;
        }
        if (*ppcArtistID)
        {
            free(*ppcArtistID);
            *ppcArtistID = NULL;
        }
        if (*ppcAlbumArtistID)
        {
            free(*ppcAlbumArtistID);
            *ppcAlbumArtistID = NULL;
        }
        if (*ppcReleaseGroupID)
        {
            free(*ppcReleaseGroupID);
            *ppcReleaseGroupID = NULL;
        }
        return FALSE;
    }
}

BOOL CPTL_WriteMusicBrainzIDs(const char* pcFilePath,
                              const char* pcTrackID,
                              const char* pcReleaseID,
                              const char* pcArtistID,
                              const char* pcAlbumArtistID,
                              const char* pcReleaseGroupID)
{
    if (!pcFilePath)
        return FALSE;
        
    try
    {
        // Open file with TagLib
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull() || !fileRef.file())
            return FALSE;
            
        TagLib::PropertyMap properties = fileRef.file()->properties();
        
        // Set MusicBrainz Track ID if provided
        if (pcTrackID && *pcTrackID)
        {
            properties.replace("MUSICBRAINZ_TRACKID", TagLib::String(pcTrackID, TagLib::String::UTF8));
        }
        
        // Set MusicBrainz Release ID if provided
        if (pcReleaseID && *pcReleaseID)
        {
            properties.replace("MUSICBRAINZ_ALBUMID", TagLib::String(pcReleaseID, TagLib::String::UTF8));
        }
        
        // Set MusicBrainz Artist ID if provided
        if (pcArtistID && *pcArtistID)
        {
            properties.replace("MUSICBRAINZ_ARTISTID", TagLib::String(pcArtistID, TagLib::String::UTF8));
        }
        
        // Set MusicBrainz Album Artist ID if provided
        if (pcAlbumArtistID && *pcAlbumArtistID)
        {
            properties.replace("MUSICBRAINZ_ALBUMARTISTID", TagLib::String(pcAlbumArtistID, TagLib::String::UTF8));
        }
        
        // Set MusicBrainz Release Group ID if provided
        if (pcReleaseGroupID && *pcReleaseGroupID)
        {
            properties.replace("MUSICBRAINZ_RELEASEGROUPID", TagLib::String(pcReleaseGroupID, TagLib::String::UTF8));
        }
        
        // Apply properties and save
        fileRef.file()->setProperties(properties);
        return fileRef.save() ? TRUE : FALSE;
    }
    catch (...)
    {
        return FALSE;
    }
}

unsigned int CPTL_SkipID3v2Tag(const void* pBuffer, unsigned int iBufferSize)
{
    const unsigned char* pBytes = (const unsigned char*)pBuffer;
    unsigned int iOffset = 0;
    
    if (!pBuffer || iBufferSize < 10)
        return 0;
        
    // Check for ID3v2 header
    if (memcmp(pBytes, "ID3", 3) == 0)
    {
        // Calculate tag size from sync-safe integer
        iOffset = 10; // Header size
        iOffset += (pBytes[6] << 21) | (pBytes[7] << 14) | (pBytes[8] << 7) | pBytes[9];
    }
    
    return iOffset;
}

// Backwards compatibility function
char* DecodeID3String(const char* pcSource, const int iLength)
{
    char* pcDest;
    int i, iDestPos = 0;
    
    if (!pcSource || iLength <= 0)
        return NULL;
        
    pcDest = CALLOC_TYPE(char, iLength + 1);
    if (!pcDest)
        return NULL;
        
    // Copy string, removing padding spaces
    for (i = 0; i < iLength && pcSource[i] != '\0'; i++)
    {
        if (pcSource[i] != ' ' || iDestPos > 0)
            pcDest[iDestPos++] = pcSource[i];
    }
    
    // Remove trailing spaces
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
// Album Art / Cover Art Implementation
//
////////////////////////////////////////////////////////////////////////////////

// Album art cache entry
typedef struct _CPs_AlbumArtCacheEntry
{
    char* m_pcFilePath;
    HBITMAP m_hBitmap;
    unsigned int m_iWidth;
    unsigned int m_iHeight;
    time_t m_tLastAccess;
    unsigned int m_iMemoryUsed;
    struct _CPs_AlbumArtCacheEntry* m_pNext;
} CPs_AlbumArtCacheEntry;

// Global cache
static CPs_AlbumArtCacheEntry* g_pAlbumArtCache = NULL;
static int g_iCacheEntryCount = 0;
static unsigned int g_iTotalCacheMemory = 0;

// Initialize album art cache (WIC is initialized per-thread as needed)
void CPTL_InitAlbumArtCache(void)
{
    g_pAlbumArtCache = NULL;
    g_iCacheEntryCount = 0;
    g_iTotalCacheMemory = 0;
}

// Cleanup album art cache
void CPTL_CleanupAlbumArtCache(void)
{
    CPTL_ClearAlbumArtCache();
}

// Clear all cache entries
void CPTL_ClearAlbumArtCache(void)
{
    CPs_AlbumArtCacheEntry* pCurrent = g_pAlbumArtCache;
    CPs_AlbumArtCacheEntry* pNext;
    
    while (pCurrent)
    {
        pNext = pCurrent->m_pNext;
        
        if (pCurrent->m_pcFilePath)
            free(pCurrent->m_pcFilePath);
        if (pCurrent->m_hBitmap)
            DeleteObject(pCurrent->m_hBitmap);
            
        free(pCurrent);
        pCurrent = pNext;
    }
    
    g_pAlbumArtCache = NULL;
    g_iCacheEntryCount = 0;
    g_iTotalCacheMemory = 0;
}

// Remove specific entry from cache
void CPTL_RemoveFromAlbumArtCache(const char* pcFilePath)
{
    CPs_AlbumArtCacheEntry* pCurrent = g_pAlbumArtCache;
    CPs_AlbumArtCacheEntry* pPrev = NULL;
    
    if (!pcFilePath)
        return;
        
    while (pCurrent)
    {
        if (pCurrent->m_pcFilePath && stricmp(pCurrent->m_pcFilePath, pcFilePath) == 0)
        {
            // Remove from list
            if (pPrev)
                pPrev->m_pNext = pCurrent->m_pNext;
            else
                g_pAlbumArtCache = pCurrent->m_pNext;
                
            // Free resources
            g_iTotalCacheMemory -= pCurrent->m_iMemoryUsed;
            g_iCacheEntryCount--;
            
            if (pCurrent->m_pcFilePath)
                free(pCurrent->m_pcFilePath);
            if (pCurrent->m_hBitmap)
                DeleteObject(pCurrent->m_hBitmap);
            free(pCurrent);
            return;
        }
        
        pPrev = pCurrent;
        pCurrent = pCurrent->m_pNext;
    }
}

// Evict least recently used entry
static void CPTL_EvictLRU(void)
{
    CPs_AlbumArtCacheEntry* pCurrent = g_pAlbumArtCache;
    CPs_AlbumArtCacheEntry* pOldest = NULL;
    CPs_AlbumArtCacheEntry* pOldestPrev = NULL;
    CPs_AlbumArtCacheEntry* pPrev = NULL;
    time_t tOldest = 0;
    
    // Find oldest entry
    while (pCurrent)
    {
        if (!pOldest || pCurrent->m_tLastAccess < tOldest)
        {
            tOldest = pCurrent->m_tLastAccess;
            pOldest = pCurrent;
            pOldestPrev = pPrev;
        }
        pPrev = pCurrent;
        pCurrent = pCurrent->m_pNext;
    }
    
    if (pOldest)
    {
        // Remove from list
        if (pOldestPrev)
            pOldestPrev->m_pNext = pOldest->m_pNext;
        else
            g_pAlbumArtCache = pOldest->m_pNext;
            
        // Free resources
        g_iTotalCacheMemory -= pOldest->m_iMemoryUsed;
        g_iCacheEntryCount--;
        
        if (pOldest->m_pcFilePath)
            free(pOldest->m_pcFilePath);
        if (pOldest->m_hBitmap)
            DeleteObject(pOldest->m_hBitmap);
        free(pOldest);
    }
}

// Helper function to extract album art from ID3v2 tags (MP3)
static BOOL CPTL_ExtractID3v2Art(const char* pcFilePath, CPs_AlbumArt* pAlbumArt)
{
    TagLib::MPEG::File file(pcFilePath);
    
    if (!file.isValid() || !file.ID3v2Tag())
        return FALSE;
        
    TagLib::ID3v2::Tag *tag = file.ID3v2Tag();
    TagLib::ID3v2::FrameList frameList = tag->frameList("APIC");
    
    if (frameList.isEmpty())
        return FALSE;
        
    // Get first picture frame
    TagLib::ID3v2::AttachedPictureFrame *frame = 
        static_cast<TagLib::ID3v2::AttachedPictureFrame*>(frameList.front());
        
    if (!frame)
        return FALSE;
        
    TagLib::ByteVector pictureData = frame->picture();
    
    if (pictureData.isEmpty())
        return FALSE;
        
    // Allocate and copy image data
    pAlbumArt->m_iImageSize = pictureData.size();
    pAlbumArt->m_pImageData = (BYTE*)malloc(pAlbumArt->m_iImageSize);
    
    if (!pAlbumArt->m_pImageData)
        return FALSE;
        
    memcpy(pAlbumArt->m_pImageData, pictureData.data(), pAlbumArt->m_iImageSize);
    
    // Set MIME type
    TagLib::String mimeType = frame->mimeType();
    if (!mimeType.isEmpty())
    {
        std::string mimeStr = mimeType.to8Bit(true);
        pAlbumArt->m_pcMimeType = _strdup(mimeStr.c_str());
    }
    else
    {
        // Try to detect from data
        if (pAlbumArt->m_iImageSize >= 2)
        {
            if (pAlbumArt->m_pImageData[0] == 0xFF && pAlbumArt->m_pImageData[1] == 0xD8)
                pAlbumArt->m_pcMimeType = _strdup("image/jpeg");
            else if (pAlbumArt->m_iImageSize >= 8 && 
                     memcmp(pAlbumArt->m_pImageData, "\x89PNG\r\n\x1a\n", 8) == 0)
                pAlbumArt->m_pcMimeType = _strdup("image/png");
        }
    }
    
    return TRUE;
}

// Helper function to extract album art from FLAC files
static BOOL CPTL_ExtractFLACArt(const char* pcFilePath, CPs_AlbumArt* pAlbumArt)
{
    TagLib::FLAC::File file(pcFilePath);
    
    if (!file.isValid())
        return FALSE;
        
    TagLib::List<TagLib::FLAC::Picture*> picList = file.pictureList();
    
    if (picList.isEmpty())
        return FALSE;
        
    // Get first picture
    TagLib::FLAC::Picture *picture = picList.front();
    
    if (!picture)
        return FALSE;
        
    TagLib::ByteVector pictureData = picture->data();
    
    if (pictureData.isEmpty())
        return FALSE;
        
    // Allocate and copy image data
    pAlbumArt->m_iImageSize = pictureData.size();
    pAlbumArt->m_pImageData = (BYTE*)malloc(pAlbumArt->m_iImageSize);
    
    if (!pAlbumArt->m_pImageData)
        return FALSE;
        
    memcpy(pAlbumArt->m_pImageData, pictureData.data(), pAlbumArt->m_iImageSize);
    
    // Set MIME type
    TagLib::String mimeType = picture->mimeType();
    if (!mimeType.isEmpty())
    {
        std::string mimeStr = mimeType.to8Bit(true);
        pAlbumArt->m_pcMimeType = _strdup(mimeStr.c_str());
    }
    
    return TRUE;
}

// Helper function to extract album art from MP4/M4A files
static BOOL CPTL_ExtractMP4Art(const char* pcFilePath, CPs_AlbumArt* pAlbumArt)
{
    TagLib::MP4::File file(pcFilePath);
    
    if (!file.isValid() || !file.tag())
        return FALSE;
        
    TagLib::MP4::Tag *tag = file.tag();
    
    if (!tag->contains("covr"))
        return FALSE;
        
    TagLib::MP4::CoverArtList coverList = tag->item("covr").toCoverArtList();
    
    if (coverList.isEmpty())
        return FALSE;
        
    // Get first cover art
    TagLib::MP4::CoverArt cover = coverList.front();
    TagLib::ByteVector pictureData = cover.data();
    
    if (pictureData.isEmpty())
        return FALSE;
        
    // Allocate and copy image data
    pAlbumArt->m_iImageSize = pictureData.size();
    pAlbumArt->m_pImageData = (BYTE*)malloc(pAlbumArt->m_iImageSize);
    
    if (!pAlbumArt->m_pImageData)
        return FALSE;
        
    memcpy(pAlbumArt->m_pImageData, pictureData.data(), pAlbumArt->m_iImageSize);
    
    // Set MIME type based on format
    switch (cover.format())
    {
        case TagLib::MP4::CoverArt::JPEG:
            pAlbumArt->m_pcMimeType = _strdup("image/jpeg");
            break;
        case TagLib::MP4::CoverArt::PNG:
            pAlbumArt->m_pcMimeType = _strdup("image/png");
            break;
        default:
            pAlbumArt->m_pcMimeType = _strdup("image/jpeg");
            break;
    }
    
    return TRUE;
}

// Helper function to extract album art from OGG files (via METADATA_BLOCK_PICTURE)
static BOOL CPTL_ExtractOGGArt(const char* pcFilePath, CPs_AlbumArt* pAlbumArt)
{
    TagLib::Ogg::Vorbis::File file(pcFilePath);
    
    if (!file.isValid() || !file.tag())
        return FALSE;
        
    TagLib::Ogg::XiphComment *tag = file.tag();
    
    // Try to get METADATA_BLOCK_PICTURE
    TagLib::List<TagLib::FLAC::Picture*> picList = tag->pictureList();
    
    if (picList.isEmpty())
        return FALSE;
        
    // Get first picture
    TagLib::FLAC::Picture *picture = picList.front();
    
    if (!picture)
        return FALSE;
        
    TagLib::ByteVector pictureData = picture->data();
    
    if (pictureData.isEmpty())
        return FALSE;
        
    // Allocate and copy image data
    pAlbumArt->m_iImageSize = pictureData.size();
    pAlbumArt->m_pImageData = (BYTE*)malloc(pAlbumArt->m_iImageSize);
    
    if (!pAlbumArt->m_pImageData)
        return FALSE;
        
    memcpy(pAlbumArt->m_pImageData, pictureData.data(), pAlbumArt->m_iImageSize);
    
    // Set MIME type
    TagLib::String mimeType = picture->mimeType();
    if (!mimeType.isEmpty())
    {
        std::string mimeStr = mimeType.to8Bit(true);
        pAlbumArt->m_pcMimeType = _strdup(mimeStr.c_str());
    }
    
    return TRUE;
}

// Read album art from file using TagLib C++ API
BOOL CPTL_ReadAlbumArt(const char* pcFilePath, CPs_AlbumArt* pAlbumArt)
{
    const char* pcExt;
    BOOL bResult = FALSE;
    
    if (!pcFilePath || !pAlbumArt)
        return FALSE;
        
    memset(pAlbumArt, 0, sizeof(CPs_AlbumArt));
    
    // Get file extension to determine format
    pcExt = strrchr(pcFilePath, '.');
    if (!pcExt)
        return FALSE;
        
    pcExt++; // Skip the dot
    
    try
    {
        // Try format-specific extraction based on extension
        if (_stricmp(pcExt, "mp3") == 0)
        {
            bResult = CPTL_ExtractID3v2Art(pcFilePath, pAlbumArt);
        }
        else if (_stricmp(pcExt, "flac") == 0)
        {
            bResult = CPTL_ExtractFLACArt(pcFilePath, pAlbumArt);
        }
        else if (_stricmp(pcExt, "m4a") == 0 || _stricmp(pcExt, "m4p") == 0 || 
                 _stricmp(pcExt, "mp4") == 0 || _stricmp(pcExt, "m4b") == 0)
        {
            bResult = CPTL_ExtractMP4Art(pcFilePath, pAlbumArt);
        }
        else if (_stricmp(pcExt, "ogg") == 0 || _stricmp(pcExt, "oga") == 0)
        {
            bResult = CPTL_ExtractOGGArt(pcFilePath, pAlbumArt);
        }
        else
        {
            // Try ID3v2 as a fallback (some files may have wrong extensions)
            bResult = CPTL_ExtractID3v2Art(pcFilePath, pAlbumArt);
        }
    }
    catch (...)
    {
        // TagLib threw an exception
        CPTL_FreeAlbumArt(pAlbumArt);
        return FALSE;
    }
    
    return bResult;
}

// Free album art structure
void CPTL_FreeAlbumArt(CPs_AlbumArt* pAlbumArt)
{
    if (!pAlbumArt)
        return;
        
    if (pAlbumArt->m_pImageData)
        free(pAlbumArt->m_pImageData);
    if (pAlbumArt->m_pcMimeType)
        free(pAlbumArt->m_pcMimeType);
        
    memset(pAlbumArt, 0, sizeof(CPs_AlbumArt));
}

// Write album art to file
BOOL CPTL_WriteAlbumArt(const char* pcFilePath, 
                        const BYTE* pImageData, 
                        unsigned int iImageSize, 
                        const char* pcMimeType)
{
    const char* pcExt;
    BOOL bResult = FALSE;
    
    if (!pcFilePath || !pImageData || iImageSize == 0)
        return FALSE;
        
    // Get file extension
    pcExt = strrchr(pcFilePath, '.');
    if (!pcExt)
        return FALSE;
        
    pcExt++; // Skip the dot
    
    try
    {
        if (_stricmp(pcExt, "mp3") == 0)
        {
            TagLib::MPEG::File file(pcFilePath);
            
            if (!file.isValid())
                return FALSE;
                
            // Ensure ID3v2 tag exists
            if (!file.ID3v2Tag())
                file.ID3v2Tag(true);
                
            TagLib::ID3v2::Tag *tag = file.ID3v2Tag();
            
            // Remove existing APIC frames
            tag->removeFrames("APIC");
            
            // Create new picture frame
            TagLib::ID3v2::AttachedPictureFrame *frame = 
                new TagLib::ID3v2::AttachedPictureFrame();
                
            frame->setMimeType(pcMimeType ? pcMimeType : "image/jpeg");
            frame->setPicture(TagLib::ByteVector((const char*)pImageData, iImageSize));
            frame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
            
            tag->addFrame(frame);
            bResult = file.save();
        }
        else if (_stricmp(pcExt, "flac") == 0)
        {
            TagLib::FLAC::File file(pcFilePath);
            
            if (!file.isValid())
                return FALSE;
                
            // Remove existing pictures
            file.removePictures();
            
            // Create new picture
            TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
            picture->setMimeType(pcMimeType ? pcMimeType : "image/jpeg");
            picture->setData(TagLib::ByteVector((const char*)pImageData, iImageSize));
            picture->setType(TagLib::FLAC::Picture::FrontCover);
            
            file.addPicture(picture);
            bResult = file.save();
        }
        else if (_stricmp(pcExt, "m4a") == 0 || _stricmp(pcExt, "m4p") == 0 || 
                 _stricmp(pcExt, "mp4") == 0 || _stricmp(pcExt, "m4b") == 0)
        {
            TagLib::MP4::File file(pcFilePath);
            
            if (!file.isValid() || !file.tag())
                return FALSE;
                
            TagLib::MP4::Tag *tag = file.tag();
            
            // Determine format from MIME type
            TagLib::MP4::CoverArt::Format format = TagLib::MP4::CoverArt::JPEG;
            if (pcMimeType && strstr(pcMimeType, "png"))
                format = TagLib::MP4::CoverArt::PNG;
                
            // Create cover art
            TagLib::MP4::CoverArt cover(format, 
                TagLib::ByteVector((const char*)pImageData, iImageSize));
                
            TagLib::MP4::CoverArtList coverList;
            coverList.append(cover);
            
            tag->setItem("covr", coverList);
            bResult = file.save();
        }
        else if (_stricmp(pcExt, "ogg") == 0 || _stricmp(pcExt, "oga") == 0)
        {
            TagLib::Ogg::Vorbis::File file(pcFilePath);
            
            if (!file.isValid() || !file.tag())
                return FALSE;
                
            TagLib::Ogg::XiphComment *tag = file.tag();
            
            // Remove existing pictures
            tag->removeAllPictures();
            
            // Create new picture
            TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
            picture->setMimeType(pcMimeType ? pcMimeType : "image/jpeg");
            picture->setData(TagLib::ByteVector((const char*)pImageData, iImageSize));
            picture->setType(TagLib::FLAC::Picture::FrontCover);
            
            tag->addPicture(picture);
            bResult = file.save();
        }
    }
    catch (...)
    {
        return FALSE;
    }
    
    return bResult;
}

// Check if file has album art
BOOL CPTL_HasAlbumArt(const char* pcFilePath)
{
    CPs_AlbumArt art;
    BOOL bResult = CPTL_ReadAlbumArt(pcFilePath, &art);
    if (bResult)
        CPTL_FreeAlbumArt(&art);
    return bResult;
}

// Convert image data to HBITMAP using GDI+
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
    IWICImagingFactory* pFactory = NULL;
    IWICStream* pStream = NULL;
    IWICBitmapDecoder* pDecoder = NULL;
    IWICBitmapFrameDecode* pFrame = NULL;
    IWICFormatConverter* pConverter = NULL;
    UINT iWidth = 0, iHeight = 0;
    UINT iScaledWidth = 0, iScaledHeight = 0;
    void* pBits = NULL;
    HDC hScreenDC = NULL;
    
    // Initialize COM for this thread if needed
    CoInitialize(NULL);
    
    // Create WIC factory
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                   IID_IWICImagingFactory, (void**)&pFactory);
    if (FAILED(hr) || !pFactory)
        goto cleanup;
    
    // Create stream
    hr = pFactory->CreateStream(&pStream);
    if (FAILED(hr) || !pStream)
        goto cleanup;
    
    // Initialize stream with memory
    hr = pStream->InitializeFromMemory((BYTE*)pImageData, iImageSize);
    if (FAILED(hr))
        goto cleanup;
    
    // Create decoder
    hr = pFactory->CreateDecoderFromStream((IStream*)pStream, NULL,
                                            WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr) || !pDecoder)
        goto cleanup;
    
    // Get first frame
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr) || !pFrame)
        goto cleanup;
    
    // Get image dimensions
    pFrame->GetSize(&iWidth, &iHeight);
    
    // Calculate scaling to fit within max dimensions while preserving aspect ratio
    iScaledWidth = iWidth;
    iScaledHeight = iHeight;
    
    if (iMaxWidth > 0 && iMaxHeight > 0 && iWidth > 0 && iHeight > 0)
    {
        // Scale to fit within max dimensions (both up and down)
        float fScaleW = (float)iMaxWidth / iWidth;
        float fScaleH = (float)iMaxHeight / iHeight;
        float fScale = (fScaleW < fScaleH) ? fScaleW : fScaleH;
        
        iScaledWidth = (UINT)(iWidth * fScale);
        iScaledHeight = (UINT)(iHeight * fScale);
        
        // Ensure minimum size of 1
        if (iScaledWidth < 1) iScaledWidth = 1;
        if (iScaledHeight < 1) iScaledHeight = 1;
    }
    
    if (piActualWidth)
        *piActualWidth = iScaledWidth;
    if (piActualHeight)
        *piActualHeight = iScaledHeight;
    
    // Create format converter to convert to 32bpp BGRA
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr) || !pConverter)
        goto cleanup;
    
    hr = pConverter->Initialize((IWICBitmapSource*)pFrame,
                                 GUID_WICPixelFormat32bppBGRA,
                                 WICBitmapDitherTypeNone, NULL, 0.0,
                                 WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
        goto cleanup;
    
    // Create bitmap
    {
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(BITMAPINFO));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = iScaledWidth;
        bmi.bmiHeader.biHeight = -(LONG)iScaledHeight;  // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        hScreenDC = GetDC(NULL);
        hBitmap = CreateDIBSection(hScreenDC, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
        ReleaseDC(NULL, hScreenDC);
        hScreenDC = NULL;
        
        if (hBitmap && pBits)
        {
            // Need to scale if dimensions changed
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
                    {
                        // Copy pixels
                        pScaler->CopyPixels(NULL, iScaledWidth * 4,
                                            iScaledWidth * iScaledHeight * 4, (BYTE*)pBits);
                    }
                    pScaler->Release();
                }
            }
            else
            {
                // Copy pixels directly
                pConverter->CopyPixels(NULL, iScaledWidth * 4,
                                       iScaledWidth * iScaledHeight * 4, (BYTE*)pBits);
            }
        }
    }
    
cleanup:
    if (pConverter)
        pConverter->Release();
    if (pFrame)
        pFrame->Release();
    if (pDecoder)
        pDecoder->Release();
    if (pStream)
        pStream->Release();
    if (pFactory)
        pFactory->Release();
    
    return hBitmap;
}

// Load album art at specific target size (bypasses cache, caller must free HBITMAP)
// This scales the image to fit within the target dimensions while preserving aspect ratio
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
    
    // Load raw album art from file
    if (!CPTL_ReadAlbumArt(pcFilePath, &art))
        return NULL;
    
    // Create bitmap at target size
    hBitmap = CPTL_CreateBitmapFromImageData(art.m_pImageData, art.m_iImageSize,
                                              iTargetWidth, iTargetHeight,
                                              piActualWidth, piActualHeight);
    CPTL_FreeAlbumArt(&art);
    
    return hBitmap;
}
// Get album art from cache or load
HBITMAP CPTL_GetAlbumArtBitmap(const char* pcFilePath,
                                unsigned int iMaxWidth,
                                unsigned int iMaxHeight,
                                unsigned int* piActualWidth,
                                unsigned int* piActualHeight)
{
    CPs_AlbumArtCacheEntry* pCurrent;
    CPs_AlbumArt art;
    HBITMAP hBitmap;
    unsigned int iWidth, iHeight;
    
    if (!pcFilePath)
        return NULL;
        
    // Search cache
    pCurrent = g_pAlbumArtCache;
    while (pCurrent)
    {
        if (pCurrent->m_pcFilePath && stricmp(pCurrent->m_pcFilePath, pcFilePath) == 0)
        {
            // Found in cache - update access time
            pCurrent->m_tLastAccess = time(NULL);
            
            if (piActualWidth)
                *piActualWidth = pCurrent->m_iWidth;
            if (piActualHeight)
                *piActualHeight = pCurrent->m_iHeight;
                
            return pCurrent->m_hBitmap;
        }
        pCurrent = pCurrent->m_pNext;
    }
    
    // Not in cache - load from file
    if (!CPTL_ReadAlbumArt(pcFilePath, &art))
        return NULL;
        
    // Convert to bitmap
    hBitmap = CPTL_CreateBitmapFromImageData(art.m_pImageData, art.m_iImageSize,
                                              iMaxWidth, iMaxHeight,
                                              &iWidth, &iHeight);
    CPTL_FreeAlbumArt(&art);
    
    if (!hBitmap)
        return NULL;
        
    // Add to cache
    // Check if we need to evict entries
    unsigned int iMemoryUsed = iWidth * iHeight * 4; // Assume 32-bit color
    
    while (g_iCacheEntryCount >= CPC_ALBUMART_CACHE_SIZE ||
           g_iTotalCacheMemory + iMemoryUsed > CPC_ALBUMART_MAX_MEMORY_MB * 1024 * 1024)
    {
        CPTL_EvictLRU();
        if (g_iCacheEntryCount == 0)
            break;
    }
    
    // Create new cache entry
    CPs_AlbumArtCacheEntry* pNew = (CPs_AlbumArtCacheEntry*)malloc(sizeof(CPs_AlbumArtCacheEntry));
    if (pNew)
    {
        pNew->m_pcFilePath = _strdup(pcFilePath);
        pNew->m_hBitmap = hBitmap;
        pNew->m_iWidth = iWidth;
        pNew->m_iHeight = iHeight;
        pNew->m_tLastAccess = time(NULL);
        pNew->m_iMemoryUsed = iMemoryUsed;
        pNew->m_pNext = g_pAlbumArtCache;
        
        g_pAlbumArtCache = pNew;
        g_iCacheEntryCount++;
        g_iTotalCacheMemory += iMemoryUsed;
    }
    
    if (piActualWidth)
        *piActualWidth = iWidth;
    if (piActualHeight)
        *piActualHeight = iHeight;
        
    return hBitmap;
}

} // extern "C"
