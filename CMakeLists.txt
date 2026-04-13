cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# vcpkg toolchain autodetection
# Must be set BEFORE project() so vcpkg can inject its find_package overrides.
#
# The VS-bundled vcpkg in C:\Program Files is marked readonly:true and cannot
# acquire the .vcpkg-root filesystem lock in a non-admin process, causing an
# infinite "waiting to take filesystem lock" loop.  We therefore require a
# standalone (writable) vcpkg installation.
#
# Priority:
#   1. Explicit -DCMAKE_TOOLCHAIN_FILE=... on the cmake command-line
#   2. VCPKG_ROOT environment variable
#   3. Common standalone installation paths
#   4. Fatal error with install instructions
# ---------------------------------------------------------------------------

# Helper: return TRUE in <out_var> if the vcpkg at <root> is NOT readonly.
function(_vcpkg_is_writable root out_var)
    set(${out_var} TRUE PARENT_SCOPE)
    if(EXISTS "${root}/vcpkg-bundle.json")
        file(READ "${root}/vcpkg-bundle.json" _bundle)
        if(_bundle MATCHES "\"readonly\"[ \t\r\n]*:[ \t\r\n]*[Tt]rue")
            set(${out_var} FALSE PARENT_SCOPE)
        endif()
    endif()
endfunction()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    set(_vcpkg_root "")

    # 1. VCPKG_ROOT env var
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        _vcpkg_is_writable("$ENV{VCPKG_ROOT}" _writable)
        if(_writable)
            set(_vcpkg_root "$ENV{VCPKG_ROOT}")
            message(STATUS "vcpkg: using VCPKG_ROOT env: ${_vcpkg_root}")
        else()
            message(WARNING "vcpkg: VCPKG_ROOT points to a readonly VS-bundled installation ($ENV{VCPKG_ROOT}); ignoring it. Install standalone vcpkg and update VCPKG_ROOT.")
        endif()
    endif()

    # 2. Common standalone installation paths (user-writable, not in Program Files)
    if(NOT _vcpkg_root)
        set(_standalone_candidates
            "C:/vcpkg"
            "$ENV{USERPROFILE}/vcpkg"
            "$ENV{USERPROFILE}/source/vcpkg"
            "$ENV{USERPROFILE}/source/repos/vcpkg"
            "$ENV{LOCALAPPDATA}/vcpkg/standalone"
        )
        foreach(_candidate IN LISTS _standalone_candidates)
            if(EXISTS "${_candidate}/scripts/buildsystems/vcpkg.cmake")
                _vcpkg_is_writable("${_candidate}" _writable)
                if(_writable)
                    set(_vcpkg_root "${_candidate}")
                    message(STATUS "vcpkg: found standalone installation: ${_vcpkg_root}")
                    break()
                endif()
            endif()
        endforeach()
    endif()

    # 3. No writable vcpkg found — fatal with clear install instructions
    if(NOT _vcpkg_root)
        message(FATAL_ERROR
            "No writable vcpkg installation found.\n"
            "The VS-bundled vcpkg in 'C:\\Program Files' is read-only and cannot be used from the CLI.\n\n"
            "To fix, install standalone vcpkg:\n"
            "  git clone https://github.com/microsoft/vcpkg.git C:\\vcpkg\n"
            "  C:\\vcpkg\\bootstrap-vcpkg.bat\n"
            "Then either:\n"
            "  a) Set the VCPKG_ROOT environment variable:  \$env:VCPKG_ROOT = 'C:\\vcpkg'\n"
            "  b) Pass it on the cmake command line:  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake\n"
        )
    endif()

    set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "vcpkg toolchain" FORCE)
    unset(_vcpkg_root)
    unset(_standalone_candidates)
    unset(_writable)
endif()

# ---------------------------------------------------------------------------
# vcpkg manifest feature selection
# Map CMake option flags to vcpkg features BEFORE project() so that vcpkg
# installs the right set of packages during the initial configure.
# ---------------------------------------------------------------------------
if(NOT DEFINED VCPKG_MANIFEST_FEATURES)
    set(VCPKG_MANIFEST_FEATURES "")
