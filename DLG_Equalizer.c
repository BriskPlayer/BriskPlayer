/*
 * BriskPlayer - Blazing fast audio player.
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
// Equalizer Settings Dialog
//
// Modeless dialog presenting 8 vertical sliders (one per frequency band) with
// frequency labels, live dB readouts, a preset selector, and an enable toggle.
// All changes take effect immediately by calling main_set_eq().
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "globals.h"
#include "resource.h"
#include "DLG_Equalizer.h"
#include "CP_SafeGlobals.h"
#include "CPI_Player.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations for main.c helpers referenced here
////////////////////////////////////////////////////////////////////////////////

extern void main_set_eq(void);

////////////////////////////////////////////////////////////////////////////////
// Band configuration
////////////////////////////////////////////////////////////////////////////////

// Frequency labels matching the 8 EQ bands (classic 8-band layout)
// Used by the resource dialog; kept here for documentation purposes.
static const char* const g_szBandLabels[8] __attribute__((unused)) =
{
    "60Hz", "170Hz", "310Hz", "600Hz", "1kHz", "3kHz", "6kHz", "12kHz"
};

// Slider and value label control IDs, indexed 0-7
static const int g_nSliderIDs[8] =
{
    IDC_EQ_BAND_1, IDC_EQ_BAND_2, IDC_EQ_BAND_3, IDC_EQ_BAND_4,
    IDC_EQ_BAND_5, IDC_EQ_BAND_6, IDC_EQ_BAND_7, IDC_EQ_BAND_8
};

static const int g_nValueIDs[8] =
{
    IDC_EQ_VAL_1, IDC_EQ_VAL_2, IDC_EQ_VAL_3, IDC_EQ_VAL_4,
    IDC_EQ_VAL_5, IDC_EQ_VAL_6, IDC_EQ_VAL_7, IDC_EQ_VAL_8
};

////////////////////////////////////////////////////////////////////////////////
// Preset definitions
// Values are in the internal -127..+127 range (≈ -12 to +12 dB).
// Formula: internal = round(dB * 127 / 12)
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    const char* name;
    int         bands[8];  // indices 0-7 correspond to bands 1-8
} CPs_EQPreset;

static const CPs_EQPreset g_Presets[] =
{
    { "Flat",         {   0,   0,   0,   0,   0,   0,   0,   0 } },
    { "Bass Boost",   {  85,  64,  21,   0,   0,   0,   0,   0 } },
    { "Treble Boost", {   0,   0,   0,   0,  21,  64,  85, 106 } },
    { "Rock",         {  74,  53, -32, -42,  11,  53,  74,  74 } },
    { "Classical",    {  64,  53,  21,   0,   0,  11,  42,  53 } },
    { "Pop",          { -11,  42,  64,  53,   0, -21, -21, -21 } },
    { "Jazz",         {  42,  32,  11,  32, -21, -21,  11,  32 } },
    { "Dance",        {  85,  64,  21, -42, -21,   0,  64,  64 } },
};

#define EQ_PRESET_COUNT  ((int)(sizeof(g_Presets) / sizeof(g_Presets[0])))

// Slider logical range: -127 (cut) to +127 (boost).
// TBM_SETPOS receives this directly because we configured the range as such.
#define EQ_SLIDER_MIN   (-127)
#define EQ_SLIDER_MAX   ( 127)

////////////////////////////////////////////////////////////////////////////////
// Helpers
////////////////////////////////////////////////////////////////////////////////

// Convert an internal band value (-127..+127) to a displayable dB string.
static void FormatDB(int value, char* buf, int bufLen)
{
    float dB = (float)value * 12.0f / 127.0f;
    if (value == 0)
        sprintf_s(buf, bufLen, "0.0 dB");
    else if (dB > 0.0f)
        sprintf_s(buf, bufLen, "+%.1f dB", (double)dB);
    else
        sprintf_s(buf, bufLen, "%.1f dB", (double)dB);
}

// Push the current options.eq_settings into all 8 sliders and dB labels.
static void SyncSlidersToOptions(HWND hwndDlg)
{
    for (int i = 0; i < 8; i++)
    {
        int value = options.eq_settings[i + 1];

        // Clamp to legal range
        if (value > EQ_SLIDER_MAX) value = EQ_SLIDER_MAX;
        if (value < EQ_SLIDER_MIN) value = EQ_SLIDER_MIN;

        SendDlgItemMessage(hwndDlg, g_nSliderIDs[i], TBM_SETPOS, TRUE, (LPARAM)value);

        char buf[32];
        FormatDB(value, buf, sizeof(buf));
        SetDlgItemTextA(hwndDlg, g_nValueIDs[i], buf);
    }
}

// Read all 8 sliders into options.eq_settings, then apply to the engine and
// repaint the skin's embedded EQ sliders.
static void CommitSlidersToEngine(HWND hwndDlg)
{
    for (int i = 0; i < 8; i++)
    {
        int value = (int)SendDlgItemMessage(hwndDlg, g_nSliderIDs[i], TBM_GETPOS, 0, 0);

        // Clamp to legal range (defensive)
        if (value > EQ_SLIDER_MAX) value = EQ_SLIDER_MAX;
        if (value < EQ_SLIDER_MIN) value = EQ_SLIDER_MIN;

        options.eq_settings[i + 1] = value;

        char buf[32];
        FormatDB(value, buf, sizeof(buf));
        SetDlgItemTextA(hwndDlg, g_nValueIDs[i], buf);
    }

    main_set_eq();

    // Repaint the skin's embedded EQ sliders so they stay in sync
    if (windows.wnd_main)
        InvalidateRect(windows.wnd_main, NULL, FALSE);
}

// Load a preset by index, update sliders, and commit.
static void ApplyPreset(HWND hwndDlg, int presetIdx)
{
    if (presetIdx < 0 || presetIdx >= EQ_PRESET_COUNT)
        return;

    for (int i = 0; i < 8; i++)
        options.eq_settings[i + 1] = g_Presets[presetIdx].bands[i];

    SyncSlidersToOptions(hwndDlg);
    main_set_eq();

    if (windows.wnd_main)
        InvalidateRect(windows.wnd_main, NULL, FALSE);
}

////////////////////////////////////////////////////////////////////////////////
// Dialog procedure
////////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK eq_windowproc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            // Configure each trackbar
            for (int i = 0; i < 8; i++)
            {
                HWND hSlider = GetDlgItem(hwndDlg, g_nSliderIDs[i]);

                // Vertical slider: TBS_REVERSED means thumb-up = larger value (boost)
                SendMessage(hSlider, TBM_SETRANGEMIN, FALSE, (LPARAM)EQ_SLIDER_MIN);
                SendMessage(hSlider, TBM_SETRANGEMAX, TRUE,  (LPARAM)EQ_SLIDER_MAX);
                SendMessage(hSlider, TBM_SETTICFREQ,  (WPARAM)25, 0);
                SendMessage(hSlider, TBM_SETPAGESIZE, 0, (LPARAM)10);
                SendMessage(hSlider, TBM_SETLINESIZE, 0, (LPARAM)1);
            }

            // Populate preset combo
            HWND hCombo = GetDlgItem(hwndDlg, IDC_EQ_PRESET);
            for (int i = 0; i < EQ_PRESET_COUNT; i++)
                SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)g_Presets[i].name);
            SendMessage(hCombo, CB_SETCURSEL, 0, 0);  // Select "Flat" initially

            // Set the enable checkbox
            CheckDlgButton(hwndDlg, IDC_EQ_ENABLE,
                           options.equalizer ? BST_CHECKED : BST_UNCHECKED);

            // Load current band values into sliders
            SyncSlidersToOptions(hwndDlg);

            return TRUE;
        }

        case WM_VSCROLL:
        case WM_HSCROLL:
        {
            // A trackbar sent a scroll notification — find which one
            HWND hSlider = (HWND)lParam;
            for (int i = 0; i < 8; i++)
            {
                if (hSlider == GetDlgItem(hwndDlg, g_nSliderIDs[i]))
                {
                    CommitSlidersToEngine(hwndDlg);
                    // Clear preset selection since the user customised manually
                    SendDlgItemMessage(hwndDlg, IDC_EQ_PRESET, CB_SETCURSEL, (WPARAM)-1, 0);
                    break;
                }
            }
            return TRUE;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDC_EQ_ENABLE:
                    options.equalizer = (IsDlgButtonChecked(hwndDlg, IDC_EQ_ENABLE) == BST_CHECKED);
                    main_set_eq();
                    if (windows.wnd_main)
                        InvalidateRect(windows.wnd_main, NULL, FALSE);
                    break;

                case IDC_EQ_FLAT:
                    // Reset all bands to 0 (flat response)
                    for (int i = 1; i <= 8; i++)
                        options.eq_settings[i] = 0;
                    SyncSlidersToOptions(hwndDlg);
                    SendDlgItemMessage(hwndDlg, IDC_EQ_PRESET, CB_SETCURSEL, 0, 0); // "Flat"
                    main_set_eq();
                    if (windows.wnd_main)
                        InvalidateRect(windows.wnd_main, NULL, FALSE);
                    break;

                case IDC_EQ_PRESET:
                    if (HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        int sel = (int)SendDlgItemMessage(hwndDlg, IDC_EQ_PRESET,
                                                          CB_GETCURSEL, 0, 0);
                        if (sel != CB_ERR)
                            ApplyPreset(hwndDlg, sel);
                    }
                    break;

                case IDOK:
                case IDCANCEL:
                    // Just hide the modeless dialog rather than destroying it
                    ShowWindow(hwndDlg, SW_HIDE);
                    break;
            }
            return TRUE;
        }

        case WM_CLOSE:
            ShowWindow(hwndDlg, SW_HIDE);
            return TRUE;
    }

    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////////////////////

void eq_create(HWND hWndParent)
{
    if (windows.wnd_equalizer == NULL)
    {
        windows.wnd_equalizer = CreateDialog(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(IDD_EQUALIZER),
            hWndParent,
            (DLGPROC)eq_windowproc);
    }

    if (windows.wnd_equalizer)
    {
        // Sync sliders to current options before showing
        SyncSlidersToOptions(windows.wnd_equalizer);
        CheckDlgButton(windows.wnd_equalizer, IDC_EQ_ENABLE,
                       options.equalizer ? BST_CHECKED : BST_UNCHECKED);
        ShowWindow(windows.wnd_equalizer, SW_SHOWNORMAL);
        SetForegroundWindow(windows.wnd_equalizer);
    }
}
