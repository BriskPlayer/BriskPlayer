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

#include "stdafx.h"
#include "CPI_Gettext.h"
#include "WindowsOS.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <winnls.h>
#endif

// Global state with C23 initialization
static bool g_initialized = false;
static char g_current_language[16] = "en";
static char g_locale_directory[MAX_PATH] = "./locale";

// Thread-local storage for translation buffers
_Thread_local char* tl_translation_buffer = NULL;
_Thread_local size_t tl_buffer_size = 0;

// Compile-time constants
#define TRANSLATION_BUFFER_SIZE 1024
#define MAX_DOMAIN_NAME 64

// Initialize gettext system with C23 features
BOOL CPG_Initialize(const GetTextConfig* config)
{
    if (g_initialized) {
        return TRUE;
    }
    
    // Use designated initializer fallback if config is NULL
    const GetTextConfig defaultConfig = DEFAULT_GETTEXT_CONFIG;
    const GetTextConfig* actualConfig = config ? config : &defaultConfig;
    
    // Set locale directory
    strncpy(g_locale_directory, actualConfig->directory, sizeof(g_locale_directory) - 1);
    g_locale_directory[sizeof(g_locale_directory) - 1] = '\0';
    
#ifdef ENABLE_NLS
    // Set locale to user's environment
    if (setlocale(LC_ALL, "") == NULL) {
        // Fallback to C locale
        setlocale(LC_ALL, "C");
    }
    
    // Bind text domain
    if (bindtextdomain(actualConfig->domain, g_locale_directory) == NULL) {
        return FALSE;
    }
    
    // Set text domain
    if (textdomain(actualConfig->domain) == NULL) {
        return FALSE;
    }
    
    // Enable UTF-8 output if requested
    if (actualConfig->use_utf8) {
        bind_textdomain_codeset(actualConfig->domain, "UTF-8");
    }
    
#ifdef _WIN32
    // Set Windows console to UTF-8 for proper display
    if (actualConfig->use_utf8) {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }
#endif

#endif // ENABLE_NLS
    
    // Detect system language if not explicitly set
    if (strlen(g_current_language) <= 2) {
#ifdef _WIN32
        const char* systemLang = CPG_GetSystemLanguage();
        if (systemLang) {
            strncpy(g_current_language, systemLang, sizeof(g_current_language) - 1);
            g_current_language[sizeof(g_current_language) - 1] = '\0';
        }
#else
        const char* lang = getenv("LANG");
        if (lang && strlen(lang) >= 2) {
            strncpy(g_current_language, lang, 2);
            g_current_language[2] = '\0';
        }
#endif
    }
    
    g_initialized = true;
    return TRUE;
}

// Set current language with C23 auto variables
void CPG_SetLanguage(const char* language)
{
    if (!language || strlen(language) == 0) {
        return;
    }
    
    // Copy language code with bounds checking
    size_t langLen = strlen(language);
    size_t copyLen = (langLen < sizeof(g_current_language) - 1) ? langLen : sizeof(g_current_language) - 1;
    
    strncpy(g_current_language, language, copyLen);
    g_current_language[copyLen] = '\0';
    
#ifdef ENABLE_NLS
    // Create locale string (e.g., "de_DE.UTF-8")
    char locale_string[32];
    snprintf(locale_string, sizeof(locale_string), "%s.UTF-8", language);
    
    // Try to set the locale
    if (setlocale(LC_ALL, locale_string) == NULL) {
        // Fallback to language code only
        if (setlocale(LC_ALL, language) == NULL) {
            // Final fallback to POSIX
            setlocale(LC_ALL, "POSIX");
        }
    }
#endif

#ifdef _WIN32
    // Update Windows locale
    CPG_SetWindowsLocale(language);
#endif
}

// Get current language
const char* CPG_GetCurrentLanguage(void)
{
    return g_current_language;
}

// Load additional translation domain
BOOL CPG_LoadDomain(const char* domain, const char* directory)
{
    if (!domain || !directory) {
        return FALSE;
    }
    
#ifdef ENABLE_NLS
    if (bindtextdomain(domain, directory) == NULL) {
        return FALSE;
    }
    
    // Set UTF-8 encoding for this domain too
    bind_textdomain_codeset(domain, "UTF-8");
#endif
    
    return TRUE;
}

