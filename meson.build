project('briskplayer', ['c', 'cpp'],
    version : '2.20.0',
    license : 'GPL-2.0-or-later',
    default_options : ['warning_level=3', 'c_std=c11', 'cpp_std=c++11'])

cc = meson.get_compiler('c')

vorbisdep = dependency('vorbis', static : true)
vorbisfiledep = dependency('vorbisfile', static : true)
oggdep = dependency('ogg', static : true)
maddep = dependency('mad', static : true)
flacdep = dependency('flac', static : true)
zlibdep = dependency('zlib', static : true)
taglibdep = dependency('taglib_c', static : true)
faudiodep = dependency('FAudio', required : false, static : true)
winmm = cc.find_library('winmm')
comctl32 = cc.find_library('comctl32')
wininet = cc.find_library('wininet')
dsound = cc.find_library('dsound')
mfplat = cc.find_library('mfplat')
propsys = cc.find_library('propsys')
mfuuid = cc.find_library('mfuuid')
mfreadwrite = cc.find_library('mfreadwrite')

wrc = find_program('windres')
res_ext = '.o'
wrc_generator = generator(wrc,
output    : [ '@BASENAME@' + res_ext ],
arguments : [ '-i', '@INPUT@', '-o', '@OUTPUT@' ])

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