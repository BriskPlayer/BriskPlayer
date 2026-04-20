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

////////////////////////////////////////////////////////////////////////////////
//
// Consolidated metadata functions
//
////////////////////////////////////////////////////////////////////////////////

void CPTL_InitMetadata(CPs_AllMetadata* pMetadata)
{
    if (!pMetadata) return;
    memset(pMetadata, 0, sizeof(CPs_AllMetadata));
}

void CPTL_FreeMetadata(CPs_AllMetadata* pMetadata)
{
    if (!pMetadata) return;
    
    // Free basic tags
    if (pMetadata->m_pcTitle) { free(pMetadata->m_pcTitle); pMetadata->m_pcTitle = NULL; }
    if (pMetadata->m_pcArtist) { free(pMetadata->m_pcArtist); pMetadata->m_pcArtist = NULL; }
    if (pMetadata->m_pcAlbum) { free(pMetadata->m_pcAlbum); pMetadata->m_pcAlbum = NULL; }
    if (pMetadata->m_pcYear) { free(pMetadata->m_pcYear); pMetadata->m_pcYear = NULL; }
    if (pMetadata->m_pcComment) { free(pMetadata->m_pcComment); pMetadata->m_pcComment = NULL; }
    if (pMetadata->m_pcGenre) { free(pMetadata->m_pcGenre); pMetadata->m_pcGenre = NULL; }
    
    // Free extended metadata
    if (pMetadata->m_pcComposer) { free(pMetadata->m_pcComposer); pMetadata->m_pcComposer = NULL; }
    if (pMetadata->m_pcAlbumArtist) { free(pMetadata->m_pcAlbumArtist); pMetadata->m_pcAlbumArtist = NULL; }
    if (pMetadata->m_pcGrouping) { free(pMetadata->m_pcGrouping); pMetadata->m_pcGrouping = NULL; }
    if (pMetadata->m_pcCopyright) { free(pMetadata->m_pcCopyright); pMetadata->m_pcCopyright = NULL; }
    if (pMetadata->m_pcLyrics) { free(pMetadata->m_pcLyrics); pMetadata->m_pcLyrics = NULL; }
    
    // Free audio properties
    if (pMetadata->m_pcCodec) { free(pMetadata->m_pcCodec); pMetadata->m_pcCodec = NULL; }
    if (pMetadata->m_pcBitrateMode) { free(pMetadata->m_pcBitrateMode); pMetadata->m_pcBitrateMode = NULL; }
    
    // Free multiple artists
    if (pMetadata->m_pcArtists) { free(pMetadata->m_pcArtists); pMetadata->m_pcArtists = NULL; }
    if (pMetadata->m_pcFeaturedArtist) { free(pMetadata->m_pcFeaturedArtist); pMetadata->m_pcFeaturedArtist = NULL; }
    if (pMetadata->m_pcRemixer) { free(pMetadata->m_pcRemixer); pMetadata->m_pcRemixer = NULL; }
    
    // Free MusicBrainz IDs
    if (pMetadata->m_pcMB_TrackID) { free(pMetadata->m_pcMB_TrackID); pMetadata->m_pcMB_TrackID = NULL; }
    if (pMetadata->m_pcMB_ReleaseID) { free(pMetadata->m_pcMB_ReleaseID); pMetadata->m_pcMB_ReleaseID = NULL; }
    if (pMetadata->m_pcMB_ArtistID) { free(pMetadata->m_pcMB_ArtistID); pMetadata->m_pcMB_ArtistID = NULL; }
    if (pMetadata->m_pcMB_AlbumArtistID) { free(pMetadata->m_pcMB_AlbumArtistID); pMetadata->m_pcMB_AlbumArtistID = NULL; }
    if (pMetadata->m_pcMB_ReleaseGroupID) { free(pMetadata->m_pcMB_ReleaseGroupID); pMetadata->m_pcMB_ReleaseGroupID = NULL; }
}

// Helper to safely duplicate a TagLib string
static char* CPTL_DupTagString(const TagLib::String& str)
{
    if (str.isEmpty()) return NULL;
    std::string utf8 = str.to8Bit(true);
    return utf8.empty() ? NULL : _strdup(utf8.c_str());
}

// Helper to get first value from property map
static char* CPTL_GetProperty(const TagLib::PropertyMap& props, const char* key)
{
    if (!props.contains(key)) return NULL;
    const TagLib::StringList& list = props[key];
    if (list.isEmpty()) return NULL;
    return CPTL_DupTagString(list.front());
}