// Thread-safe translation function
const char* CPG_GetTranslationThreadSafe(const char* msgid)
{
    if (!msgid) {
        return "";
    }
    
    // Initialize thread-local buffer if needed
    if (!tl_translation_buffer) {
        tl_buffer_size = TRANSLATION_BUFFER_SIZE;
        tl_translation_buffer = malloc(tl_buffer_size);
        if (!tl_translation_buffer) {
            return msgid; // Fallback to original string
        }
    }
    
#ifdef ENABLE_NLS
    const char* translated = gettext(msgid);
    size_t transLen = strlen(translated);
    
    // Resize buffer if needed
    if (transLen >= tl_buffer_size) {
        tl_buffer_size = transLen + 256; // Add some extra space
        char* newBuffer = realloc(tl_translation_buffer, tl_buffer_size);
        if (!newBuffer) {
            return translated; // Return original gettext result
        }
        tl_translation_buffer = newBuffer;
    }
    
    strcpy(tl_translation_buffer, translated);
    return tl_translation_buffer;
#else
    // Copy msgid to thread buffer for consistency
    size_t msgLen = strlen(msgid);
    if (msgLen >= tl_buffer_size) {
        tl_buffer_size = msgLen + 256;
        char* newBuffer = realloc(tl_translation_buffer, tl_buffer_size);
        if (!newBuffer) {
            return msgid;
        }
        tl_translation_buffer = newBuffer;
    }
    
    strcpy(tl_translation_buffer, msgid);
    return tl_translation_buffer;
#endif
}

// Enumerate available languages
int CPG_EnumerateLanguages(LanguageInfo* languages, int maxLanguages)
{
    if (!languages || maxLanguages <= 0) {
        return 0;
    }
    
    int count = 0;
    
    // Add built-in English
    if (count < maxLanguages) {
        LanguageInfo* lang = &languages[count];
        strcpy(lang->code, "en");
        strcpy(lang->name, "English");
        strcpy(lang->region, "US");
        lang->available = true;
        count++;
    }
    
#ifdef _WIN32
    // Enumerate locale directories on Windows
    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", g_locale_directory);
    
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                findData.cFileName[0] != '.') {
                
                // Check if this is a valid language directory
                char moPath[MAX_PATH];
                snprintf(moPath, sizeof(moPath), "%s\\%s\\LC_MESSAGES\\%s.mo",
                        g_locale_directory, findData.cFileName, BRISKPLAYER_DOMAIN);
                
                DWORD fileAttr = GetFileAttributesA(moPath);
                if (fileAttr != INVALID_FILE_ATTRIBUTES && count < maxLanguages) {
                    LanguageInfo* lang = &languages[count];
                    strncpy(lang->code, findData.cFileName, sizeof(lang->code) - 1);
                    lang->code[sizeof(lang->code) - 1] = '\0';
                    
                    // Map language codes to display names
                    if (strcmp(findData.cFileName, "de") == 0) {
                        strcpy(lang->name, "Deutsch");
                        strcpy(lang->region, "DE");
                    } else if (strcmp(findData.cFileName, "fr") == 0) {
                        strcpy(lang->name, "Français");
                        strcpy(lang->region, "FR");
                    } else if (strcmp(findData.cFileName, "es") == 0) {
                        strcpy(lang->name, "Español");
                        strcpy(lang->region, "ES");
                    } else if (strcmp(findData.cFileName, "it") == 0) {
                        strcpy(lang->name, "Italiano");
                        strcpy(lang->region, "IT");
                    } else if (strcmp(findData.cFileName, "pt") == 0) {
                        strcpy(lang->name, "Português");
                        strcpy(lang->region, "PT");
                    } else if (strcmp(findData.cFileName, "ru") == 0) {
                        strcpy(lang->name, "Русский");
                        strcpy(lang->region, "RU");
                    } else if (strcmp(findData.cFileName, "ja") == 0) {
                        strcpy(lang->name, "日本語");
                        strcpy(lang->region, "JP");
                    } else if (strcmp(findData.cFileName, "zh") == 0) {
                        strcpy(lang->name, "中文");
                        strcpy(lang->region, "CN");
                    } else if (strcmp(findData.cFileName, "ko") == 0) {
                        strcpy(lang->name, "한국어");
                        strcpy(lang->region, "KR");
                    } else if (strcmp(findData.cFileName, "nl") == 0) {
                        strcpy(lang->name, "Nederlands");
                        strcpy(lang->region, "NL");
                    } else if (strcmp(findData.cFileName, "sv") == 0) {
                        strcpy(lang->name, "Svenska");
                        strcpy(lang->region, "SE");
                    } else if (strcmp(findData.cFileName, "no") == 0) {
                        strcpy(lang->name, "Norsk");
                        strcpy(lang->region, "NO");
                    } else if (strcmp(findData.cFileName, "da") == 0) {
                        strcpy(lang->name, "Dansk");
                        strcpy(lang->region, "DK");
                    } else if (strcmp(findData.cFileName, "fi") == 0) {
                        strcpy(lang->name, "Suomi");
                        strcpy(lang->region, "FI");
                    } else if (strcmp(findData.cFileName, "pl") == 0) {
                        strcpy(lang->name, "Polski");
                        strcpy(lang->region, "PL");
                    } else {
                        // Generic fallback - capitalize first letter
                        strncpy(lang->name, findData.cFileName, sizeof(lang->name) - 1);
                        lang->name[sizeof(lang->name) - 1] = '\0';
                        if (lang->name[0] >= 'a' && lang->name[0] <= 'z') {
                            lang->name[0] = lang->name[0] - 'a' + 'A';
                        }
                        lang->region[0] = '\0';
                    }
                    
                    lang->available = true;
                    count++;
                }
            }
        } while (FindNextFileA(hFind, &findData) && count < maxLanguages);
        
        FindClose(hFind);
    }
