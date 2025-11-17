project('briskplayer', ['c', 'cpp'],
    version : '3.0.0',
    license : 'GPL-2.0-or-later',
    default_options : ['warning_level=3', 'c_std=c23', 'cpp_std=c++23'])

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
fs = import('fs')  # Import filesystem module for file existence checks
i18n = import('i18n')  # Import i18n module for gettext support

# Check for C23 features
c23_features = [
    'typeof',
    'decimal_floating_point',
    'binary_literals',
    'enhanced_enumerations',
    'auto_type_inference',
    'improved_bitfields',
    'enhanced_attributes'
]

c23_feature_defines = []
foreach feature : c23_features
    has_feature = cc.has_header_symbol('stddef.h', '_Static_assert', prefix: '#include <stddef.h>')
    if has_feature
        c23_feature_defines += '-DHAS_C23_' + feature.to_upper()
    endif
endforeach

# Add C23-specific compiler flags
c23_args = []
if cc.get_id() == 'gcc'
    # GCC C23 support
    c23_args += ['-std=c23', '-Wno-declaration-after-statement']
elif cc.get_id() == 'clang'
    # Clang C23 support  
    c23_args += ['-std=c23', '-Wno-declaration-after-statement']
elif cc.get_id() == 'msvc'
    # MSVC C23 support (when available)
    c23_args += ['/std:c23']
endif

# Find dependencies with fallback support
# Try both cmake and pkg-config methods for maximum compatibility
vorbisdep = dependency('vorbis', static : true, method : 'pkg-config', required : false)
if not vorbisdep.found()
    vorbisdep = dependency('vorbis', static : true, method : 'cmake', required : true)
endif

vorbisfiledep = dependency('vorbisfile', static : true, method : 'pkg-config', required : false)
if not vorbisfiledep.found()
    vorbisfiledep = dependency('vorbisfile', static : true, method : 'cmake', required : true)
endif

oggdep = dependency('ogg', static : true, method : 'pkg-config', required : false)
if not oggdep.found()
    oggdep = dependency('ogg', static : true, method : 'cmake', required : true)
endif

maddep = dependency('mad', static : true, method : 'pkg-config', required : false)
if not maddep.found()
    maddep = dependency('mad', static : true, method : 'cmake', required : false)
endif

flacdep = dependency('flac', static : true, method : 'pkg-config', required : false)
if not flacdep.found()
    flacdep = dependency('flac', static : true, method : 'cmake', required : false)
endif

# fdk-aac for AAC decoding
fdkaacDep = dependency('fdk-aac', static : true, method : 'pkg-config', required : false)
if not fdkaacDep.found()
    fdkaacDep = dependency('FDK-AAC', static : true, method : 'cmake', required : false)
endif

# FFmpeg libraries for universal audio decoding
ffmpeg_libavcodec = dependency('libavcodec', static : true, method : 'pkg-config', required : false)
if not ffmpeg_libavcodec.found()
    ffmpeg_libavcodec = dependency('libavcodec', static : true, method : 'cmake', required : false)
endif

ffmpeg_libavformat = dependency('libavformat', static : true, method : 'pkg-config', required : false)
if not ffmpeg_libavformat.found()
    ffmpeg_libavformat = dependency('libavformat', static : true, method : 'cmake', required : false)
endif

ffmpeg_libavutil = dependency('libavutil', static : true, method : 'pkg-config', required : false)
if not ffmpeg_libavutil.found()
    ffmpeg_libavutil = dependency('libavutil', static : true, method : 'cmake', required : false)
endif

ffmpeg_libswresample = dependency('libswresample', static : true, method : 'pkg-config', required : false)
if not ffmpeg_libswresample.found()
    ffmpeg_libswresample = dependency('libswresample', static : true, method : 'cmake', required : false)
endif

# Check if FFmpeg is fully available
ffmpeg_available = ffmpeg_libavcodec.found() and ffmpeg_libavformat.found() and ffmpeg_libavutil.found() and ffmpeg_libswresample.found()
prefer_ffmpeg = get_option('prefer_ffmpeg')

if ffmpeg_available
    message('FFmpeg detected: libavcodec, libavformat, libavutil, libswresample')
    if prefer_ffmpeg
        message('FFmpeg preference enabled: will disable native codecs and use FFmpeg for all formats')
    endif