// Helper to get first value from multiple possible keys
static char* CPTL_GetPropertyMultiKey(const TagLib::PropertyMap& props, const char* const* keys)
{
    for (int i = 0; keys[i] != NULL; i++)
    {
        char* val = CPTL_GetProperty(props, keys[i]);
        if (val) return val;
    }
    return NULL;
}

// Read ALL metadata in a single file open
BOOL CPTL_ReadAllMetadata(const char* pcFilePath, CPs_AllMetadata* pMetadata)
{
    if (!pcFilePath || !pMetadata)
        return FALSE;
    
    CPTL_InitMetadata(pMetadata);
    
    try
    {
        // Single file open - this is the key optimization
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull())
            return FALSE;
        
        TagLib::File* file = fileRef.file();
        if (!file)
            return FALSE;
        
        TagLib::Tag* tag = fileRef.tag();
        TagLib::PropertyMap properties = file->properties();
        
        //
        // === Basic Tags ===
        //
        if (tag)
        {
            pMetadata->m_pcTitle = CPTL_DupTagString(tag->title());
            pMetadata->m_pcArtist = CPTL_DupTagString(tag->artist());
            pMetadata->m_pcAlbum = CPTL_DupTagString(tag->album());
            pMetadata->m_pcComment = CPTL_DupTagString(tag->comment());
            pMetadata->m_pcGenre = CPTL_DupTagString(tag->genre());
            
            unsigned int year = tag->year();
            if (year > 0)
            {
                char yearBuf[16];
                sprintf_s(yearBuf, sizeof(yearBuf), "%u", year);
                pMetadata->m_pcYear = _strdup(yearBuf);
            }
            
            pMetadata->m_iTrackNum = tag->track();
            
            // Determine tag type
            if (pMetadata->m_pcTitle || pMetadata->m_pcArtist || pMetadata->m_pcAlbum ||
                pMetadata->m_pcYear || pMetadata->m_pcComment || pMetadata->m_pcGenre ||
                pMetadata->m_iTrackNum > 0)
            {
                pMetadata->m_iTagType = 2; // ttID3v2 (generic "has tags")
            }
            
            pMetadata->m_bHasBasicTags = TRUE;
        }
        
        //
        // === Audio Properties ===
        //
        TagLib::AudioProperties* props = fileRef.audioProperties();
        if (props)
        {
            pMetadata->m_iLength = props->lengthInSeconds();
            pMetadata->m_iBitrate = props->bitrate();
            pMetadata->m_iSampleRate = props->sampleRate();
            pMetadata->m_cChannels = (unsigned char)props->channels();
            
            // Determine codec and bitrate mode from file type
            const char* codecName = "Unknown";
            const char* bitrateMode = "Unknown";
            
            if (dynamic_cast<TagLib::MPEG::File*>(file))
            {
                codecName = "MP3";
                bitrateMode = "CBR";
                pMetadata->m_iBitDepth = 16;
                
                TagLib::MPEG::File* mpegFile = dynamic_cast<TagLib::MPEG::File*>(file);
                (void)mpegFile; // May be used for VBR detection later
            }
            else if (dynamic_cast<TagLib::FLAC::File*>(file))
            {
                codecName = "FLAC";
                bitrateMode = "VBR";
                TagLib::FLAC::File* flacFile = dynamic_cast<TagLib::FLAC::File*>(file);
                if (flacFile && flacFile->audioProperties())
                {
                    pMetadata->m_iBitDepth = flacFile->audioProperties()->bitsPerSample();
                }
            }
            else if (dynamic_cast<TagLib::Ogg::Vorbis::File*>(file))
            {
                codecName = "Vorbis";
                bitrateMode = "VBR";
                pMetadata->m_iBitDepth = 16;
            }
            else if (dynamic_cast<TagLib::MP4::File*>(file))
            {
                codecName = "AAC";
                bitrateMode = "VBR";
                pMetadata->m_iBitDepth = 16;
            }
            else
            {
                // Determine from extension
                const char* ext = strrchr(pcFilePath, '.');
                if (ext)
                {
                    ext++;
                    if (_stricmp(ext, "wav") == 0 || _stricmp(ext, "wave") == 0)
                    {
                        codecName = "WAV/PCM";
                        bitrateMode = "CBR";
                        pMetadata->m_iBitDepth = 16;
                    }
                    else if (_stricmp(ext, "aiff") == 0 || _stricmp(ext, "aif") == 0)
                    {
                        codecName = "AIFF";
                        bitrateMode = "CBR";
                        pMetadata->m_iBitDepth = 16;
                    }
                }
            }
            
            pMetadata->m_pcCodec = _strdup(codecName);
            pMetadata->m_pcBitrateMode = _strdup(bitrateMode);
            pMetadata->m_bHasAudioProperties = TRUE;
        }
        
        // Get file size
        struct _stat64 fileStat;
        if (_stat64(pcFilePath, &fileStat) == 0)
        {
            pMetadata->m_iFileSize = (unsigned int)fileStat.st_size;
        }
        
        //
        // === Extended Metadata (from PropertyMap) ===
        //
        pMetadata->m_pcComposer = CPTL_GetProperty(properties, "COMPOSER");
        pMetadata->m_pcAlbumArtist = CPTL_GetProperty(properties, "ALBUMARTIST");
        pMetadata->m_pcGrouping = CPTL_GetProperty(properties, "GROUPING");
        pMetadata->m_pcCopyright = CPTL_GetProperty(properties, "COPYRIGHT");
        pMetadata->m_pcLyrics = CPTL_GetProperty(properties, "LYRICS");
        
        // Disc Number
        if (properties.contains("DISCNUMBER"))
        {
            std::string discStr = properties["DISCNUMBER"].front().to8Bit(true);
            int discNum = atoi(discStr.c_str());
            if (discNum > 0 && discNum <= 65535)
                pMetadata->m_iDiscNumber = (unsigned short)discNum;
        }
        
        // BPM
        if (properties.contains("BPM"))
        {
            std::string bpmStr = properties["BPM"].front().to8Bit(true);
            int bpm = atoi(bpmStr.c_str());
            if (bpm > 0 && bpm <= 65535)
                pMetadata->m_iBPM = (unsigned short)bpm;
        }
        
        if (pMetadata->m_pcComposer || pMetadata->m_pcAlbumArtist || pMetadata->m_pcGrouping ||
            pMetadata->m_pcCopyright || pMetadata->m_pcLyrics || pMetadata->m_iDiscNumber > 0 ||
            pMetadata->m_iBPM > 0)
        {
            pMetadata->m_bHasExtendedTags = TRUE;
        }
        
        //
        // === ReplayGain ===
        //
        if (properties.contains("REPLAYGAIN_TRACK_GAIN"))
        {
            std::string gainStr = properties["REPLAYGAIN_TRACK_GAIN"].front().to8Bit(true);
            pMetadata->m_fTrackGain = (float)atof(gainStr.c_str());
            pMetadata->m_bHasReplayGain = TRUE;
        }
        if (properties.contains("REPLAYGAIN_TRACK_PEAK"))
        {
            std::string peakStr = properties["REPLAYGAIN_TRACK_PEAK"].front().to8Bit(true);
            pMetadata->m_fTrackPeak = (float)atof(peakStr.c_str());
            pMetadata->m_bHasReplayGain = TRUE;
        }
        if (properties.contains("REPLAYGAIN_ALBUM_GAIN"))
        {
            std::string gainStr = properties["REPLAYGAIN_ALBUM_GAIN"].front().to8Bit(true);
            pMetadata->m_fAlbumGain = (float)atof(gainStr.c_str());
            pMetadata->m_bHasReplayGain = TRUE;
        }
        if (properties.contains("REPLAYGAIN_ALBUM_PEAK"))
        {
            std::string peakStr = properties["REPLAYGAIN_ALBUM_PEAK"].front().to8Bit(true);
            pMetadata->m_fAlbumPeak = (float)atof(peakStr.c_str());
            pMetadata->m_bHasReplayGain = TRUE;
        }
        
        //
        // === Multiple Artists ===
        //
        if (properties.contains("ARTISTS"))
        {
            std::string artistsStr = properties["ARTISTS"].toString("; ").toCString(true);
            pMetadata->m_pcArtists = _strdup(artistsStr.c_str());
            pMetadata->m_bHasMultipleArtists = TRUE;
        }
        
        static const char* featuredTags[] = {"PERFORMER", "INVOLVEDPEOPLE", "FEATURED", NULL};
        pMetadata->m_pcFeaturedArtist = CPTL_GetPropertyMultiKey(properties, featuredTags);
        if (pMetadata->m_pcFeaturedArtist) pMetadata->m_bHasMultipleArtists = TRUE;
        
        static const char* remixerTags[] = {"REMIXER", "MIXARTIST", "MODIFIEDBY", NULL};
        pMetadata->m_pcRemixer = CPTL_GetPropertyMultiKey(properties, remixerTags);
        if (pMetadata->m_pcRemixer) pMetadata->m_bHasMultipleArtists = TRUE;
        
        //
        // === MusicBrainz IDs ===
        //
        static const char* trackTags[] = {"MUSICBRAINZ_TRACKID", "MUSICBRAINZ TRACK ID", NULL};
        pMetadata->m_pcMB_TrackID = CPTL_GetPropertyMultiKey(properties, trackTags);
        
        static const char* releaseTags[] = {"MUSICBRAINZ_ALBUMID", "MUSICBRAINZ ALBUM ID", NULL};
        pMetadata->m_pcMB_ReleaseID = CPTL_GetPropertyMultiKey(properties, releaseTags);
        
        static const char* artistTags[] = {"MUSICBRAINZ_ARTISTID", "MUSICBRAINZ ARTIST ID", NULL};
        pMetadata->m_pcMB_ArtistID = CPTL_GetPropertyMultiKey(properties, artistTags);
        
        static const char* albumArtistTags[] = {"MUSICBRAINZ_ALBUMARTISTID", "MUSICBRAINZ ALBUM ARTIST ID", NULL};
        pMetadata->m_pcMB_AlbumArtistID = CPTL_GetPropertyMultiKey(properties, albumArtistTags);
        
        static const char* releaseGroupTags[] = {"MUSICBRAINZ_RELEASEGROUPID", "MUSICBRAINZ RELEASE GROUP ID", NULL};
        pMetadata->m_pcMB_ReleaseGroupID = CPTL_GetPropertyMultiKey(properties, releaseGroupTags);
        
        if (pMetadata->m_pcMB_TrackID || pMetadata->m_pcMB_ReleaseID || pMetadata->m_pcMB_ArtistID ||
            pMetadata->m_pcMB_AlbumArtistID || pMetadata->m_pcMB_ReleaseGroupID)
        {
            pMetadata->m_bHasMusicBrainzIDs = TRUE;
        }
        
        return TRUE;
    }
    catch (...)
    {
        CPTL_FreeMetadata(pMetadata);
        return FALSE;
    }
}