#else
    // Unix/Linux implementation - scan locale directories
    // Implementation would be similar but using readdir()
#endif
    
    return count;
}

// Check if language is available
BOOL CPG_IsLanguageAvailable(const char* languageCode)
{
    if (!languageCode) {
        return FALSE;
    }
    
    // English is always available
    if (strcmp(languageCode, "en") == 0) {
        return TRUE;
    }
    
    // Check for .mo file
    char moPath[MAX_PATH];
    snprintf(moPath, sizeof(moPath), "%s/%s/LC_MESSAGES/%s.mo",
            g_locale_directory, languageCode, BRISKPLAYER_DOMAIN);
    
#ifdef _WIN32
    DWORD fileAttr = GetFileAttributesA(moPath);
    return (fileAttr != INVALID_FILE_ATTRIBUTES);
#else
    FILE* file = fopen(moPath, "rb");
    if (file) {
        fclose(file);
        return TRUE;
    }
    return FALSE;
#endif
}

#ifdef _WIN32
// Function pointer types for Vista+ APIs
typedef int (WINAPI *GetUserDefaultLocaleNameProc)(LPWSTR, int);
typedef LCID (WINAPI *LocaleNameToLCIDProc)(LPCWSTR, DWORD);

// Cache for dynamically loaded function pointers
static GetUserDefaultLocaleNameProc g_pGetUserDefaultLocaleName = NULL;
static LocaleNameToLCIDProc g_pLocaleNameToLCID = NULL;
static BOOL g_bVistaApisChecked = FALSE;

// Initialize Vista+ API pointers once
static void CPG_InitVistaApis(void)
{
    if (g_bVistaApisChecked) {
        return;
    }
    
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (hKernel32) {
        g_pGetUserDefaultLocaleName = 
            (GetUserDefaultLocaleNameProc)GetProcAddress(hKernel32, "GetUserDefaultLocaleName");
        g_pLocaleNameToLCID = 
            (LocaleNameToLCIDProc)GetProcAddress(hKernel32, "LocaleNameToLCID");
    }
    
    g_bVistaApisChecked = TRUE;
}

// Get system language on Windows (XP+ compatible)
const char* CPG_GetSystemLanguage(void)
{
    static char langCode[8] = {0};
    
    // Initialize Vista+ API pointers once
    CPG_InitVistaApis();
    
    // Try to use Vista+ API if available
    if (g_pGetUserDefaultLocaleName) {
        wchar_t localeName[85]; // LOCALE_NAME_MAX_LENGTH = 85
        if (g_pGetUserDefaultLocaleName(localeName, 85) > 0) {
            // Convert to narrow string and extract language part
            char narrowName[32];
            if (WideCharToMultiByte(CP_UTF8, 0, localeName, -1, narrowName, sizeof(narrowName), NULL, NULL) > 0) {
                // Extract just the language part (before '-' or '_')
                char* separator = strchr(narrowName, '-');
                if (!separator) separator = strchr(narrowName, '_');
                
                if (separator) {
                    size_t langLen = (size_t)(separator - narrowName);
                    if (langLen < sizeof(langCode)) {
                        strncpy(langCode, narrowName, langLen);
                        langCode[langLen] = '\0';
                        return langCode;
                    }
                } else if ((size_t)strlen(narrowName) < sizeof(langCode)) {
                    strcpy(langCode, narrowName);
                    return langCode;
                }
            }
        }
    }
    
    // Fallback to XP-compatible API
    LCID lcid = GetUserDefaultLCID();
    char localeInfo[10];
    if (GetLocaleInfoA(lcid, LOCALE_SISO639LANGNAME, localeInfo, sizeof(localeInfo)) > 0) {
        strncpy(langCode, localeInfo, sizeof(langCode) - 1);
        langCode[sizeof(langCode) - 1] = '\0';
        return langCode;
    }
    
    return "en"; // Ultimate fallback
}

