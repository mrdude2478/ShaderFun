## $${\color{yellow} ShaderFun}$$ $${\color{yellow}Switch}$$ - $${\color{yellow}Audio}$$  $${\color{yellow}Reactive}$$  $${\color{yellow}Visualizer}$$
A real-time audio reactive shader visualizer for Nintendo Switch that creates mesmerizing visuals synchronized to your music.

Whether you are a noob or an expert or just want to listen to music with nice visualisations, this program has you covered.
You can mess about the included shader files or create your own. There's no need to compile anything - just ftp the frag/glsl
file straight to your switch, press down on the left stick and your new shader should show.

## Features
🎵 Audio Reactive Visuals: Real-time FFT analysis driving beautiful shaders\
🎨 Custom Shader Support: Load your own GLSL fragment shaders\
🎧 Multiple Audio Formats: Supports MP3, WAV, OGG, FLAC, MOD, XM, S3M, IT, MIDI\
📁 FTP Server: Built-in FTP server for easy file management\
🎮 Intuitive Controls: Full controller support with comprehensive music controls\
🎛️ Music Player: Auto-hiding overlay with a library browser, queue, shuffle/repeat and touch support\
💡 LED Feedback: Visual indicators for FTP server status\
🔄 Hot Reloading: Rescan files without restarting the application\
📋 Queue & Playlists: add songs to the queue, build, name, rename and delete playlists on the console\
📝 Lyrics: timed `.lrc` files or lyrics embedded in your music files, with male/female/duet colours\
😴 Sleep Timer: fades the music out, pauses (and can close the app so the Switch can sleep)\
💾 Remembers Where You Were: last song and position, queue, shader, image and 3D model\
🧊 3D Models: show textured OBJ models in your shaders, with a separate skin image if you like\
🌐 Network Music: browse and stream songs from an SMB share (router with a USB drive, NAS, PC), set up with the on-screen keyboard\
💽 USB Drives: play music from a USB drive in the dock (FAT, exFAT, NTFS, EXT)

## Installation
Download the latest release from the Releases page\
Extract the .nro file to /switch/shaderfun/\
On your MicroSD card create the following directory structure:\
sdmc:/switch/shaderfun/\
├── shaderfun.nro\
├── music/          # Default music folder (change it from the in-app library); put song.lrc next to song.mp3 for lyrics\
├── images/         # Images for shaders (iChannel2/iChannel3) and 3D model skins\
├── models/         # 3D models (.obj) and their pictures\
├── playlists/      # Playlists (.m3u), created from the in-app library\
├── cache/          # Created automatically: temporary downloads of network songs\
├── smb.ini         # Created automatically: your network share details (see Network music)\
├── smb.log         # Created automatically: a small log of network activity, restarted on every run\
├── config.ini      # Created automatically (music folder, volume, play mode, last song/shader, sleep timer options...)\
├── library.idx     # Created automatically: cache of your music tags\
├── last_queue.txt  # Created automatically: the queue you chose, restored on the next start\
├── scroller.txt    # Optional: your own scroller message\
├── shaders/        # Put your .frag/.glsl shaders here\
└── test/           # Alternative shader location for testing new shader files

## Music Player
The player is an overlay on top of the visualisation. It appears when you press a music button or touch the screen
and fades out after 5 seconds. The track title also scrolls through the sinus scroller (press B to show it).

Overlay: title / artist, progress bar, play-pause, previous/next, rewind/forward 10s, volume, play mode and a Library button.\
Touch: tap the buttons, drag the progress bar to seek, drag the volume slider, tap the mode icon to change repeat/shuffle.