// Read BASIC metadata only - for fast initial playlist loading
// Only reads: title, artist, album, year, comment, genre, track#, length, audio properties
BOOL CPTL_ReadBasicMetadataOnly(const char* pcFilePath, CPs_AllMetadata* pMetadata)
{
    if (!pcFilePath || !pMetadata)
        return FALSE;
    
    CPTL_InitMetadata(pMetadata);
    
    try
    {
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull())
            return FALSE;
        
        TagLib::File* file = fileRef.file();
        if (!file)
            return FALSE;
        
        TagLib::Tag* tag = fileRef.tag();
        
        // Basic Tags
        if (tag)
        {
            pMetadata->m_pcTitle = CPTL_DupTagString(tag->title());
            pMetadata->m_pcArtist = CPTL_DupTagString(tag->artist());
            pMetadata->m_pcAlbum = CPTL_DupTagString(tag->album());
            pMetadata->m_pcComment = CPTL_DupTagString(tag->comment());
            pMetadata->m_pcGenre = CPTL_DupTagString(tag->genre());
            
            unsigned int year = tag->year();
            if (year > 0)
            {
                char yearBuf[16];
                sprintf_s(yearBuf, sizeof(yearBuf), "%u", year);
                pMetadata->m_pcYear = _strdup(yearBuf);
            }
            
            pMetadata->m_iTrackNum = tag->track();
            
            if (pMetadata->m_pcTitle || pMetadata->m_pcArtist || pMetadata->m_pcAlbum ||
                pMetadata->m_pcYear || pMetadata->m_pcComment || pMetadata->m_pcGenre ||
                pMetadata->m_iTrackNum > 0)
            {
                pMetadata->m_iTagType = 2;
            }
            
            pMetadata->m_bHasBasicTags = TRUE;
        }
        
        // Audio Properties (essential for display)
        TagLib::AudioProperties* props = fileRef.audioProperties();
        if (props)
        {
            pMetadata->m_iLength = props->lengthInSeconds();
            pMetadata->m_iBitrate = props->bitrate();
            pMetadata->m_iSampleRate = props->sampleRate();
            pMetadata->m_cChannels = (unsigned char)props->channels();
            
            // Codec detection (minimal)
            const char* codecName = "Unknown";
            if (dynamic_cast<TagLib::MPEG::File*>(file)) codecName = "MP3";
            else if (dynamic_cast<TagLib::FLAC::File*>(file)) codecName = "FLAC";
            else if (dynamic_cast<TagLib::Ogg::Vorbis::File*>(file)) codecName = "Vorbis";
            else if (dynamic_cast<TagLib::MP4::File*>(file)) codecName = "AAC";
            
            pMetadata->m_pcCodec = _strdup(codecName);
            pMetadata->m_bHasAudioProperties = TRUE;
        }
        
        // File size
        struct _stat64 fileStat;
        if (_stat64(pcFilePath, &fileStat) == 0)
        {
            pMetadata->m_iFileSize = (unsigned int)fileStat.st_size;
        }
        
        return TRUE;
    }
    catch (...)
    {
        CPTL_FreeMetadata(pMetadata);
        return FALSE;
    }
}