// Set Windows locale (XP+ compatible)
BOOL CPG_SetWindowsLocale(const char* languageCode)
{
    if (!languageCode) {
        return FALSE;
    }
    
    // Initialize Vista+ API pointers once
    CPG_InitVistaApis();
    
    // Create Windows locale string
    char localeString[32];
    snprintf(localeString, sizeof(localeString), "%s_%s.UTF-8", 
             languageCode, 
             (strcmp(languageCode, "en") == 0) ? "US" : 
             (strcmp(languageCode, "de") == 0) ? "DE" :
             (strcmp(languageCode, "fr") == 0) ? "FR" : "XX");
    
    // Try to use Vista+ API if available
    if (g_pLocaleNameToLCID) {
        wchar_t wLocaleString[32];
        MultiByteToWideChar(CP_UTF8, 0, localeString, -1, wLocaleString, sizeof(wLocaleString) / sizeof(wchar_t));
        LCID lcid = g_pLocaleNameToLCID(wLocaleString, 0);
        if (lcid != 0) {
            return SetThreadLocale(lcid);
        }
    }
    
    // XP fallback: Try to map common language codes to LCIDs directly
    LCID lcid = 0;
    if (strcmp(languageCode, "en") == 0) lcid = 0x0409; // en-US
    else if (strcmp(languageCode, "de") == 0) lcid = 0x0407; // de-DE
    else if (strcmp(languageCode, "fr") == 0) lcid = 0x040C; // fr-FR
    else if (strcmp(languageCode, "es") == 0) lcid = 0x0C0A; // es-ES
    else if (strcmp(languageCode, "it") == 0) lcid = 0x0410; // it-IT
    else if (strcmp(languageCode, "ja") == 0) lcid = 0x0411; // ja-JP
    else if (strcmp(languageCode, "zh") == 0) lcid = 0x0804; // zh-CN
    else if (strcmp(languageCode, "ko") == 0) lcid = 0x0412; // ko-KR
    else if (strcmp(languageCode, "ru") == 0) lcid = 0x0419; // ru-RU
    else if (strcmp(languageCode, "pt") == 0) lcid = 0x0416; // pt-BR
    else if (strcmp(languageCode, "nl") == 0) lcid = 0x0413; // nl-NL
    else if (strcmp(languageCode, "pl") == 0) lcid = 0x0415; // pl-PL
    else if (strcmp(languageCode, "tr") == 0) lcid = 0x041F; // tr-TR
    else if (strcmp(languageCode, "sv") == 0) lcid = 0x041D; // sv-SE
    else if (strcmp(languageCode, "cs") == 0) lcid = 0x0405; // cs-CZ
    
    if (lcid != 0) {
        return SetThreadLocale(lcid);
    }
    
    return FALSE;
}

// Get Unicode translation (for Windows APIs)
// Uses thread-local storage to avoid data races if called from multiple threads
wchar_t* CPG_GetTranslationW(const char* msgid)
{
    static __thread wchar_t wideBuffer[512];
    
    const char* translated = _(msgid);
    if (MultiByteToWideChar(CP_UTF8, 0, translated, -1, wideBuffer, 
                           sizeof(wideBuffer) / sizeof(wchar_t)) > 0) {
        return wideBuffer;
    }
    
    return L"Translation Error";
}
#endif

// Cleanup function
void CPG_Cleanup(void)
{
    if (tl_translation_buffer) {
        free(tl_translation_buffer);
        tl_translation_buffer = NULL;
        tl_buffer_size = 0;
    }
    
    g_initialized = false;
}