**Library** (press Y or tap the Library button): a panel slides in from the left while the shader keeps running behind it.\
It opens on a menu: **Folders**, **Albums**, **Artists**, **Genres**, **Playlists**, **Network**, **USB drive**, **All songs** and **Rescan library**.\
Albums, artists and genres come from the tags of your music files. The first time, the music folder is read in the background\
(the menu shows the progress) and cached in `sdmc:/switch/shaderfun/library.idx`, so later starts only look at new or changed files.\
Untagged files are grouped by folder name. Two albums with the same name by different artists stay separate, and albums with an\
album-artist tag (compilations) stay together.\
Pick a track to play it and queue the rest of that album/list, or use "Play album" / "Play all" at the top of a page.\
**Playlists** shows `.m3u` / `.m3u8` files from `sdmc:/switch/shaderfun/playlists/` and from your music folder (see *Queue and playlists* below).\
**Folders** is the old file browser: "Play this folder" queues everything inside, "Set as music folder" makes it your default (saved in `config.ini` and re-indexed).\
Touch: tap a row, drag to scroll, use the Back row or the header buttons (menu / music folder / close), tap outside the panel to close.\
**Controller in the library**\
D-Pad Up/Down: move, A: open / play, B: back (closes at the top), X: play the highlighted album/artist/genre/playlist/folder, Y: close, L: menu, R: music folder\
ZL/ZR: previous / next track\
Left stick right / left: move 10 rows down / up (hold to repeat). Left stick up / down: jump to the first / last row\
Right stick right / left: jump to the next / previous first letter (A, B, C ...). *All songs* and the song lists inside genres are sorted by title, so this lands on the right songs\
L3 (left stick press): add the highlighted item to the end of the queue\
R3 (right stick press): add the highlighted item to a playlist (see below)

The menu page also has **Sleep timer**, **Resume on launch** and **Auto-change visualisation** rows (see below).\
**Network** and **USB drive** open music that is not on the SD card (see *Network music* and *USB drives* below).

Play modes (R3 with the library closed, or the overlay icon): Repeat all, Shuffle, Repeat one, Play once.

## Queue and playlists
**Add to queue** (L3 in the library): adds the highlighted song, album, artist, genre, playlist or folder to the end of the queue without\
interrupting the track that is playing. It respects shuffle. If nothing is queued yet, it starts playing.

**Add to playlist** (R3 in the library): opens a page with **New playlist** and your existing playlists. Only the highlighted tracks are added\
(songs already in that playlist are skipped). A new playlist is named with the Switch on-screen keyboard. If the keyboard can't open,\
it gets an automatic name like `Playlist 1`.

**Save my queued songs as a playlist** (top of the Playlists page): saves what *you* picked in this session: albums/folders you played and songs you queued.\
The queue the player fills by itself at start-up is not included.

**Open a playlist** to play it, and to use **Rename this playlist** (keyboard) or **Delete this playlist** (press A twice to confirm).\
The Playlists count on the menu page updates as you add or delete. Playlists are plain `.m3u` files, so you can also edit them over FTP.\
Songs from a network share or a USB drive can't be added to playlists (their paths can change between runs).

## Sleep timer
Library menu page, **Sleep timer**: press A to cycle Off, 15, 30, 45, 60, 90 minutes or *after this track*.\
The volume fades out over the last 15 seconds and the music pauses (so the position is kept). While a timer runs, the menu shows\
**When the sleep timer ends** (*close the app* or *pause only*) and the player overlay shows the countdown. With *close the app*, the app\
closes 5 seconds after the music pauses (press any button in that time to stay). The Switch may then sleep by itself, following the auto-sleep\
setting in System Settings. The timer is not remembered between launches.

## Remembering where you were
On exit (and every 20 seconds while running) the app saves your last song and position, the queue you chose, play mode, volume and the\
current shader, image, 3D model, material and scroller state. On the next start the song is loaded at its old position, paused.\
Library menu page, **Resume on launch**, switches this to *keeps playing*. Delete `config.ini` / `last_queue.txt` to reset.

## Network music (SMB)
Play music straight from a shared folder on your network: a router with a USB drive plugged in, a NAS or a PC.\
Only SMB2 and SMB3 are supported (the old SMB1 is not).

**Setting it up:** library menu, **Network**. The first time this opens **Network settings**, where each row is typed with the Switch keyboard:\
**Server address** (for example `192.168.0.1`), **Share name** (the name shown under `\\server` in Windows Explorer), **User name** (empty = guest),\
**Password**, **Domain** (only for some Windows domains) and **Start folder** (a folder inside the share to start in). A single space clears a field.\
Choose **Connect** when you are done. The details are saved in `sdmc:/switch/shaderfun/smb.ini`, which you can also edit by hand.\
Later, the top page of the network browser has a **Network settings** row to change them.

