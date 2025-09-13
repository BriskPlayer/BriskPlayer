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

// Read tag information from file
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

// Skip ID3v2 tag for codec parsers (maintains compatibility)
unsigned int CPTL_SkipID3v2Tag(const void* pBuffer, unsigned int iBufferSize);

// Utility functions for backwards compatibility
char* DecodeID3String(const char* pcSource, const int iLength);

#endif // _CPI_TAGLIB_H_
