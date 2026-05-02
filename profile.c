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

#include "stdafx.h"
#include "globals.h"
#include "CP_Config.h"
#include "CPI_Playlist.h"
#include "CPI_PlaylistItem.h"


/* Pre-built key name tables avoid snprintf overhead in the read/write loops.
   Column indices 0-10, EQ bands 1-8. */
static const char* const s_pcColKeys[11][3] = {
	{"PlaylistCol0","PlaylistSeq0","PlaylistVis0"},
	{"PlaylistCol1","PlaylistSeq1","PlaylistVis1"},
	{"PlaylistCol2","PlaylistSeq2","PlaylistVis2"},
	{"PlaylistCol3","PlaylistSeq3","PlaylistVis3"},
	{"PlaylistCol4","PlaylistSeq4","PlaylistVis4"},
	{"PlaylistCol5","PlaylistSeq5","PlaylistVis5"},
	{"PlaylistCol6","PlaylistSeq6","PlaylistVis6"},
	{"PlaylistCol7","PlaylistSeq7","PlaylistVis7"},
	{"PlaylistCol8","PlaylistSeq8","PlaylistVis8"},
	{"PlaylistCol9","PlaylistSeq9","PlaylistVis9"},
	{"PlaylistCol10","PlaylistSeq10","PlaylistVis10"},
};
static const char* const s_pcEqKeys[] = {
	NULL,    /* index 0 = "ActivePreset", handled separately */
	"Eq1","Eq2","Eq3","Eq4","Eq5","Eq6","Eq7","Eq8"
};
static const char* CPL_SkinKey(char *buf, size_t bufSz, int n)
{
	snprintf(buf, bufSz, "SkinFile%d", n);
	return buf;
}


////////////////////////////////////////////////////////////
//
//
//
void    playlist_write_default(void)
{
	char    exepath[MAX_PATH];
	main_get_program_file_path("default.m3u", exepath, MAX_PATH);
	CPL_ExportPlaylist(globals.m_hPlaylist, exepath);
}

