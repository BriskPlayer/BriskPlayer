cmake_minimum_required(VERSION 3.20)

# Note: CXX language is required because TagLib (even tag_c.lib) is a C++ library
# Your source code is pure C, but we need C++ runtime to link against TagLib
project(briskplayer VERSION 3.0.0 LANGUAGES C CXX)

# Set C standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Set C++ standard for TagLib compatibility
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(ENABLE_NLS "Enable internationalization support" ON)
option(BUILD_STATIC "Build with static linking (all libraries linked statically)" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# For backwards compatibility
if(DEFINED STATIC_LINK AND NOT DEFINED BUILD_STATIC)
    set(BUILD_STATIC ${STATIC_LINK})
endif()

# Configure static linking preferences
if(BUILD_STATIC)
    # Prefer static libraries when using find_library
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib ${CMAKE_FIND_LIBRARY_SUFFIXES})
    
    # Set BLA_STATIC for BLAS/LAPACK if needed
    set(BLA_STATIC ON)
    
    message(STATUS "Static build enabled - preferring static libraries")
endif()

# Platform-specific settings
if(MSVC)
    add_compile_options(/W3 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32 _WINDOWS)
    
    # Windows XP compatibility
    add_compile_definitions(_WIN32_WINNT=0x0501 WINVER=0x0501)
    
    # Disable vectorized string operations that require newer runtime symbols
    # This avoids the ___std_find_*_trivial_pos symbols that may not be available
    add_compile_options(/d2FH4-)
    
    # Use static runtime - /MT for Release, /MTd for Debug
    # Note: vcpkg x86-windows-static uses /MT (Release runtime)
    # For Debug builds with vcpkg static libs, use x86-windows-static-md or build in Release
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # If building Debug with x86-windows-static, force release runtime to match vcpkg libs
    if(VCPKG_TARGET_TRIPLET STREQUAL "x86-windows-static" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
        message(WARNING "Building Debug with Release runtime (/MT) to match vcpkg x86-windows-static libraries")
    endif()
    
    # NOTE: If you get linker errors about ___std_find_* symbols, your vcpkg libraries
    # were built with a different toolset (v142 vs v143). To fix:
    # - Use: cmake -T v142 ... to match vcpkg's v142 toolset
    # - OR rebuild vcpkg libraries: vcpkg remove taglib:x86-windows-static && vcpkg install taglib:x86-windows-static
else()
    # Windows XP compatibility for MinGW
    if(CMAKE_CXX_COMPILER MATCHES "mingw")
        add_compile_definitions(_WIN32_WINNT=0x0501 WINVER=0x0501)
        # Use XP-compatible threading in libstdc++
        add_compile_definitions(_GLIBCXX_USE_C99_STDINT_TR1)
    endif()
    
    # GCC/Clang
    add_compile_options(-Wall -Wextra)
    
    # Static linking for MinGW
    if(BUILD_STATIC AND MINGW)
        if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # Clang on Windows uses different flags
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
        else()
            # GCC/MinGW-w64
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
            # Use g++ for linking since we link C++ libraries (TagLib)
            set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
        endif()
    endif()
endif()

# Source files
set(BRISK_SOURCES
    about.c
    bitmap2region.c
    CLV_ListView.c
    CompositeFile.c
    CPI_CircleBuffer.c
    CPI_Equaliser_Basic.c
    CPI_TagLib.c
    CPI_AlbumArtTooltip.c
    CPI_Image.c
    CPI_Indicators.c
    CPI_Interface.c
    CPI_InterfacePart.c
    CPI_InterfacePart_CommandButton.c
    CPI_InterfacePart_Indicator.c
    CPI_Keyboard.c
    CPI_Player.c
    CPI_Player_Callbacks.c
    CPI_Player_CoDec_WAV.c
    CPI_Player_CoDec_WinAmpPlugin.c
    CPI_Player_CoDec_FFmpeg.c
    CPI_Player_Engine.c
    CPI_Player_FileAssoc.c
    CPI_Player_Output_DirectSound.c
    CPI_Player_Output_FAudio.c
    CPI_Player_Output_File.c
    CPI_Player_Output_Wave.c
    CPI_Playlist.c
    CPI_Playlist_Callbacks.c
    CPI_PlaylistItem.c
    CPI_PlaylistWindow.c
    CPI_Stream.c
    CPI_Stream_Internet.c
    CPI_Stream_LocalFile.c
    CPI_Translation.c
    CPI_Gettext.c
    CPString.c
    CPI_Verbs.c
    CPSK_Skin.c
    DLG_Find.c
    DLG_Properties.c
    globals.c
    main.c
    MainMenu.c
    options.c
    profile.c
    RotatingIcon.c
    shwapi.c
    skin.c
    stdafx.c
    WindowsOS.c
    WindowSnapping.c
    WinModern.c
    CP_Config.c
)

# Resource file
set(BRISK_RC briskplayer.rc)

# Find required libraries - expect them to be installed in system paths
find_package(ZLIB REQUIRED)

# Find TagLib C interface - try pkg-config first, then fall back to manual detection
# Note that tag_c depends on tag (C++ library)
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(TAGLIB taglib_c)
    if(NOT TAGLIB_FOUND)
        pkg_check_modules(TAGLIB taglib)
    endif()
endif()

if(NOT TAGLIB_FOUND)
    # Fallback to manual detection for vcpkg or other installations
    find_library(TAGLIB_C_LIBRARY NAMES tag_c)
    find_library(TAGLIB_LIBRARY NAMES tag)
    find_path(TAGLIB_INCLUDE_DIR NAMES tag_c.h PATH_SUFFIXES taglib)
    
    if(TAGLIB_C_LIBRARY AND TAGLIB_LIBRARY AND TAGLIB_INCLUDE_DIR)
        set(TAGLIB_FOUND TRUE)
        set(TAGLIB_LIBRARIES ${TAGLIB_C_LIBRARY} ${TAGLIB_LIBRARY})
        set(TAGLIB_INCLUDE_DIRS ${TAGLIB_INCLUDE_DIR})
        message(STATUS "Found TagLib (manual detection): ${TAGLIB_LIBRARIES}")
    else()
        message(FATAL_ERROR "TagLib is required but not found. Please install TagLib development libraries.")
    endif()
endif()

# FFmpeg components - try pkg-config first, then fall back to find_package
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG libavcodec libavformat libavutil libswresample)
endif()

