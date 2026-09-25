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
#include "CPI_Gettext.h"
#include "WinModern.h"
#include "CPI_DpiScale.h"

// Forward declarations for CPSK functions
void CPSK_DestroySkin(CPs_Skin* pSkin);
CPs_Skin* CPSK_LoadSkin(CP_COMPOSITEFILE hComposite, const char* pcSkinFile, const unsigned int iFileSize);

// Forward declarations for playlist window functions
void CPlaylistWindow_Create(void);
void CPlaylistWindow_Destroy(void);
void CPlaylistWindow_SetVisible(const BOOL bNewVisibleState);

// Forward declarations for interface functions
HWND IF_GetHWnd(CP_HINTERFACE hInterface);

// Name -> enum Objects table shared by every builtin variant's Skin.ini
// parse (see main_skin_load_builtin_variant). File-scope const: built once,
// not per call - previously this table was rebuilt on the stack inside the
// now-removed external-file loader (main_skin_open) on every skin load.
static const Associate kSkinAssociate[ReducedSize] =
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

// Sub-file name of each builtin variant's Skin.ini inside res/Default.CPSkin
// (IDR_DEFAULTSKIN). See res/build_default_cpskin.ps1 for how the archive
// is packaged. Backslash separator: .NET's ZipFile.CreateFromDirectory
// stores entry names with the OS-native separator on Windows, and
// CF_FindFile does an exact stricmp - no path normalization - so this must
// match the packaged archive's actual entry names byte-for-byte.
static const char* const kBuiltinSkinIniSubfile[BUILTIN_SKIN_COUNT] =
{
	"Normal\\Skin.ini",	// BUILTIN_SKIN_NORMAL
	"EQ\\Skin.ini",		// BUILTIN_SKIN_EQ
	"Shade\\Skin.ini",	// BUILTIN_SKIN_SHADE
};

// Feeds each ini-style data line of raw (non-null-terminated) text pulled
// from a CPSkin subfile into the unchanged main_skin_check_ini_value().
// Replaces what GetPrivateProfileSection() used to provide for free when
// skins lived as real files on disk: splits on \r/\n, skips blank lines and
// ';'-comment/'['-section-header lines, and copies each line into a private
// stack buffer so main_skin_check_ini_value()'s in-place '='/','->' '
// mutation never touches the shared CF_GetSubFile buffer.
static void main_skin_parse_ini_text(const char *pcText, unsigned int iLen)
{
	unsigned int lineStart = 0, i;

	for (i = 0; i <= iLen; i++)
	{
		if (i != iLen && pcText[i] != '\r' && pcText[i] != '\n')
			continue;

		if (i > lineStart)
		{
			const char *p = pcText + lineStart;
			unsigned int lineLen = i - lineStart;

			while (lineLen && (*p == ' ' || *p == '\t'))
			{
				p++;
				lineLen--;
			}

			if (lineLen && *p != ';' && *p != '[')
			{
				char line[512];
				unsigned int copyLen = lineLen < sizeof(line) - 1 ? lineLen : sizeof(line) - 1;
				memcpy(line, p, copyLen);
				line[copyLen] = '\0';
				main_skin_check_ini_value(line, (Associate*)kSkinAssociate);
			}
		}

		lineStart = i + 1;
	}
}

// Computes the label main_skin_select_menu()/the Skin submenu use for a
// given skin: "Default" for the embedded skin (NULL/empty path), or the
// external file's base name with ".CPSkin" stripped otherwise. Shared by
// this file's menu-select call and main.c's menu population so the checked
// label and the inserted label can never drift apart.
void main_skin_get_display_name(const char* pcSkinPath, char* pcOut, size_t cbOut)
{
	const char* pcFileName;
	size_t len, extlen;
	static const char ext[] = ".CPSkin";

	if (!pcSkinPath || !pcSkinPath[0])
	{
		strcpy_s(pcOut, cbOut, "Default");
		return;
	}

	pcFileName = strrchr(pcSkinPath, '\\');
	pcFileName = pcFileName ? pcFileName + 1 : pcSkinPath;
	strcpy_s(pcOut, cbOut, pcFileName);

	len = strlen(pcOut);
	extlen = sizeof(ext) - 1;
	if (len > extlen && stricmp(pcOut + len - extlen, ext) == 0)
		pcOut[len - extlen] = '\0';
}