else
    message('FFmpeg not fully available: will use native codec implementations')
    message('  libavcodec: ' + (ffmpeg_libavcodec.found() ? 'found' : 'NOT FOUND'))
    message('  libavformat: ' + (ffmpeg_libavformat.found() ? 'found' : 'NOT FOUND'))
    message('  libavutil: ' + (ffmpeg_libavutil.found() ? 'found' : 'NOT FOUND'))
    message('  libswresample: ' + (ffmpeg_libswresample.found() ? 'found' : 'NOT FOUND'))
endif

zlibdep = dependency('zlib', static : true, method : 'pkg-config', required : false)
if not zlibdep.found()
    zlibdep = dependency('zlib', static : true, method : 'cmake', required : true)
endif

# TagLib might be named differently in vcpkg vs other systems
taglibdep = dependency('taglib_c', static : true, method : 'pkg-config', required : false)
if not taglibdep.found()
    taglibdep = dependency('taglib', static : true, method : 'pkg-config', required : false)
endif
if not taglibdep.found()
    taglibdep = dependency('taglib_c', static : true, method : 'cmake', required : false)
endif
if not taglibdep.found()
    taglibdep = dependency('taglib', static : true, method : 'cmake', required : true)
endif

faudiodep = dependency('FAudio', required : true, static : true, method : 'pkg-config')
if not faudiodep.found()
    faudiodep = dependency('FAudio', required : true, static : true, method : 'cmake')
endif

# Determine which native codecs to enable based on FFmpeg availability and user preferences
flac_option = get_option('flac_codec')
mpeg_option = get_option('mpeg_codec')
ogg_option = get_option('ogg_codec')
aac_option = get_option('aac_codec')

# Logic: If FFmpeg is available AND prefer_ffmpeg is true, disable native codecs by default
# Users can still force enable with -Dcodec=enabled
if ffmpeg_available and prefer_ffmpeg
    # FFmpeg is preferred - only enable native codecs if explicitly requested
    flac_codec_enabled = flac_option.enabled()
    mpeg_codec_enabled = mpeg_option.enabled()
    ogg_codec_enabled = ogg_option.enabled()
    aac_codec_enabled = aac_option.enabled()
else
    # No FFmpeg or not preferred - enable native codecs by default (auto or enabled)
    flac_codec_enabled = not flac_option.disabled()
    mpeg_codec_enabled = not mpeg_option.disabled()
    ogg_codec_enabled = not ogg_option.disabled()
    aac_codec_enabled = not aac_option.disabled()
endif

message('Codec configuration:')
message('  Native FLAC codec: ' + (flac_codec_enabled ? 'enabled' : 'disabled'))
message('  Native MPEG codec: ' + (mpeg_codec_enabled ? 'enabled' : 'disabled'))
message('  Native OGG/Vorbis codec: ' + (ogg_codec_enabled ? 'enabled' : 'disabled'))
message('  Native AAC codec: ' + (aac_codec_enabled ? 'enabled' : 'disabled'))
message('  FFmpeg codec: ' + (ffmpeg_available ? 'enabled (will be compiled)' : 'disabled (libraries not found)'))

# Internationalization setup with proper i18n module
nls_option = get_option('nls')

# Find gettext tools and libraries
gettext_programs = ['xgettext', 'msgmerge', 'msgfmt']
gettext_tools_found = true

if nls_option.enabled() or nls_option.auto()
    foreach prog : gettext_programs
        tool = find_program(prog, required : nls_option.enabled())
        if not tool.found()
            gettext_tools_found = false
            if nls_option.enabled()
                error('Gettext tool ' + prog + ' is required but not found')
            else
                message('Gettext tool ' + prog + ' not found')
            endif
        endif
    endforeach

    # Find gettext library
    gettextdep = dependency('intl', required : false, static : true)
    if not gettextdep.found()
        # Try to find it as a library if dependency detection fails
        gettextdep = cc.find_library('intl', required : false, static : true)
    endif
    
    if not gettextdep.found() and nls_option.enabled()
        error('Gettext library is required but not found')
    endif
else
    gettext_tools_found = false
    gettextdep = dependency('', required : false)
endif

# Enable NLS if both gettext tools and library are available
nls_enabled = gettextdep.found() and (gettext_tools_found or not nls_option.auto())
if nls_enabled
    add_project_arguments('-DENABLE_NLS=1', language: 'c')
    if gettext_tools_found
        message('Full gettext support enabled (library + tools)')
    else
        message('Gettext library found but tools missing - runtime translations only')
    endif
