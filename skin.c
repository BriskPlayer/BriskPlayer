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
#include "CompositeFile.h"
#include "resource.h"

// Forward declarations for CPSK functions
void CPSK_DestroySkin(CPs_Skin* pSkin);
CPs_Skin* CPSK_LoadSkin(CP_COMPOSITEFILE hComposite, const char* pcSkinFile, const unsigned int iFileSize);

// Forward declarations for image functions
CPs_Image* CPIG_CreateImage_FromFile(const char* pcFilename);
void CPIG_DestroyImage(CPs_Image* pImage);

// Helper function to create CPs_Image from external file path
CPs_Image* CreateImageFromExternalFile(const char* filePath);

// Forward declaration for playlist INI skin loading
void main_load_playlist_skin_from_ini(const char* iniFilePath);

// Forward declarations for playlist window functions
void CPlaylistWindow_Create(void);
void CPlaylistWindow_Destroy(void);
void CPlaylistWindow_SetVisible(const BOOL bNewVisibleState);

// Forward declarations for interface functions
HWND IF_GetHWnd(CP_HINTERFACE hInterface);

int main_set_default_skin(void)
{

	HINSTANCE hInstance;
	float   positionpercentage;
	
	if (Skin.Object[PositionSlider].maxw == 1)
	{
		positionpercentage =
			(float) globals.main_int_track_position /
			(float) Skin.Object[PositionSlider].h;
	}
	
	else
	{
		positionpercentage =
			(float) globals.main_int_track_position /
			(float) Skin.Object[PositionSlider].w;
	}
	
	globals.main_int_title_scroll_position = 0;
	globals.mail_int_title_scroll_max_position = 0;

	memset(&Skin, 0, sizeof(Skin));
	
	main_skin_set_struct_value(PlaySwitch, 52, 108, 31, 32, 0, 52, 108, 31,
							   32, "");
	main_skin_set_struct_value(StopSwitch, 120, 113, 27, 27, 0, 120, 113, 27, 27,
							   "");
	main_skin_set_struct_value(PauseSwitch, 87, 111, 29, 29, 0, 87, 111,
							   29, 29, "");
	main_skin_set_struct_value(RepeatSwitch, 274, 33, 33, 32, 0, 274, 33,
							   33, 32, "");
	main_skin_set_struct_value(ShuffleSwitch, 306, 32, 30, 26, 0, 306, 32,
							   30, 26, "");
	main_skin_set_struct_value(EqSwitch, 359, 14, 51, 20, 0, 359, 14, 51, 20,
							   "");
	main_skin_set_struct_value(MinimizeButton, 262, 0, 27, 14, 0, 0, 0, 0, 0,
							   "");
	main_skin_set_struct_value(ExitButton, 315, 0, 28, 14, 0, 0, 0, 0, 0,
							   "");
	main_skin_set_struct_value(NextSkinButton, 288, 0, 28, 14, 0, 0, 0, 0,
							   0, "");
	main_skin_set_struct_value(EjectButton, 186, 113, 27, 28, 0, 0, 0, 0, 0,
							   "");
	main_skin_set_struct_value(NextButton, 151, 114, 26, 25, 0, 0, 0, 0, 0,
							   "");
	main_skin_set_struct_value(PrevButton, 22, 114, 26, 25, 0, 0, 0, 0, 0,
							   "");
	main_skin_set_struct_value(MoveArea, 0, 0, 229, 12, 0, 0, 0, 0, 0, "");
	main_skin_set_struct_value(PlaylistButton, 214, 113, 28, 28, 0, 0, 0, 0,
							   0, "");
	main_skin_set_struct_value(VolumeSlider, 252, 121, 65, 11, 0, 252, 121, 20, 11,
							   "");
	main_skin_set_struct_value(PositionSlider, 19, 92, 298, 11, 0, 18, 92, 20, 11,
							   "");
	main_skin_set_struct_value(Eq1, 360, 40, 9, 53, 1, 360, 40, 9, 11, "");
	main_skin_set_struct_value(Eq2, 378, 40, 9, 53, 1, 378, 40, 9, 11, "");
	main_skin_set_struct_value(Eq3, 396, 40, 9, 53, 1, 396, 40, 9, 11, "");
	main_skin_set_struct_value(Eq4, 414, 40, 9, 53, 1, 414, 40, 9, 11, "");
	main_skin_set_struct_value(Eq5, 432, 40, 9, 53, 1, 432, 40, 9, 11, "");
	main_skin_set_struct_value(Eq6, 450, 40, 9, 53, 1, 450, 40, 9, 11, "");
	main_skin_set_struct_value(Eq7, 468, 40, 9, 53, 1, 468, 40, 9, 11, "");
	main_skin_set_struct_value(Eq8, 486, 40, 9, 53, 1, 486, 40, 9, 11, "");
	main_skin_set_struct_value(SongtitleText, 30, 70, 6, 11, 30, 0, 0, 0,
							   0, "");
	main_skin_set_struct_value(TrackText, 42, 37, 14, 21, 0, 0, 0, 0, 0,
							   "");
	main_skin_set_struct_value(TimeText, 105, 37, 14, 21, 0, 0, 0, 0, 0,
							   "");

	main_skin_set_struct_value(BitrateText, 280, 70, 6, 11, 4, 0, 0, 0, 0,
							   "");
	main_skin_set_struct_value(FreqText, 316, 70, 6, 11, 4, 0, 0, 0, 0, "");
	
	Skin.transparentcolor = 0xff00ff;
	hInstance = GetModuleHandle(NULL);
	graphics.bmp_main_up =
		(HBITMAP) LoadImage(hInstance, MAKEINTRESOURCE(IDB_MAINUP),
							IMAGE_BITMAP, 0, 0, 0L);
	graphics.bmp_main_down =
		(HBITMAP) LoadImage(hInstance, MAKEINTRESOURCE(IDB_MAINDOWN),
							IMAGE_BITMAP, 0, 0, 0L);
	graphics.bmp_main_switch = graphics.bmp_main_down;
	graphics.bmp_main_time_font =
		(HBITMAP) LoadImage(hInstance, MAKEINTRESOURCE(IDB_MAINBIGFONT),
							IMAGE_BITMAP, 0, 0, 0L);
	graphics.bmp_main_track_font = graphics.bmp_main_time_font;
	graphics.bmp_main_title_font =
		(HBITMAP) LoadImage(hInstance, MAKEINTRESOURCE(IDB_MAINSMALLFONT),
							IMAGE_BITMAP, 0, 0, 0L);
	                        
	if (Skin.Object[PositionSlider].maxw == 1)
	{
		globals.main_int_track_position =
			(int)((float)(Skin.Object[PositionSlider].h) *
				  positionpercentage);
	}
	
	else
	{
		globals.main_int_track_position =
			(int)((float)(Skin.Object[PositionSlider].w) *
				  positionpercentage);
	}
	
	globals.main_bool_skin_next_is_default = FALSE;
	
	main_update_title_text();
	main_skin_select_menu("Default");
	
	return TRUE;
}