// Read EXTENDED metadata only - for lazy loading when user views details
// Reads: composer, album artist, grouping, copyright, lyrics, disc#, BPM, 
//        ReplayGain, multiple artists, MusicBrainz IDs, detailed audio props
BOOL CPTL_ReadExtendedMetadataOnly(const char* pcFilePath, CPs_AllMetadata* pMetadata)
{
    if (!pcFilePath || !pMetadata)
        return FALSE;
    
    try
    {
        TagLib::FileRef fileRef(pcFilePath);
        if (fileRef.isNull())
            return FALSE;
        
        TagLib::File* file = fileRef.file();
        if (!file)
            return FALSE;
        
        TagLib::PropertyMap properties = file->properties();
        
        // Extended Metadata
        pMetadata->m_pcComposer = CPTL_GetProperty(properties, "COMPOSER");
        pMetadata->m_pcAlbumArtist = CPTL_GetProperty(properties, "ALBUMARTIST");
        pMetadata->m_pcGrouping = CPTL_GetProperty(properties, "GROUPING");
        pMetadata->m_pcCopyright = CPTL_GetProperty(properties, "COPYRIGHT");
        pMetadata->m_pcLyrics = CPTL_GetProperty(properties, "LYRICS");
        
        if (properties.contains("DISCNUMBER"))
        {
            std::string discStr = properties["DISCNUMBER"].front().to8Bit(true);
            int discNum = atoi(discStr.c_str());
            if (discNum > 0 && discNum <= 65535)
                pMetadata->m_iDiscNumber = (unsigned short)discNum;
        }
        
        if (properties.contains("BPM"))
        {
            std::string bpmStr = properties["BPM"].front().to8Bit(true);
            int bpm = atoi(bpmStr.c_str());
            if (bpm > 0 && bpm <= 65535)
                pMetadata->m_iBPM = (unsigned short)bpm;
        }
        
        if (pMetadata->m_pcComposer || pMetadata->m_pcAlbumArtist || pMetadata->m_pcGrouping ||
            pMetadata->m_pcCopyright || pMetadata->m_pcLyrics || pMetadata->m_iDiscNumber > 0 ||
            pMetadata->m_iBPM > 0)
        {
            pMetadata->m_bHasExtendedTags = TRUE;
        }
        
        // ReplayGain
        if (properties.contains("REPLAYGAIN_TRACK_GAIN"))
        {
            std::string gainStr = properties["REPLAYGAIN_TRACK_GAIN"].front().to8Bit(true);
            pMetadata->m_fTrackGain = (float)atof(gainStr.c_str());
            pMetadata->m_bHasReplayGain = TRUE;
        }
        if (properties.contains("REPLAYGAIN_TRACK_PEAK"))
        {
            std::string peakStr = properties["REPLAYGAIN_TRACK_PEAK"].front().to8Bit(true);
            pMetadata->m_fTrackPeak = (float)atof(peakStr.c_str());
            pMetadata->m_bHasReplayGain = TRUE;
        }
        if (properties.contains("REPLAYGAIN_ALBUM_GAIN"))
        {
            std::string gainStr = properties["REPLAYGAIN_ALBUM_GAIN"].front().to8Bit(true);
            pMetadata->m_fAlbumGain = (float)atof(gainStr.c_str());
            pMetadata->m_bHasReplayGain = TRUE;
        }
        if (properties.contains("REPLAYGAIN_ALBUM_PEAK"))
        {
            std::string peakStr = properties["REPLAYGAIN_ALBUM_PEAK"].front().to8Bit(true);
            pMetadata->m_fAlbumPeak = (float)atof(peakStr.c_str());
            pMetadata->m_bHasReplayGain = TRUE;
        }
        
        // Multiple Artists
        if (properties.contains("ARTISTS"))
        {
            std::string artistsStr = properties["ARTISTS"].toString("; ").toCString(true);
            pMetadata->m_pcArtists = _strdup(artistsStr.c_str());
            pMetadata->m_bHasMultipleArtists = TRUE;
        }
        
        static const char* featuredTags[] = {"PERFORMER", "INVOLVEDPEOPLE", "FEATURED", NULL};
        pMetadata->m_pcFeaturedArtist = CPTL_GetPropertyMultiKey(properties, featuredTags);
        if (pMetadata->m_pcFeaturedArtist) pMetadata->m_bHasMultipleArtists = TRUE;
        
        static const char* remixerTags[] = {"REMIXER", "MIXARTIST", "MODIFIEDBY", NULL};
        pMetadata->m_pcRemixer = CPTL_GetPropertyMultiKey(properties, remixerTags);
        if (pMetadata->m_pcRemixer) pMetadata->m_bHasMultipleArtists = TRUE;
        
        // MusicBrainz IDs
        static const char* trackTags[] = {"MUSICBRAINZ_TRACKID", "MUSICBRAINZ TRACK ID", NULL};
        pMetadata->m_pcMB_TrackID = CPTL_GetPropertyMultiKey(properties, trackTags);
        
        static const char* releaseTags[] = {"MUSICBRAINZ_ALBUMID", "MUSICBRAINZ ALBUM ID", NULL};
        pMetadata->m_pcMB_ReleaseID = CPTL_GetPropertyMultiKey(properties, releaseTags);
        
        static const char* artistTags[] = {"MUSICBRAINZ_ARTISTID", "MUSICBRAINZ ARTIST ID", NULL};
        pMetadata->m_pcMB_ArtistID = CPTL_GetPropertyMultiKey(properties, artistTags);
        
        static const char* albumArtistTags[] = {"MUSICBRAINZ_ALBUMARTISTID", "MUSICBRAINZ ALBUM ARTIST ID", NULL};
        pMetadata->m_pcMB_AlbumArtistID = CPTL_GetPropertyMultiKey(properties, albumArtistTags);
        
        static const char* releaseGroupTags[] = {"MUSICBRAINZ_RELEASEGROUPID", "MUSICBRAINZ RELEASE GROUP ID", NULL};
        pMetadata->m_pcMB_ReleaseGroupID = CPTL_GetPropertyMultiKey(properties, releaseGroupTags);
        
        if (pMetadata->m_pcMB_TrackID || pMetadata->m_pcMB_ReleaseID || pMetadata->m_pcMB_ArtistID ||
            pMetadata->m_pcMB_AlbumArtistID || pMetadata->m_pcMB_ReleaseGroupID)
        {
            pMetadata->m_bHasMusicBrainzIDs = TRUE;
        }
        
        // Extended audio properties (bit depth, bitrate mode)
        if (dynamic_cast<TagLib::FLAC::File*>(file))
        {
            TagLib::FLAC::File* flacFile = dynamic_cast<TagLib::FLAC::File*>(file);
            if (flacFile && flacFile->audioProperties())
            {
                pMetadata->m_iBitDepth = flacFile->audioProperties()->bitsPerSample();
                pMetadata->m_pcBitrateMode = _strdup("VBR");
            }
        }
        else if (dynamic_cast<TagLib::MPEG::File*>(file))
        {
            pMetadata->m_iBitDepth = 16;
            pMetadata->m_pcBitrateMode = _strdup("CBR");
        }
        else
        {
            pMetadata->m_iBitDepth = 16;
            pMetadata->m_pcBitrateMode = _strdup("Unknown");
        }
        
        return TRUE;
    }
    catch (...)
    {
        return FALSE;
    }
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

// Case-insensitive hash for genre names
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

// Initialize genre hash table (called lazily on first use)
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
        g_GenreEntries[i].m_iIndex = i;
        
        hash = CPTL_HashGenre(glb_pcGenres[i]);
        g_GenreEntries[i].m_pNext = g_pGenreHashTable[hash];
        g_pGenreHashTable[hash] = &g_GenreEntries[i];
    }
    
    g_bGenreHashInitialized = TRUE;
}

