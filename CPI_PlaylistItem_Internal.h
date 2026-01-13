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
// Cooler PlaylistItem Internal
//
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
//
typedef enum _CPe_TagType
{
	ttUnread,
	ttNone,
	ttID3v1,
	ttID3v2
} CPe_TagType;
//
////////////////////////////////////////////////////////////////////////////////


#define CPC_TRACKNUMASTEXTBUFFERSIZE			16
////////////////////////////////////////////////////////////////////////////////
//

typedef struct _CPs_PlaylistItem
{
	char* m_pcPath;
	char* m_pcFilename;
	
	BOOL m_bID3Tag_SaveRequired;
	CPe_TagType m_enTagType;
	BOOL m_bDestroyOnDeactivate;
	BOOL m_bExtendedMetadataLoaded;  // TRUE if extended metadata has been lazy-loaded
	
	char m_cTrackStackPos_AsText[16];
	int m_iTrackStackPos;
	char* m_pcArtist;
	char* m_pcAlbum;
	char* m_pcTrackName;
	char* m_pcYear;
	char* m_pcComment;
	unsigned char m_cTrackNum;
	char* m_pcTrackNum_AsText;
	unsigned char m_cGenre;
	char* m_pcTrackLength_AsText;
	unsigned int m_iTrackLength;
	
	// Extended metadata fields
	char* m_pcComposer;
	char* m_pcAlbumArtist;
	char* m_pcGrouping;
	char* m_pcCopyright;
	char* m_pcLyrics;
	unsigned short m_iDiscNumber;
	unsigned short m_iBPM;
	
	// ReplayGain fields
	float m_fReplayGain_Track_Gain;  // in dB
	float m_fReplayGain_Track_Peak;  // 0.0-1.0
	float m_fReplayGain_Album_Gain;  // in dB
	float m_fReplayGain_Album_Peak;  // 0.0-1.0
	
	// Audio properties
	unsigned int m_iBitrate;         // in kbps
	unsigned int m_iSampleRate;      // in Hz (44100, 48000, etc.)
	unsigned short m_iBitDepth;      // 16, 24, 32, etc.
	unsigned char m_cChannels;       // 1=mono, 2=stereo, 6=5.1, etc.
	char* m_pcCodec;                 // "MP3", "FLAC", "Vorbis", etc.
	char* m_pcBitrateMode;           // "CBR", "VBR", "ABR"
	unsigned int m_iFileSize;        // in bytes
	
	// Multiple artists support
	char* m_pcArtists;               // All artists (may include features)
	char* m_pcFeaturedArtist;        // Featured artist(s)
	char* m_pcRemixer;               // Remixer/producer
	
	// MusicBrainz IDs (UUIDs for database integration)
	char* m_pcMusicBrainz_TrackID;        // Recording ID
	char* m_pcMusicBrainz_ReleaseID;      // Release/Album ID
	char* m_pcMusicBrainz_ArtistID;       // Artist ID
	char* m_pcMusicBrainz_AlbumArtistID;  // Album Artist ID
	char* m_pcMusicBrainz_ReleaseGroupID; // Release Group ID
	
	int m_iCookie;
	
	CP_HPLAYLISTITEM m_hNext;
	CP_HPLAYLISTITEM m_hPrev;
	
} CPs_PlaylistItem;

//
#define CPLII_DECODEHANDLE(hitem)				((CPs_PlaylistItem*)(hitem))
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
CP_HPLAYLISTITEM CPLII_CreateItem(const char* pcPath);
void CPLII_DestroyItem(CP_HPLAYLISTITEM hItem);