int     main_add_tooltips(HWND hWnd, BOOL update)
{

	TOOLINFO ti;  // tool information
	
	char   *tips[] =
	{
		"Play",
		"Stop",
		"Pause",
		"Eject",
		"Repeat",
		"Shuffle",
		"Equalizer",
		"Next",
		"Previous",
		"Playlist",
		"Minimize",
		"Skinswitch",
		"Exit"
	};
	
	int     teller;
	ti.cbSize = sizeof(TOOLINFO);
	ti.uFlags = 0;
	ti.hwnd = hWnd;
	ti.hinst = GetModuleHandle(NULL);
	
	for (teller = PlaySwitch; teller <= ExitButton; teller++)
	{
		ti.uId = (UINT) teller;
		
		if (*Skin.Object[teller].tooltip)
			ti.lpszText = (LPSTR) Skin.Object[teller].tooltip;
		else
			ti.lpszText = (LPSTR) tips[teller];
			
		ti.rect.left = Skin.Object[teller].x;
		ti.rect.top = Skin.Object[teller].y;
		ti.rect.right = Skin.Object[teller].x + Skin.Object[teller].w;
		ti.rect.bottom = Skin.Object[teller].y + Skin.Object[teller].h;
		
		SendMessage(windows.wnd_tooltip,
					update ? TTM_NEWTOOLRECT : TTM_ADDTOOL, 0,
					(LPARAM)(LPTOOLINFO) & ti);
		            
		if (update == TRUE)
			SendMessage(windows.wnd_tooltip, TTM_UPDATETIPTEXT, 0,
						(LPARAM)(LPTOOLINFO) & ti);
			            
	}
	
	return 1;
}

