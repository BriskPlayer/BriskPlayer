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
#include "CPI_Player.h"
#include "CPI_Playlist.h"
#include "CPI_Translation.h"
#include "CPI_Gettext.h"

// Helper function to translate static text controls that have ID -1
static void TranslateStaticControls(HWND hwndDlg)
{
    HWND hChild = GetWindow(hwndDlg, GW_CHILD);
    char buffer[256];
    
    while (hChild) {
        // Get the window text
        GetWindowTextA(hChild, buffer, sizeof(buffer));
        
        // Check if this is one of our static texts that needs translation
        if (strcmp(buffer, "Track Delay (sec)") == 0) {
            SetWindowTextA(hChild, T(STR_OPTIONS_TRACK_DELAY_SEC));
        }
        else if (strcmp(buffer, "Skinlist length") == 0) {
            SetWindowTextA(hChild, T(STR_OPTIONS_SKINLIST_LENGTH));
        }
        else if (strcmp(buffer, "Output") == 0) {
            SetWindowTextA(hChild, T(STR_OPTIONS_OUTPUT));
        }
        else if (strcmp(buffer, "Volume controls") == 0) {
            SetWindowTextA(hChild, T(STR_OPTIONS_VOLUME_CONTROLS));
        }
        else if (strcmp(buffer, "Skin") == 0) {
            SetWindowTextA(hChild, T(STR_OPTIONS_SKIN));
        }
        
        // Move to next child window
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }
}

// Helper function to resize dialog based on translated text lengths
static void ResizeDialogForTranslations(HWND hwndDlg)
{
    // Dialog template has been updated to accommodate longer German text
    // No dynamic resizing needed anymore
    (void)hwndDlg; // Suppress unused parameter warning
}