else
    message('Gettext not available - NLS disabled')
endif

winmm = cc.find_library('winmm', static : true)
comctl32 = cc.find_library('comctl32', static : true)
wininet = cc.find_library('wininet', static : true)
dsound = cc.find_library('dsound', static : true)
mfplat = cc.find_library('mfplat', static : true)
propsys = cc.find_library('propsys', static : true)
mfuuid = cc.find_library('mfuuid', static : true)
mfreadwrite = cc.find_library('mfreadwrite', static : true)

# Resource compiler detection - support both MinGW and Visual Studio
compiler_id = cc.get_id()
message('Detected C compiler: ' + compiler_id)

if compiler_id == 'msvc'
    # Visual Studio - use rc.exe
    wrc = find_program('rc', required: true)
    res_ext = '.res'
    wrc_generator = generator(wrc,
    output    : [ '@BASENAME@' + res_ext ],
    arguments : [ '/fo', '@OUTPUT@', '@INPUT@' ])
    message('Using Visual Studio resource compiler (rc.exe)')
elif compiler_id == 'gcc' or compiler_id == 'clang'
    # MinGW/GCC/Clang - use windres
    wrc = find_program('windres', required: true)
    res_ext = '.o'
    wrc_generator = generator(wrc,
    output    : [ '@BASENAME@' + res_ext ],
    arguments : [ '-i', '@INPUT@', '-o', '@OUTPUT@' ])
    message('Using MinGW/GCC resource compiler (windres)')
else
    # Fallback - try windres first, then rc
    wrc = find_program('windres', 'rc', required: true)
    if wrc.path().contains('windres')
        res_ext = '.o'
        wrc_generator = generator(wrc,
        output    : [ '@BASENAME@' + res_ext ],
        arguments : [ '-i', '@INPUT@', '-o', '@OUTPUT@' ])
        message('Using windres (fallback)')
    else
        res_ext = '.res'
        wrc_generator = generator(wrc,
        output    : [ '@BASENAME@' + res_ext ],
        arguments : [ '/fo', '@OUTPUT@', '@INPUT@' ])
        message('Using rc.exe (fallback)')
    endif
endif

briskres = wrc_generator.process('briskplayer.rc')

brisksrc = files([
    'about.c',
    'bitmap2region.c',
    'CLV_ListView.c',
    'CompositeFile.c',
    'CPI_CircleBuffer.c',
    'CPI_Equaliser_Basic.c',
    'CPI_TagLib.c',
    'CPI_Image.c',
    'CPI_Indicators.c',
    'CPI_Interface.c',
    'CPI_InterfacePart.c',
    'CPI_InterfacePart_CommandButton.c',
    'CPI_InterfacePart_Indicator.c',
    'CPI_Keyboard.c',
    'CPI_Player.c',
    'CPI_Player_Callbacks.c',
    'CPI_Player_CoDec_WAV.c',
    'CPI_Player_CoDec_WinAmpPlugin.c',
    'CPI_Player_Engine.c',
    'CPI_Player_FileAssoc.c',
    'CPI_Player_Output_DirectSound.c',
    'CPI_Player_Output_FAudio.c',
    'CPI_Player_Output_File.c',
    'CPI_Player_Output_Wave.c',
    'CPI_Playlist.c',
    'CPI_Playlist_Callbacks.c',
    'CPI_PlaylistItem.c',
    'CPI_PlaylistWindow.c',
    'CPI_Stream.c',
    'CPI_Stream_Internet.c',
    'CPI_Stream_LocalFile.c',
    'CPI_Translation.c',
    'CPI_Gettext.c',
    'CPString.c',
    'CPI_Verbs.c',
    'CPSK_Skin.c',
    'DLG_Find.c',
    'globals.c',
    'main.c',
    'options.c',
    'profile.c',
    'RotatingIcon.c',
    'shwapi.c',
    'skin.c',
    'stdafx.c',
    'WindowsOS.c'
])

# Conditionally add codec source files based on configuration
if mpeg_codec_enabled
    brisksrc += files('CPI_Player_CoDec_MPEG.c')
endif

if ogg_codec_enabled
    brisksrc += files('CPI_Player_CoDec_OGG.c')
endif

if flac_codec_enabled
    brisksrc += files('CPI_Player_CoDec_FLAC.c')
endif

if aac_codec_enabled
    brisksrc += files('CPI_Player_CoDec_AAC.c')
endif