int



main_skin_set_struct_value(int object, int x, int y, int w, int h, int maxw, int x2, int y2, int w2, int h2,
						   char *tooltip)
{
	Skin.Object[object].x = x;
	Skin.Object[object].y = y;
	Skin.Object[object].w = w;
	Skin.Object[object].h = h;
	Skin.Object[object].maxw = maxw;
	Skin.Object[object].x2 = x2;
	Skin.Object[object].y2 = y2;
	Skin.Object[object].w2 = w2;
	Skin.Object[object].h2 = h2;
	strcpy(Skin.Object[object].tooltip, tooltip);
	
	return TRUE;
}

int     main_skin_open(char *name)
{
	(void)name;  // Suppress unused parameter warning
	char    pathbuf[MAX_PATH];
	char    values[32768];
	char   *textposition;
	char    buffer[4096];
	char    errorbuf[4096] = "";
	// int     teller = 0;
	int     returnval;
	HINSTANCE hInstance;
	
	Associate associate[] =
	{
		{ "PlaySwitch", PlaySwitch },
		{ "StopSwitch", StopSwitch },
		{ "PauseSwitch", PauseSwitch },
		{ "EjectButton", EjectButton },
		{ "RepeatSwitch", RepeatSwitch },
		{ "ShuffleSwitch", ShuffleSwitch },
		{ "EqSwitch", EqSwitch },
		{ "NextButton", NextButton },
		{ "PrevButton", PrevButton },
		{ "PlaylistButton", PlaylistButton },
		{ "MinimizeButton", MinimizeButton },
		{ "NextSkinButton", NextSkinButton },
		{ "ExitButton", ExitButton },
		{ "MoveArea", MoveArea },
		{ "VolumeSlider", VolumeSlider },
		{ "PositionSlider", PositionSlider },
		{ "Eq1", Eq1 },
		{ "Eq2", Eq2 },
		{ "Eq3", Eq3 },
		{ "Eq4", Eq4 },
		{ "Eq5", Eq5 },
		{ "Eq6", Eq6 },
		{ "Eq7", Eq7 },
		{ "Eq8", Eq8 },
		{ "SongtitleText", SongtitleText },
		{ "TrackText", TrackText },
		{ "TimeText", TimeText },
		{ "BitrateText", BitrateText },
		{ "FreqText", FreqText }
	};
	float   positionpercentage;
	
	if (Skin.Object[PositionSlider].maxw == 1)
	{
		positionpercentage =
			(float) globals.main_int_track_position /
			(float) Skin.Object[PositionSlider].h;
	}
	
	else
	{
		positionpercentage =
			(float) globals.main_int_track_position /
			(float) Skin.Object[PositionSlider].w;
	}
	
	globals.main_int_title_scroll_position = 0;
	globals.mail_int_title_scroll_max_position = 0;
	
	if (*options.main_skin_file == 0)
	{
		MessageBox(GetForegroundWindow(), "No Skin file selected!",
				   "Error", MB_ICONERROR);
		options.use_default_skin = TRUE;
		return FALSE;
	}
	
	else
		strcpy(pathbuf, (char*)options.main_skin_file);
		
	memset(&Skin, 0, sizeof(Skin));
	
	GetPrivateProfileString(NULL, NULL, NULL,
							buffer, sizeof(buffer), pathbuf);
	                        
	returnval = GetPrivateProfileSection("BriskPlayer Skin", // address of section name
										 values, // address of return buffer
										 32767, // size of return buffer
										 pathbuf // address of initialization filename
										);
	                                    
	if (returnval == 0)
	{
		char    textbuf[MAX_PATH + 50];
		sprintf(textbuf, "Not a valid BriskPlayer Skin file: %s", pathbuf);
		MessageBox(GetForegroundWindow(), textbuf, "error", MB_ICONERROR);
		options.use_default_skin = TRUE;
		return FALSE;
	}
	
	textposition = values;
	
	while (*textposition != 0)
	{
	
		main_skin_check_ini_value(textposition, associate);
		textposition = textposition + strlen(textposition) + 1;
	}
	
	path_remove_filespec(pathbuf);
	
	strcat(pathbuf, Skin.CoolUp);
	hInstance = GetModuleHandle(NULL);
	DeleteObject(graphics.bmp_main_up);
	graphics.bmp_main_up =
		(HBITMAP) LoadImage(hInstance, pathbuf, IMAGE_BITMAP, 0, 0,
							LR_LOADFROMFILE);
	                        
	if (!graphics.bmp_main_up)
	{
		strcat(errorbuf, pathbuf);
		strcat(errorbuf, "\n");
	}
	
	path_remove_filespec(pathbuf);
	
	strcat(pathbuf, Skin.CoolDown);
	DeleteObject(graphics.bmp_main_down);
	graphics.bmp_main_down =
		(HBITMAP) LoadImage(hInstance, pathbuf, IMAGE_BITMAP, 0, 0,
							LR_LOADFROMFILE);
	                        
	if (!graphics.bmp_main_down)
	{
		strcat(errorbuf, pathbuf);
		strcat(errorbuf, "\n");
	}
	
	path_remove_filespec(pathbuf);
	
	strcat(pathbuf, Skin.CoolSwitch);
	DeleteObject(graphics.bmp_main_switch);
	graphics.bmp_main_switch =
		(HBITMAP) LoadImage(hInstance, pathbuf, IMAGE_BITMAP, 0, 0,
							LR_LOADFROMFILE);
	                        
	if (!graphics.bmp_main_switch)
	{
		strcat(errorbuf, pathbuf);
		strcat(errorbuf, "\n");
	}
	
	path_remove_filespec(pathbuf);
	
	strcat(pathbuf, Skin.aTimeFont);
	DeleteObject(graphics.bmp_main_time_font);
	graphics.bmp_main_time_font =
		(HBITMAP) LoadImage(hInstance, pathbuf, IMAGE_BITMAP, 0, 0,
							LR_LOADFROMFILE);
	                        
	if (!graphics.bmp_main_time_font)
	{
		strcat(errorbuf, pathbuf);
		strcat(errorbuf, "\n");
	}
	
	path_remove_filespec(pathbuf);
	
	strcat(pathbuf, Skin.aTrackFont);
	DeleteObject(graphics.bmp_main_track_font);
	graphics.bmp_main_track_font =
		(HBITMAP) LoadImage(hInstance, pathbuf, IMAGE_BITMAP, 0, 0,
							LR_LOADFROMFILE);
	                        
	if (!graphics.bmp_main_track_font)
	{
		strcat(errorbuf, pathbuf);
		strcat(errorbuf, "\n");
	}
	
	path_remove_filespec(pathbuf);
	
	strcat(pathbuf, Skin.aTextFont);
	DeleteObject(graphics.bmp_main_title_font);
	graphics.bmp_main_title_font =
		(HBITMAP) LoadImage(hInstance, pathbuf, IMAGE_BITMAP, 0, 0,
							LR_LOADFROMFILE);
	                        
	if (!graphics.bmp_main_title_font)
	{
		strcat(errorbuf, pathbuf);
		strcat(errorbuf, "\n");
	}
	
	if (!graphics.bmp_main_up || !graphics.bmp_main_down
			|| !graphics.bmp_main_switch || !graphics.bmp_main_time_font
			|| !graphics.bmp_main_title_font || !graphics.bmp_main_track_font)
	{
		char    errorstring[5000];
		
		sprintf(errorstring, "Can\'t load bitmaps!\n%s", errorbuf);
		MessageBox(GetForegroundWindow(), errorstring, "error",
				   MB_ICONERROR);
		options.use_default_skin = TRUE;
		return FALSE;
		
	}
	
	if (Skin.Object[PositionSlider].maxw == 1)
	{
		globals.main_int_track_position =
			(int)((float)(Skin.Object[PositionSlider].h) *
				  positionpercentage);
		          
	}
	
	else
	{
		globals.main_int_track_position =
			(int)((float)(Skin.Object[PositionSlider].w) *
				  positionpercentage);
	}
	
	main_update_title_text();
	
	// Always recreate the playlist window after skin change to ensure consistency
	{
		extern CPs_Skin* glb_pSkin;
		BOOL bPlaylistWindowWasVisible = FALSE;
		
		
		// Check if playlist window is currently visible
		if (windows.m_hifPlaylist && IsWindowVisible(IF_GetHWnd(windows.m_hifPlaylist)))
		{
			bPlaylistWindowWasVisible = TRUE;
		}
		
		// Destroy current playlist window if it exists
		if (windows.m_hifPlaylist)
		{
			CPlaylistWindow_Destroy();
		}
		
		// Update playlist skin colors to match main skin if we have a playlist skin
		if (glb_pSkin)
		{
			// Update the transparent color to match main skin
			glb_pSkin->m_clrTransparent = Skin.transparentcolor;
			
			// Derive harmonious colors from main skin's transparent color
			COLORREF mainColor = Skin.transparentcolor;
			BYTE r = GetRValue(mainColor);
			BYTE g = GetGValue(mainColor);
			BYTE b = GetBValue(mainColor);
			
			// Generate colors that contrast well with the main skin
			// Use complementary colors for good readability
			BYTE textR = (r > 128) ? r - 64 : r + 128;
			BYTE textG = (g > 128) ? g - 64 : g + 128;
			BYTE textB = (b > 128) ? b - 64 : b + 128;
			
			// Apply derived colors to playlist skin elements
			glb_pSkin->mpl_ListTextColour = RGB(textR, textG, textB);
			glb_pSkin->mpl_ListTextColour_Selected = RGB(255 - textR, 255 - textG, 255 - textB);
			glb_pSkin->mpl_ListTextColour_HotItem = RGB((textR + 255) / 2, (textG + 255) / 2, (textB + 255) / 2);
			glb_pSkin->mpl_ListHeaderColour = RGB(textR, textG, textB);
		}
		
		// If there's a separate playlist skin file specified and playlist skin is enabled, try to load it
		if (*options.playlist_skin_file && options.use_playlist_skin)
		{
			CP_COMPOSITEFILE hComposite;
			char* pcSkinFile;
			unsigned int iFileSize;
			
			// Check if it's an INI file (case insensitive)
			char* file_ext = strrchr((char*)options.playlist_skin_file, '.');
			if (file_ext && stricmp(file_ext, ".ini") == 0)
			{
				// Load INI-style playlist skin with bitmap files
				main_load_playlist_skin_from_ini((char*)options.playlist_skin_file);
			}
			else
			{
				// Destroy current playlist skin
				if (glb_pSkin)
				{
					CPSK_DestroySkin(glb_pSkin);
					glb_pSkin = NULL;
				}
				
				// Load new playlist skin from external file
				hComposite = CF_Create_FromFile((char*)options.playlist_skin_file);
				if (hComposite)
				{
					if (CF_GetSubFile(hComposite, "Skin.def", (void **) &pcSkinFile, &iFileSize))
					{
						glb_pSkin = CPSK_LoadSkin(hComposite, pcSkinFile, iFileSize);
						free(pcSkinFile);
					}
					CF_Destroy(hComposite);
				}
				
				// If loading failed, fall back to default
				if (!glb_pSkin)
				{
					hComposite = CF_Create_FromResource(NULL, IDR_DEFAULTSKIN, "SKIN");
					if (hComposite)
					{
						CF_GetSubFile(hComposite, "Skin.def", (void **) &pcSkinFile, &iFileSize);
						glb_pSkin = CPSK_LoadSkin(hComposite, pcSkinFile, iFileSize);
						free(pcSkinFile);
						CF_Destroy(hComposite);
					}
				}
			}
			
			// If loading failed, fall back to default
			if (!glb_pSkin)
			{
				hComposite = CF_Create_FromResource(NULL, IDR_DEFAULTSKIN, "SKIN");
				if (hComposite)
				{
					CF_GetSubFile(hComposite, "Skin.def", (void **) &pcSkinFile, &iFileSize);
					glb_pSkin = CPSK_LoadSkin(hComposite, pcSkinFile, iFileSize);
					free(pcSkinFile);
					CF_Destroy(hComposite);
				}
			}
			
			// Apply color matching to the newly loaded skin too
			if (glb_pSkin)
			{
				// Apply the same harmonious colors for consistency
				COLORREF mainColor = Skin.transparentcolor;
				BYTE r = GetRValue(mainColor);
				BYTE g = GetGValue(mainColor);
				BYTE b = GetBValue(mainColor);
				
				BYTE textR = (r > 128) ? r - 64 : r + 128;
				BYTE textG = (g > 128) ? g - 64 : g + 128;
				BYTE textB = (b > 128) ? b - 64 : b + 128;
				
				glb_pSkin->m_clrTransparent = Skin.transparentcolor;
				glb_pSkin->mpl_ListTextColour = RGB(textR, textG, textB);
				glb_pSkin->mpl_ListTextColour_Selected = RGB(255 - textR, 255 - textG, 255 - textB);
				glb_pSkin->mpl_ListTextColour_HotItem = RGB((textR + 255) / 2, (textG + 255) / 2, (textB + 255) / 2);
				glb_pSkin->mpl_ListHeaderColour = RGB(textR, textG, textB);
			}
		}
		
		// Recreate playlist window (using either new skin or existing default skin with updated colors)
		if (glb_pSkin)
		{
			// Destroy existing playlist window if it exists
			if (windows.m_hifPlaylist)
			{
				CPlaylistWindow_Destroy();
			}
			
			// Create new playlist window with the updated skin
			CPlaylistWindow_Create();
			
			// Restore visibility state
			if (bPlaylistWindowWasVisible)
			{
				CPlaylistWindow_SetVisible(TRUE);
			}
		}
	}

	return 1;
}