int CPTL_GetGenreIndex(const char* pcGenre)
{
    unsigned int hash;
    CPs_GenreHashEntry* pEntry;
    
    if (!pcGenre)
        return -1;
    
    // Initialize hash table on first use
    if (!g_bGenreHashInitialized)
        CPTL_InitGenreHash();
    
    // O(1) hash table lookup
    hash = CPTL_HashGenre(pcGenre);
    pEntry = g_pGenreHashTable[hash];
    
    while (pEntry)
    {
        if (stricmp(pcGenre, pEntry->m_pcGenre) == 0)
            return pEntry->m_iIndex;
        pEntry = pEntry->m_pNext;
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
            snprintf(discStr, sizeof(discStr), "%u", iDiscNumber);
            properties.replace("DISCNUMBER", TagLib::String(discStr, TagLib::String::UTF8));
        }
        
        // Set BPM
        if (iBPM > 0)
        {
            char bpmStr[16];
            snprintf(bpmStr, sizeof(bpmStr), "%u", iBPM);
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
            snprintf(gainStr, sizeof(gainStr), "%.2f dB", fTrackGain);
            properties.replace("REPLAYGAIN_TRACK_GAIN", TagLib::String(gainStr, TagLib::String::UTF8));
        }
        
        // Set REPLAYGAIN_TRACK_PEAK (format: "0.XXXXXX")
        if (fTrackPeak != 0.0f)
        {
            char peakStr[32];
            snprintf(peakStr, sizeof(peakStr), "%.6f", fTrackPeak);
            properties.replace("REPLAYGAIN_TRACK_PEAK", TagLib::String(peakStr, TagLib::String::UTF8));
        }
        
        // Set REPLAYGAIN_ALBUM_GAIN
        if (fAlbumGain != 0.0f)
        {
            char gainStr[32];
            snprintf(gainStr, sizeof(gainStr), "%.2f dB", fAlbumGain);
            properties.replace("REPLAYGAIN_ALBUM_GAIN", TagLib::String(gainStr, TagLib::String::UTF8));
        }
        
        // Set REPLAYGAIN_ALBUM_PEAK
        if (fAlbumPeak != 0.0f)
        {
            char peakStr[32];
            snprintf(peakStr, sizeof(peakStr), "%.6f", fAlbumPeak);
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
    struct _CPs_AlbumArtCacheEntry* m_pNext;       // Next in hash bucket
    struct _CPs_AlbumArtCacheEntry* m_pLRUNext;    // LRU linked list
    struct _CPs_AlbumArtCacheEntry* m_pLRUPrev;    // LRU linked list
} CPs_AlbumArtCacheEntry;

// Hash table for O(1) cache lookup
#define ALBUMART_HASH_BUCKETS 64

static CPs_AlbumArtCacheEntry* g_pAlbumArtHashTable[ALBUMART_HASH_BUCKETS] = {0};
static CPs_AlbumArtCacheEntry* g_pLRUHead = NULL;  // Most recently used
static CPs_AlbumArtCacheEntry* g_pLRUTail = NULL;  // Least recently used
static int g_iCacheEntryCount = 0;
static unsigned int g_iTotalCacheMemory = 0;

// Simple hash function for file paths (case-insensitive)
static unsigned int CPTL_HashFilePath(const char* pcFilePath)
{
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*pcFilePath++) != 0)
    {
        // Case-insensitive hash
        if (c >= 'A' && c <= 'Z') c += 32;
        hash = ((hash << 5) + hash) + c;
    }
    return hash % ALBUMART_HASH_BUCKETS;
}