# Always include FFmpeg codec if available
if ffmpeg_available
    brisksrc += files('CPI_Player_CoDec_FFmpeg.c')
endif

# Handle C++ standard library based on compiler
libstdcpp = dependency('', required : false)
cpp_stdlib_args = []
if cc.get_id() == 'clang' and host_machine.system() == 'windows'
    # For clang, prefer libc++ if available
    libstdcpp = cpp.find_library('c++', required : false, static : true)
    if libstdcpp.found()
        cpp_stdlib_args = ['-stdlib=libc++']
    else
        # Fallback to libstdc++
        libstdcpp = cpp.find_library('stdc++', required : true, static : true)
    endif
else
    # For GCC and other compilers, use libstdc++
    libstdcpp = cpp.find_library('stdc++', required : false, static : true)
    if not libstdcpp.found()
        libstdcpp = cpp.find_library('c++', required : false, static : true)
    endif
endif

deps = [zlibdep, taglibdep, winmm, comctl32, wininet, dsound, faudiodep, mfplat, propsys, mfuuid, mfreadwrite, libstdcpp]

# Add codec-specific dependencies based on enabled codecs
if ogg_codec_enabled
    deps += [oggdep, vorbisdep, vorbisfiledep]
endif

if mpeg_codec_enabled
    deps += maddep
endif

if flac_codec_enabled
    deps += flacdep
endif

if aac_codec_enabled
    deps += fdkaacDep
endif

if ffmpeg_available
    deps += [ffmpeg_libavcodec, ffmpeg_libavformat, ffmpeg_libavutil, ffmpeg_libswresample]
endif

# Add gettext dependency if available
if nls_enabled
    deps += gettextdep
endif

# Configure static linking for different compilers
static_link_args = []
static_compile_args = []

# Add codec-specific preprocessor defines
if flac_codec_enabled
    static_compile_args += ['-DHAVE_FLAC_CODEC=1']
endif

if mpeg_codec_enabled
    static_compile_args += ['-DHAVE_MPEG_CODEC=1']
endif

if ogg_codec_enabled
    static_compile_args += ['-DHAVE_OGG_CODEC=1']
endif

if aac_codec_enabled
    static_compile_args += ['-DHAVE_AAC_CODEC=1']
endif

if ffmpeg_available
    static_compile_args += ['-DHAVE_FFMPEG_CODEC=1']
endif

if cc.get_id() == 'gcc' and host_machine.system() == 'windows'
    # Use full static linking including libgcc and libstdc++
    # Explicitly link against static libgcc
    static_link_args = [
        '-static', 
        '-static-libgcc', 
        '-static-libstdc++',
        '-Wl,--as-needed'
    ]
    # Don't clear static_compile_args here - we need the codec defines!
    
    # Add global C and C++ args for static linking
    add_global_arguments('-static-libgcc', language : 'c')
    add_global_link_arguments('-static-libgcc', language : 'c')
    add_global_arguments('-static-libstdc++', language : 'cpp')
    add_global_link_arguments('-static-libstdc++', language : 'cpp')
    
elif cc.get_id() == 'clang' and host_machine.system() == 'windows'
    # For clang, use static linking with libc++
    static_link_args = ['-static-libgcc', '-static'] + cpp_stdlib_args
    static_compile_args = ['-static-libgcc']
endif

briskplayer_exe = executable('briskplayer', brisksrc, briskres, 
    dependencies : deps,
    link_args : static_link_args,
    c_args : c23_args + c23_feature_defines + static_compile_args,
    cpp_args : cpp_stdlib_args + static_compile_args,
    win_subsystem : 'windows')

# Process po subdirectory for translations
if nls_enabled and gettext_tools_found
    subdir('po')
    message('Translation support: POT generation and PO compilation enabled')
    message('Use "ninja briskplayer-pot" to update translation template')
    message('Use "ninja briskplayer-update-po" to update existing translations')
elif nls_enabled
    # Fallback: install existing translations without regeneration
    foreach lang : ['de', 'en']
        mo_file = 'lang' / lang + '.mo'
        if fs.exists(mo_file)
            install_data(mo_file, install_dir : 'locale' / lang / 'LC_MESSAGES',
                         rename: 'briskplayer.mo')
            message('Installing pre-built translation: ' + lang)
        endif
    endforeach
    
    message('Gettext library available but tools missing - using pre-built translations')
else
    message('Translation support: Disabled - application will run in English only')
endif