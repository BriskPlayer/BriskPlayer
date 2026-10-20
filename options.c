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
#include "CP_SafeGlobals.h"
#include "CPI_Player.h"
#include "CPI_Playlist.h"
#include "CPI_Translation.h"
#include "CPI_Gettext.h"
#ifdef ENABLE_DISCORD_RPC
#include "CPI_DiscordRPC.h"
#endif

//
//
//
INT_PTR CALLBACK
url_windowproc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;  // Suppress unused parameter warning
	switch (msg)
	{
	
		case WM_INITDIALOG:
			CPG_TranslateDialog(hwndDlg);
			return TRUE;
			
		case WM_CLOSE:
			EndDialog(hwndDlg, 1);
			return TRUE;
			
		case WM_COMMAND:
		
			switch (LOWORD(wParam))
			{
			
			case IDOK:
			{
				char urlbuf[2048];
				GetDlgItemText(hwndDlg, IDC_URL, urlbuf, sizeof(urlbuf));
				SAFE_PLAYLIST_CALL(CPL_Empty);
				SAFE_PLAYLIST_CALL(CPL_SyncLoadNextFile);
				SAFE_PLAYLIST_CALL1(CPL_AddFile, urlbuf);
				SAFE_PLAYLIST_CALL2(CPL_PlayItem, TRUE, pmCurrentItem);
			}
			
			EndDialog(hwndDlg, TRUE);
			break;
			
			case IDCANCEL:
				EndDialog(hwndDlg, TRUE);
				break;
			}
	}
	
	return FALSE;
}