// Loads one of the three builtin main/EQ/shade window skins from
// res/Default.CPSkin (IDR_DEFAULTSKIN) - the same embedded CPSkin archive
// CPSK_Initialise() already uses for the playlist skin. Coordinates come
// from that variant's packaged Skin.ini (parsed via the classic, unchanged
// main_skin_check_ini_value()); bitmaps come from the filenames that
// parse just wrote into Skin.CoolUp/CoolDown/CoolSwitch/aTimeFont/
// aTrackFont/aTextFont, loaded via CF_GetSubFile + WIC_LoadImageFromMemory -
// the exact same pattern CPI_Image.c's CPIG_CreateImage_FromSubFile already
// uses for the playlist's bitmaps.
static int main_skin_load_builtin_variant(BuiltinSkinVariant variant)
{
	float positionpercentage;
	CP_COMPOSITEFILE hComposite;
	char *pcIniText = NULL;
	unsigned int iIniLen = 0;
	char errorbuf[4096] = "";

	if (Skin.Object[PositionSlider].maxw == 1)
		positionpercentage = (float)globals.main_int_track_position / (float)Skin.Object[PositionSlider].h;
	else
		positionpercentage = (float)globals.main_int_track_position / (float)Skin.Object[PositionSlider].w;

	globals.main_int_title_scroll_position = 0;
	globals.mail_int_title_scroll_max_position = 0;

	memset(&Skin, 0, sizeof(Skin));

	hComposite = CF_Create_ForActiveSkin();
	if (!hComposite || !CF_GetSubFile(hComposite, kBuiltinSkinIniSubfile[variant],
									  (void**)&pcIniText, &iIniLen))
	{
		if (hComposite)
			CF_Destroy(hComposite);
		MessageBoxA(GetForegroundWindow(), T(STR_ERR_INVALID_SKIN), T(STR_ERR_ERROR), MB_ICONERROR);
		return FALSE;
	}

	main_skin_parse_ini_text(pcIniText, iIniLen);
	free(pcIniText);

#define LOAD_SKIN_BMP(dst, subfileNameField) \
	do { \
		void *pData_; unsigned int iLen_; \
		DeleteObject(dst); \
		dst = NULL; \
		if (CF_GetSubFile(hComposite, (subfileNameField), &pData_, &iLen_)) \
		{ \
			dst = WIC_LoadImageFromMemory(pData_, iLen_, NULL, NULL); \
			free(pData_); \
		} \
		if (!dst) \
		{ \
			strcat_s(errorbuf, sizeof(errorbuf), (subfileNameField)); \
			strcat_s(errorbuf, sizeof(errorbuf), "\n"); \
		} \
	} while (0)

	LOAD_SKIN_BMP(graphics.bmp_main_up,         Skin.CoolUp);
	LOAD_SKIN_BMP(graphics.bmp_main_down,       Skin.CoolDown);
	LOAD_SKIN_BMP(graphics.bmp_main_switch,     Skin.CoolSwitch);
	LOAD_SKIN_BMP(graphics.bmp_main_time_font,  Skin.aTimeFont);
	LOAD_SKIN_BMP(graphics.bmp_main_track_font, Skin.aTrackFont);
	LOAD_SKIN_BMP(graphics.bmp_main_title_font, Skin.aTextFont);

#undef LOAD_SKIN_BMP

	CF_Destroy(hComposite);

	if (!graphics.bmp_main_up || !graphics.bmp_main_down || !graphics.bmp_main_switch
			|| !graphics.bmp_main_time_font || !graphics.bmp_main_track_font || !graphics.bmp_main_title_font)
	{
		char errorstring[5000];
		snprintf(errorstring, sizeof(errorstring), "%s\n%s", T(STR_ERR_CANT_LOAD_BITMAPS), errorbuf);
		MessageBoxA(GetForegroundWindow(), errorstring, T(STR_ERR_ERROR), MB_ICONERROR);
		return FALSE;
	}

	if (Skin.Object[PositionSlider].maxw == 1)
		globals.main_int_track_position = (int)((float)(Skin.Object[PositionSlider].h) * positionpercentage);
	else
		globals.main_int_track_position = (int)((float)(Skin.Object[PositionSlider].w) * positionpercentage);

	globals.builtin_skin_variant = variant;

	DPI_ApplySkinScaling();
	main_update_title_text();
	{
		char skin_label[MAX_PATH];
		main_skin_get_display_name(options.active_skin_path, skin_label, sizeof(skin_label));
		main_skin_select_menu(skin_label);
	}

	return TRUE;
}

int main_set_default_skin(void)
{
	return main_skin_load_builtin_variant(BUILTIN_SKIN_NORMAL);
}

// Set the built-in shade (compact) skin
int main_set_shade_skin(void)
{
	return main_skin_load_builtin_variant(BUILTIN_SKIN_SHADE);
}

// Set the built-in EQ skin (with EQ panel visible)
int main_set_eq_skin(void)
{
	return main_skin_load_builtin_variant(BUILTIN_SKIN_EQ);
}

// Set the next built-in skin variant (cycles through Normal -> EQ -> Shade -> Normal)
int main_set_next_builtin_skin(void)
{
	// Cycle to the next variant
	BuiltinSkinVariant nextVariant = (globals.builtin_skin_variant + 1) % BUILTIN_SKIN_COUNT;

	switch (nextVariant)
	{
		case BUILTIN_SKIN_EQ:
			return main_set_eq_skin();
		case BUILTIN_SKIN_SHADE:
			return main_set_shade_skin();
		case BUILTIN_SKIN_NORMAL:
		default:
			return main_set_default_skin();
	}
}

