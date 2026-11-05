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
💡 LED Feedback: Visual indicators for FTP server status\
🔄 Hot Reloading: Rescan files without restarting the application

## Installation
Download the latest release from the Releases page\
Extract the .nro file to /switch/shaderfun/\
On your MicroSD card create the following directory structure:\
sdmc:/switch/shaderfun/\
├── shaderfun.nro\
├── music/          # Put your music files here\
├── shaders/        # Put your .frag/.glsl shaders here\
└── test/           # Alternative shader location for testing new shader files

## Music Controls
A Button: Play/Pause\
ZL/ZR: Previous/Next Song\
D-Pad Up/Down: Volume Control\
D-Pad Left/Right: Seek ±10 Seconds

## Shader Controls
L/R Buttons: Previous/Next Shader\
Y Button: Rescan Music & Shader Folders / Restart song\
Left Stick Press: Rescan Shaders Only\
Right Stick Press: Rescan Music Only

## System Controls
Plus Button: Exit Application\
Minus Button: Start/Stop FTP Server\
X Button: Toggle LED Patterns (Debug)

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
SDL2, SDL2_mixer\
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