void    main_skin_check_ini_value(char *textposition,
								  Associate * associate)
{
	char    name[128] = "";
	int     x = 0, y = 0, w = 0, h = 0, maxw = 0, x2 = 0, y2 = 0, w2 =
										 0, h2 = 0;
	char    tooltip[100] = "";
	int teller = 0;
	
	while (teller < strlen(textposition))
	{
		if (textposition[teller] == '=' || textposition[teller] == ',')
			textposition[teller] = ' ';
			
		teller++;
	}
	
	// sscanf(textposition, "%s %d %d %d %d %d %d %d %d %d %[^\0]",
	sscanf(textposition, "%s %d %d %d %d %d %d %d %d %d %s",
		   name, &x, &y, &w, &h, &maxw, &x2, &y2, &w2, &h2, tooltip);
	       
	for (teller = 0; teller < Lastone; teller++)
	{
		if (stricmp(name, associate[teller].name) == 0)
		{
			main_skin_set_struct_value(associate[teller].Object, x, y, w,
									   h, maxw, x2, y2, w2, h2, tooltip);
			return;
		}
		
		if (stricmp(name, "PlaylistSkin") == 0)
		{
			char    pathbuf[MAX_PATH];
			
			if (path_is_relative(textposition + strlen(name) + 1))
			{
				strcpy(pathbuf, (char*)options.main_skin_file);
				path_remove_filespec(pathbuf);
				strcat(pathbuf, textposition + strlen(name) + 1);
			}
			
			else
				strcpy(pathbuf, textposition + strlen(name) + 1);
				
			if (!globals.playlist_bool_force_skin_from_options)
				strcpy((char*)options.playlist_skin_file, pathbuf);
		}
		
		if (stricmp(name, "transparentcolor") == 0)
		{
			unsigned int     colortext;
			sscanf(textposition, "%s %x", name, &colortext);
			Skin.transparentcolor = colortext;
			return;
		}
		
		if (stricmp(name, "BmpCoolUp") == 0)
		{
			strcpy(Skin.CoolUp, textposition + strlen(name) + 1);
		}
		
		if (stricmp(name, "BmpCoolDown") == 0)
		{
			strcpy(Skin.CoolDown, textposition + strlen(name) + 1);
		}
		
		if (stricmp(name, "BmpCoolSwitch") == 0)
		{
			strcpy(Skin.CoolSwitch, textposition + strlen(name) + 1);
		}
		
		if (stricmp(name, "BmpTextFont") == 0)
		{
			strcpy(Skin.aTextFont, textposition + strlen(name) + 1);
		}
		
		if (stricmp(name, "BmpTimeFont") == 0)
		{
			strcpy(Skin.aTimeFont, textposition + strlen(name) + 1);
		}
		
		if (stricmp(name, "BmpTrackFont") == 0)
		{
			strcpy(Skin.aTrackFont, textposition + strlen(name) + 1);
		}
		
		if (stricmp(name, "NextSkin") == 0)
		{
			if (stricmp(textposition + strlen(name) + 1, "default") == 0)
			{
				globals.main_bool_skin_next_is_default = TRUE;
			}
			
			else
			{
				char    drive[_MAX_DRIVE];
				char    fname[MAX_PATH];
				char    modpathbuf[MAX_PATH];
				char    ext[_MAX_EXT];
				char    dir[_MAX_DIR];
				char    skinfile2[MAX_PATH];
				strcpy(skinfile2, (char*)options.main_skin_file);
				path_remove_filespec(skinfile2);
				
				main_get_program_path(GetModuleHandle(NULL), modpathbuf,
									  MAX_PATH);
				_splitpath(textposition + strlen(name) + 1, drive, dir,
						   fname, ext);
				           
				if (strcmp(drive, "") == 0)
				{
					sprintf((char*)options.main_skin_file, "%s%s%s", skinfile2,
							fname, ext);
				}
				
				else
					strcpy((char*)options.main_skin_file,
						   textposition + strlen(name) + 1);
					       
				if (_access((char*)options.main_skin_file, 0) == -1)
				{
					sprintf((char*)options.main_skin_file, "%s%s%s%s", modpathbuf,
							dir, fname, ext);
				}
				
				globals.main_bool_skin_next_is_default = FALSE;
			}
		}
	}
}

