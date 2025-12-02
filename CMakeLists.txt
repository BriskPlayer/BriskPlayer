cmake_minimum_required(VERSION 3.20)

project(briskplayer VERSION 3.0.0 LANGUAGES C CXX)

# Set C and C++ standards
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(ENABLE_NLS "Enable internationalization support" ON)
option(STATIC_LINK "Link statically" ON)

# Platform-specific settings
if(MSVC)
    add_compile_options(/W3 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32 _WINDOWS)
    
    # Always use dynamic runtime (/MT for release, /MTd for debug)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    # GCC/Clang
    add_compile_options(-Wall -Wextra)
    
    # Static linking for MinGW
    if(STATIC_LINK AND MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
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

# Try to find TagLib (may need TagLib::tag or TagLib::tag_c)
find_library(TAGLIB_LIBRARY NAMES tag tag_c taglib REQUIRED)
find_path(TAGLIB_INCLUDE_DIR NAMES tag_c.h taglib/tag_c.h REQUIRED)

# FFmpeg components
find_library(AVCODEC_LIBRARY NAMES avcodec REQUIRED)
find_library(AVFORMAT_LIBRARY NAMES avformat REQUIRED)
find_library(AVUTIL_LIBRARY NAMES avutil REQUIRED)
find_library(SWRESAMPLE_LIBRARY NAMES swresample REQUIRED)
find_path(AVCODEC_INCLUDE_DIR NAMES libavcodec/avcodec.h REQUIRED)

# FAudio
find_library(FAUDIO_LIBRARY NAMES FAudio REQUIRED)
find_path(FAUDIO_INCLUDE_DIR NAMES FAudio.h REQUIRED)

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
    ${AVCODEC_INCLUDE_DIR}
    ${FAUDIO_INCLUDE_DIR}
)

if(Intl_FOUND)
    target_include_directories(briskplayer PRIVATE ${Intl_INCLUDE_DIRS})
endif()

# Link libraries
target_link_libraries(briskplayer PRIVATE
    ZLIB::ZLIB
    ${TAGLIB_LIBRARY}
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
    ${FAUDIO_LIBRARY}
    ${WINDOWS_LIBRARIES}
)

if(Intl_FOUND)
    target_link_libraries(briskplayer PRIVATE ${Intl_LIBRARIES})
endif()

# Add C++ standard library for MinGW/GCC
if(NOT MSVC)
    target_link_libraries(briskplayer PRIVATE stdc++)
endif()

# Installation
install(TARGETS briskplayer RUNTIME DESTINATION bin)
install(DIRECTORY res/ DESTINATION share/briskplayer TYPE FILE)

# Add translations subdirectory
if(ENABLE_NLS)
    add_subdirectory(po)
endif()
