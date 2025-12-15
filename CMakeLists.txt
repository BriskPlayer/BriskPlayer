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
    if(STATIC_LINK AND MINGW)
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
    set_target_properties(briskplayer PROPERTIES LINK_FLAGS "/MANIFEST:NO")
else()
    # For MinGW, compile resource file separately
    if(MINGW)
        enable_language(RC)
        set(CMAKE_RC_COMPILER windres)
        set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -i <SOURCE> -o <OBJECT>")
    endif()
    add_executable(briskplayer WIN32 ${BRISK_SOURCES} ${BRISK_RC})
    
    # Use C++ linker for MinGW since we link against C++ libraries (TagLib)
    if(MINGW)
        set_target_properties(briskplayer PROPERTIES LINKER_LANGUAGE CXX)
    endif()
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
    set(WINPTHREAD_LIB "${COMPILER_ROOT}/i686-w64-mingw32/lib/libwinpthread.a")
    if(EXISTS "${WINPTHREAD_LIB}")
        target_link_libraries(briskplayer PRIVATE "${WINPTHREAD_LIB}")
        message(STATUS "✓ Linked winpthread for XP compatibility: ${WINPTHREAD_LIB}")
    endif()
    
    # Link the static C++ standard library using full path
    set(STDCXX_LIB "${COMPILER_ROOT}/i686-w64-mingw32/lib/libstdc++.a")
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