// Move entry to front of LRU list (most recently used)
static void CPTL_MoveToLRUFront(CPs_AlbumArtCacheEntry* pEntry)
{
    if (!pEntry || pEntry == g_pLRUHead)
        return;
    
    // Remove from current position
    if (pEntry->m_pLRUPrev)
        pEntry->m_pLRUPrev->m_pLRUNext = pEntry->m_pLRUNext;
    if (pEntry->m_pLRUNext)
        pEntry->m_pLRUNext->m_pLRUPrev = pEntry->m_pLRUPrev;
    
    // Update tail if needed
    if (pEntry == g_pLRUTail)
        g_pLRUTail = pEntry->m_pLRUPrev;
    
    // Insert at front
    pEntry->m_pLRUPrev = NULL;
    pEntry->m_pLRUNext = g_pLRUHead;
    if (g_pLRUHead)
        g_pLRUHead->m_pLRUPrev = pEntry;
    g_pLRUHead = pEntry;
    
    if (!g_pLRUTail)
        g_pLRUTail = pEntry;
}

// Add entry to LRU list (at front)
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

// Remove entry from LRU list
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

// Initialize album art cache
void CPTL_InitAlbumArtCache(void)
{
    memset(g_pAlbumArtHashTable, 0, sizeof(g_pAlbumArtHashTable));
    g_pLRUHead = NULL;
    g_pLRUTail = NULL;
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

// Find entry in hash table - O(1) average case
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

// Remove specific entry from cache
void CPTL_RemoveFromAlbumArtCache(const char* pcFilePath)
{
    if (!pcFilePath)
        return;
    
    unsigned int hash = CPTL_HashFilePath(pcFilePath);
    CPs_AlbumArtCacheEntry* pCurrent = g_pAlbumArtHashTable[hash];
    CPs_AlbumArtCacheEntry* pPrev = NULL;
    
    while (pCurrent)
    {
        if (pCurrent->m_pcFilePath && stricmp(pCurrent->m_pcFilePath, pcFilePath) == 0)
        {
            // Remove from hash bucket
            if (pPrev)
                pPrev->m_pNext = pCurrent->m_pNext;
            else
                g_pAlbumArtHashTable[hash] = pCurrent->m_pNext;
            
            // Remove from LRU list
            CPTL_RemoveFromLRU(pCurrent);
            
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

// Evict least recently used entry - O(1) with LRU tail pointer
static void CPTL_EvictLRU(void)
{
    CPs_AlbumArtCacheEntry* pOldest = g_pLRUTail;
    if (!pOldest)
        return;
    
    // Remove from hash bucket
    unsigned int hash = CPTL_HashFilePath(pOldest->m_pcFilePath);
    CPs_AlbumArtCacheEntry* pCurrent = g_pAlbumArtHashTable[hash];
    CPs_AlbumArtCacheEntry* pPrev = NULL;
    
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
        pPrev = pCurrent;
        pCurrent = pCurrent->m_pNext;
    }
    
    // Remove from LRU list
    CPTL_RemoveFromLRU(pOldest);
    
    // Free resources
    g_iTotalCacheMemory -= pOldest->m_iMemoryUsed;
    g_iCacheEntryCount--;
    
    if (pOldest->m_pcFilePath)
        free(pOldest->m_pcFilePath);
    if (pOldest->m_hBitmap)
        DeleteObject(pOldest->m_hBitmap);
    free(pOldest);
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
// Get album art from cache or load - O(1) cache lookup via hash table
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
        
    // O(1) hash table lookup
    pEntry = CPTL_FindInCache(pcFilePath, &hash);
    if (pEntry)
    {
        // Found in cache - move to front of LRU (most recently used)
        CPTL_MoveToLRUFront(pEntry);
        pEntry->m_tLastAccess = time(NULL);
        
        if (piActualWidth)
            *piActualWidth = pEntry->m_iWidth;
        if (piActualHeight)
            *piActualHeight = pEntry->m_iHeight;
            
        return pEntry->m_hBitmap;
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
        memset(pNew, 0, sizeof(CPs_AlbumArtCacheEntry));
        pNew->m_pcFilePath = _strdup(pcFilePath);
        pNew->m_hBitmap = hBitmap;
        pNew->m_iWidth = iWidth;
        pNew->m_iHeight = iHeight;
        pNew->m_tLastAccess = time(NULL);
        pNew->m_iMemoryUsed = iMemoryUsed;
        
        // Add to hash table bucket
        pNew->m_pNext = g_pAlbumArtHashTable[hash];
        g_pAlbumArtHashTable[hash] = pNew;
        
        // Add to LRU list (front = most recently used)
        CPTL_AddToLRU(pNew);
        
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