// Helper function to adjust control positions for better layout with longer text
static void AdjustControlLayout(HWND hwndDlg)
{
    // Dialog template has been updated to accommodate longer German text
    // No dynamic adjustments needed anymore
    (void)hwndDlg; // Suppress unused parameter warning
}

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
			// Set translated dialog title and text
			SetWindowText(hwndDlg, T(STR_DLG_URL_TITLE));
			SetDlgItemText(hwndDlg, IDC_URL_DESCRIPTION, T(STR_URL_DESCRIPTION));
			SetDlgItemText(hwndDlg, IDC_URL_STATIC, T(STR_URL_LABEL));
			SetDlgItemText(hwndDlg, IDOK, T(STR_OPTIONS_OK));
			SetDlgItemText(hwndDlg, IDCANCEL, T(STR_OPTIONS_CANCEL));
			return TRUE;
			
		case WM_CLOSE:
			EndDialog(hwndDlg, 1);
			return TRUE;
			
		case WM_COMMAND:
		
			switch (LOWORD(wParam))
			{
			
				case IDOK:
				{
					char urlbuf[MAX_PATH];
					GetDlgItemText(hwndDlg, IDC_URL, urlbuf, MAX_PATH);
					CPL_Empty(globals.m_hPlaylist);
					CPL_SyncLoadNextFile(globals.m_hPlaylist);
					CPL_AddFile(globals.m_hPlaylist, urlbuf);
					CPL_PlayItem(globals.m_hPlaylist, TRUE, pmCurrentItem);
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
			
			// Set translated dialog title and control text
			SetWindowText(hwndDlg, T(STR_DLG_OPTIONS_TITLE));
			SetDlgItemText(hwndDlg, IDC_ONTOP, T(STR_OPTIONS_ALWAYS_ON_TOP));
			SetDlgItemText(hwndDlg, IDC_AUTOEXIT, T(STR_OPTIONS_EXIT_AFTER_PLAYING));
			SetDlgItemText(hwndDlg, IDC_ROTATE, T(STR_OPTIONS_ROTATE_SYSTRAY));
			SetDlgItemText(hwndDlg, IDC_SCROLLTITLE, T(STR_OPTIONS_SCROLL_TITLE));
			SetDlgItemText(hwndDlg, IDC_FILEONCE, T(STR_OPTIONS_FILE_ONCE_PLAYLIST));
			SetDlgItemText(hwndDlg, IDC_AUTOPLAY, T(STR_OPTIONS_AUTOPLAY_STARTUP));
			SetDlgItemText(hwndDlg, IDC_MULTIPLEINSTANCES, T(STR_OPTIONS_MULTIPLE_INSTANCES));
			SetDlgItemText(hwndDlg, IDC_REMAINING, T(STR_OPTIONS_SHOW_REMAINING_TIME));
			SetDlgItemText(hwndDlg, IDC_TASKBAR, T(STR_OPTIONS_SHOW_ON_TASKBAR));
			SetDlgItemText(hwndDlg, IDC_REGFILETYPE, T(STR_OPTIONS_REGISTER_FILETYPES));
			SetDlgItemText(hwndDlg, IDC_ADDICONS, T(STR_OPTIONS_ADD_START_MENU));
			SetDlgItemText(hwndDlg, IDC_READTAG, T(STR_OPTIONS_READ_ID3_TAG));
			SetDlgItemText(hwndDlg, IDC_READSELTAG, T(STR_OPTIONS_READ_ID3_SELECTED));
			SetDlgItemText(hwndDlg, IDC_SUPPORTID3_V2, T(STR_OPTIONS_SUPPORT_ID3V2));
			SetDlgItemText(hwndDlg, IDC_PREFERNATIVEOGGTAGS, T(STR_OPTIONS_PREFER_NATIVE_OGG));
			SetDlgItemText(hwndDlg, IDC_READID3INBACKGROUND, T(STR_OPTIONS_READ_ID3_BACKGROUND));
			SetDlgItemText(hwndDlg, IDC_READTRACKTIME, T(STR_OPTIONS_WORK_OUT_LENGTHS));
			SetDlgItemText(hwndDlg, IDC_EASYMOVE, T(STR_OPTIONS_EASY_MOVE));
			SetDlgItemText(hwndDlg, IDC_REMEMBERPLS, T(STR_OPTIONS_REMEMBER_PLAYLIST));
			SetDlgItemText(hwndDlg, IDC_REMSONG, T(STR_OPTIONS_REMEMBER_LAST_PLAYED));
			SetDlgItemText(hwndDlg, IDC_FLUSH_SKINLIST, T(STR_OPTIONS_FLUSH));
			SetDlgItemText(hwndDlg, IDC_PLAYERSKINCHECK, T(STR_OPTIONS_PLAYER));
			SetDlgItemText(hwndDlg, IDC_SKINBUTTON, T(STR_OPTIONS_OPEN));
			SetDlgItemText(hwndDlg, IDOK, T(STR_OPTIONS_OK));
			SetDlgItemText(hwndDlg, IDCANCEL, T(STR_OPTIONS_CANCEL));
			
			// Translate static text controls (these have ID -1, so we need to find them by text)
			TranslateStaticControls(hwndDlg);
			
			// Resize dialog if needed to accommodate longer translations
			ResizeDialogForTranslations(hwndDlg);
			
			// Adjust control layout for better appearance
			AdjustControlLayout(hwndDlg);
			
			if (options.use_default_skin == TRUE)
				SendDlgItemMessage(hwndDlg, IDC_PLAYERSKINCHECK,
								   BM_SETCHECK, BST_UNCHECKED, 0);
			else
				SendDlgItemMessage(hwndDlg, IDC_PLAYERSKINCHECK,
								   BM_SETCHECK, BST_CHECKED, 0);
				                   
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
			                   
			SendDlgItemMessage(hwndDlg, IDC_PREFERNATIVEOGGTAGS, BM_SETCHECK,
							   options.prefer_native_ogg_tags, 0);
			                   
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
			                   
			SendDlgItemMessage(hwndDlg, IDC_REMEMBERSKIN, UDM_SETRANGE,
							   0, MAKELONG(50, 1));
			                   
			SendDlgItemMessage(hwndDlg, IDC_DELAYTIMES, UDM_SETRANGE, 0,
							   MAKELONG(10, 0));
			                   
			SetDlgItemInt(hwndDlg, IDC_REMSKINVAL,
						  options.remember_skin_count, FALSE);
			              
			SetDlgItemInt(hwndDlg, IDC_DELAYTIME,
						  options.seconds_delay_after_track, FALSE);
			              
			SetDlgItemText(hwndDlg, IDC_LOADSKIN, (char*)options.main_skin_file);
			
			if (*options.playlist_skin_file)
				SendDlgItemMessage(hwndDlg, IDC_PLAYLISTSKINCHECK,
								   BM_SETCHECK, options.use_playlist_skin,
								   0);
				                   
					SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_ADDSTRING, 0, (LPARAM)T(STR_VOLUME_SYSTEM_MASTER));
			
			SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_ADDSTRING, 0, (LPARAM)T(STR_VOLUME_SYSTEM_WAVE));
			
			SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_ADDSTRING, 0, (LPARAM)T(STR_VOLUME_INTERNAL));
			if (globals.m_enMixerMode == mmMasterVolume)
				SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_SETCURSEL, 0, 0);
			else if (globals.m_enMixerMode == mmWaveVolume)
				SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_SETCURSEL, 1, 0);
			else
				SendDlgItemMessage(hwndDlg, IDC_MIXER, CB_SETCURSEL, 2, 0);
				
			CPI_Player__EnumOutputDevices(globals.m_hPlayer);
			
			globals.m_bOptions_ChangedSkin = FALSE;
			
			return TRUE;
		}

		case WM_COMMAND:
		
			switch (LOWORD(wParam))
			{
			
				case IDC_SKINBUTTON:
				{
					OPENFILENAME fn;
					char    filefilter[512];
					sprintf(filefilter, "%s\\0*.ini\\0%s\\0*.*\\0", T(STR_FILTER_SKIN_FILES), T(STR_FILTER_ALL_FILES));
					BOOL    returnval;
					char    initialfilename[MAX_PATH * 100] = "";
					char    pathbuffie[MAX_PATH];
					strcpy(pathbuffie, (char*)options.main_skin_file);
					path_remove_filespec(pathbuffie);
					fn.lStructSize = sizeof(OPENFILENAME);
					fn.hwndOwner = hwndDlg;
					fn.hInstance = NULL;
					fn.lpstrFilter = filefilter;
					fn.lpstrCustomFilter = NULL;
					fn.nMaxCustFilter = 0;
					fn.nFilterIndex = 0;
					fn.lpstrFile = initialfilename;
					fn.nMaxFile = MAX_PATH * 200;
					fn.lpstrFileTitle = NULL;
					fn.nMaxFileTitle = 0;
					fn.lpstrInitialDir = pathbuffie;
					fn.lpstrTitle = NULL;
					fn.Flags =
						OFN_HIDEREADONLY | OFN_EXPLORER | OFN_FILEMUSTEXIST
						| OFN_PATHMUSTEXIST | OFN_ENABLESIZING;
					fn.nFileOffset = 0;
					fn.nFileExtension = 0;
					fn.lpstrDefExt = NULL;
					fn.lCustData = 0;
					fn.lpfnHook = NULL;
					fn.lpTemplateName = NULL;
					returnval = GetOpenFileName(&fn);
					
					if (returnval != FALSE)
					{
						// char    pathbuf[MAX_PATH] = "";
						SetDlgItemText(hwndDlg, IDC_LOADSKIN, fn.lpstrFile);
						SendDlgItemMessage(hwndDlg, IDC_PLAYERSKINCHECK,
										   BM_SETCHECK, BST_CHECKED, 0);
						globals.m_bOptions_ChangedSkin = TRUE;
					}
					
					break;
				}
				
				case IDC_FLUSH_SKINLIST:
				
				{
					int     itemcounter =
						GetMenuItemCount(GetSubMenu
										 (globals.main_menu_popup, SKIN_SUBMENU_INDEX));
					int     teller;
					
					for (teller = 0; teller < itemcounter - 1; teller++)
					{
						RemoveMenu(GetSubMenu(globals.main_menu_popup, SKIN_SUBMENU_INDEX), 0,
								   MF_BYPOSITION);
					}
					
				}
				
				break;
				
				case IDCANCEL:
					EndDialog(hwndDlg, 1);
					break;

				case IDOK:
				{
					BOOL    duplicatesalreadyremoved =
						options.allow_file_once_in_playlist;
					int     index;
					HWND    hWnd = GetParent(hwndDlg);
					BOOL bSkinChosen;
					
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
					options.prefer_native_ogg_tags =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_PREFERNATIVEOGGTAGS, BM_GETCHECK,
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
					                       
					if (options.show_on_taskbar)
					{
						SetWindowLong(hWnd, GWL_EXSTYLE,
									  GetWindowLong(hWnd,
													GWL_EXSTYLE) &
									  ~WS_EX_TOOLWINDOW);
						SetWindowLong(hWnd, GWL_STYLE,
									  GetWindowLong(hWnd,
													GWL_STYLE) | WS_SYSMENU);
					}
					
					else
					{
						ShowWindow(hWnd, SW_HIDE);
						SetWindowLong(hWnd, GWL_EXSTYLE,
									  GetWindowLong(hWnd,
													GWL_EXSTYLE) |
									  WS_EX_TOOLWINDOW);
						SetWindowLong(hWnd, GWL_STYLE,
									  GetWindowLong(hWnd,
													GWL_STYLE) | WS_SYSMENU);
						ShowWindow(hWnd, SW_SHOW);
						
					}
					
					options.remember_last_played_track =
					
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_REMSONG, BM_GETCHECK,
										   0, 0);
					                       
					options.remember_skin_count =
						GetDlgItemInt(hwndDlg, IDC_REMSKINVAL, NULL, FALSE);
					options.seconds_delay_after_track =
						GetDlgItemInt(hwndDlg, IDC_DELAYTIME, NULL, FALSE);
					    
					options.scroll_track_title =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_SCROLLTITLE,
										   BM_GETCHECK, 0, 0);
					options.always_on_top =
						(BOOL)SendDlgItemMessage(hwndDlg, IDC_ONTOP, BM_GETCHECK, 0,
										   0);
					window_set_always_on_top(hWnd, options.always_on_top);
					
					GetDlgItemText(hwndDlg, IDC_LOADSKIN,
								   (char*)options.main_skin_file, MAX_PATH);
					               
					               
					bSkinChosen = (BOOL)SendDlgItemMessage(hwndDlg, IDC_PLAYERSKINCHECK, BM_GETCHECK, 0, 0);
					
					if (bSkinChosen != !options.use_default_skin
							|| globals.m_bOptions_ChangedSkin == TRUE)
					{
						options.use_default_skin = !bSkinChosen;
						globals.main_bool_skin_next_is_default = options.use_default_skin;
						
						globals.playlist_bool_force_skin_from_options = TRUE;
						main_play_control(ID_LOADSKIN, hWnd);
					}
					
					index = (int)SendDlgItemMessage(hwndDlg, IDC_OUTPUT, CB_GETCURSEL, 0, 0);
					
					if (options.decoder_output_mode != index)
					{
						options.decoder_output_mode = index;
						CPI_Player__OnOutputDeviceChange(globals.m_hPlayer);
					}
					
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
							CPI_Player__ReopenMixer(globals.m_hPlayer);
							
							// Setup UI
							
							if (enNewMixerMode == mmInternal)
								globals.m_iVolume = 100;
							else
								globals.m_iVolume = CPI_Player__GetVolume(globals.m_hPlayer);
								
							main_draw_vu_from_value(windows.wnd_main, VolumeSlider, globals.m_iVolume);
						}
					}
					
					
					if (!duplicatesalreadyremoved && options.allow_file_once_in_playlist)
						CPL_RemoveDuplicates(globals.m_hPlaylist);
						
					EndDialog(hwndDlg, 1);
					
					break;
				}
				
				case IDC_ONTOP:
					break;
					
				case IDC_REGFILETYPE:
				{
					HKEY    result;
					DWORD   lpdwDisposition;
					char    pathbuf[MAX_PATH];
					char    stringval[MAX_PATH + 3];
					
					CPI_Player__AssociateFileExtensions(globals.m_hPlayer);
					
					GetModuleFileName(NULL, pathbuf, MAX_PATH);
					sprintf(stringval, "%s,%1d", pathbuf, 1);
					RegCreateKeyEx(HKEY_CLASSES_ROOT, CIC_COOLPLAYER_FILETYPE,
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegSetValueEx(result, NULL, 0, REG_SZ,
								  (const BYTE*)T(STR_APP_AUDIO_FILE_DESC), strlen(T(STR_APP_AUDIO_FILE_DESC)) + 1);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_FILETYPE "\\DefaultIcon",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegSetValueEx(result, NULL, 0, REG_SZ, (const BYTE*)stringval,
								  strlen(stringval) + 1);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_FILETYPE "\\Shell", 0,
								   NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_FILETYPE "\\Shell\\Open",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_FILETYPE "\\Shell\\Open\\command",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					sprintf(stringval, "\"%s\" \"%%1\"", pathbuf);
					RegSetValueEx(result, NULL, 0, REG_SZ, (const BYTE*)stringval,
								  strlen(stringval) + 1);
					RegCloseKey(result);
					
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_FILETYPE "\\Shell\\BriskPlayer Queue",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_FILETYPE "\\Shell\\BriskPlayer Queue\\command",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					sprintf(stringval, "\"%s\" \"%%1\" -add", pathbuf);
					RegSetValueEx(result, NULL, 0, REG_SZ, (const BYTE*)stringval,
								  strlen(stringval) + 1);
					RegCloseKey(result);
					
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_FILETYPE "\\Shell\\BriskPlayer Play",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_FILETYPE "\\Shell\\BriskPlayer Play\\command",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					sprintf(stringval, "\"%s\" \"%%1\"", pathbuf);
					RegSetValueEx(result, NULL, 0, REG_SZ, (const BYTE*)stringval,
								  strlen(stringval) + 1);
					RegCloseKey(result);
					
					
					RegCreateKeyEx(HKEY_CLASSES_ROOT, ".m3u", 0, NULL,
								   REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegSetValueEx(result, NULL, 0, REG_SZ,
								  (const BYTE*)CIC_COOLPLAYER_PLAYLISTFILETYPE, strlen(CIC_COOLPLAYER_PLAYLISTFILETYPE) + 1);
					RegCloseKey(result);
					
					RegCreateKeyEx(HKEY_CLASSES_ROOT, ".pls", 0, NULL,
								   REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegSetValueEx(result, NULL, 0, REG_SZ,
								  (const BYTE*)CIC_COOLPLAYER_PLAYLISTFILETYPE, strlen(CIC_COOLPLAYER_PLAYLISTFILETYPE) + 1);
					RegCloseKey(result);
					
					RegCreateKeyEx(HKEY_CLASSES_ROOT, CIC_COOLPLAYER_PLAYLISTFILETYPE,
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegSetValueEx(result, NULL, 0, REG_SZ,
								  (const BYTE*)T(STR_APP_PLAYLIST_DESC), strlen(T(STR_APP_PLAYLIST_DESC)) + 1);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_PLAYLISTFILETYPE "\\DefaultIcon",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					sprintf(stringval, "%s,%d", pathbuf, 2);
					RegSetValueEx(result, NULL, 0, REG_SZ, (const BYTE*)stringval,
								  strlen(stringval) + 1);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_PLAYLISTFILETYPE "\\Shell", 0,
								   NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_PLAYLISTFILETYPE "\\Shell\\Open",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_PLAYLISTFILETYPE "\\Shell\\Open\\command",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					sprintf(stringval, "\"%s\" \"%%1\"", pathbuf);
					RegSetValueEx(result, NULL, 0, REG_SZ, (const BYTE*)stringval,
								  strlen(stringval) + 1);
					RegCloseKey(result);
					
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_PLAYLISTFILETYPE "\\Shell\\BriskPlayer Queue",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_PLAYLISTFILETYPE "\\Shell\\BriskPlayer Queue\\command",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					sprintf(stringval, "\"%s\" \"%%1\" -add", pathbuf);
					RegSetValueEx(result, NULL, 0, REG_SZ, (const BYTE*)stringval,
								  strlen(stringval) + 1);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_PLAYLISTFILETYPE "\\Shell\\BriskPlayer Play",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   CIC_COOLPLAYER_PLAYLISTFILETYPE "\\Shell\\BriskPlayer Play\\command",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					sprintf(stringval, "\"%s\" \"%%1\"", pathbuf);
					RegSetValueEx(result, NULL, 0, REG_SZ, (const BYTE*)stringval,
								  strlen(stringval) + 1);
					RegCloseKey(result);
					
					// Folder handlers
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   "Folder\\Shell\\BriskPlayer Play",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					RegCloseKey(result);
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   "Folder\\Shell\\BriskPlayer Play\\command",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					sprintf(stringval, "\"%s\" \"%%1\"", pathbuf);
					RegSetValueEx(result, NULL, 0, REG_SZ, (const BYTE*)stringval,
								  strlen(stringval) + 1);
					RegCloseKey(result);
					
					RegCreateKeyEx(HKEY_CLASSES_ROOT,
								   "Folder\\Shell\\BriskPlayer Queue\\command",
								   0, NULL, REG_OPTION_NON_VOLATILE,
								   KEY_ALL_ACCESS, NULL, &result,
								   &lpdwDisposition);
					sprintf(stringval, "\"%s\" \"%%1\" -add", pathbuf);
					RegSetValueEx(result, NULL, 0, REG_SZ, (const BYTE*)stringval,
								  strlen(stringval) + 1);
					RegCloseKey(result);
					
					
					RegDeleteKey(HKEY_CLASSES_ROOT, CIC_COOLPLAYER_FILETYPE "\\Shell\\Enqueue in BriskPlayer\\command");
					RegDeleteKey(HKEY_CLASSES_ROOT, CIC_COOLPLAYER_FILETYPE "\\Shell\\Enqueue in BriskPlayer\\");
					RegDeleteKey(HKEY_CLASSES_ROOT, CIC_COOLPLAYER_PLAYLISTFILETYPE "\\Shell\\Enqueue in BriskPlayer\\command");
					RegDeleteKey(HKEY_CLASSES_ROOT, CIC_COOLPLAYER_PLAYLISTFILETYPE "\\Shell\\Enqueue in BriskPlayer\\");
					
					MessageBox(hwndDlg,
							   T(STR_MSG_FILETYPES_REGISTERED),
							   T(STR_APP_NAME), MB_ICONINFORMATION);
					           
					break;
				}
				
				case IDC_ADDICONS:
				{
					char    pathname[MAX_PATH];
					char    startmenu[MAX_PATH];
					char    linkname[MAX_PATH];
					char    linkname2[MAX_PATH];
					
					LPITEMIDLIST ppidl;
					// int     buflen = MAX_PATH;
					// long    vartype = REG_SZ;
					
					GetModuleFileName(NULL, pathname, MAX_PATH);
					CoInitialize(NULL);
					
					SHGetSpecialFolderLocation(hwndDlg, CSIDL_STARTMENU,
											   &ppidl);
					                           
					SHGetPathFromIDList(ppidl, startmenu);
					
					sprintf(linkname, "%s\\BriskPlayer.lnk", startmenu);
					ExpandEnvironmentStrings(linkname, // pointer to string with environment variables
											 linkname2, // pointer to string with expanded environment
											 // variables
											 MAX_PATH);
					                         
					path_create_link(pathname, linkname2, NULL);
					SHGetSpecialFolderLocation(hwndDlg, CSIDL_DESKTOP, &ppidl);
					
					SHGetPathFromIDList(ppidl, startmenu);
					
					sprintf(linkname, "%s\\BriskPlayer.lnk", startmenu);
					ExpandEnvironmentStrings(linkname, // pointer to string with environment variables
											 linkname2, // pointer to string with expanded environment
											 // variables
											 MAX_PATH);
					                         
					path_create_link(pathname, linkname2, NULL);
					CoUninitialize();
					MessageBox(hwndDlg,
							   T(STR_MSG_ICONS_CREATED),
							   T(STR_APP_NAME), MB_ICONINFORMATION);
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