//
//
//
INT_PTR CALLBACK
options_windowproc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;  // Suppress unused parameter warning

	switch (msg)
	{
			/* This message means the dialog is started but not yet visible.
			   Do All initializations here
			 */
			
		case WM_INITDIALOG:
		{
			// this is needed as the options dialog is created with DialogBox
			windows.dlg_options = hwndDlg;
			
			CPG_TranslateDialog(hwndDlg);
			SetDlgItemText(hwndDlg, IDC_SKINSFOLDER, options.skins_folder_path);

			SendDlgItemMessage(hwndDlg, IDC_EASYMOVE, BM_SETCHECK,
							   options.easy_move, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_SCROLLTITLE, BM_SETCHECK,
							   options.scroll_track_title, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_ONTOP, BM_SETCHECK,
							   options.always_on_top, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_AUTOEXIT, BM_SETCHECK,
							   options.auto_exit_after_playing, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_REMEMBERPLS, BM_SETCHECK,
							   options.remember_playlist, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_REMAINING, BM_SETCHECK,
							   options.show_remaining_time, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_READTAG, BM_SETCHECK,
							   options.read_id3_tag, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_SUPPORTID3_V2, BM_SETCHECK,
							   options.support_id3v2, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_READTRACKTIME, BM_SETCHECK,
							   options.work_out_track_lengths, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_READID3INBACKGROUND, BM_SETCHECK,
							   options.read_id3_tag_in_background, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_ROTATE, BM_SETCHECK,
							   options.rotate_systray_icon, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_REMSONG, BM_SETCHECK,
							   options.remember_last_played_track, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_FILEONCE, BM_SETCHECK,
							   options.allow_file_once_in_playlist, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_MULTIPLEINSTANCES, BM_SETCHECK,
							   options.allow_multiple_instances, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_READSELTAG, BM_SETCHECK,
							   options.read_id3_tag_of_selected, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_AUTOPLAY, BM_SETCHECK,
							   options.auto_play_when_started, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_TASKBAR, BM_SETCHECK,
							   options.show_on_taskbar, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_STICKYWINDOWS, BM_SETCHECK,
							   options.sticky_windows, 0);
			                   
			SendDlgItemMessage(hwndDlg, IDC_DISCORDRPC, BM_SETCHECK,
							   options.discord_rpc_enabled, 0);
			                   
			// ReplayGain controls
			SendDlgItemMessage(hwndDlg, IDC_REPLAYGAIN_MODE, CB_ADDSTRING, 0, (LPARAM)T(STR_REPLAYGAIN_OFF));
			SendDlgItemMessage(hwndDlg, IDC_REPLAYGAIN_MODE, CB_ADDSTRING, 0, (LPARAM)T(STR_REPLAYGAIN_TRACK));
			SendDlgItemMessage(hwndDlg, IDC_REPLAYGAIN_MODE, CB_ADDSTRING, 0, (LPARAM)T(STR_REPLAYGAIN_ALBUM));
			SendDlgItemMessage(hwndDlg, IDC_REPLAYGAIN_MODE, CB_SETCURSEL, options.replaygain_mode, 0);
			SendDlgItemMessage(hwndDlg, IDC_REPLAYGAIN_PREAMP_SPIN, UDM_SETRANGE, 0, MAKELONG(12, -12));
			SetDlgItemInt(hwndDlg, IDC_REPLAYGAIN_PREAMP_VAL, options.replaygain_preamp_db, TRUE);
			SendDlgItemMessage(hwndDlg, IDC_REPLAYGAIN_NOCLIP, BM_SETCHECK, options.replaygain_prevent_clipping, 0);
			
			SendDlgItemMessage(hwndDlg, IDC_GAPLESS, BM_SETCHECK, options.gapless_playback, 0);

			SendDlgItemMessage(hwndDlg, IDC_DELAYTIMES, UDM_SETRANGE, 0,
							   MAKELONG(10, 0));

			SetDlgItemInt(hwndDlg, IDC_DELAYTIME,
						  options.seconds_delay_after_track, FALSE);

					SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_ADDSTRING, 0, (LPARAM)T(STR_VOLUME_SYSTEM_MASTER));
			
			SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_ADDSTRING, 0, (LPARAM)T(STR_VOLUME_SYSTEM_WAVE));
			
			SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_ADDSTRING, 0, (LPARAM)T(STR_VOLUME_INTERNAL));
			if (globals.m_enMixerMode == mmMasterVolume)
				SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_SETCURSEL, 0, 0);
			else if (globals.m_enMixerMode == mmWaveVolume)
				SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_SETCURSEL, 1, 0);
			else
				SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_SETCURSEL, 2, 0);
				
			SAFE_PLAYER_CALL(CPI_Player__EnumOutputDevices);

			return TRUE;
		}

		case WM_COMMAND:
		
			switch (LOWORD(wParam))
			{
			
				case IDCANCEL:
					EndDialog(hwndDlg, 1);
					break;

				case IDOK:
				{
					BOOL    duplicatesalreadyremoved =
						options.allow_file_once_in_playlist;
					int     index;
					HWND    hWnd = GetParent(hwndDlg);

					options.auto_exit_after_playing =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_AUTOEXIT, BM_GETCHECK,
										   0, 0);
					options.remember_playlist =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_REMEMBERPLS,
										   BM_GETCHECK, 0, 0);
					options.show_remaining_time =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_REMAINING, BM_GETCHECK,
										   0, 0);
					options.read_id3_tag =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_READTAG, BM_GETCHECK,
										   0, 0);
					options.support_id3v2 =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_SUPPORTID3_V2, BM_GETCHECK,
										   0, 0);
					options.read_id3_tag_in_background =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_READID3INBACKGROUND, BM_GETCHECK,
										   0, 0);
					options.work_out_track_lengths =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_READTRACKTIME, BM_GETCHECK,
										   0, 0);
					options.easy_move =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_EASYMOVE, BM_GETCHECK,
										   0, 0);
					options.rotate_systray_icon =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_ROTATE, BM_GETCHECK, 0,
										   0);
					options.allow_file_once_in_playlist =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_FILEONCE, BM_GETCHECK,
										   0, 0);
					options.allow_multiple_instances =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_MULTIPLEINSTANCES, BM_GETCHECK,
										   0, 0);
					options.read_id3_tag_of_selected =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_READSELTAG,
										   BM_GETCHECK, 0, 0);
					options.auto_play_when_started =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_AUTOPLAY, BM_GETCHECK,
										   0, 0);
					options.show_on_taskbar =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_TASKBAR, BM_GETCHECK,
										   0, 0);
					options.sticky_windows =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_STICKYWINDOWS, BM_GETCHECK,
										   0, 0);
					{
#ifdef ENABLE_DISCORD_RPC
						BOOL bWasEnabled = options.discord_rpc_enabled;
#endif
						options.discord_rpc_enabled =
							(BOOL)SendDlgItemMessage(hwndDlg, IDC_DISCORDRPC, BM_GETCHECK,
											   0, 0);
#ifdef ENABLE_DISCORD_RPC
						// Start or stop Discord RPC based on toggle
						if (options.discord_rpc_enabled && !bWasEnabled)
						{
							CPI_DiscordRPC_Init();
							// Push current playback state immediately
							CP_HPLAYLISTITEM hCurrent = CPL_GetActiveItem(globals.m_hPlaylist);
							if (globals.m_enPlayerState == cppsPlaying && hCurrent)
								CPI_DiscordRPC_SetPlaying(hCurrent, globals.main_long_track_duration);
							else if (globals.m_enPlayerState == cppsPaused && hCurrent)
								CPI_DiscordRPC_SetPaused(hCurrent);
						}
						else if (!options.discord_rpc_enabled && bWasEnabled)
						{
							CPI_DiscordRPC_Clear();
							CPI_DiscordRPC_Shutdown();
						}
#endif
					}
					                       
					if (options.show_on_taskbar)
					{
						SetWindowLongPtr(hWnd, GWL_EXSTYLE,
									  GetWindowLongPtr(hWnd,
												GWL_EXSTYLE) &
									  ~WS_EX_TOOLWINDOW);
						SetWindowLongPtr(hWnd, GWL_STYLE,
									  GetWindowLongPtr(hWnd,
												GWL_STYLE) | WS_SYSMENU);
					}					else
					{
						ShowWindow(hWnd, SW_HIDE);
						SetWindowLongPtr(hWnd, GWL_EXSTYLE,
									  GetWindowLongPtr(hWnd,
												GWL_EXSTYLE) |
									  WS_EX_TOOLWINDOW);
						SetWindowLongPtr(hWnd, GWL_STYLE,
									  GetWindowLongPtr(hWnd,
												GWL_STYLE) | WS_SYSMENU);
						ShowWindow(hWnd, SW_SHOW);					}
					
					options.remember_last_played_track =
					
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_REMSONG, BM_GETCHECK,
										   0, 0);
					                       
					options.seconds_delay_after_track =
						GetDlgItemInt(hwndDlg, IDC_DELAYTIME, NULL, FALSE);
					    
					options.scroll_track_title =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_SCROLLTITLE,
										   BM_GETCHECK, 0, 0);
					options.always_on_top =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_ONTOP, BM_GETCHECK, 0,
										   0);
						(void)window_set_always_on_top(hWnd, options.always_on_top);

					GetDlgItemText(hwndDlg, IDC_SKINSFOLDER, options.skins_folder_path, MAX_PATH);

					index = (int)SendDlgItemMessage(hwndDlg, IDC_OUTPUT, CB_GETCURSEL, 0, 0);
					
					if (options.decoder_output_mode != index)
					{
						options.decoder_output_mode = index;
						SAFE_PLAYER_CALL(CPI_Player__OnOutputDeviceChange);
					}
					
					// ReplayGain settings
					options.replaygain_mode = (int)SendDlgItemMessage(hwndDlg, IDC_REPLAYGAIN_MODE, CB_GETCURSEL, 0, 0);
					options.replaygain_preamp_db = (int)GetDlgItemInt(hwndDlg, IDC_REPLAYGAIN_PREAMP_VAL, NULL, TRUE);
					options.replaygain_prevent_clipping = (BOOL)SendDlgItemMessage(hwndDlg, IDC_REPLAYGAIN_NOCLIP, BM_GETCHECK, 0, 0);
					
					// Gapless playback
					options.gapless_playback = (BOOL)SendDlgItemMessage(hwndDlg, IDC_GAPLESS, BM_GETCHECK, 0, 0);

					// Mixer control
					{
						int iMixerSelection;
						CPe_MixerMode enNewMixerMode;
						
						iMixerSelection = (int)SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_GETCURSEL, 0, 0);
						
						if (iMixerSelection == 0)
							enNewMixerMode = mmMasterVolume;
						else if (iMixerSelection == 1)
							enNewMixerMode = mmWaveVolume;
						else
							enNewMixerMode = mmInternal;
							
						if (enNewMixerMode != globals.m_enMixerMode)
						{
							// Change mixer
							globals.m_enMixerMode = enNewMixerMode;
							SAFE_PLAYER_CALL(CPI_Player__ReopenMixer);
							
							// Setup UI
							
							if (enNewMixerMode == mmInternal)
								globals.m_iVolume = 100;
							else
								globals.m_iVolume = SAFE_GET_VOLUME();
								
							main_draw_vu_from_value(windows.wnd_main, VolumeSlider, globals.m_iVolume);
						}
					}
					
					
					if (!duplicatesalreadyremoved && options.allow_file_once_in_playlist)
						SAFE_PLAYLIST_CALL(CPL_RemoveDuplicates);
						
					EndDialog(hwndDlg, 1);
					
					break;
				}
				
				case IDC_ONTOP:
					break;

				case IDC_SKINSFOLDER_BROWSE:
				{
					BROWSEINFO browseinfo;
					LPITEMIDLIST itemlist;
					char folderchoice[MAX_PATH];

					browseinfo.hwndOwner = hwndDlg;
					browseinfo.pidlRoot = NULL;
					browseinfo.pszDisplayName = folderchoice;
					browseinfo.lpszTitle = T(STR_OPTIONS_SKINS_FOLDER);
					browseinfo.ulFlags = BIF_EDITBOX | BIF_RETURNONLYFSDIRS;
					browseinfo.lpfn = NULL;
					browseinfo.lParam = 0;
					browseinfo.iImage = 0;

					itemlist = SHBrowseForFolder(&browseinfo);

					if (itemlist != NULL)
					{
						char pathbuf[MAX_PATH];
						SHGetPathFromIDList(itemlist, pathbuf);
						SetDlgItemText(hwndDlg, IDC_SKINSFOLDER, pathbuf);
					}

					break;
				}
			}
			
			switch (HIWORD(wParam))
			{
			
				case CBN_SELENDOK:
				{
					break;
				}
			}
	}
	
	return FALSE;
}

BOOL    window_set_always_on_top(HWND hWnd, BOOL yes)
{
	if (yes)
	{
		SetWindowPos(hWnd, // handle to window
					 HWND_TOPMOST, // placement-order handle
					 0,  // horizontal position
					 0,  // vertical position
					 0,  // width
					 0,  // height
					 SWP_NOMOVE | SWP_NOSIZE); // window-positioning flags
		             
	}
	
	else
	{
		SetWindowPos(hWnd, // handle to window
					 HWND_NOTOPMOST, // placement-order handle
					 0,  // horizontal position
					 0,  // vertical position
					 0,  // width
					 0,  // height
					 SWP_NOMOVE | SWP_NOSIZE); // window-positioning flags
	}
	
	return TRUE;
}
