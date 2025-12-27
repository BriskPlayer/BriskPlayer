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
#include "CPI_Playlist.h"
#include "CPI_PlaylistItem.h"


////////////////////////////////////////////////////////////
//
//
//
void    playlist_write_default(void)
{
	char    exepath[MAX_PATH];
	main_get_program_path(GetModuleHandle(NULL), exepath, MAX_PATH);
	strcat_s(exepath, sizeof(exepath), "default.m3u");
	CPL_ExportPlaylist(globals.m_hPlaylist, exepath);
}

//
//
//
void    options_read(void)
{
	char    pathbuf[MAX_PATH];
	int     teller;
	int     widths[] = {   20,   200,  200,  200,  50,   70,   70,    100,  100,   100, 80};
	int     visibles[] = { FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, TRUE };
	int     sequences[] = { 5,    1,    2,    3,    4,    6,    7,     8,    9,     0,   10};
	int iColIDX;
	
	main_get_program_path(NULL, pathbuf, MAX_PATH);
	strcat(pathbuf, "briskplayer.ini");
	
	for (iColIDX = PLAYLIST_first; iColIDX <= PLAYLIST_last; iColIDX++)
	{
		char keyname[100];
		sprintf_s(keyname, sizeof(keyname), "PlaylistCol%d", iColIDX);
		options.playlist_column_widths[iColIDX] = GetPrivateProfileInt("WindowPos", keyname, widths[iColIDX], pathbuf);
		
		sprintf_s(keyname, sizeof(keyname), "PlaylistSeq%d", iColIDX);
		options.playlist_column_seq[iColIDX] = GetPrivateProfileInt("WindowPos", keyname, sequences[iColIDX], pathbuf);
		
		sprintf_s(keyname, sizeof(keyname), "PlaylistVis%d", iColIDX);
		options.playlist_column_visible[iColIDX] = GetPrivateProfileInt("WindowPos", keyname, visibles[iColIDX], pathbuf) ? TRUE : FALSE;
	}
	
	options.main_window_pos.x = GetPrivateProfileInt("WindowPos", // address of section name
	
								"WindowX", // address of key name
								100, // return value if key name is not found
								pathbuf); // address of initialization filename
	                            
	if (options.main_window_pos.x < -10)
		options.main_window_pos.x = 100;
		
	options.main_window_pos.y = GetPrivateProfileInt("WindowPos", // address of section name
								"WindowY", // address of key name
								100, // return value if key name is not found
								pathbuf); // address of initialization filename
	                            
	if (options.main_window_pos.y < -10)
		options.main_window_pos.y = 100;
		
	options.playlist_window_pos.left = GetPrivateProfileInt("WindowPos", // address of section name
									   "PlaylistX", // address of key name
									   350, // return value if key name is not found (offset from main window)
									   pathbuf); // address of initialization filename
	                                   
	if (options.playlist_window_pos.left < -10)
		options.playlist_window_pos.left = 350;
		
	options.playlist_window_pos.top = GetPrivateProfileInt("WindowPos", // address of section name
									  "PlaylistY", // address of key name
									  150, // return value if key name is not found (slightly below main window)
									  pathbuf); // address of initialization filename
	                                  
	if (options.playlist_window_pos.top < -10)
		options.playlist_window_pos.top = 150;
		
	options.playlist_window_pos.right = GetPrivateProfileInt("WindowPos", // address of section name
										"PlaylistW", // address of key name
										650, // return value if key name is not found (increased default width)
										pathbuf) + options.playlist_window_pos.left; // address of initialization filename
	                                    
	options.playlist_window_pos.bottom = GetPrivateProfileInt("WindowPos", // address of section name
										 "PlaylistH", // address of key name
										 500, // return value if key name is not found (increased default height)
										 pathbuf) + options.playlist_window_pos.top; // address of initialization filename
	                                     
	GetPrivateProfileString("LastDirectory", // points to section name
							"Directory", // points to key name
							"", // points to default string
							options.last_used_directory, // points to destination buffer
							MAX_PATH, // size of destination buffer
							pathbuf); // points to initialization filename
	                        
	options.repeat_playlist =
		GetPrivateProfileInt("Misc", "Repeat", 0, pathbuf);
	    
	options.shuffle_play =
		GetPrivateProfileInt("Misc", "Shuffle", 0, pathbuf);
	    
	options.always_on_top =
		GetPrivateProfileInt("Misc", "Ontop", 0, pathbuf);
	    
	options.auto_exit_after_playing =
		GetPrivateProfileInt("Misc", "Autoexit", 0, pathbuf);
	    
	options.remember_playlist =
		GetPrivateProfileInt("Misc", "Rememberpls", 1, pathbuf);
	    
	options.show_remaining_time =
		GetPrivateProfileInt("Misc", "Remaining", 0, pathbuf);
	    
	options.read_id3_tag =
		GetPrivateProfileInt("Misc", "ReadID3tag", 1, pathbuf);
	    
	options.support_id3v2 =
		GetPrivateProfileInt("Misc", "SuportID3v2", 1, pathbuf);
	    
	options.prefer_native_ogg_tags =
		GetPrivateProfileInt("Misc", "PreferNativeOGGtags", 1, pathbuf);
	    
	options.read_id3_tag_in_background =
		GetPrivateProfileInt("Misc", "BackgroundReadID3", 1, pathbuf);
	    
	options.work_out_track_lengths =
		GetPrivateProfileInt("Misc", "WorkOutTrackLengths", 1, pathbuf);
	    
	options.allow_multiple_instances =
		GetPrivateProfileInt("Misc", "AllowMultipleInstances", 0, pathbuf);
	    
	options.read_id3_tag_of_selected =
		GetPrivateProfileInt("Misc", "ReadSelID3tag", 1, pathbuf);
	    
	options.seconds_delay_after_track =
		GetPrivateProfileInt("Misc", "DelayTime", 0, pathbuf);
	    
	options.decoder_output_mode =
		GetPrivateProfileInt("Misc", "Outputmode", 1, pathbuf);
	
	// Read preferred language
	GetPrivateProfileString("Misc", "Language", "", options.preferred_language, 
	                        sizeof(options.preferred_language), pathbuf);
	    
	options.easy_move =
		GetPrivateProfileInt("Misc", "Easymove", 1, pathbuf);
	    
	options.remember_skin_count =
		GetPrivateProfileInt("Misc", "RememberSkins", 4, pathbuf);
	    
	options.allow_file_once_in_playlist =
		GetPrivateProfileInt("Misc", "Fileonce", 1, pathbuf);
	    
	options.auto_play_when_started =
		GetPrivateProfileInt("Misc", "Autoplay", 0, pathbuf);
	    
	options.show_on_taskbar =
		GetPrivateProfileInt("Misc", "TaskBar", 1, pathbuf);
	    
	options.show_playlist = GetPrivateProfileInt("Misc", "ShowPlaylist", 0, pathbuf);
	
	options.rotate_systray_icon =
		GetPrivateProfileInt("Misc", "RotateIcon", 1, pathbuf);
	    
	options.scroll_track_title =
		GetPrivateProfileInt("Misc", "Scrolltitle", 1, pathbuf);
	    
	options.sticky_windows =
		GetPrivateProfileInt("Misc", "StickyWindows", 1, pathbuf);
	    
	GetPrivateProfileString("Misc", // points to section name
							"RememberLastSong", // points to key name
							"", // points to default string
							options.initial_file, // points to destination buffer
							MAX_PATH, // size of destination buffer
							pathbuf); // points to initialization filename
	                        
	if (*options.initial_file)
		options.remember_last_played_track = TRUE;
		
	options.last_selected_skin_number =
		GetPrivateProfileInt("Skin", "LastSkin", 0, pathbuf);
	    
	options.use_default_skin =
		GetPrivateProfileInt("Skin", "UseDefault", 1, pathbuf);
	    
	options.use_playlist_skin =
		GetPrivateProfileInt("Skin", "Useplaylistskin", 0, pathbuf);
	    
	{
		int     teller;
		
		for (teller = MENU_SKIN_DEFAULT + 1; teller < MENU_SKIN_DEFAULT + 1 + options.remember_skin_count;
				teller++)
		{
			char    SkinFileString[MAX_PATH];
			char    skinpath[MAX_PATH];
			snprintf(SkinFileString, sizeof(SkinFileString), "SkinFile%d", teller - MENU_SKIN_DEFAULT);
			GetPrivateProfileString("Skin", SkinFileString, "",
									skinpath, MAX_PATH, pathbuf);
			                        
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
	
	GetPrivateProfileString("Skin", "PlaylistSkin", "",
	
							(char*)options.playlist_skin_file, MAX_PATH, pathbuf);
	options.equalizer =
		GetPrivateProfileInt("Equalizer", "Active", 0, pathbuf);
	    
	for (teller = 1; teller < ARRAY_SIZE(options.eq_settings); teller++)
	{
		char    keyname[100];
		snprintf(keyname, sizeof(keyname), "Eq%d", teller);
		options.eq_settings[teller] =
			GetPrivateProfileInt("Equalizer", keyname, 0, pathbuf);
	}
	
	// Read quick find defaults
	{
		char pcQuickFindOption[2];
		GetPrivateProfileString("Misc", "QuickFindSearchTerm", "T", pcQuickFindOption, 2, pathbuf);
		
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
		GetPrivateProfileString("Mixer", "Mode", "Master", cMixerMode, 32, pathbuf);
		
		if (stricmp(cMixerMode, "wave") == 0)
			globals.m_enMixerMode = mmWaveVolume;
		else if (stricmp(cMixerMode, "internal") == 0)
			globals.m_enMixerMode = mmInternal;
		else
			globals.m_enMixerMode = mmMasterVolume;
			
		globals.m_iVolume = GetPrivateProfileInt("Mixer", "InternalVolume", 60, pathbuf);
	}
}

void    options_write(void)
{
	char    intbuf[33];
	int     teller;
	char    pathbuf[MAX_PATH];
	int iColIDX;
	
	main_get_program_path(NULL, pathbuf, MAX_PATH);
	strcat_s(pathbuf, sizeof(pathbuf), "briskplayer.ini");
	
	for (iColIDX = PLAYLIST_first; iColIDX <= PLAYLIST_last; iColIDX++)
	{
		char keyname[100];
		
		// Write the width
		snprintf(keyname, sizeof(keyname), "PlaylistCol%d", iColIDX);
		_itoa_s(options.playlist_column_widths[iColIDX], intbuf, sizeof(intbuf), 10);
		WritePrivateProfileString("WindowPos", keyname, intbuf, pathbuf);
		
		// Write the order array
		snprintf(keyname, sizeof(keyname), "PlaylistSeq%d", iColIDX);
		_itoa_s(options.playlist_column_seq[iColIDX], intbuf, sizeof(intbuf), 10);
		WritePrivateProfileString("WindowPos", keyname, intbuf, pathbuf);
		
		// Write the visiblity array
		snprintf(keyname, sizeof(keyname), "PlaylistVis%d", iColIDX);
		WritePrivateProfileString("WindowPos", keyname, options.playlist_column_visible[iColIDX] ? "1" : "0", pathbuf);
	}
	
	_itoa_s(options.main_window_pos.x, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("WindowPos", // pointer to section name
	
							  "WindowX", // pointer to key name
							  intbuf, // pointer to string to add
							  pathbuf // pointer to initialization filename
							 );
	_itoa_s(options.main_window_pos.y, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("WindowPos", // pointer to section name
							  "WindowY", // pointer to key name
							  intbuf, // pointer to string to add
							  pathbuf // pointer to initialization filename
							 );
	                         
	_itoa_s(options.playlist_window_pos.left, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("WindowPos", // pointer to section name
							  "PlaylistX", // pointer to key name
							  intbuf, // pointer to string to add
							  pathbuf // pointer to initialization filename
							 );
	_itoa_s(options.playlist_window_pos.top, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("WindowPos", // pointer to section name
							  "PlaylistY", // pointer to key name
							  intbuf, // pointer to string to add
							  pathbuf // pointer to initialization filename
							 );
	_itoa_s(options.playlist_window_pos.right - options.playlist_window_pos.left, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("WindowPos", // pointer to section name
							  "PlaylistW", // pointer to key name
							  intbuf, // pointer to string to add
							  pathbuf // pointer to initialization filename
							 );
	_itoa_s(options.playlist_window_pos.bottom - options.playlist_window_pos.top, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("WindowPos", // pointer to section name
							  "PlaylistH", // pointer to key name
							  intbuf, // pointer to string to add
							  pathbuf // pointer to initialization filename
							 );
	                         
	WritePrivateProfileString("LastDirectory", // pointer to section name
							  "Directory", // pointer to key name
							  options.last_used_directory, // pointer to string to add
							  pathbuf // pointer to initialization filename
							 );
	                         
	WritePrivateProfileString("Skin", "PlaylistSkin",
							  (char*)options.playlist_skin_file, pathbuf);
	                          
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
				
				snprintf(SkinFileString, sizeof(SkinFileString), "SkinFile%d", profileteller++);
				
				WritePrivateProfileString("Skin", SkinFileString,
										  (char*)options.main_skin_file, pathbuf);
				                          
			}
			
			else
			{
				snprintf(SkinFileString, sizeof(SkinFileString), "SkinFile%d", profileteller++);
				
				WritePrivateProfileString("Skin", SkinFileString, NULL,
										  pathbuf);
			}
		}
	}
	
	_itoa_s(options.last_selected_skin_number, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Skin", "LastSkin",
							  intbuf, pathbuf);
	_itoa_s(options.use_playlist_skin, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Skin", "UsePlaylistSkin",
							  intbuf,
							  pathbuf);
	                          
	_itoa_s(options.use_default_skin, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Skin", "UseDefault",
							  intbuf,
							  pathbuf);
	_itoa_s(options.repeat_playlist, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "Repeat",
							  intbuf,
							  pathbuf);
	_itoa_s(options.shuffle_play, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "Shuffle",
							  intbuf,
							  pathbuf);
	_itoa_s(options.easy_move, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "Easymove",
							  intbuf,
							  pathbuf);
	_itoa_s(options.rotate_systray_icon, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "RotateIcon",
							  intbuf, pathbuf);
	_itoa_s(options.always_on_top, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "Ontop",
							  intbuf,
							  pathbuf);
	_itoa_s(options.auto_exit_after_playing, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "Autoexit",
							  intbuf, pathbuf);
	_itoa_s(options.remember_playlist, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "Rememberpls",
							  intbuf,
							  pathbuf);
	_itoa_s(options.show_remaining_time, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "Remaining",
							  intbuf, pathbuf);
	_itoa_s(options.read_id3_tag, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "ReadID3tag",
							  intbuf,
							  pathbuf);
	_itoa_s(options.read_id3_tag_of_selected, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "ReadSelID3tag",
							  intbuf, pathbuf);
	_itoa_s(options.support_id3v2, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "SuportID3v2",
							  intbuf,
							  pathbuf);
	_itoa_s(options.prefer_native_ogg_tags, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "PreferNativeOGGtags",
							  intbuf,
							  pathbuf);
	_itoa_s(options.read_id3_tag_in_background, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "BackgroundReadID3",
							  intbuf,
							  pathbuf);
	_itoa_s(options.work_out_track_lengths, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "WorkOutTrackLengths",
							  intbuf,
							  pathbuf);
	_itoa_s(options.allow_multiple_instances, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "AllowMultipleInstances",
							  intbuf, pathbuf);
	_itoa_s(options.decoder_output_mode, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "Outputmode",
							  intbuf, pathbuf);
	
	// Write preferred language
	WritePrivateProfileString("Misc", "Language", 
							  options.preferred_language, pathbuf);
	
	_itoa_s(options.scroll_track_title, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "Scrolltitle",
							  intbuf, pathbuf);	_itoa_s(options.sticky_windows, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "StickyWindows",
						  intbuf, pathbuf);	_itoa_s(options.show_playlist, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "ShowPlaylist",
							  intbuf,
							  pathbuf);
	                          
	{
		if (options.remember_last_played_track)
			WritePrivateProfileString("Misc", "RememberLastSong", options.initial_file, pathbuf);
		else
			WritePrivateProfileString("Misc", "RememberLastSong", "", pathbuf);
	}
	
	_itoa_s(options.allow_file_once_in_playlist, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "Fileonce",
							  intbuf, pathbuf);
	_itoa_s(options.auto_play_when_started, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "Autoplay",
							  intbuf, pathbuf);
	_itoa_s(options.show_on_taskbar, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "TaskBar",
							  intbuf,
							  pathbuf);
	_itoa_s(options.seconds_delay_after_track, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "DelayTime",
							  intbuf, pathbuf);
	_itoa_s(options.remember_skin_count, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Misc", "RememberSkins",
							  intbuf, pathbuf);
	_itoa_s(options.equalizer, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Equalizer", "Active",
							  intbuf,
							  pathbuf);
	
	for (teller = 1; teller < ARRAY_SIZE(options.eq_settings); teller++)
	{
		char    keyname[100];
		snprintf(keyname, sizeof(keyname), "Eq%d", teller);
		_itoa_s(options.eq_settings[teller], intbuf, sizeof(intbuf), 10);
		WritePrivateProfileString("Equalizer", keyname,
								  intbuf, pathbuf);
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
			
		WritePrivateProfileString("Misc", "QuickFindSearchTerm", pcQuickFindOption, pathbuf);
	}
	
	// Write out mixer mode
	
	if (globals.m_enMixerMode == mmMasterVolume)
		WritePrivateProfileString("Mixer", "Mode", "Master", pathbuf);
	else if (globals.m_enMixerMode == mmWaveVolume)
		WritePrivateProfileString("Mixer", "Mode", "Wave", pathbuf);
	else
		WritePrivateProfileString("Mixer", "Mode", "Internal", pathbuf);
		
	_itoa_s(globals.m_iVolume, intbuf, sizeof(intbuf), 10);
	WritePrivateProfileString("Mixer", "InternalVolume", intbuf, pathbuf);
}
