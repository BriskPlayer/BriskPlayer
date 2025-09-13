project('briskplayer', ['c', 'cpp'],
    version : '3.0.0',
    license : 'GPL-2.0-or-later',
    default_options : ['warning_level=3', 'c_std=c11', 'cpp_std=c++11'])

cc = meson.get_compiler('c')

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

faudiodep = dependency('FAudio', required : false, static : true, method : 'pkg-config')
if not faudiodep.found()
    faudiodep = dependency('FAudio', required : false, static : true, method : 'cmake')
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

# Determine dependencies based on availability
cpp = meson.get_compiler('cpp')
libstdcpp = cpp.find_library('stdc++', required : false)
if not libstdcpp.found()
    libstdcpp = cpp.find_library('c++', required : false)
endif

deps = [zlibdep, oggdep, vorbisdep, vorbisfiledep, maddep, flacdep, taglibdep, winmm, comctl32, wininet, dsound]
if libstdcpp.found()
    deps += [libstdcpp]
endif
if faudiodep.found()
    deps += [faudiodep, mfplat, propsys, mfuuid, mfreadwrite]
    add_project_arguments('-DHAVE_FAUDIO', language : 'c')
endif

briskplayer_exe = executable('briskplayer', brisksrc, briskres, 
    dependencies : deps,
    win_subsystem : 'windows')