if(NOT FFMPEG_FOUND)
    # Fallback to find_package for vcpkg or other CMake-based installations
    find_package(FFMPEG QUIET)
endif()

if(NOT FFMPEG_FOUND)
    # Last resort: try to find libraries manually
    find_library(AVCODEC_LIBRARY NAMES avcodec)
    find_library(AVFORMAT_LIBRARY NAMES avformat)
    find_library(AVUTIL_LIBRARY NAMES avutil)
    find_library(SWRESAMPLE_LIBRARY NAMES swresample)
    find_path(FFMPEG_INCLUDE_DIR NAMES libavcodec/avcodec.h)
    
    if(AVCODEC_LIBRARY AND AVFORMAT_LIBRARY AND AVUTIL_LIBRARY AND SWRESAMPLE_LIBRARY AND FFMPEG_INCLUDE_DIR)
        set(FFMPEG_FOUND TRUE)
        set(FFMPEG_LIBRARIES ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWRESAMPLE_LIBRARY})
        set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR})
        message(STATUS "Found FFmpeg (manual detection): ${FFMPEG_LIBRARIES}")
    else()
        message(FATAL_ERROR "FFmpeg is required but not found. Please install FFmpeg development libraries.")
    endif()
endif()

# FAudio - try pkg-config first, then CONFIG, then manual detection
if(PKG_CONFIG_FOUND)
    pkg_check_modules(FAUDIO FAudio)
endif()

if(NOT FAUDIO_FOUND)
    # Try CMake config file (vcpkg provides this)
    find_package(FAudio CONFIG QUIET)
    if(FAudio_FOUND)
        set(FAUDIO_FOUND TRUE)
    endif()
endif()

if(NOT FAUDIO_FOUND)
    # Manual detection as last resort
    find_library(FAUDIO_LIBRARY NAMES FAudio)
    find_path(FAUDIO_INCLUDE_DIR NAMES FAudio.h)
    
    if(FAUDIO_LIBRARY AND FAUDIO_INCLUDE_DIR)
        set(FAUDIO_FOUND TRUE)
        set(FAUDIO_LIBRARIES ${FAUDIO_LIBRARY})
        set(FAUDIO_INCLUDE_DIRS ${FAUDIO_INCLUDE_DIR})
        message(STATUS "Found FAudio (manual detection): ${FAUDIO_LIBRARY}")
    else()
        message(FATAL_ERROR "FAudio is required but not found. Please install FAudio development libraries.")
    endif()
endif()

# Windows system libraries
set(WINDOWS_LIBRARIES
    winmm
    comctl32
    comdlg32
    wininet
    dsound
    mfplat
    propsys
    mfuuid
    mfreadwrite
)

# Add MSVC-specific libraries for C++ runtime symbols
if(MSVC)
    list(APPEND WINDOWS_LIBRARIES
        legacy_stdio_definitions
        oldnames
        vcruntime
    )
endif()

