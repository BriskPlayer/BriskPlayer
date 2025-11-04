project('briskplayer', ['c', 'cpp'],
    version : '3.0.0',
    license : 'GPL-2.0-or-later',
    default_options : ['warning_level=3', 'c_std=c23', 'cpp_std=c++23'])

cc = meson.get_compiler('c')
fs = import('fs')  # Import filesystem module for file existence checks

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

# FAAD2 for AAC decoding
faad2dep = dependency('faad2', static : true, method : 'pkg-config', required : false)
if not faad2dep.found()
    faad2dep = dependency('faad2', static : true, method : 'cmake', required : false)
endif
if not faad2dep.found()
    faad2dep = dependency('FAAD2', static : true, method : 'cmake', required : false)
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

# Gettext for internationalization
gettextdep = dependency('intl', required : false, static : true)
if not gettextdep.found()
    # Try to find it as a library if dependency detection fails
    gettextdep = cc.find_library('intl', required : false, static : true)
endif

# Enable NLS if gettext is available
nls_enabled = gettextdep.found()
if nls_enabled
    add_project_arguments('-DENABLE_NLS=1', language: 'c')
    message('Gettext support enabled')
else
    message('Gettext not found, NLS disabled - will use fallback translations')
endif

winmm = cc.find_library('winmm')
comctl32 = cc.find_library('comctl32')
wininet = cc.find_library('wininet')
dsound = cc.find_library('dsound')
mfplat = cc.find_library('mfplat')
propsys = cc.find_library('propsys')
mfuuid = cc.find_library('mfuuid')
mfreadwrite = cc.find_library('mfreadwrite')

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

# Determine dependencies based on availability and compiler
cpp = meson.get_compiler('cpp')
cc = meson.get_compiler('c')

# Handle C++ standard library based on compiler
libstdcpp = dependency('', required : false)
cpp_stdlib_args = []
if cc.get_id() == 'clang' and host_machine.system() == 'windows'
    # For clang, prefer libc++ if available
    libstdcpp = cpp.find_library('c++', required : false)
    if libstdcpp.found()
        cpp_stdlib_args = ['-stdlib=libc++']
    else
        # Fallback to libstdc++
        libstdcpp = cpp.find_library('stdc++', required : true)
    endif
else
    # For GCC and other compilers, use libstdc++
    libstdcpp = cpp.find_library('stdc++', required : false)
    if not libstdcpp.found()
        libstdcpp = cpp.find_library('c++', required : false)
    endif
endif

deps = [zlibdep, oggdep, vorbisdep, vorbisfiledep, maddep, flacdep, taglibdep, winmm, comctl32, wininet, dsound, faudiodep, mfplat, propsys, mfuuid, mfreadwrite, faad2dep, libstdcpp]

# Add gettext dependency if available
if nls_enabled
    deps += gettextdep
endif

# Configure static linking for different compilers
static_link_args = []
if cc.get_id() == 'gcc' and host_machine.system() == 'windows'
    # Use full static linking to avoid symbol conflicts
    static_link_args = ['-static']
elif cc.get_id() == 'clang' and host_machine.system() == 'windows'
    # For clang, use static linking with libc++
    static_link_args = ['-static-libgcc', '-static'] + cpp_stdlib_args
endif

briskplayer_exe = executable('briskplayer', brisksrc, briskres, 
    dependencies : deps,
    link_args : static_link_args,
    c_args : c23_args + c23_feature_defines,
    cpp_args : cpp_stdlib_args,
    win_subsystem : 'windows')

# Install gettext translations if available
if nls_enabled
    # Install .mo files from locale directory
    locale_files = []
    
    # Check for built translation files
    foreach lang : ['de', 'fr', 'es', 'it', 'pt', 'ru', 'ja', 'zh']
        mo_file = 'locale' / lang / 'LC_MESSAGES' / 'briskplayer.mo'
        if fs.exists(mo_file)
            install_data(mo_file, install_dir : 'locale' / lang / 'LC_MESSAGES')
            message('Installing translation: ' + lang)
        endif
    endforeach
    
    # Install the translation template for translators
    if fs.exists('po/briskplayer.pot')
        install_data('po/briskplayer.pot', install_dir : 'po')
    endif
else
    message('Gettext not available - application will run in English only')
endif