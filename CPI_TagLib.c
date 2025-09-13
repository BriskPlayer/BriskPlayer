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

#include "stdafx.h"
#include "CPI_TagLib.h"
#include "CPI_PlaylistItem.h"
#include <string.h>
#include <stdlib.h>
#define TAGLIB_STATIC
#include <tag_c.h>

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
}

void CPTL_Cleanup(void)
{
    // TagLib C interface handles cleanup automatically
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
        *ppcTitle = (char*)malloc(strlen(str_value) + 1);
        if (*ppcTitle)
            strcpy(*ppcTitle, str_value);
    }
    
    // Read artist
    str_value = taglib_tag_artist(tag);
    if (str_value && *str_value)
    {
        *ppcArtist = (char*)malloc(strlen(str_value) + 1);
        if (*ppcArtist)
            strcpy(*ppcArtist, str_value);
    }
    
    // Read album
    str_value = taglib_tag_album(tag);
    if (str_value && *str_value)
    {
        *ppcAlbum = (char*)malloc(strlen(str_value) + 1);
        if (*ppcAlbum)
            strcpy(*ppcAlbum, str_value);
    }
    
    // Read year
    unsigned int year = taglib_tag_year(tag);
    if (year > 0)
    {
        *ppcYear = (char*)malloc(16);
        if (*ppcYear)
            sprintf(*ppcYear, "%u", year);
    }
    
    // Read comment
    str_value = taglib_tag_comment(tag);
    if (str_value && *str_value)
    {
        *ppcComment = (char*)malloc(strlen(str_value) + 1);
        if (*ppcComment)
            strcpy(*ppcComment, str_value);
    }
    
    // Read genre
    str_value = taglib_tag_genre(tag);
    if (str_value && *str_value)
    {
        *ppcGenre = (char*)malloc(strlen(str_value) + 1);
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
        
    // Try to open the file for writing
    hFile = CreateFile(pcFilePath, GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                       OPEN_EXISTING, 0, 0);
                       
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
        
    pcDest = malloc(iLength + 1);
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