# Gettext support (optional)
if(ENABLE_NLS)
    find_package(Intl)
    if(Intl_FOUND)
        add_compile_definitions(ENABLE_NLS=1)
        message(STATUS "Internationalization support: enabled")
    else()
        message(STATUS "Internationalization support: disabled (gettext not found)")
    endif()
endif()

# FFmpeg codec enabled
add_compile_definitions(HAVE_FFMPEG_CODEC=1)

# Create executable
if(MSVC)
    add_executable(briskplayer WIN32 ${BRISK_SOURCES} ${BRISK_RC})
    
    # Compile CPI_TagLib.c as C++ since it uses TagLib C++ API for album art
    # Disable NLS for this file to avoid conflicts with TagLib templates
    set_source_files_properties(CPI_TagLib.c PROPERTIES 
        LANGUAGE CXX
        COMPILE_FLAGS "/UENABLE_NLS")
    set_source_files_properties(CPI_AlbumArtTooltip.c PROPERTIES LANGUAGE CXX)
    
    set_target_properties(briskplayer PROPERTIES LINK_FLAGS "/MANIFEST:NO")
else()
    # For MinGW, compile resource file separately
    if(MINGW)
        enable_language(RC)
        # Try to detect the proper windres for cross-compilation (e.g., MXE)
        # CMAKE_RC_COMPILER might already be set by CMake
        if(NOT CMAKE_RC_COMPILER)
            # Extract the target triplet from the C compiler name
            get_filename_component(COMPILER_NAME ${CMAKE_C_COMPILER} NAME_WE)
            if(COMPILER_NAME MATCHES "^(.+)-gcc$")
                # Compiler is something like i686-w64-mingw32.static-gcc
                string(REGEX REPLACE "-gcc$" "-windres" RC_COMPILER_NAME ${COMPILER_NAME})
                find_program(CMAKE_RC_COMPILER NAMES ${RC_COMPILER_NAME} windres)
            else()
                find_program(CMAKE_RC_COMPILER NAMES windres)
            endif()
        endif()
        
        if(NOT CMAKE_RC_COMPILER)
            message(FATAL_ERROR "Could not find windres resource compiler")
        endif()
        
        message(STATUS "Using resource compiler: ${CMAKE_RC_COMPILER}")
        # Configure windres for cross-compilation on Unix
        # The --preprocessor option tells windres to use gcc's preprocessor which handles paths better
        # We need to use the matching preprocessor for the target architecture
        get_filename_component(COMPILER_NAME ${CMAKE_C_COMPILER} NAME_WE)
        if(COMPILER_NAME MATCHES "^(.+)-gcc$")
            string(REGEX REPLACE "-gcc$" "-cpp" CPP_NAME ${COMPILER_NAME})
            find_program(TARGET_CPP NAMES ${CPP_NAME} cpp)
            if(TARGET_CPP)
                set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> --preprocessor=${TARGET_CPP} --preprocessor-arg=-E --preprocessor-arg=-xc-header --preprocessor-arg=-DRC_INVOKED --include-dir=${CMAKE_CURRENT_SOURCE_DIR} --include-dir=${CMAKE_CURRENT_SOURCE_DIR}/res -O coff -i <SOURCE> -o <OBJECT>")
            else()
                set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> --use-temp-file --include-dir=${CMAKE_CURRENT_SOURCE_DIR} --include-dir=${CMAKE_CURRENT_SOURCE_DIR}/res -O coff -i <SOURCE> -o <OBJECT>")
            endif()
        else()
            set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> --use-temp-file --include-dir=${CMAKE_CURRENT_SOURCE_DIR} --include-dir=${CMAKE_CURRENT_SOURCE_DIR}/res -O coff -i <SOURCE> -o <OBJECT>")
        endif()
    endif()
    add_executable(briskplayer WIN32 ${BRISK_SOURCES} ${BRISK_RC})
    
    # Compile CPI_TagLib.c as C++ since it uses TagLib C++ API for album art
    # Disable NLS for this file to avoid conflicts with TagLib templates
    set_source_files_properties(CPI_TagLib.c PROPERTIES 
        LANGUAGE CXX
        COMPILE_FLAGS "-Wno-error=template-body -UENABLE_NLS")
    set_source_files_properties(CPI_AlbumArtTooltip.c PROPERTIES LANGUAGE CXX)
    
    # Use C++ linker for MinGW since we link against C++ libraries (TagLib)
    if(MINGW)
        set_target_properties(briskplayer PROPERTIES LINKER_LANGUAGE CXX)
    endif()
endif()

