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
🔄 Hot Reloading: Rescan files without restarting the application

## Installation
Download the latest release from the Releases page\
Extract the .nro file to /switch/shaderfun/\
On your MicroSD card create the following directory structure:\
sdmc:/switch/shaderfun/\
├── shaderfun.nro\
├── music/          # Default music folder (change it from the in-app library)\
├── images/         # Images for shaders (iChannel2/iChannel3)\
├── config.ini      # Created automatically (music folder, volume, play mode)\
├── scroller.txt    # Optional: your own scroller message\
├── shaders/        # Put your .frag/.glsl shaders here\
└── test/           # Alternative shader location for testing new shader files

## Music Player
The player is an overlay on top of the visualisation. It appears when you press a music button or touch the screen
and fades out after 5 seconds. The track title also scrolls through the sinus scroller (press B to show it).

Overlay: title / artist, progress bar, play-pause, previous/next, rewind/forward 10s, volume, play mode and a Library button.\
Touch: tap the buttons, drag the progress bar to seek, drag the volume slider, tap the mode icon to change repeat/shuffle.

**Library** (press Y or tap the Library button): a panel slides in from the left while the shader keeps running behind it.\
It opens on a menu: **Folders**, **Albums**, **Artists**, **Genres**, **Playlists**, **All songs** and **Rescan library**.\
Albums, artists and genres come from the tags of your music files. The first time, the music folder is read in the background\
(the menu shows the progress) and cached in `sdmc:/switch/shaderfun/library.idx`, so later starts only look at new or changed files.\
Untagged files are grouped by folder name. Two albums with the same name by different artists stay separate, and albums with an\
album-artist tag (compilations) stay together.\
Pick a track to play it and queue the rest of that album/list, or use "Play album" / "Play all" at the top of a page.\
**Playlists** shows `.m3u` / `.m3u8` files from `sdmc:/switch/shaderfun/playlists/` and from your music folder, and can save the current queue as a new playlist.\
**Folders** is the old file browser: "Play this folder" queues everything inside, "Set as music folder" makes it your default (saved in `config.ini` and re-indexed).\
Touch: tap a row, drag to scroll, use the Back row or the header buttons (menu / music folder / close), tap outside the panel to close.\
Controller: D-Pad move, A open/play, B back (closes at the top), X play the highlighted album/artist/genre/playlist/folder, Y close, L menu, R music folder.

Play modes (R3 or the overlay icon): Repeat all, Shuffle, Repeat one, Play once.

**Track info** (L3, or tap the title / the (i) chip): a panel on the right shows the embedded cover art, title, artist, album,\
album artist, composer, track and disc number, year, genre and comment, plus format, bitrate, sample rate, channels, length, size and path.\
Supported tags: ID3v2.2/2.3/2.4 and ID3v1 (MP3), FLAC and Ogg Vorbis/Opus comments (including embedded pictures), WAV INFO and tracker module titles.

## Music Controls
A: Play/Pause\
ZL/ZR: Previous/Next track (Previous restarts the track after 3 seconds)\
D-Pad Up/Down: Volume\
D-Pad Left/Right: Seek -/+ 10 seconds\
Right Stick Press: Change play mode\
Left Stick Press (L3): Track info - tags, cover art and file details (press again or B to close)\
Y: Open/close the library

In the library: see above.

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
OpenGL ES 2.0

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

FTP Server Issues:\
Verify network connection\
Check firewall settings\
Ensure sufficient free memory

## Credits
KissFFT - Fast Fourier Transform library\
SDL2 - Cross-platform development library\
SDL2_mixer - Audio mixing library\
DejaVu Sans - UI font (see DejaVu-LICENSE.txt)\
Switch Homebrew Community\
[Mod Archive](https://modarchive.org/)

## License
This project is licensed under the MIT License - see the LICENSE file for details.

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