//
//
//
void    options_read(void)
{
	int     teller;
	int     widths[] = {   20,   200,  200,  200,  50,   70,   70,    100,  100,   100, 80};
	int     visibles[] = { FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, TRUE };
	int     sequences[] = { 5,    1,    2,    3,    4,    6,    7,     8,    9,     0,   10};
	int iColIDX;
	
	for (iColIDX = PLAYLIST_first; iColIDX <= PLAYLIST_last; iColIDX++)
	{
		options.playlist_column_widths[iColIDX]  = CPConfig_GetInt("WindowPos", s_pcColKeys[iColIDX][0], widths[iColIDX]);
		options.playlist_column_seq[iColIDX]     = CPConfig_GetInt("WindowPos", s_pcColKeys[iColIDX][1], sequences[iColIDX]);
		options.playlist_column_visible[iColIDX] = CPConfig_GetInt("WindowPos", s_pcColKeys[iColIDX][2], visibles[iColIDX]) ? TRUE : FALSE;
	}
	
	options.main_window_pos.x = CPConfig_GetInt("WindowPos", "WindowX", 100);
	if (options.main_window_pos.x < -10)
		options.main_window_pos.x = 100;
		
	options.main_window_pos.y = CPConfig_GetInt("WindowPos", "WindowY", 100);
	if (options.main_window_pos.y < -10)
		options.main_window_pos.y = 100;
		
	options.playlist_window_pos.left = CPConfig_GetInt("WindowPos", "PlaylistX", 350);
	if (options.playlist_window_pos.left < -10)
		options.playlist_window_pos.left = 350;
		
	options.playlist_window_pos.top = CPConfig_GetInt("WindowPos", "PlaylistY", 150);
	if (options.playlist_window_pos.top < -10)
		options.playlist_window_pos.top = 150;
		
	options.playlist_window_pos.right = CPConfig_GetInt("WindowPos", "PlaylistW", 650)
		+ options.playlist_window_pos.left;
	options.playlist_window_pos.bottom = CPConfig_GetInt("WindowPos", "PlaylistH", 500)
		+ options.playlist_window_pos.top;
	
	CPConfig_GetString("LastDirectory", "Directory", "", options.last_used_directory, MAX_PATH);
	
	options.repeat_playlist = CPConfig_GetInt("Misc", "Repeat", 0);
	options.shuffle_play = CPConfig_GetInt("Misc", "Shuffle", 0);
	options.always_on_top = CPConfig_GetInt("Misc", "Ontop", 0);
	options.auto_exit_after_playing = CPConfig_GetInt("Misc", "Autoexit", 0);
	options.remember_playlist = CPConfig_GetInt("Misc", "Rememberpls", 1);
	options.show_remaining_time = CPConfig_GetInt("Misc", "Remaining", 0);
	options.read_id3_tag = CPConfig_GetInt("Misc", "ReadID3tag", 1);
	options.support_id3v2 = CPConfig_GetInt("Misc", "SuportID3v2", 1);
	options.prefer_native_ogg_tags = CPConfig_GetInt("Misc", "PreferNativeOGGtags", 1);
	options.read_id3_tag_in_background = CPConfig_GetInt("Misc", "BackgroundReadID3", 1);
	options.work_out_track_lengths = CPConfig_GetInt("Misc", "WorkOutTrackLengths", 1);
	options.allow_multiple_instances = CPConfig_GetInt("Misc", "AllowMultipleInstances", 0);
	options.read_id3_tag_of_selected = CPConfig_GetInt("Misc", "ReadSelID3tag", 1);
	options.seconds_delay_after_track = CPConfig_GetInt("Misc", "DelayTime", 0);
	options.decoder_output_mode = CPConfig_GetInt("Misc", "Outputmode", 1);
	
	CPConfig_GetString("Misc", "Language", "", options.preferred_language,
	                   sizeof(options.preferred_language));
	
	options.easy_move = CPConfig_GetInt("Misc", "Easymove", 1);
	options.remember_skin_count = CPConfig_GetInt("Misc", "RememberSkins", 4);
	options.allow_file_once_in_playlist = CPConfig_GetInt("Misc", "Fileonce", 1);
	options.auto_play_when_started = CPConfig_GetInt("Misc", "Autoplay", 0);
	options.show_on_taskbar = CPConfig_GetInt("Misc", "TaskBar", 1);
	options.show_playlist = CPConfig_GetInt("Misc", "ShowPlaylist", 0);
	options.rotate_systray_icon = CPConfig_GetInt("Misc", "RotateIcon", 1);
	options.scroll_track_title = CPConfig_GetInt("Misc", "Scrolltitle", 1);
	options.sticky_windows = CPConfig_GetInt("Misc", "StickyWindows", 1);
	options.discord_rpc_enabled = CPConfig_GetInt("Misc", "DiscordRPC", 1);
	options.replaygain_mode = CPConfig_GetInt("Misc", "ReplayGainMode", 0);
	options.replaygain_preamp_db = CPConfig_GetInt("Misc", "ReplayGainPreamp", 0);
	options.replaygain_prevent_clipping = CPConfig_GetInt("Misc", "ReplayGainNoClip", 1);
	options.gapless_playback = CPConfig_GetInt("Misc", "GaplessPlayback", 1);

	CPConfig_GetString("Misc", "RememberLastSong", "", options.initial_file, MAX_PATH);
	if (*options.initial_file)
		options.remember_last_played_track = TRUE;
		
	options.last_selected_skin_number = CPConfig_GetInt("Skin", "LastSkin", 0);
	options.use_default_skin = CPConfig_GetInt("Skin", "UseDefault", 1);
	options.use_playlist_skin = CPConfig_GetInt("Skin", "Useplaylistskin", 0);
	globals.builtin_skin_variant = (BuiltinSkinVariant)CPConfig_GetInt("Skin", "BuiltinVariant", BUILTIN_SKIN_NORMAL);
	if (globals.builtin_skin_variant < BUILTIN_SKIN_NORMAL || globals.builtin_skin_variant >= BUILTIN_SKIN_COUNT)
		globals.builtin_skin_variant = BUILTIN_SKIN_NORMAL;
	
	{
		int     teller;
		
		for (teller = MENU_SKIN_DEFAULT + 1; teller < MENU_SKIN_DEFAULT + 1 + options.remember_skin_count;
				teller++)
		{
			char    SkinFileString[MAX_PATH];
			char    skinpath[MAX_PATH];
			CPL_SkinKey(SkinFileString, sizeof(SkinFileString), teller - MENU_SKIN_DEFAULT);
			CPConfig_GetString("Skin", SkinFileString, "", skinpath, MAX_PATH);
			
			if (*skinpath != 0)
			{
				main_skin_add_to_menu(skinpath);
				
				if (options.last_selected_skin_number == teller - MENU_SKIN_DEFAULT)
				{
					strcpy_s((char*)options.main_skin_file, sizeof(options.main_skin_file), skinpath);
				}
			}
		}
	}
	
	CPConfig_GetString("Skin", "PlaylistSkin", "",
	                   (char*)options.playlist_skin_file, MAX_PATH);
	options.equalizer = CPConfig_GetInt("Equalizer", "Active", 0);
	options.eq_settings[0] = CPConfig_GetInt("Equalizer", "ActivePreset", -1);

	for (teller = 1; teller < (int)ARRAY_SIZE(options.eq_settings); teller++)
	{
		options.eq_settings[teller] = CPConfig_GetInt("Equalizer", s_pcEqKeys[teller], 0);
	}
	
	// Read quick find defaults
	{
		char pcQuickFindOption[2];
		CPConfig_GetString("Misc", "QuickFindSearchTerm", "T", pcQuickFindOption, 2);
		
		if (pcQuickFindOption[0] == 'M' || pcQuickFindOption[0] == 'm')
			options.m_enQuickFindTerm = qftAlbum;
		else if (pcQuickFindOption[0] == 'A' || pcQuickFindOption[0] == 'a')
			options.m_enQuickFindTerm = qftArtist;
		else
			options.m_enQuickFindTerm = qftTitle;
	}
	
	// Read mixer mode
	{
		char cMixerMode[32];
		CPConfig_GetString("Mixer", "Mode", "internal", cMixerMode, 32);
		
		if (stricmp(cMixerMode, "wave") == 0)
			globals.m_enMixerMode = mmWaveVolume;
		else if (stricmp(cMixerMode, "internal") == 0)
			globals.m_enMixerMode = mmInternal;
		else
			globals.m_enMixerMode = mmMasterVolume;
			
		globals.m_iVolume = CPConfig_GetInt("Mixer", "InternalVolume", 60);
	}
}

