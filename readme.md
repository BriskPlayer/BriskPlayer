# **Welcome to BriskPlayer**
### A Blazing fast media player forked from CoolPlayer and written in C

![BriskPlayer](https://i.imgur.com/NalzNPs.png)


## **Features**
* GNU General Public License
* Freeform Skins
* Simple User Interface
* Advanced Playlist editor
* Internet streaming
* File Renamer
* Fast mp3->wav converter
* Optional [FFMpeg](https://www.ffmpeg.org/) codec support (see below)
* Winamp input plugins support
* Smallest executable programmed in blazing fast 'C'
* Continuous play
* 8 band convolution equalizer 
* Translation Engine using gettext
* id3 support made possible using [taglib](https://taglib.org/)
* Modernized codebase fork of the [CoolPlayer media player ](https://coolplayer.sourceforge.net/)

## **Optional FFmpeg support**
BriskPlayer is not built against FFmpeg. To play formats beyond the built-in codecs (APE, WavPack, WMA, AC3/DTS, TTA, Musepack, ...), put a shared FFmpeg 7.x, 8.x or 9.x build matching the player's architecture (32/64-bit) into a folder named `ffmpeg` next to `briskplayer.exe`:

```
briskplayer.exe
ffmpeg\avutil-NN.dll
ffmpeg\swresample-NN.dll
ffmpeg\avcodec-NN.dll
ffmpeg\avformat-NN.dll
ffmpeg\(any other DLLs those depend on)
```

An FFmpeg release archive (for example gyan.dev's `ffmpeg-release-full-shared.7z`) can also simply be extracted into the `ffmpeg` folder: `ffmpeg\bin\` and `ffmpeg\<subfolder>\bin\` are searched as well. The DLLs are picked up on startup. Built-in codecs keep handling the formats they support; FFmpeg handles the rest and acts as a fallback for files a built-in codec can't open.

**Skin was created and is copyright [NeoRame](http://www.neorame.de/) and released under the terms GPL  with my own modifications to fit the name of the fork.**

If you like to see your language in briskplayer, then use this [POT](https://github.com/BriskPlayer/BriskPlayer/blob/main/po/briskplayer.pot) File to begin translating!

