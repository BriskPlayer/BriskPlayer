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
option(STATIC_LINK "Link statically" ON)

# Platform-specific settings
if(MSVC)
    add_compile_options(/W3 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32 _WINDOWS)
    
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
    # GCC/Clang
    add_compile_options(-Wall -Wextra)
    
    # Static linking for MinGW
    if(STATIC_LINK AND MINGW)
        if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # Clang on Windows uses different flags
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
        else()
            # GCC/MinGW-w64
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
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
    globals.c
    main.c
    options.c
    profile.c
    RotatingIcon.c
    shwapi.c
    skin.c
    stdafx.c
    WindowsOS.c
)

# Resource file
set(BRISK_RC briskplayer.rc)

# Find required libraries - expect them to be installed in system paths
find_package(ZLIB REQUIRED)

# Find TagLib C interface - note that tag_c depends on tag (C++ library)
find_library(TAGLIB_C_LIBRARY NAMES tag_c REQUIRED)
find_library(TAGLIB_LIBRARY NAMES tag REQUIRED)
find_path(TAGLIB_INCLUDE_DIR NAMES tag_c.h PATH_SUFFIXES taglib REQUIRED)

# FFmpeg components
find_package(FFMPEG REQUIRED)

# FAudio
find_package(FAudio CONFIG REQUIRED)

# Windows system libraries
set(WINDOWS_LIBRARIES
    winmm
    comctl32
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
    set_target_properties(briskplayer PROPERTIES LINK_FLAGS "/MANIFEST:NO")
else()
    # For MinGW, compile resource file separately
    if(MINGW)
        enable_language(RC)
        set(CMAKE_RC_COMPILER windres)
        set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -i <SOURCE> -o <OBJECT>")
    endif()
    add_executable(briskplayer WIN32 ${BRISK_SOURCES} ${BRISK_RC})
endif()

# Include directories
target_include_directories(briskplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${TAGLIB_INCLUDE_DIR}
    ${FFMPEG_INCLUDE_DIRS}
)

if(Intl_FOUND)
    target_include_directories(briskplayer PRIVATE ${Intl_INCLUDE_DIRS})
endif()

# Link libraries
target_link_libraries(briskplayer PRIVATE
    ZLIB::ZLIB
    ${TAGLIB_C_LIBRARY}
    ${TAGLIB_LIBRARY}
    ${FFMPEG_LIBRARIES}
    FAudio::FAudio-static
    ${WINDOWS_LIBRARIES}
)

# Add C++ standard library for TagLib (C++ library) - required for __cxxabiv1 symbols
if(MINGW)
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # Clang uses libc++ or libstdc++ without gcc-specific libraries
        target_link_libraries(briskplayer PRIVATE c++ c++abi)
    else()
        # GCC/MinGW-w64 uses libstdc++ and gcc runtime
        target_link_libraries(briskplayer PRIVATE stdc++ gcc_s gcc)
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