endif()
if(NOT ENABLE_FFMPEG)
    list(APPEND VCPKG_MANIFEST_FEATURES "native-codecs")
else()
    list(APPEND VCPKG_MANIFEST_FEATURES "ffmpeg")
endif()
if(ENABLE_DISCORD_RPC)
    list(APPEND VCPKG_MANIFEST_FEATURES "discord-rpc")
endif()
set(VCPKG_MANIFEST_FEATURES "${VCPKG_MANIFEST_FEATURES}" CACHE STRING "" FORCE)

# Note: CXX language is required because TagLib (even tag_c.lib) is a C++ library
# Your source code is pure C, but we need C++ runtime to link against TagLib
project(briskplayer VERSION 3.0.2 LANGUAGES C CXX)

# Calculate build number from version (MAJOR * 100 + MINOR * 10 + PATCH)
math(EXPR CP_BUILD_NUMBER "${PROJECT_VERSION_MAJOR} * 100 + ${PROJECT_VERSION_MINOR} * 10 + ${PROJECT_VERSION_PATCH}")

# Generate version header from template (in source dir so windres can find it)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/CP_Version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/CP_Version.h
    @ONLY
)

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
    add_compile_options(/W3 /utf-8 /std:c17 /experimental:c11atomics)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32 _WINDOWS)
    
    # Windows Vista as minimum version (FAudio and FFmpeg 8 require Vista+)
    add_compile_definitions(_WIN32_WINNT=0x0600 WINVER=0x0600)
    
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

    # Size optimization: let linker discard unused sections
    add_compile_options(-ffunction-sections -fdata-sections)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")

    # In Release builds, prefer -Os (size) over -O2 (speed) and strip symbols
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        string(REPLACE "-O2" "-Os" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
        string(REPLACE "-O2" "-Os" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
    endif()

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
    CPI_DpiScale.c
    CPI_Equaliser_Basic.c
    CPI_ReplayGain.c
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
    CPI_Player_DSP.c
    CPI_Player_Engine.c
    CPI_Player_FileAssoc.c
    CPI_Player_Output_DirectSound.c
    CPI_Player_Output_FAudio.c
    CPI_Player_Output_File.c
    CPI_Player_Output_Wave.c
    CPI_Player_Output_WASAPI.c
    CPI_Playlist.c
    CPI_Playlist_Callbacks.c
    CPI_PlaylistItem.c
    CPI_PlaylistWindow.c
    CPI_Stream.c
    CPI_Stream_Internet.c
    CPI_Stream_LocalFile.c
    CPI_TaskbarIntegration.c
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

# Discord RPC (optional)
if(ENABLE_DISCORD_RPC)
    list(APPEND BRISK_SOURCES CPI_DiscordRPC.c)
endif()

# Resource file
set(BRISK_RC briskplayer.rc)

# ---------------------------------------------------------------------------
# Codec selection: FFmpeg vs native libs (libmad/vorbis/FLAC/opus/fdk-aac)
# Native codecs are the default. Use -DENABLE_FFMPEG=ON to use FFmpeg instead.
# When using FFmpeg, install with: vcpkg install briskplayer[ffmpeg]
# ---------------------------------------------------------------------------
option(ENABLE_FFMPEG "Use FFmpeg for audio decoding" OFF)

# ---------------------------------------------------------------------------
# Discord Rich Presence (optional)
# Shows "Now Playing" info in the user's Discord profile.
# Install with: vcpkg install briskplayer[discord-rpc]
# ---------------------------------------------------------------------------
option(ENABLE_DISCORD_RPC "Enable Discord Rich Presence integration" OFF)

if(ENABLE_FFMPEG)
    message(STATUS "Codec backend: FFmpeg")
    list(APPEND BRISK_SOURCES CPI_Player_CoDec_FFmpeg.c)
else()
    message(STATUS "Codec backend: native (libmad/vorbis/FLAC/opus/aac)")
    list(APPEND BRISK_SOURCES
        CPI_Player_CoDec_MPEG.c
        CPI_Player_CoDec_OGG.c
        CPI_Player_CoDec_FLAC.c
        CPI_Player_CoDec_AAC.c
        CPI_Player_CoDec_OPUS.c
    )
endif()

# Find required libraries via vcpkg-provided CMake config files.
# In manifest mode vcpkg installs all packages listed in vcpkg.json before
# CMake's find_package calls are reached, so CONFIG-mode finds are reliable.
find_package(ZLIB REQUIRED)

# TagLib — exports TagLib::TagLib (C++) and, in recent versions, TagLib::tag_c
find_package(taglib CONFIG REQUIRED)
if(TARGET TagLib::tag_c)
    set(TAGLIB_TARGETS TagLib::tag_c TagLib::TagLib)
else()
    # Older taglib releases merged tag_c into the main target; find the .lib directly
    find_library(TAGLIB_C_LIBRARY NAMES tag_c REQUIRED)
    set(TAGLIB_TARGETS ${TAGLIB_C_LIBRARY} TagLib::TagLib)
endif()

if(ENABLE_FFMPEG)
    # FFmpeg — vcpkg adds its FindFFmpeg.cmake module to CMAKE_MODULE_PATH via the
    # toolchain, so MODULE-mode find_package works without pkg-config.
    find_package(FFMPEG REQUIRED COMPONENTS avcodec avformat avutil swresample)
else()
    # Native codec libraries
    find_package(mad CONFIG QUIET)
    if(NOT mad_FOUND)
        find_library(MAD_LIBRARY NAMES mad REQUIRED)
        add_library(mad::mad UNKNOWN IMPORTED)
        set_target_properties(mad::mad PROPERTIES IMPORTED_LOCATION "${MAD_LIBRARY}")
        find_path(MAD_INCLUDE_DIR mad.h REQUIRED)
        set_target_properties(mad::mad PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MAD_INCLUDE_DIR}")
    endif()

    find_package(Ogg CONFIG REQUIRED)
    find_package(Vorbis CONFIG REQUIRED)
    find_package(FLAC CONFIG REQUIRED)
    find_package(fdk-aac CONFIG REQUIRED)
    find_package(opusfile CONFIG REQUIRED)
endif()

# FAudio — vcpkg installs a CMake config file
find_package(FAudio CONFIG REQUIRED)

# Discord RPC (optional)
if(ENABLE_DISCORD_RPC)
    find_path(DISCORD_RPC_INCLUDE_DIR discord_rpc.h REQUIRED)
    # Multi-config generators (Visual Studio) need separate Release/Debug libraries
    # to avoid _ITERATOR_DEBUG_LEVEL / RuntimeLibrary mismatches.
    find_library(DISCORD_RPC_LIBRARY_RELEASE NAMES discord-rpc
        PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib" NO_DEFAULT_PATH REQUIRED)
    find_library(DISCORD_RPC_LIBRARY_DEBUG NAMES discord-rpc
        PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib" NO_DEFAULT_PATH REQUIRED)
    add_library(discord-rpc::discord-rpc STATIC IMPORTED)
    set_target_properties(discord-rpc::discord-rpc PROPERTIES
        IMPORTED_LOCATION "${DISCORD_RPC_LIBRARY_RELEASE}"
        IMPORTED_LOCATION_DEBUG "${DISCORD_RPC_LIBRARY_DEBUG}"
        IMPORTED_LOCATION_RELEASE "${DISCORD_RPC_LIBRARY_RELEASE}"
        INTERFACE_INCLUDE_DIRECTORIES "${DISCORD_RPC_INCLUDE_DIR}"
    )
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
    windowscodecs
    uuid
    ole32
)

# Add MSVC-specific libraries for C++ runtime symbols
if(MSVC)
    list(APPEND WINDOWS_LIBRARIES
        legacy_stdio_definitions
        oldnames
    )
endif()

# Gettext support (optional)
if(ENABLE_NLS)
    # Hint CMake to the vcpkg host-tools directory where gettext installs
    # msgfmt/msgmerge/xgettext (host deps go to x64-windows, not the target triplet)
    if(DEFINED VCPKG_INSTALLED_DIR)
        set(_vcpkg_host_triplet "${VCPKG_HOST_TRIPLET}")
        if(NOT _vcpkg_host_triplet)
            set(_vcpkg_host_triplet "x64-windows")
        endif()
        list(APPEND CMAKE_PROGRAM_PATH
            "${VCPKG_INSTALLED_DIR}/${_vcpkg_host_triplet}/tools/gettext/bin")
    endif()

    find_package(Intl)
    find_package(Gettext)

    if(Intl_FOUND AND GETTEXT_FOUND)
        add_compile_definitions(ENABLE_NLS=1)
        message(STATUS "Internationalization support: enabled (libintl + tools found)")
    elseif(Intl_FOUND)
        add_compile_definitions(ENABLE_NLS=1)
        message(STATUS "Internationalization support: enabled (libintl found, tools missing – .mo files won't be built)")
    else()
        message(STATUS "Internationalization support: disabled (gettext-libintl not found)")
    endif()
endif()

# Codec compile-time flag
if(ENABLE_FFMPEG)
    add_compile_definitions(HAVE_FFMPEG_CODEC=1)
endif()

# Discord RPC compile-time flag
if(ENABLE_DISCORD_RPC)
    add_compile_definitions(ENABLE_DISCORD_RPC=1)
endif()

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
        COMPILE_FLAGS "-UENABLE_NLS")
    set_source_files_properties(CPI_AlbumArtTooltip.c PROPERTIES LANGUAGE CXX)
    
    # Use C++ linker for MinGW since we link against C++ libraries (TagLib)
    if(MINGW)
        set_target_properties(briskplayer PROPERTIES LINKER_LANGUAGE CXX)
    endif()
endif()

# Include directories
# TagLib and FAudio include dirs are propagated automatically via their
# imported targets; only FFMPEG still uses plain variables.
target_include_directories(briskplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(ENABLE_FFMPEG AND DEFINED FFMPEG_INCLUDE_DIRS)
    target_include_directories(briskplayer PRIVATE ${FFMPEG_INCLUDE_DIRS})
endif()

if(Intl_FOUND)
    target_include_directories(briskplayer PRIVATE ${Intl_INCLUDE_DIRS})
endif()

# Link libraries
target_link_libraries(briskplayer PRIVATE
    ZLIB::ZLIB
)

# Link TagLib libraries (targets set above based on what taglib exports)
target_link_libraries(briskplayer PRIVATE ${TAGLIB_TARGETS})

# Link FAudio — the CONFIG-mode target handles static vs shared automatically
if(TARGET FAudio::FAudio)
    target_link_libraries(briskplayer PRIVATE FAudio::FAudio)
endif()

# Link Discord RPC (optional)
if(ENABLE_DISCORD_RPC)
    target_link_libraries(briskplayer PRIVATE discord-rpc::discord-rpc)
endif()

# Link FFmpeg or native codec libraries depending on ENABLE_FFMPEG
if(ENABLE_FFMPEG)
    if(FFMPEG_FOUND)
        target_link_libraries(briskplayer PRIVATE ${FFMPEG_LIBRARIES})
    endif()
else()
    # Native codec libraries
    if(TARGET mad::mad)
        target_link_libraries(briskplayer PRIVATE mad::mad)
    elseif(DEFINED MAD_LIBRARY)
        target_link_libraries(briskplayer PRIVATE ${MAD_LIBRARY})
    endif()
    target_link_libraries(briskplayer PRIVATE
        Ogg::ogg
        Vorbis::vorbis
        Vorbis::vorbisfile
        FLAC::FLAC
        FDK-AAC::fdk-aac
        OpusFile::opusfile
    )
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
    
    # Build list of potential library search paths
    # Priority: GCC library paths (where libstdc++.a actually lives in MXE), 
    # then CMAKE_FIND_ROOT_PATH, then compiler-relative paths
    set(LIB_SEARCH_PATHS "")
    set(MXE_TARGETS "i686-w64-mingw32.static;x86_64-w64-mingw32.static;i686-w64-mingw32.shared;x86_64-w64-mingw32.shared;i686-w64-mingw32;x86_64-w64-mingw32")
    set(GCC_VERSIONS "14.2.0;14.1.0;14.0.0;13.3.0;13.2.0;13.1.0;13.0.0;12.4.0;12.3.0;12.2.0;12.1.0;12.0.0;11.5.0;11.4.0;11.3.0;11.2.0;11.1.0;11.0.0;10.5.0;10.4.0;10.3.0;10.2.0;10.1.0")
    
    # MXE stores libstdc++.a in lib/gcc/<target>/<version>/
    # Check relative to CMAKE_FIND_ROOT_PATH parent (mxe/usr)
    if(CMAKE_FIND_ROOT_PATH)
        foreach(ROOT_PATH ${CMAKE_FIND_ROOT_PATH})
            get_filename_component(MXE_USR "${ROOT_PATH}" DIRECTORY)
            foreach(MXE_TARGET ${MXE_TARGETS})
                foreach(GCC_VER ${GCC_VERSIONS})
                    list(APPEND LIB_SEARCH_PATHS "${MXE_USR}/lib/gcc/${MXE_TARGET}/${GCC_VER}")
                endforeach()
            endforeach()
        endforeach()
    endif()
    
    # Also check relative to compiler root
    foreach(MXE_TARGET ${MXE_TARGETS})
        foreach(GCC_VER ${GCC_VERSIONS})
            list(APPEND LIB_SEARCH_PATHS "${COMPILER_ROOT}/lib/gcc/${MXE_TARGET}/${GCC_VER}")
        endforeach()
    endforeach()
    
    # MXE sets CMAKE_FIND_ROOT_PATH to the target sysroot (e.g., .../mxe/usr/i686-w64-mingw32.static)
    if(CMAKE_FIND_ROOT_PATH)
        foreach(ROOT_PATH ${CMAKE_FIND_ROOT_PATH})
            list(APPEND LIB_SEARCH_PATHS "${ROOT_PATH}/lib")
        endforeach()
    endif()
    
    # Also check MXE target directories relative to compiler root
    foreach(MXE_TARGET ${MXE_TARGETS})
        list(APPEND LIB_SEARCH_PATHS "${COMPILER_ROOT}/${MXE_TARGET}/lib")
    endforeach()
    
    # Fallback paths for native MSYS2/MinGW
    list(APPEND LIB_SEARCH_PATHS "${COMPILER_ROOT}/lib")
    
    # For Windows XP compatibility, link winpthread before libstdc++
    # This provides XP-compatible implementations of threading primitives
    set(WINPTHREAD_LIB "")
    foreach(SEARCH_PATH ${LIB_SEARCH_PATHS})
        set(CANDIDATE_LIB "${SEARCH_PATH}/libwinpthread.a")
        if(EXISTS "${CANDIDATE_LIB}")
            set(WINPTHREAD_LIB "${CANDIDATE_LIB}")
            break()
        endif()
    endforeach()
    if(WINPTHREAD_LIB)
        target_link_libraries(briskplayer PRIVATE "${WINPTHREAD_LIB}")
        message(STATUS "✓ Linked winpthread for XP compatibility: ${WINPTHREAD_LIB}")
    endif()
    
    # Link the static C++ standard library using full path
    set(STDCXX_LIB "")
    foreach(SEARCH_PATH ${LIB_SEARCH_PATHS})
        set(CANDIDATE_LIB "${SEARCH_PATH}/libstdc++.a")
        if(EXISTS "${CANDIDATE_LIB}")
            set(STDCXX_LIB "${CANDIDATE_LIB}")
            break()
        endif()
    endforeach()
    if(STDCXX_LIB)
        target_link_libraries(briskplayer PRIVATE "${STDCXX_LIB}")
        message(STATUS "✓ Linked C++ standard library: ${STDCXX_LIB}")
    else()
        message(WARNING "Could not find libstdc++.a - the linker will search standard paths")
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