// Switches the active skin: pcSkinPath NULL/"" reverts to the embedded
// Default skin; otherwise it must be the full path of an external .CPSkin
// file discovered in options.skins_folder_path. Reloads both the playlist
// skin and whichever main-window variant (Normal/EQ/Shade) is currently on
// screen. On failure, rolls back to the previously-active skin and shows an
// error rather than silently landing on a different skin than the one the
// user actually picked.
void main_skin_switch(const char* pcSkinPath)
{
	char previous[MAX_PATH];
	BOOL bSuccess;

	strcpy_s(previous, sizeof(previous), options.active_skin_path);
	strcpy_s(options.active_skin_path, sizeof(options.active_skin_path),
			 (pcSkinPath && pcSkinPath[0]) ? pcSkinPath : "");

	CPSK_Uninitialise();
	bSuccess = CPSK_Initialise();

	if (bSuccess)
		bSuccess = main_skin_load_builtin_variant(globals.builtin_skin_variant);

	// The playlist window is created once at startup and otherwise never
	// rebuilt - it's not just holding a glb_pSkin pointer, its subparts
	// (IF_AddSubPart_CommandButton, etc.) were wired up at creation time
	// against the *old* glb_pSkin's CPs_CommandTarget/CPs_Image_WithState
	// objects, which CPSK_Uninitialise() just freed. Destroying and
	// recreating it rebuilds those subparts against whichever glb_pSkin
	// ends up active below - without this, the window keeps referencing
	// freed memory (visually looks unchanged, then crashes on the next
	// hover/hit-test against a dangling pointer).
	CPlaylistWindow_Destroy();

	if (!bSuccess)
	{
		strcpy_s(options.active_skin_path, sizeof(options.active_skin_path), previous);
		CPSK_Uninitialise();
		CPSK_Initialise();
		main_skin_load_builtin_variant(globals.builtin_skin_variant);
		CPlaylistWindow_Create();
		MessageBoxA(GetForegroundWindow(), T(STR_ERR_INVALID_SKIN), T(STR_ERR_ERROR), MB_ICONERROR);
		return;
	}

	CPlaylistWindow_Create();
	options_write();
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
	strcpy_s(Skin.Object[object].tooltip, sizeof(Skin.Object[object].tooltip), tooltip);

	return TRUE;
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
	sscanf_s(textposition, "%s %d %d %d %d %d %d %d %d %d %s",
		   name, (unsigned)sizeof(name), &x, &y, &w, &h, &maxw, &x2, &y2, &w2, &h2, tooltip, (unsigned)sizeof(tooltip));

	// Bounded by ReducedSize, not Lastone: `associate[]` (built in the
	// caller) has exactly ReducedSize entries (PlaySwitch..FreqText);
	// Lastone is ReducedSize + 1, so `teller < Lastone` read one element
	// past the end of the array on every skin .ini line parsed.
	// (Note: `associate` is a pointer parameter here, not the array itself —
	// sizeof(associate) would give sizeof(Associate*), not the real count.)
	for (teller = 0; teller < ReducedSize; teller++)
	{
		if (stricmp(name, associate[teller].name) == 0)
		{
			main_skin_set_struct_value(associate[teller].Object, x, y, w,
									   h, maxw, x2, y2, w2, h2, tooltip);
			return;
		}

		if (stricmp(name, "transparentcolor") == 0)
		{
			unsigned int     colortext;
			sscanf_s(textposition, "%s %x", name, (unsigned)sizeof(name), &colortext);
			Skin.transparentcolor = colortext;
			return;
		}


	if (stricmp(name, "BmpCoolUp") == 0)
	{
		strcpy_s(Skin.CoolUp, sizeof(Skin.CoolUp), textposition + strlen(name) + 1);
	}
	if (stricmp(name, "BmpCoolDown") == 0)
	{
		strcpy_s(Skin.CoolDown, sizeof(Skin.CoolDown), textposition + strlen(name) + 1);
	}
	if (stricmp(name, "BmpCoolSwitch") == 0)
	{
		strcpy_s(Skin.CoolSwitch, sizeof(Skin.CoolSwitch), textposition + strlen(name) + 1);
	}
	if (stricmp(name, "BmpTextFont") == 0)
	{
		strcpy_s(Skin.aTextFont, sizeof(Skin.aTextFont), textposition + strlen(name) + 1);
	}
	if (stricmp(name, "BmpTimeFont") == 0)
	{
		strcpy_s(Skin.aTimeFont, sizeof(Skin.aTimeFont), textposition + strlen(name) + 1);
	}
	if (stricmp(name, "BmpTrackFont") == 0)
	{
		strcpy_s(Skin.aTrackFont, sizeof(Skin.aTrackFont), textposition + strlen(name) + 1);
	}
	}
}