# Include directories
target_include_directories(briskplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${TAGLIB_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
    ${FAUDIO_INCLUDE_DIRS}
)

if(Intl_FOUND)
    target_include_directories(briskplayer PRIVATE ${Intl_INCLUDE_DIRS})
endif()

# Link libraries
target_link_libraries(briskplayer PRIVATE
    ZLIB::ZLIB
)

# Link TagLib libraries
if(TAGLIB_FOUND)
    if(TAGLIB_LINK_LIBRARIES)
        # pkg-config provides TAGLIB_LINK_LIBRARIES
        target_link_libraries(briskplayer PRIVATE ${TAGLIB_LINK_LIBRARIES})
    else()
        # Manual detection provides individual library variables
        target_link_libraries(briskplayer PRIVATE ${TAGLIB_LIBRARIES})
    endif()
endif()

# Link FAudio libraries
if(FAUDIO_FOUND)
    if(BUILD_STATIC AND TARGET FAudio::FAudio-static)
        # Prefer static library when BUILD_STATIC is enabled
        target_link_libraries(briskplayer PRIVATE FAudio::FAudio-static)
    elseif(TARGET FAudio::FAudio)
        # Fall back to shared library target
        target_link_libraries(briskplayer PRIVATE FAudio::FAudio)
    elseif(TARGET FAudio::FAudio-static)
        # CMake CONFIG provides the static target
        target_link_libraries(briskplayer PRIVATE FAudio::FAudio-static)
    elseif(FAUDIO_LINK_LIBRARIES)
        # pkg-config provides FAUDIO_LINK_LIBRARIES
        target_link_libraries(briskplayer PRIVATE ${FAUDIO_LINK_LIBRARIES})
    else()
        # Manual detection
        target_link_libraries(briskplayer PRIVATE ${FAUDIO_LIBRARIES})
    endif()
endif()

# Link FFmpeg libraries
if(FFMPEG_FOUND)
    if(FFMPEG_LINK_LIBRARIES)
        # pkg-config provides FFMPEG_LINK_LIBRARIES
        target_link_libraries(briskplayer PRIVATE ${FFMPEG_LINK_LIBRARIES})
    else()
        # Manual or find_package detection
        target_link_libraries(briskplayer PRIVATE ${FFMPEG_LIBRARIES})
    endif()
endif()

# Link Windows system libraries (after FAudio/FFmpeg which need them)
target_link_libraries(briskplayer PRIVATE
    ${WINDOWS_LIBRARIES}
    gdiplus  # For album art decoding
)

# Explicitly link C++ standard library for MinGW-w64 cross-compilation
# Since we're linking against TagLib (C++ library), we need the C++ runtime
# The C compiler (gcc) is used for linking but doesn't automatically include libstdc++
# Check if we're using a mingw compiler by looking at the compiler path
if(CMAKE_CXX_COMPILER MATCHES "mingw" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Configuring C++ standard library for MinGW-w64...")
    # Get the compiler's library directory
    get_filename_component(COMPILER_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    get_filename_component(COMPILER_ROOT ${COMPILER_DIR} DIRECTORY)
    
    # For Windows XP compatibility, link winpthread before libstdc++
    # This provides XP-compatible implementations of threading primitives
    # Check for both cross-compilation path and native MSYS2 path
    set(WINPTHREAD_LIB "${COMPILER_ROOT}/i686-w64-mingw32/lib/libwinpthread.a")
    if(NOT EXISTS "${WINPTHREAD_LIB}")
        set(WINPTHREAD_LIB "${COMPILER_ROOT}/lib/libwinpthread.a")
    endif()
    if(EXISTS "${WINPTHREAD_LIB}")
        target_link_libraries(briskplayer PRIVATE "${WINPTHREAD_LIB}")
        message(STATUS "✓ Linked winpthread for XP compatibility: ${WINPTHREAD_LIB}")
    endif()
    
    # Link the static C++ standard library using full path
    # Check for both cross-compilation path and native MSYS2 path
    set(STDCXX_LIB "${COMPILER_ROOT}/i686-w64-mingw32/lib/libstdc++.a")
    if(NOT EXISTS "${STDCXX_LIB}")
        set(STDCXX_LIB "${COMPILER_ROOT}/lib/libstdc++.a")
    endif()
    if(EXISTS "${STDCXX_LIB}")
        target_link_libraries(briskplayer PRIVATE "${STDCXX_LIB}")
        message(STATUS "✓ Linked C++ standard library: ${STDCXX_LIB}")
    else()
        message(WARNING "Could not find libstdc++.a at ${STDCXX_LIB}")
    endif()
endif()

if(Intl_FOUND)
    target_link_libraries(briskplayer PRIVATE ${Intl_LIBRARIES})
endif()

# Installation
install(TARGETS briskplayer RUNTIME DESTINATION bin)

# Add translations subdirectory
if(ENABLE_NLS)
    add_subdirectory(po)
endif()

# Add tests subdirectory
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