**Browsing and playing:** folders come first, then songs (the letter jump works here too). A plays a song and queues the rest of the folder,\
**Play all songs in this folder** queues the lot, and L3 adds a song to the queue. A song starts playing about a second after its first part has arrived,\
while the rest keeps downloading (MP3, FLAC, Ogg, Opus and WAV; other formats and songs over 64 MB are downloaded completely first).\
The next song is fetched while you listen, so track changes are instant. Seeking works once the whole song has arrived.\
A `.lrc` file next to a song on the share is fetched with it, so lyrics work for network songs too.

**The cache:** downloads go to `sdmc:/switch/shaderfun/cache/` and are only temporary. Only the song playing and the next one are kept, and the folder is emptied when the app\
starts and when it exits. In **Network settings**, **Keep downloaded songs on exit** keeps them instead, so the last song is ready straight away next time.

**Limits:** network songs are not part of Albums, Artists and Genres, and can't be added to playlists. The password is stored as plain text in `smb.ini`.\
`smb.log` records what the network code is doing (connections, downloads, errors) and is restarted every run. It never contains your password.

## USB drives (docked)
Plug a USB drive into the dock and open library menu, **USB drive**. Each drive is listed with its name (the volume label if it has one), its file system and its size.\
Press A on a drive to browse it like any folder: play it, queue it, use the letter jump, lyrics and tags all work.\
FAT12/16/32, exFAT, NTFS and EXT2/3/4 drives are supported. Drives are mounted read-only, because the player never writes to them.\
A message appears when a drive is connected or removed. Use **Safely remove** on the USB page before unplugging (it stops playback from that drive first).\
If you unplug a drive while browsing it, you are taken back to the menu.

**Limits:** USB drives only work with the dock (not handheld). A folder on a drive can't be your music folder, USB music is not part of Albums, Artists and Genres, songs from it\
can't be added to playlists, and a song from a drive is not restored when the app starts (the drive is not mounted yet). If the row says *not available*, the USB service could not start.

