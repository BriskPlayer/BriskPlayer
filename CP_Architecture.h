/**
 * @file CP_Architecture.h
 * @brief BriskPlayer Architecture and Coding Guidelines
 * 
 * @mainpage BriskPlayer Audio Player
 * 
 * @section intro Introduction
 * 
 * BriskPlayer (formerly CoolPlayer) is a fast, lightweight audio player for Windows.
 * Originally developed by Niek Albers (2000-2001), it has been modernized and updated
 * by Zach Bacon (2025).
 * 
 * @section arch Architecture Overview
 * 
 * The application follows a modular architecture with clear separation of concerns:
 * 
 * @subsection core Core Modules
 * 
 * | Module | Description |
 * |--------|-------------|
 * | main.c | Application entry point, main window procedure |
 * | globals.c/h | Global state and configuration structures |
 * | options.c | Settings dialogs and preference handling |
 * | profile.c | INI file configuration persistence |
 * 
 * @subsection player Player Engine
 * 
 * | Module | Description |
 * |--------|-------------|
 * | CPI_Player.c | Main player controller and state machine |
 * | CPI_Player_Engine.c | Audio decoding and playback engine |
 * | CPI_Player_CoDec_*.c | Format-specific codec implementations |
 * | CPI_Player_Output_*.c | Audio output backends (DirectSound, FAudio, Wave) |
 * 
 * @subsection playlist Playlist System
 * 
 * | Module | Description |
 * |--------|-------------|
 * | CPI_Playlist.c | Playlist management and operations |
 * | CPI_PlaylistItem.c | Individual playlist item handling |
 * | CPI_PlaylistWindow.c | Playlist UI and list view |
 * 
 * @subsection ui User Interface
 * 
 * | Module | Description |
 * |--------|-------------|
 * | CPI_Interface.c | Skinned interface rendering |
 * | CPI_InterfacePart.c | Interface component system |
 * | CPSK_Skin.c | Skin loading and parsing |
 * | skin.c | Skin management and switching |
 * 
 * @subsection stream Streaming
 * 
 * | Module | Description |
 * |--------|-------------|
 * | CPI_Stream.c | Abstract stream interface |
 * | CPI_Stream_LocalFile.c | Local file streaming |
 * | CPI_Stream_Internet.c | HTTP/Internet streaming |
 * 
 * @subsection util Utilities
 * 
 * | Module | Description |
 * |--------|-------------|
 * | CP_Constants.h | Centralized constants and magic numbers |
 * | CP_Cleanup.h | Resource cleanup macros |
 * | CP_Result.h | Standardized error codes |
 * | CP_Config.h/c | INI configuration abstraction |
 * | CP_Unicode.h | Unicode conversion utilities |
 * | CPString.c | String manipulation utilities |
 * | WindowsOS.c | Windows API compatibility layer |
 * | WinModern.c | Modern Windows API wrappers |
 * 
 * @section coding Coding Guidelines
 * 
 * @subsection naming Naming Conventions
 * 
 * - **Files**: PascalCase with underscore separators (e.g., CPI_Player.c)
 * - **Functions**: Module prefix + PascalCase (e.g., CPI_Player_Play)
 * - **Structs**: _CPs_ prefix for private, typedef to remove underscore
 * - **Constants**: CPC_ prefix with SCREAMING_CASE
 * - **Globals**: g_ prefix (e.g., g_bInitialized)
 * - **Pointers**: p prefix (e.g., pBuffer)
 * - **Handles**: h prefix (e.g., hPlayer, CP_HPLAYER)
 * 
 * @subsection prefixes Module Prefixes
 * 
 * | Prefix | Module |
 * |--------|--------|
 * | CPI_ | Player Interface (core player) |
 * | CPSK_ | Skin |
 * | CP_ | Common/Core |
 * | CLV_ | Custom List View |
 * | DLG_ | Dialog |
 * 
 * @subsection memory Memory Management
 * 
 * Always use the safe cleanup macros from CP_Cleanup.h:
 * 
 * @code
 * // Allocate
 * char* pBuffer = (char*)malloc(size);
 * 
 * // Use...
 * 
 * // Cleanup - sets pointer to NULL
 * SAFE_FREE(pBuffer);
 * @endcode
 * 
 * For GDI objects:
 * @code
 * HBITMAP hBmp = LoadBitmap(...);
 * // Use...
 * SAFE_DELETE_OBJECT(hBmp);
 * @endcode
 * 
 * @subsection errors Error Handling
 * 
 * Use CP_Result for functions that can fail:
 * 
 * @code
 * CP_Result MyFunction(int param)
 * {
 *     if (param < 0) return CP_ERROR_INVALID_PARAMETER;
 *     // ...
 *     return CP_OK;
 * }
 * 
 * // Caller:
 * CP_Result result = MyFunction(42);
 * if (CP_FAILED(result)) {
 *     CP_LOG_ERROR("MyFunction failed: %s\n", CP_ResultToString(result));
 *     return result;
 * }
 * @endcode
 * 
 * @subsection strings String Handling
 * 
 * Always use safe string functions:
 * 
 * @code
 * char buffer[256];
 * cp_strcpy_s(buffer, sizeof(buffer), source);
 * cp_strcat_s(buffer, sizeof(buffer), suffix);
 * cp_snprintf(buffer, sizeof(buffer), "Value: %d", value);
 * @endcode
 * 
 * For Unicode conversion, use CP_Unicode.h:
 * @code
 * wchar_t wbuffer[256];
 * CPU_Utf8ToWide(utf8String, wbuffer, 256);
 * @endcode
 * 
 * @subsection config Configuration
 * 
 * Use CP_Config for INI file access:
 * 
 * @code
 * int value = CPConfig_GetInt("Section", "Key", defaultValue);
 * CPConfig_SetInt("Section", "Key", newValue);
 * @endcode
 * 
 * @subsection i18n Internationalization
 * 
 * All user-visible strings should use the translation system:
 * 
 * @code
 * // In C code - returns UTF-8 string
 * const char* text = T(STR_MENU_OPEN);
 * 
 * // For Windows API (wide strings)
 * wchar_t wtext[256];
 * CPU_Utf8ToWide(T(STR_MENU_OPEN), wtext, 256);
 * SetWindowTextW(hWnd, wtext);
 * @endcode
 * 
 * @section build Building
 * 
 * BriskPlayer supports two build systems:
 * 
 * @subsection cmake CMake
 * @code
 * mkdir build && cd build
 * cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
 * ninja
 * @endcode
 * 
 * @subsection meson Meson
 * @code
 * meson setup build --buildtype=release
 * meson compile -C build
 * @endcode
 * 
 * @section compat Compatibility
 * 
 * - **Minimum OS**: Windows 7 (0x0601)
 * - **Compilers**: MSVC, GCC (MinGW-w64), Clang
 * - **C Standard**: C17
 * 
 * @section license License
 * 
 * BriskPlayer is licensed under the GNU General Public License v2.0 or later.
 * See license.md for details.
 */

#ifndef CP_ARCHITECTURE_H
#define CP_ARCHITECTURE_H

// This is a documentation-only header
// Include it to see the project documentation in your IDE

#endif // CP_ARCHITECTURE_H