void    options_write(void)
{
	int     teller;
	int iColIDX;
	
	for (iColIDX = PLAYLIST_first; iColIDX <= PLAYLIST_last; iColIDX++)
	{
		CPConfig_SetInt( "WindowPos", s_pcColKeys[iColIDX][0], options.playlist_column_widths[iColIDX]);
		CPConfig_SetInt( "WindowPos", s_pcColKeys[iColIDX][1], options.playlist_column_seq[iColIDX]);
		CPConfig_SetBool("WindowPos", s_pcColKeys[iColIDX][2], options.playlist_column_visible[iColIDX]);
	}
	
	CPConfig_SetInt("WindowPos", "WindowX", options.main_window_pos.x);
	CPConfig_SetInt("WindowPos", "WindowY", options.main_window_pos.y);
	CPConfig_SetInt("WindowPos", "PlaylistX", options.playlist_window_pos.left);
	CPConfig_SetInt("WindowPos", "PlaylistY", options.playlist_window_pos.top);
	CPConfig_SetInt("WindowPos", "PlaylistW", options.playlist_window_pos.right - options.playlist_window_pos.left);
	CPConfig_SetInt("WindowPos", "PlaylistH", options.playlist_window_pos.bottom - options.playlist_window_pos.top);
	
	CPConfig_SetString("LastDirectory", "Directory", options.last_used_directory);
	CPConfig_SetString("Skin", "PlaylistSkin", (char*)options.playlist_skin_file);
	
	{
		int     teller;
		int     profileteller = 1;
		char    SkinFileString[MAX_PATH];
		
		for (teller = MENU_SKIN_DEFAULT + 1; teller < MENU_SKIN_DEFAULT + 1 + options.remember_skin_count;
				teller++)
		{
		
			if (GetMenuString
					(globals.main_menu_popup, teller, (char*)options.main_skin_file,
					 MAX_PATH, MF_BYCOMMAND))
			{
			
				if (GetMenuState
						(globals.main_menu_popup, teller,
						 MF_BYCOMMAND) & MF_CHECKED)
				{
					options.last_selected_skin_number = profileteller;
				}
				
				CPL_SkinKey(SkinFileString, sizeof(SkinFileString), profileteller++);
				CPConfig_SetString("Skin", SkinFileString, (char*)options.main_skin_file);
			}
			else
			{
				CPL_SkinKey(SkinFileString, sizeof(SkinFileString), profileteller++);
				CPConfig_SetString("Skin", SkinFileString, NULL);
			}
		}
	}
	
	CPConfig_SetInt("Skin", "LastSkin", options.last_selected_skin_number);
	CPConfig_SetInt("Skin", "UsePlaylistSkin", options.use_playlist_skin);
	CPConfig_SetInt("Skin", "UseDefault", options.use_default_skin);
	CPConfig_SetInt("Skin", "BuiltinVariant", (int)globals.builtin_skin_variant);
	CPConfig_SetInt("Misc", "Repeat", options.repeat_playlist);
	CPConfig_SetInt("Misc", "Shuffle", options.shuffle_play);
	CPConfig_SetInt("Misc", "Easymove", options.easy_move);
	CPConfig_SetInt("Misc", "RotateIcon", options.rotate_systray_icon);
	CPConfig_SetInt("Misc", "Ontop", options.always_on_top);
	CPConfig_SetInt("Misc", "Autoexit", options.auto_exit_after_playing);
	CPConfig_SetInt("Misc", "Rememberpls", options.remember_playlist);
	CPConfig_SetInt("Misc", "Remaining", options.show_remaining_time);
	CPConfig_SetInt("Misc", "ReadID3tag", options.read_id3_tag);
	CPConfig_SetInt("Misc", "ReadSelID3tag", options.read_id3_tag_of_selected);
	CPConfig_SetInt("Misc", "SuportID3v2", options.support_id3v2);
	CPConfig_SetInt("Misc", "PreferNativeOGGtags", options.prefer_native_ogg_tags);
	CPConfig_SetInt("Misc", "BackgroundReadID3", options.read_id3_tag_in_background);
	CPConfig_SetInt("Misc", "WorkOutTrackLengths", options.work_out_track_lengths);
	CPConfig_SetInt("Misc", "AllowMultipleInstances", options.allow_multiple_instances);
	CPConfig_SetInt("Misc", "Outputmode", options.decoder_output_mode);
	CPConfig_SetString("Misc", "Language", options.preferred_language);
	CPConfig_SetInt("Misc", "Scrolltitle", options.scroll_track_title);
	CPConfig_SetInt("Misc", "StickyWindows", options.sticky_windows);
	CPConfig_SetInt("Misc", "DiscordRPC", options.discord_rpc_enabled);
	CPConfig_SetInt("Misc", "ReplayGainMode", options.replaygain_mode);
	CPConfig_SetInt("Misc", "ReplayGainPreamp", options.replaygain_preamp_db);
	CPConfig_SetInt("Misc", "ReplayGainNoClip", options.replaygain_prevent_clipping);
	CPConfig_SetInt("Misc", "GaplessPlayback", options.gapless_playback);
	CPConfig_SetInt("Misc", "ShowPlaylist", options.show_playlist);
	
	CPConfig_SetString("Misc", "RememberLastSong",
	                   options.remember_last_played_track ? options.initial_file : "");
	
	CPConfig_SetInt("Misc", "Fileonce", options.allow_file_once_in_playlist);
	CPConfig_SetInt("Misc", "Autoplay", options.auto_play_when_started);
	CPConfig_SetInt("Misc", "TaskBar", options.show_on_taskbar);
	CPConfig_SetInt("Misc", "DelayTime", options.seconds_delay_after_track);
	CPConfig_SetInt("Misc", "RememberSkins", options.remember_skin_count);
	CPConfig_SetInt("Equalizer", "Active", options.equalizer);
	CPConfig_SetInt("Equalizer", "ActivePreset", options.eq_settings[0]);

	for (teller = 1; teller < (int)ARRAY_SIZE(options.eq_settings); teller++)
	{
		CPConfig_SetInt("Equalizer", s_pcEqKeys[teller], options.eq_settings[teller]);
	}
	
	// Write quick find defaults
	{
		char pcQuickFindOption[2];
		pcQuickFindOption[1] = '\0';
		
		if (options.m_enQuickFindTerm == qftTitle)
			pcQuickFindOption[0] = 'T';
		else if (options.m_enQuickFindTerm == qftArtist)
			pcQuickFindOption[0] = 'A';
		else if (options.m_enQuickFindTerm == qftAlbum)
			pcQuickFindOption[0] = 'M';
			
		CPConfig_SetString("Misc", "QuickFindSearchTerm", pcQuickFindOption);
	}
	
	// Write out mixer mode
	if (globals.m_enMixerMode == mmMasterVolume)
		CPConfig_SetString("Mixer", "Mode", "Master");
	else if (globals.m_enMixerMode == mmWaveVolume)
		CPConfig_SetString("Mixer", "Mode", "Wave");
	else
		CPConfig_SetString("Mixer", "Mode", "Internal");
		
	CPConfig_SetInt("Mixer", "InternalVolume", globals.m_iVolume);
}