## Lyrics
With the library closed, push the **right stick right** to show lyrics and **left** to hide them. They stay on for the next songs and hide\
themselves on songs that have none (a message appears if you ask for lyrics a song doesn't have).

Lyrics come from a `.lrc` file with the same name as the song (`Song.mp3` + `Song.lrc`), otherwise from the song's own tags (MP3 ID3 lyrics frame,\
FLAC/Ogg/Opus `LYRICS` tag). If embedded lyrics contain `[mm:ss.xx]` timestamps they are treated as timed.

**Timed lyrics** appear in a box over the visualisation: the line being sung is large and yellow, the next line is light green, and lines slide past in time with the music.\
**Plain lyrics** (no timestamps) open as a full page: D-Pad Up/Down or the left stick scroll, B (or right stick left) closes it.

Supported in `.lrc` files: several timestamps on one line, `[offset:ms]`, enhanced word tags (ignored), `[ti:Title]` and `[ar:Artist]` shown in a header above the lyrics,\
and `M:` / `F:` / `D:` at the start of a line (male light blue, female pink, duet violet). A line without a marker keeps the previous singer's colour;\
this is `kSingerSticks` in `lyrics.cpp` (set it to `false` to show unmarked lines in the normal colours). Files can be UTF-8, UTF-16 or Latin-1.\
Colours, the box and the font size can be changed in `ui.cpp` (`LYR_CURRENT`, `LYR_NEXT`, `lyricsSingerColor()`, `FONT_PT`).

**Track info** (L3, or tap the title / the (i) chip): a panel on the right shows the embedded cover art, title, artist, album,\
album artist, composer, track and disc number, year, genre and comment, plus format, bitrate, sample rate, channels, length, size and path.\
Supported tags: ID3v2.2/2.3/2.4 and ID3v1 (MP3), FLAC and Ogg Vorbis/Opus comments (including embedded pictures and lyrics), WAV INFO and tracker module titles.

## Music Controls
A: Play/Pause\
ZL/ZR: Previous/Next track (Previous restarts the track after 3 seconds)\
D-Pad Up/Down: Volume\
D-Pad Left/Right: Seek -/+ 10 seconds\
Right Stick Press (R3): Change play mode (in the library: add to a playlist)\
Right Stick right / left: Show / hide lyrics\
Left Stick Press (L3): Track info - tags, cover art and file details (press again or B to close)\
Y: Open/close the library

In the library: see above.

## 3D Models
Shaders can show a textured 3D model (Wavefront `.obj`) on top of the visualisation. Put models in `sdmc:/switch/shaderfun/models/` (sub-folders are fine).\
A model only appears when the shader asks for it with a comment near the top of the file:

```
//!model "cube_atlas.obj"          // show this model (use `//!model on` for the currently selected one, `//!model off` to hide)
//!model "wood.jpg"                // the skin (picture) for the model - see below
//!material neon                   // matte | neon | chrome | metallic
//!model_rotate 0.6                // spin speed (also _rotate_x / _rotate_z)
//!model_zoom 5.5                  // camera distance
//!model_pos 0 0.2 0               // move the model
//!model_yaw 0   //!model_pitch 0  // fixed angle (turns auto-rotate off)
//!model_auto on                   // auto-rotate on / off
//!model_audio off                 // stop the model reacting to the music
//!model_ground off                // hide the ground plane
```

**Skins:** by default a model uses the picture with the same name (`cube_atlas.obj` + `cube_atlas.jpg`). A shader can choose a different picture, so the\
same model can look different in different shaders: `//!model "something.jpg"` (or `//!model_texture "something.jpg"`; `//!model_texture none` for no texture).\
Both can share a line: `//!model "cube_atlas.obj" "something.jpg"`. The picture is looked for next to the `.obj`, then in `models/`, then in `images/`\
(or give a full path such as `sdmc:/...`). A missing picture falls back to the model's own one. The skin applies while that shader is showing.

**Controls** (library closed): right stick up/down zooms, hold ZL and use the right stick to look around (turns auto-rotate off), hold ZR and flick the right stick left/right to change the material.

## Docked-only shaders
Heavy shaders run much slower in handheld mode (the Switch's GPU is clocked down). Put them in a folder whose name starts with `Docked`\
(for example `shaders/Docked/` or `shaders/Fractals/Docked-intensive/`). They are listed only while the console is docked and are hidden in handheld mode.\
Docking or undocking switches the list immediately; if the shader you were watching disappears, the first available shader is loaded.

## Auto-change visualisation
The visualisation can change by itself. Tap the small equaliser-bars icon on the player overlay (next to the (i) chip) to cycle\
Off, 30 s, 45 s, 60 s and 2 min; the chosen interval is shown beside it. It can also be set from the controller: open the library (Y),\
go to the menu (L) and press A on **Auto-change visualisation**. When enabled, the next shader (in list order) starts after the time you set;\
when off, it stays on the current one. Switching shaders yourself with L/R restarts the countdown. The setting is saved in `config.ini`.

## Shader Controls
L/R Buttons: Previous/Next Shader\
B Button: Show/hide the sinus scroller\
X Button: Next image for iChannel2\
New, removed and changed shader files are picked up automatically (no rescan button needed)

## System Controls
Plus Button: Exit Application\
Minus Button: Start/Stop FTP Server

## Audio
Supported Audio Formats\
MP3, WAV, OGG, FLAC\
MOD, XM, S3M, IT (Tracker modules)\
MIDI, AIFF

## Shader Files
Place .frag or .glsl files in /switch/shaderfun/shaders/ or /switch/shaderfun/test/\
(Note: test folder takes priority, Shaderfun folder is used if test is empty)

## Shaders support Shadertoy-style uniforms:
iResolution (vec3): Viewport resolution\
iTime (float): Time in seconds\
iChannel0 (sampler2D): Waveform data\
iChannel1 (sampler2D): Spectrum data  
iChannel2 / iChannel3 (sampler2D): Images (PNG/JPG/BMP) from /switch/shaderfun/images/  
iMouse (vec4): Touchscreen position (xy) and click position (zw), Shadertoy style  
iTimeDelta (float), iFrame (int), iChannelResolution (vec3[4]): optional, declare them to use them  

### Using images

Put images in `sdmc:/switch/shaderfun/images/`. Press **X** to cycle the image shown on iChannel2.  
A shader can also request specific images with comments near the top of the file:

```
//!iChannel2 "wood.png"
//!iChannel3 "noise.jpg"
```

### Sinus scroller

A rainbow sine-wave scroller can be drawn on top of any shader. Press **B** to show/hide it, or add `//!scroller on` to a shader to turn it on when that shader loads (see `shaders/Scroller/Image Background.frag`).  
Touch the screen: letters near your finger swell and are pulled towards it, and the text follows your finger while you drag.  
Edit the message in `sdmc:/switch/shaderfun/scroller.txt` (it reloads by itself). `#` pauses the text for 2 seconds, `$` toggles double speed.  
Use your own font by putting `font.ttf` in `sdmc:/switch/shaderfun/`.

Shaders are reloaded automatically when the file changes (e.g. after an FTP upload).  
Compile errors are written to `sdmc:/switch/shaderfun/shader_errors.log`.  


## FTP Server
The built-in FTP server allows easy file management:\
Press Minus to start the FTP server\
Connect to the IP address of your Switch

Default FTP credentials:\
FTP port: 5000\
FTP username: switch\
FTP password: ftp123\

Note: These can be changed by a program generated file, "sdmc:/switch/shaderfun/ftp_config.txt"\
A custom FTP MOTD can be loaded from a file, "sdmc:/switch/shaderfun/ftp_motd.txt"

## LED indicators (On Switch controller):
Breathing: Server running, waiting for connection\
Solid: Client connected\
Off: Server stopped

## Creating Custom Shaders
Create fragment shaders that react to audio data.\
Example structure:
```
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel0; // Waveform
uniform sampler2D iChannel1; // Spectrum
varying vec2 vUV;

void main() {
    // Your shader code here
    // Use texture2D(iChannel0, uv) for waveform data
    // Use texture2D(iChannel1, uv) for spectrum data
}
```

## Building from Source
Prerequisites:\
devkitPro with Switch toolchain\
SDL2, SDL2_mixer (2.6 or newer recommended: gives track tags, duration and accurate seeking), SDL2_image, SDL2_ttf\
OpenGL ES 2.0\
The SDL2 libraries come from devkitPro's package manager (the codec libraries SDL2_mixer needs are installed with it):\
`pacman -S switch-sdl2 switch-sdl2_mixer switch-sdl2_image switch-sdl2_ttf`\
(`pacman -Qi switch-sdl2_mixer` shows which SDL2_mixer version you have; use `dkp-pacman` if that is what your install calls it.)
libsmb2 (network music) and libusbhsfs, GPL build (USB drives): see below

**libsmb2** (SMB client):\
`git clone https://github.com/sahlberg/libsmb2.git`\
`cd libsmb2`\
`make -f Makefile.platform switch_install`\
If the install step stops with *multiple target patterns* (a make problem with Windows paths), the library itself has been built: copy `libsmb2.a` (from `build/lib`) to\
`$DEVKITPRO/portlibs/switch/lib` and the files in `include/smb2` to `$DEVKITPRO/portlibs/switch/include/smb2`.

**libusbhsfs** (USB mass storage), GPL build with NTFS and EXT support. ShaderFun uses a **modified copy** of libusbhsfs 0.2.10, included in this repository in the `libusbhsfs` folder.\
The changes make more drives mount, and add a `volume_label` field to `UsbHsFsDevice` that `usbdrive.cpp` uses to name a drive. Don't use the unmodified upstream library: some drives won't mount, and `usbdrive.cpp` won't compile.\
`pacman -S switch-ntfs-3g switch-lwext4`\
`cd libusbhsfs`\
`make BUILD_TYPE=GPL install`

### Optional: building SDL2_mixer 2.8.1 yourself
The package from `pacman` is enough to build ShaderFun. The author runs SDL2_mixer 2.8.1 built from source, which is only needed if you want a newer version than the package.\
Get the SDL_mixer source ([github.com/libsdl-org/SDL_mixer](https://github.com/libsdl-org/SDL_mixer), tag `release-2.8.1`), then in an empty `build` folder inside it:

```
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake \
  -DCMAKE_INSTALL_PREFIX=$DEVKITPRO/portlibs/switch \
  -DSDL2MIXER_BUILD_SHARED_LIBS=OFF \
  -DSDL2MIXER_VENDORED=OFF \
  -DSDL2MIXER_WAVE=ON \
  -DSDL2MIXER_FLAC=ON -DSDL2MIXER_FLAC_DRFLAC=ON \
  -DSDL2MIXER_MP3=ON -DSDL2MIXER_MP3_MINIMP3=ON \
  -DSDL2MIXER_VORBIS=TREMOR -DSDL2MIXER_VORBIS_TREMOR_SHARED=OFF \
  -DSDL2MIXER_MOD=ON -DSDL2MIXER_MOD_MODPLUG=ON -DSDL2MIXER_MOD_MODPLUG_SHARED=OFF -DSDL2MIXER_MOD_XMP=OFF \
  -DSDL2MIXER_OPUS=ON -DSDL2MIXER_OPUS_SHARED=OFF \
  -DSDL2MIXER_MIDI=ON -DSDL2MIXER_MIDI_TIMIDITY=ON -DSDL2MIXER_MIDI_FLUIDSYNTH=OFF \
  -DSDL2MIXER_GME=OFF \
  -DSDL2MIXER_WAVPACK=OFF
make -j$(nproc)
make install
```

Afterwards fix the pkg-config file, because it names Tremor differently from devkitPro's library:\
1. Open `$DEVKITPRO/portlibs/switch/lib/pkgconfig/SDL2_mixer.pc` in a text editor.\
2. Find the line starting with `Requires.private:` (or `Requires:`).\
3. In that list, replace `tremor` with `vorbisidec`.

## Build Instructions
git clone https://github.com/mrdude2478/shaderfun.git \
cd shaderfun\
make

## Troubleshooting
No Audio:\
Ensure music files are in supported formats\
Check volume isn't muted\
Verify files are in shaderfun music directory

Shaders Loaded:\
Check shader files have .frag or .glsl extension\
Ensure shaders compile without errors\
Try the built-in fallback shaders first

Lyrics not showing:\
Put `Song.lrc` next to `Song.mp3` (same name), or use files with embedded lyrics\
Push the right stick right with the library closed\

Network music not connecting:\
Check the server address and the share name in Network settings (and the user name and password)\
Only SMB2/SMB3 shares work, not SMB1\
`smb.log` in `sdmc:/switch/shaderfun/` shows what happened (it never contains your password)

USB drive not showing:\
Use the dock (USB host mode does not work handheld)\
Wait a few seconds after plugging it in. If the USB row says *not available*, the USB service could not start\

FTP Server Issues:\
Verify network connection\
Check firewall settings\
Ensure sufficient free memory

## Credits
KissFFT - Fast Fourier Transform library\
SDL2 - Cross-platform development library\
SDL2_mixer - Audio mixing library\
[libsmb2](https://github.com/sahlberg/libsmb2) - SMB2/3 client library (LGPL 2.1)\
[libusbhsfs](https://github.com/DarkMatterCore/libusbhsfs) - USB mass storage support (GPL build, modified copy included in the `libusbhsfs` folder), with NTFS-3G and lwext4\
DejaVu Sans - UI font (see DejaVu-LICENSE.txt)\
Switch Homebrew Community\
[Mod Archive](https://modarchive.org/)

## License
The ShaderFun source code is licensed under the MIT License - see the LICENSE file for details.\
USB drive support links the GPL build of libusbhsfs (together with NTFS-3G and lwext4), so program builds that include it are distributed under the terms of the GPL v2 or later.\
The modified libusbhsfs source is in the `libusbhsfs` folder, with its own `LICENSE_GPLv2+.md`.\
The network support links libsmb2 (LGPL 2.1).

## Contributing
Contributions are welcome! Please feel free to submit pull requests, report bugs, or suggest new features.

## Fork the project
Create your feature branch (git checkout -b feature/AmazingFeature)\
Commit your changes (git commit -m 'Add some AmazingFeature')\
Push to the branch (git push origin feature/AmazingFeature)

## Support
If you encounter any issues or have questions:\
Check the Issues page\
Create a new issue with detailed information\
Include your Switch firmware version and homebrew setup

## Sharing your created shader files
If you created a stunning audio reactive shader or nice non audio reactive shader and want to share I can add it to the git, just post a message with your shader code and I'll check it out.

## Disclaimer:
This is homebrew software not affiliated with Nintendo. Use at your own risk.\
Enjoy the visuals! 🎵✨

## Screenshots:
![Screenshot](https://i.ibb.co/zhc6pCfT/2.jpg)
![Screenshot](https://i.ibb.co/4nFyT4d3/3.jpg)







