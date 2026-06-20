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

#ifndef _CPI_PLAYLISTITEM_H_
#define _CPI_PLAYLISTITEM_H_

#include "stdafx.h"
#include "globals.h"

////////////////////////////////////////////////////////////////////////////////
//
// Cooler PlaylistItem
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
typedef enum _CPe_ReadWriteState
{
	rwsUnknown,
	rwsReadOnly,
	rwsReadWrite,
	rwsBadFile
} CPe_ReadWriteState;
//
typedef enum _CPe_FilenameFormat
{
	rwsArtistAlbumNumberTitle = 1,
	rwsArtistNumberTitle = 2,
	rwsAlbumNumberTitle = 3,
	rwsAlbumNumber = 4,
	rwsNumberTitle = 5,
	rwsTitle = 6
} CPe_FilenameFormat;
//
//
////////////////////////////////////////////////////////////////////////////////



#ifdef __cplusplus
extern "C" {
#endif

#define CIC_INVALIDPLAYLISTCOOKIE					0xFFFFFFFF
#define CIC_INVALIDGENRE							((unsigned char)0xFF)
#define CIC_INVALIDTRACKNUM							((unsigned char)0xFF)
#define CIC_TRACKSTACK_UNSTACKED					0xEFFFFFFF
////////////////////////////////////////////////////////////////////////////////
//
// Accessors (always use these!!)
const char* CPLI_GetPath(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetFilename(const CP_HPLAYLISTITEM hItem);
CPe_ReadWriteState CPLI_GetReadWriteState(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetExtension(const CP_HPLAYLISTITEM hItem);
//
// These may return NULL if this info isn't available
const char* CPLI_GetTrackStackPos_AsText(const CP_HPLAYLISTITEM hItem);
int CPLI_GetTrackStackPos(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetArtist(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetAlbum(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetTrackName(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetYear(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetGenre(const CP_HPLAYLISTITEM hItem);
unsigned char CPLI_GetTrackNum(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetTrackNum_AsText(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetComment(const CP_HPLAYLISTITEM hItem);
int CPLI_GetTrackLength(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetTrackLength_AsText(const CP_HPLAYLISTITEM hItem);
// Extended metadata accessors
const char* CPLI_GetComposer(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetAlbumArtist(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetGrouping(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetCopyright(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetLyrics(const CP_HPLAYLISTITEM hItem);
unsigned short CPLI_GetDiscNumber(const CP_HPLAYLISTITEM hItem);
unsigned short CPLI_GetBPM(const CP_HPLAYLISTITEM hItem);
// ReplayGain accessors
float CPLI_GetReplayGain_Track_Gain(const CP_HPLAYLISTITEM hItem);
float CPLI_GetReplayGain_Track_Peak(const CP_HPLAYLISTITEM hItem);
float CPLI_GetReplayGain_Album_Gain(const CP_HPLAYLISTITEM hItem);
float CPLI_GetReplayGain_Album_Peak(const CP_HPLAYLISTITEM hItem);
// Audio properties accessors
unsigned int CPLI_GetBitrate(const CP_HPLAYLISTITEM hItem);
unsigned int CPLI_GetSampleRate(const CP_HPLAYLISTITEM hItem);
unsigned short CPLI_GetBitDepth(const CP_HPLAYLISTITEM hItem);
unsigned char CPLI_GetChannels(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetCodec(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetBitrateMode(const CP_HPLAYLISTITEM hItem);
unsigned int CPLI_GetFileSize(const CP_HPLAYLISTITEM hItem);
// Multiple artists accessors
const char* CPLI_GetArtists(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetFeaturedArtist(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetRemixer(const CP_HPLAYLISTITEM hItem);
// MusicBrainz ID accessors
const char* CPLI_GetMusicBrainz_TrackID(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetMusicBrainz_ReleaseID(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetMusicBrainz_ArtistID(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetMusicBrainz_AlbumArtistID(const CP_HPLAYLISTITEM hItem);
const char* CPLI_GetMusicBrainz_ReleaseGroupID(const CP_HPLAYLISTITEM hItem);
//
// Update functions
void CPLI_SetTrackStackPos(CP_HPLAYLISTITEM hItem, const int iNewPos);
void CPLI_SetArtist(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetAlbum(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetTrackName(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetYear(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetGenreIDX(CP_HPLAYLISTITEM hItem, const unsigned char iNewValue);
void CPLI_SetTrackNum(CP_HPLAYLISTITEM hItem, const unsigned char iNewValue);
void CPLI_SetTrackNum_AsText(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetComment(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_CalculateLength(CP_HPLAYLISTITEM hItem);
// Extended metadata mutators
void CPLI_SetComposer(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetAlbumArtist(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetGrouping(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetCopyright(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetLyrics(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetDiscNumber(CP_HPLAYLISTITEM hItem, const unsigned short iNewValue);
void CPLI_SetBPM(CP_HPLAYLISTITEM hItem, const unsigned short iNewValue);
// ReplayGain mutators
void CPLI_SetReplayGain_Track_Gain(CP_HPLAYLISTITEM hItem, const float fNewValue);
void CPLI_SetReplayGain_Track_Peak(CP_HPLAYLISTITEM hItem, const float fNewValue);
void CPLI_SetReplayGain_Album_Gain(CP_HPLAYLISTITEM hItem, const float fNewValue);
void CPLI_SetReplayGain_Album_Peak(CP_HPLAYLISTITEM hItem, const float fNewValue);
// Audio properties mutators (usually set during tag reading, rarely modified manually)
void CPLI_SetBitrate(CP_HPLAYLISTITEM hItem, const unsigned int iNewValue);
void CPLI_SetSampleRate(CP_HPLAYLISTITEM hItem, const unsigned int iNewValue);
void CPLI_SetBitDepth(CP_HPLAYLISTITEM hItem, const unsigned short iNewValue);
void CPLI_SetChannels(CP_HPLAYLISTITEM hItem, const unsigned char cNewValue);
void CPLI_SetCodec(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetBitrateMode(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetFileSize(CP_HPLAYLISTITEM hItem, const unsigned int iNewValue);
// Multiple artists mutators
void CPLI_SetArtists(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetFeaturedArtist(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetRemixer(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
// MusicBrainz ID mutators
void CPLI_SetMusicBrainz_TrackID(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetMusicBrainz_ReleaseID(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetMusicBrainz_ArtistID(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetMusicBrainz_AlbumArtistID(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
void CPLI_SetMusicBrainz_ReleaseGroupID(CP_HPLAYLISTITEM hItem, const char* pcNewValue);
BOOL CPLI_RenameTrack(CP_HPLAYLISTITEM hItem, const CPe_FilenameFormat enFormat);
//
// For use by the playlist window
void CPLI_SetCookie(CP_HPLAYLISTITEM hItem, const int iCookie);
int CPLI_GetCookie(const CP_HPLAYLISTITEM hItem);
//
// These may return NULL if this item is at the end or start of the playlist respectivly
CP_HPLAYLISTITEM CPLI_Next(const CP_HPLAYLISTITEM hItem);
CP_HPLAYLISTITEM CPLI_Prev(const CP_HPLAYLISTITEM hItem);
//
// Linked-list plumbing — used by the Rust playlist module via FFI
void CPLI_SetNext(CP_HPLAYLISTITEM hItem, CP_HPLAYLISTITEM hNext);
void CPLI_SetPrev(CP_HPLAYLISTITEM hItem, CP_HPLAYLISTITEM hPrev);
void CPLI_DestroyItem(CP_HPLAYLISTITEM hItem);
BOOL CPLI_IsDestroyOnDeactivate(CP_HPLAYLISTITEM hItem);
void CPLI_SetDestroyOnDeactivate(CP_HPLAYLISTITEM hItem, BOOL bVal);
//
// ID3 tag
BOOL CPLI_IsTagDirty(CP_HPLAYLISTITEM hItem);
void CPLI_ReadTag(CP_HPLAYLISTITEM hItem);
void CPLI_WriteTag(CP_HPLAYLISTITEM hItem);
// Lazy loading: Load extended metadata on demand (composer, lyrics, replaygain, etc.)
void CPLI_EnsureExtendedMetadataLoaded(CP_HPLAYLISTITEM hItem);
// Check if extended metadata has been loaded
BOOL CPLI_IsExtendedMetadataLoaded(CP_HPLAYLISTITEM hItem);

#ifdef __cplusplus
}
#endif

////////////////////////////////////////////////////////////////////////////////

#endif /* _CPI_PLAYLISTITEM_H_ */