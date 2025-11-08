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
    maddep = dependency('mad', static : true, method : 'cmake', required : true)
endif

flacdep = dependency('flac', static : true, method : 'pkg-config', required : false)
if not flacdep.found()
    flacdep = dependency('flac', static : true, method : 'cmake', required : true)
endif

# fdk-aac for AAC decoding
fdkaacDep = dependency('fdk-aac', static : true, method : 'pkg-config', required : false)
if not fdkaacDep.found()
    fdkaacDep = dependency('FDK-AAC', static : true, method : 'cmake', required : false)
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
    'CPI_Player_CoDec_MPEG.c',
    'CPI_Player_CoDec_OGG.c',
    'CPI_Player_CoDec_WAV.c',
    'CPI_Player_CoDec_FLAC.c',
    'CPI_Player_CoDec_AAC.c',
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

deps = [zlibdep, oggdep, vorbisdep, vorbisfiledep, maddep, flacdep, taglibdep, winmm, comctl32, wininet, dsound, faudiodep, mfplat, propsys, mfuuid, mfreadwrite, fdkaacDep, libstdcpp]

# Add gettext dependency if available
if nls_enabled
    deps += gettextdep
endif

# Configure static linking for different compilers
static_link_args = []
static_compile_args = []
if cc.get_id() == 'gcc' and host_machine.system() == 'windows'
    # Use full static linking including libgcc and libstdc++
    # Explicitly link against static libgcc
    static_link_args = [
        '-static', 
        '-static-libgcc', 
        '-static-libstdc++',
        '-Wl,--as-needed'
    ]
    static_compile_args = []
    
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