//
// Create CPs_Image from an external bitmap file
//
CPs_Image* CreateImageFromExternalFile(const char* fullPath)
{
	if (!fullPath || !*fullPath)
		return NULL;
		
	// Load the bitmap using LoadImage
	HBITMAP hBitmap = (HBITMAP)LoadImage(NULL, fullPath, IMAGE_BITMAP, 0, 0, 
		LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	
	if (!hBitmap)
		return NULL;
		
	// Create CPs_Image structure
	CPs_Image* pImage = (CPs_Image*)malloc(sizeof(CPs_Image));
	if (!pImage)
	{
		DeleteObject(hBitmap);
		return NULL;
	}
	
	// Initialize the image structure
	memset(pImage, 0, sizeof(*pImage));
	pImage->m_hbmImage = hBitmap;
	
	// Get bitmap dimensions
	BITMAP bm;
	if (GetObject(hBitmap, sizeof(bm), &bm))
	{
		pImage->m_szSize.cx = bm.bmWidth;
		pImage->m_szSize.cy = bm.bmHeight;
	}
	
	return pImage;
}

//
// Load playlist skin from INI file with external bitmap files
//
void main_load_playlist_skin_from_ini(const char* iniFilePath)
{
	char pathbuf[MAX_PATH];
	char skinDir[MAX_PATH];
	
	// Basic safety check
	if (!iniFilePath || !*iniFilePath)
	{
		// Debug output
		return;
	}
		
	// Check if INI file exists
	if (_access(iniFilePath, 0) != 0)
	{
		return;
	}
	
	// Create a new playlist skin structure
	CPs_Skin* pNewSkin = (CPs_Skin*)malloc(sizeof(CPs_Skin));
	if (!pNewSkin)
		return;
		
	// Initialize the skin structure to zero
	memset(pNewSkin, 0, sizeof(*pNewSkin));
	
	// Set basic required fields first
	pNewSkin->m_dwSkinVersion = CPC_SKINVERSION_200;
	
	// Set transparent color safely - check if main skin is loaded
	if (Skin.transparentcolor != 0)
		pNewSkin->m_clrTransparent = Skin.transparentcolor;
	else
		pNewSkin->m_clrTransparent = RGB(255, 0, 255); // Default magenta
	
	// Get the directory containing the INI file
	strcpy(skinDir, iniFilePath);
	char* lastSlash = strrchr(skinDir, '\\');
	if (!lastSlash)
		lastSlash = strrchr(skinDir, '/');
	if (lastSlash)
		*lastSlash = '\0';
	else
		strcpy(skinDir, ".");
		
	// Load bitmaps only if the INI file has them specified
	GetPrivateProfileString("PlaylistSkin", "Background", "", pathbuf, sizeof(pathbuf), iniFilePath);
	if (*pathbuf)
	{
		char fullPath[MAX_PATH];
		sprintf(fullPath, "%s\\%s", skinDir, pathbuf);
		pNewSkin->mpl_pBackground = CreateImageFromExternalFile(fullPath);
	}
	
	GetPrivateProfileString("PlaylistSkin", "ListBackground", "", pathbuf, sizeof(pathbuf), iniFilePath);
	if (*pathbuf)
	{
		char fullPath[MAX_PATH];
		sprintf(fullPath, "%s\\%s", skinDir, pathbuf);
		pNewSkin->mpl_pListBackground = CreateImageFromExternalFile(fullPath);
	}
	
	// Read color values from INI file with safe defaults
	pNewSkin->mpl_ListTextColour = (COLORREF)GetPrivateProfileInt("PlaylistSkin", "ListTextColor", RGB(0, 0, 0), iniFilePath);
	pNewSkin->mpl_ListTextColour_Selected = (COLORREF)GetPrivateProfileInt("PlaylistSkin", "ListSelectedTextColor", RGB(255, 255, 255), iniFilePath);
	pNewSkin->mpl_ListTextColour_HotItem = (COLORREF)GetPrivateProfileInt("PlaylistSkin", "ListHotTextColor", RGB(128, 128, 255), iniFilePath);
	pNewSkin->mpl_ListHeaderColour = (COLORREF)GetPrivateProfileInt("PlaylistSkin", "ListHeaderColor", RGB(64, 64, 64), iniFilePath);
	
	// Apply color harmony with main skin if no custom colors defined or if enabled
	BOOL useColorHarmony = GetPrivateProfileInt("PlaylistSkin", "UseColorHarmony", 1, iniFilePath);
	if (useColorHarmony && Skin.transparentcolor != 0)
	{
		COLORREF mainColor = Skin.transparentcolor;
		BYTE r = GetRValue(mainColor);
		BYTE g = GetGValue(mainColor);
		BYTE b = GetBValue(mainColor);
		
		BYTE textR = (r > 128) ? r - 64 : r + 128;
		BYTE textG = (g > 128) ? g - 64 : g + 128;
		BYTE textB = (b > 128) ? b - 64 : b + 128;
		
		pNewSkin->mpl_ListTextColour = RGB(textR, textG, textB);
		pNewSkin->mpl_ListTextColour_Selected = RGB(255 - textR, 255 - textG, 255 - textB);
		pNewSkin->mpl_ListTextColour_HotItem = RGB((textR + 255) / 2, (textG + 255) / 2, (textB + 255) / 2);
		pNewSkin->mpl_ListHeaderColour = RGB(textR, textG, textB);
	}
	
	// Destroy current playlist skin AFTER the new one is created successfully
	if (glb_pSkin)
	{
		CPSK_DestroySkin(glb_pSkin);
		glb_pSkin = NULL;
	}
	
	// Set the global playlist skin
	glb_pSkin = pNewSkin;
}
