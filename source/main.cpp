/*
Created By MrDude
=================
Controls:
D-pad Up/Down: Volume control
D-pad Left/Right: Seek backward/forward 10 seconds
ZL/ZR: Previous/Next song  (also: hold ZL+RStick = free-look, hold ZR+RStick X = material)
L/R: Switch shaders
Plus: Exit
Minus: Start/Stop FTP Server
A: Play/Pause
X: Cycle background image (iChannel2)
Y: Open library / rescan
B: Toggle sinus scroller
Right Stick X flick: Cycle 3D models
Right Stick Y: Zoom camera
Hold ZL + Right Stick: Free-look orbit
Hold ZR + Right Stick X: Cycle material (Metallic/Chrome/Neon/Matte)
*/

#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <cmath>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>
#include "ftp.h"
#include "scroller.h"
#include "player.h"
#include "ui.h"
#include "library.h"
#include "model3d.h"

PadState pad;
HidsysUniquePadId g_unique_pad_ids[2] = { 0 };
s32 g_total_entries = 0;
bool g_led_state = false; // Track current LED state
// FTP LED state management
static bool wasClientConnected = false;
static bool currentLedPatternSet = false;

enum LedState {
	LED_SOLID,
	LED_BLINK_SLOW,
	LED_PULSE_FAST,
	LED_BREATHING,
	LED_DOUBLE_BLINK
};

// === KissFFT ===
extern "C" {
#include "kiss_fft.h"
}

// === Helpers ===
std::string loadFile(const char* path) {
	std::ifstream file(path);
	if (!file.is_open()) {
		printf("Failed to open shader file: %s\n", path);
		return "";
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

// === Built-in vertex shader (always used) ===
const char* vertexShaderSrc = R"(
attribute vec2 aPos;
varying vec2 vUV;
void main() {
    vUV = (aPos + 1.0) * 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

// === Fallback fragment shader (if no frag files are found) ===
const char* fallbackFragmentShader = R"(
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel0; // Waveform
uniform sampler2D iChannel1; // Spectrum
varying vec2 vUV;

vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263, 0.416, 0.557);
    return a + b * cos(6.28318 * (c * t + d));
}

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv;
    vec3 finalColor = vec3(0.0);
    
    // Sample audio data for reactivity
    float waveform = texture2D(iChannel0, vec2(uv.x, 0.0)).r;
    float spectrum = texture2D(iChannel1, vec2(uv.x * 0.5, 0.0)).r;
    
    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;
        float d = length(uv) * exp(-length(uv0));
        vec3 col = palette(length(uv0) + i * 0.4 + iTime * 0.4 + spectrum * 2.0);
        d = sin(d * 8.0 + iTime + waveform * 10.0) / 8.0;
        d = abs(d);
        d = pow(0.01 / d, 1.2 + spectrum);
        finalColor += col * d;
    }
        
    gl_FragColor = vec4(finalColor, 1.0);
}
)";

// === Small string helpers ===
static std::string toLower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return (char)std::tolower(c); });
	return s;
}

static bool hasExtension(const std::string& name, const char* ext) {
	std::string n = toLower(name);
	size_t el = strlen(ext);
	return n.size() > el && n.compare(n.size() - el, el, ext) == 0;
}

// === Shader error log (readable over FTP: sdmc:/switch/shaderfun/shader_errors.log) ===
static void logShaderError(const std::string& label, const std::string& msg) {
	printf("Shader error in %s:\n%s\n", label.c_str(), msg.c_str());
	FILE* f = fopen("sdmc:/switch/shaderfun/shader_errors.log", "a");
	if (f) {
		fprintf(f, "[%s]\n%s\n\n", label.c_str(), msg.c_str());
		fclose(f);
	}
}

// Returns 0 (and fills errOut) on failure. Never leaks the shader object.
static GLuint compileShader(GLenum type, const char* src, std::string* errOut = nullptr) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char buffer[2048];
		glGetShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
		if (errOut) *errOut = buffer;
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

// === Image channels (iChannel2 / iChannel3) ===
// iChannel0 = waveform, iChannel1 = spectrum, iChannel2/3 = images loaded from the SD card.
struct ImageTex {
	GLuint id = 0;
	int w = 1;
	int h = 1;
	std::string path;
};

static bool g_scrollerEnabled = false; // B toggles; a shader can force it with //!scroller on|off
static bool g_shaderAudioEnabled = true; // //!audio off silences iChannel0/1 for this shader

static const int NUM_IMAGE_CHANNELS = 2;
static ImageTex g_img[NUM_IMAGE_CHANNELS];
static std::vector<std::string> g_imageFiles;
static int g_imageIndex = 0;

static void setupTextureParams() {
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static bool loadImageTexture(const std::string& path, ImageTex& out) {
	if (out.path == path && out.id != 0) return true; // already loaded

	SDL_Surface* surf = IMG_Load(path.c_str());
	if (!surf) {
		printf("Failed to load image %s: %s\n", path.c_str(), IMG_GetError());
		return false;
	}
	// Convert to bytes R,G,B,A in memory (what GL_RGBA/GL_UNSIGNED_BYTE expects on little-endian)
	SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(surf);
	if (!rgba) {
		printf("Failed to convert image %s: %s\n", path.c_str(), SDL_GetError());
		return false;
	}

	GLint maxSize = 2048;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
	if (rgba->w > maxSize || rgba->h > maxSize) {
		printf("Image %s too large (%dx%d, max %d)\n", path.c_str(), rgba->w, rgba->h, (int)maxSize);
		SDL_FreeSurface(rgba);
		return false;
	}

	// Flip vertically so uv (0,0) is the bottom-left corner, like Shadertoy.
	const int rowBytes = rgba->w * 4;
	std::vector<unsigned char> pixels((size_t)rowBytes * rgba->h);
	const unsigned char* base = (const unsigned char*)rgba->pixels;
	for (int y = 0; y < rgba->h; y++) {
		memcpy(&pixels[(size_t)(rgba->h - 1 - y) * rowBytes], base + (size_t)y * rgba->pitch, rowBytes);
	}

	if (out.id == 0) glGenTextures(1, &out.id);
	glBindTexture(GL_TEXTURE_2D, out.id);
	setupTextureParams();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

	out.w = rgba->w;
	out.h = rgba->h;
	out.path = path;
	printf("Loaded image %s (%dx%d)\n", path.c_str(), out.w, out.h);
	SDL_FreeSurface(rgba);
	return true;
}

static void scanImageFolderRecursive(const std::string& dirPath, std::vector<std::string>& files) {
	DIR* dir = opendir(dirPath.c_str());
	if (!dir) return;
	struct dirent* ent;
	while ((ent = readdir(dir)) != NULL) {
		std::string name = ent->d_name;
		if (name == "." || name == "..") continue;
		std::string fullPath = dirPath + "/" + name;
		if (ent->d_type == DT_DIR) {
			scanImageFolderRecursive(fullPath, files);
		}
		else if (hasExtension(name, ".png") || hasExtension(name, ".jpg") ||
			hasExtension(name, ".jpeg") || hasExtension(name, ".bmp")) {
			files.push_back(fullPath);
		}
	}
	closedir(dir);
}

static void initImageTextures() {
	g_imageFiles.clear();
	scanImageFolderRecursive("sdmc:/switch/shaderfun/images", g_imageFiles);
	if (g_imageFiles.empty()) scanImageFolderRecursive("romfs:/images", g_imageFiles);
	std::sort(g_imageFiles.begin(), g_imageFiles.end());
	printf("Found %zu images\n", g_imageFiles.size());

	// 1x1 black placeholders so the samplers are always valid
	const unsigned char black[4] = { 0, 0, 0, 255 };
	for (int c = 0; c < NUM_IMAGE_CHANNELS; c++) {
		glGenTextures(1, &g_img[c].id);
		glBindTexture(GL_TEXTURE_2D, g_img[c].id);
		setupTextureParams();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
	}
	if (!g_imageFiles.empty()) loadImageTexture(g_imageFiles[0], g_img[0]);
}

static void cleanupImageTextures() {
	for (int c = 0; c < NUM_IMAGE_CHANNELS; c++) {
		if (g_img[c].id) glDeleteTextures(1, &g_img[c].id);
		g_img[c] = ImageTex();
	}
}

// A shader can pick its own images with a comment near the top of the file:
//   //!iChannel2 "wood.png"
//   //!iChannel3 "sdmc:/switch/shaderfun/images/other/noise.jpg"
// Bare names are looked up in sdmc:/switch/shaderfun/images/ first, then in the built-in romfs:/images/.
//   //!scroller on     (or off) shows/hides the sinus scroller when this shader loads
//   //!audio off       silences waveform/spectrum for this shader (iChannel0/1)
// 3D model directives (off by default):
//   //!model "cube_atlas.obj" | //!model on | //!model off
//   //!material matte | neon | chrome | metallic
//   //!model_rotate 0.6  //!model_rotate_x 0.4  //!model_rotate_z 0.2
//   //!model_zoom 5.5  //!model_pos 0 0.2 0
//   //!model_yaw 0  //!model_pitch 0  //!model_auto on
//   //!model_audio off  //!model_ground off
// Without a directive, iChannel2 shows the image selected with the X button.
static void applyChannelDirectives(const std::string& fragSource, const std::string& shaderPath = "") {
	bool set[NUM_IMAGE_CHANNELS] = { false, false };
	std::istringstream in(fragSource);
	std::string line;
	int lineNo = 0;
	// Default: audio reactive on; shaders may opt out with //!audio off
	g_shaderAudioEnabled = true;

	while (std::getline(in, line) && lineNo++ < 40) {
		size_t sp = line.find("//!scroller");
		if (sp != std::string::npos) {
			g_scrollerEnabled = (toLower(line.substr(sp + 11)).find("off") == std::string::npos);
		}
		sp = line.find("//!audio");
		if (sp != std::string::npos) {
			// //!audio off  |  //!audio on
			// avoid matching unrelated future flags
			std::string rest = toLower(line.substr(sp + 8));
			if (rest.find("off") != std::string::npos)
				g_shaderAudioEnabled = false;
			else if (rest.find("on") != std::string::npos)
				g_shaderAudioEnabled = true;
		}
		for (int c = 0; c < NUM_IMAGE_CHANNELS; c++) {
			std::string key = "//!iChannel" + std::to_string(c + 2);
			size_t p = line.find(key);
			if (p == std::string::npos) continue;
			size_t q1 = line.find('"', p);
			size_t q2 = (q1 == std::string::npos) ? std::string::npos : line.find('"', q1 + 1);
			if (q2 == std::string::npos) continue;
			std::string name = line.substr(q1 + 1, q2 - q1 - 1);
			std::string full = name;
			if (name.find(':') == std::string::npos) {
				struct stat ist;
				full = "sdmc:/switch/shaderfun/images/" + name;
				if (stat(full.c_str(), &ist) != 0) full = "romfs:/images/" + name;
			}
			loadImageTexture(full, g_img[c]);
			set[c] = true;
		}
	}
	if (!set[0] && !g_imageFiles.empty()) {
		loadImageTexture(g_imageFiles[g_imageIndex % g_imageFiles.size()], g_img[0]);
	}

	// 3D model / material / transform directives from the frag file
	model3d::applyShaderDirectives(fragSource);
	if (!shaderPath.empty()) {
		model3d::applyAutoMatch(shaderPath);
	}
}

// === Shader state ===
struct ShaderProgram {
	GLuint prog = 0;
	GLint iResolutionLoc = -1;
	GLint iTimeLoc = -1;
	GLint iTimeDeltaLoc = -1;
	GLint iFrameLoc = -1;
	GLint iMouseLoc = -1;
	GLint iChannelResolutionLoc = -1;
	GLint iChannelLoc[4] = { -1, -1, -1, -1 };
};

static time_t g_shaderMtime = 0; // modification time of the currently loaded shader file

// isFallback stops the "fall back to the fallback" recursion if even that fails to compile.
ShaderProgram loadShaderProgram(const char* fragSrc, const std::string& label = "built-in", bool isFallback = false) {
	std::string err;
	GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc, &err);
	GLuint fs = vs ? compileShader(GL_FRAGMENT_SHADER, fragSrc, &err) : 0;

	GLuint prog = 0;
	if (vs && fs) {
		prog = glCreateProgram();
		glAttachShader(prog, vs);
		glAttachShader(prog, fs);
		glBindAttribLocation(prog, 0, "aPos");
		glLinkProgram(prog);

		GLint linkStatus = 0;
		glGetProgramiv(prog, GL_LINK_STATUS, &linkStatus);
		if (!linkStatus) {
			char buffer[2048];
			glGetProgramInfoLog(prog, sizeof(buffer), NULL, buffer);
			err = std::string("link error: ") + buffer;
			glDeleteProgram(prog);
			prog = 0;
		}
	}
	if (vs) glDeleteShader(vs);
	if (fs) glDeleteShader(fs);

	ShaderProgram sp;
	if (!prog) {
		logShaderError(label, err);
		if (isFallback) return sp;
		printf("Using fallback shader instead.\n");
		return loadShaderProgram(fallbackFragmentShader, "fallback", true);
	}

	sp.prog = prog;
	sp.iResolutionLoc = glGetUniformLocation(prog, "iResolution");
	sp.iTimeLoc = glGetUniformLocation(prog, "iTime");
	sp.iTimeDeltaLoc = glGetUniformLocation(prog, "iTimeDelta");
	sp.iFrameLoc = glGetUniformLocation(prog, "iFrame");
	sp.iMouseLoc = glGetUniformLocation(prog, "iMouse");
	sp.iChannelResolutionLoc = glGetUniformLocation(prog, "iChannelResolution");
	for (int c = 0; c < 4; c++) {
		std::string n = "iChannel" + std::to_string(c);
		sp.iChannelLoc[c] = glGetUniformLocation(prog, n.c_str());
	}
	printf("Shader loaded: %s\n", label.c_str());
	return sp;
}

// === Load fragment shader file (with fallback) ===
ShaderProgram loadShaderFromFile(const std::string& path) {
	printf("Loading shader from: %s\n", path.c_str());

	struct stat st;
	g_shaderMtime = (stat(path.c_str(), &st) == 0) ? st.st_mtime : 0;

	std::string fragSource = loadFile(path.c_str());
	if (fragSource.empty()) {
		printf("Shader file is empty, using fallback\n");
		return loadShaderProgram(fallbackFragmentShader, path + " (empty)", true);
	}
	applyChannelDirectives(fragSource, path);
	return loadShaderProgram(fragSource.c_str(), path);
}

static bool g_scanQuiet = false; // true while the background rescan runs

// === Recursively scan shader folder and subfolders ===
void scanShaderFolderRecursive(const std::string& dirPath, std::vector<std::string>& files) {
	DIR* dir = opendir(dirPath.c_str());
	if (!dir) {
		if (!g_scanQuiet) printf("Could not open directory: %s\n", dirPath.c_str());
		return;
	}

	struct dirent* ent;
	while ((ent = readdir(dir)) != NULL) {
		std::string name = ent->d_name;

		// Skip current and parent directory entries
		if (name == "." || name == "..") {
			continue;
		}

		std::string fullPath = dirPath + "/" + name;

		// Check if it's a directory
		if (ent->d_type == DT_DIR) {
			// Recursively scan subdirectory
			scanShaderFolderRecursive(fullPath, files);
		}
		// Check if it's a .frag file
		else if ((name.size() > 5 && name.substr(name.size() - 5) == ".frag") || (name.size() > 5 && name.substr(name.size() - 5) == ".glsl")) {
			files.push_back(fullPath);
			if (!g_scanQuiet) printf("Found shader: %s\n", fullPath.c_str());
		}
	}
	closedir(dir);
}

// === Scan shader folders (wrapper function) ===
std::vector<std::string> scanShaderFolders(const char* dirPath) {
	std::vector<std::string> files;
	scanShaderFolderRecursive(dirPath, files);
	if (!g_scanQuiet) printf("Found %zu shader files in %s\n", files.size(), dirPath);
	return files;
}

// === Check directory existence ===
void checkDirectories() {
	const char* dirs[] = {
		"sdmc:/switch/shaderfun/test",
		"sdmc:/switch/shaderfun/shaders",
		"romfs:/shaders",
		"sdmc:/switch/shaderfun/music",
		"romfs:/music"
	};

	for (const char* dir : dirs) {
		DIR* testDir = opendir(dir);
		if (testDir) {
			printf("Directory exists: %s\n", dir);
			closedir(testDir);
		}
		else {
			printf("Directory does not exist: %s\n", dir);
		}
	}
}

// === Audio globals ===
const int FFT_SIZE = 512;   // must be power of 2
static float audioWaveform[FFT_SIZE];
static float audioSpectrum[FFT_SIZE / 2];

static kiss_fft_cfg fftCfg = nullptr;
static kiss_fft_cpx fftIn[FFT_SIZE];
static kiss_fft_cpx fftOut[FFT_SIZE];

// OpenGL textures for audio
GLuint audioTexWaveform;
GLuint audioTexSpectrum;


// Ring buffer write position (audio thread writes, main thread reads)
static volatile int g_waveWrite = 0;
static float audioWaveformOrdered[FFT_SIZE]; // oldest -> newest, what the shaders and FFT see
static const bool USE_HANN_WINDOW = true;    // reduces spectral leakage (gain-compensated)

// Effect callback to capture PCM
void audioEffectCallback(int chan, void* stream, int len, void* udata) {
	int16_t* samples = (int16_t*)stream;
	int count = len / 2; // 16-bit samples
	int w = g_waveWrite;
	for (int i = 0; i < count; i += 2) { // left channel
		audioWaveform[w] = samples[i] / 32768.0f;
		w = (w + 1) % FFT_SIZE;
	}
	g_waveWrite = w;
}

// Compute FFT from waveform
void computeFFT() {
	// Unroll the ring buffer so the waveform has no seam in the middle
	const int start = g_waveWrite;
	for (int i = 0; i < FFT_SIZE; i++) {
		float s = audioWaveform[(start + i) % FFT_SIZE];
		audioWaveformOrdered[i] = s;
		float win = USE_HANN_WINDOW
			? 2.0f * (0.5f - 0.5f * cosf(2.0f * 3.14159265f * (float)i / (float)(FFT_SIZE - 1)))
			: 1.0f;
		fftIn[i].r = s * win;
		fftIn[i].i = 0;
	}
	kiss_fft(fftCfg, fftIn, fftOut);

	for (int i = 0; i < FFT_SIZE / 2; i++) {
		float mag = sqrtf(fftOut[i].r * fftOut[i].r + fftOut[i].i * fftOut[i].i);
		audioSpectrum[i] = mag / (FFT_SIZE / 2);
	}
}

// Upload audio data to textures
void uploadAudioTextures() {
	if (!g_shaderAudioEnabled) {
		// Silent textures so existing shaders stop reacting without code changes
		static float zeroWave[FFT_SIZE];
		static float zeroSpec[FFT_SIZE / 2];
		static bool inited = false;
		if (!inited) {
			for (int i = 0; i < FFT_SIZE; i++) zeroWave[i] = 0.5f; // mid line for waveform
			for (int i = 0; i < FFT_SIZE / 2; i++) zeroSpec[i] = 0.0f;
			inited = true;
		}
		glBindTexture(GL_TEXTURE_2D, audioTexWaveform);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, FFT_SIZE, 1, 0, GL_LUMINANCE, GL_FLOAT, zeroWave);
		glBindTexture(GL_TEXTURE_2D, audioTexSpectrum);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, FFT_SIZE / 2, 1, 0, GL_LUMINANCE, GL_FLOAT, zeroSpec);
		return;
	}
	// Waveform -> iChannel0
	glBindTexture(GL_TEXTURE_2D, audioTexWaveform);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, FFT_SIZE, 1, 0, GL_LUMINANCE, GL_FLOAT, audioWaveformOrdered);

	// Spectrum -> iChannel1
	glBindTexture(GL_TEXTURE_2D, audioTexSpectrum);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, FFT_SIZE / 2, 1, 0, GL_LUMINANCE, GL_FLOAT, audioSpectrum);
}

// Initialize audio system
bool initAudio() {
	// Initialize SDL_mixer
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
		printf("SDL_mixer init failed: %s\n", Mix_GetError());
		return false;
	}

	// Initialize KissFFT
	fftCfg = kiss_fft_alloc(FFT_SIZE, 0, NULL, NULL);

	// Initialize audio buffers
	for (int i = 0; i < FFT_SIZE; i++) {
		audioWaveform[i] = 0.0f;
	}
	for (int i = 0; i < FFT_SIZE / 2; i++) {
		audioSpectrum[i] = 0.0f;
	}

	return true;
}

// Cleanup audio system
void cleanupAudio() {
	Mix_CloseAudio();

	if (fftCfg) {
		kiss_fft_free(fftCfg);
		fftCfg = nullptr;
	}
}

/*
Pattern Parameters Explained
baseMiniCycleDuration: Time unit multiplier (higher = slower overall)
totalMiniCycles: Number of phases in your pattern
totalFullCycles: How many times to repeat (0 = infinite)
startIntensity: Initial brightness
ledIntensity: Brightness for that phase (0x0-0xF)
transitionSteps: Steps to reach target intensity (0 = instant)
finalStepDuration: How long to maintain that intensity
*/

void set_led_pattern_for_state(HidsysNotificationLedPattern& pattern, LedState state) {
	memset(&pattern, 0, sizeof(pattern));

	switch (state) {
	case LED_SOLID:
		// Solid pattern (FTP active)
		pattern.baseMiniCycleDuration = 0x1;
		pattern.totalMiniCycles = 0x1;
		pattern.totalFullCycles = 0x0;
		pattern.startIntensity = 0xF;
		pattern.miniCycles[0].ledIntensity = 0xF;
		pattern.miniCycles[0].transitionSteps = 0x0;
		pattern.miniCycles[0].finalStepDuration = 0x7;
		break;

	case LED_BLINK_SLOW:
		// Slow blink (Waiting for connection)
		pattern.baseMiniCycleDuration = 0x8;
		pattern.totalMiniCycles = 0x2;
		pattern.totalFullCycles = 0x0;
		pattern.miniCycles[0].ledIntensity = 0xF;
		pattern.miniCycles[0].transitionSteps = 0x0;
		pattern.miniCycles[0].finalStepDuration = 0xF;
		pattern.miniCycles[1].ledIntensity = 0x0;
		pattern.miniCycles[1].transitionSteps = 0x0;
		pattern.miniCycles[1].finalStepDuration = 0xF;
		break;

	case LED_PULSE_FAST:
		// Fast pulse (File transfer in progress)
		pattern.baseMiniCycleDuration = 0x2;
		pattern.totalMiniCycles = 0x3;
		pattern.totalFullCycles = 0x0;
		pattern.miniCycles[0].ledIntensity = 0xF;
		pattern.miniCycles[0].transitionSteps = 0x5;
		pattern.miniCycles[0].finalStepDuration = 0x2;
		pattern.miniCycles[1].ledIntensity = 0x0;
		pattern.miniCycles[1].transitionSteps = 0x5;
		pattern.miniCycles[1].finalStepDuration = 0x2;
		pattern.miniCycles[2].ledIntensity = 0x0;
		pattern.miniCycles[2].transitionSteps = 0x0;
		pattern.miniCycles[2].finalStepDuration = 0x3;
		break;

	case LED_BREATHING:
		pattern.baseMiniCycleDuration = 0x4;
		pattern.totalMiniCycles = 0x2;
		pattern.totalFullCycles = 0x0;
		pattern.miniCycles[0].ledIntensity = 0xF;
		pattern.miniCycles[0].transitionSteps = 0xA;
		pattern.miniCycles[0].finalStepDuration = 0x1;
		pattern.miniCycles[1].ledIntensity = 0x0;
		pattern.miniCycles[1].transitionSteps = 0xA;
		pattern.miniCycles[1].finalStepDuration = 0x1;
		break;

	case LED_DOUBLE_BLINK:
		pattern.baseMiniCycleDuration = 0x3;
		pattern.totalMiniCycles = 0x4;
		pattern.totalFullCycles = 0x3;
		pattern.miniCycles[0].ledIntensity = 0xF;
		pattern.miniCycles[0].transitionSteps = 0x0;
		pattern.miniCycles[0].finalStepDuration = 0x3;
		pattern.miniCycles[1].ledIntensity = 0x0;
		pattern.miniCycles[1].transitionSteps = 0x0;
		pattern.miniCycles[1].finalStepDuration = 0x2;
		pattern.miniCycles[2].ledIntensity = 0xF;
		pattern.miniCycles[2].transitionSteps = 0x0;
		pattern.miniCycles[2].finalStepDuration = 0x3;
		pattern.miniCycles[3].ledIntensity = 0x0;
		pattern.miniCycles[3].transitionSteps = 0x0;
		pattern.miniCycles[3].finalStepDuration = 0x8;
		break;
	}
}

void turn_led_on(LedState patternType = LED_SOLID) {
	HidsysNotificationLedPattern pattern;
	set_led_pattern_for_state(pattern, patternType);  // Pass the pattern by reference

	// Always refresh pad IDs to ensure they're current
	padUpdate(&pad);
	g_total_entries = 0;
	memset(g_unique_pad_ids, 0, sizeof(g_unique_pad_ids));

	HidNpadIdType npad_id_type = padIsHandheld(&pad) ? HidNpadIdType_Handheld : HidNpadIdType_No1;
	Result rc = hidsysGetUniquePadsFromNpad(npad_id_type, g_unique_pad_ids, 2, &g_total_entries);

	if (R_SUCCEEDED(rc) && g_total_entries > 0) {
		for (int i = 0; i < g_total_entries; i++) {
			hidsysSetNotificationLedPattern(&pattern, g_unique_pad_ids[i]);
		}
		g_led_state = true;
		printf("LED turned on with pattern: %d\n", patternType);
	}
	else {
		printf("Failed to set LED pattern. RC: 0x%x, Entries: %d\n", rc, g_total_entries);
	}
}

void turn_led_off() {
	HidsysNotificationLedPattern pattern;
	memset(&pattern, 0, sizeof(pattern)); // Zero pattern turns LED off

	// Always refresh pad IDs to ensure they're current
	padUpdate(&pad);
	g_total_entries = 0;
	memset(g_unique_pad_ids, 0, sizeof(g_unique_pad_ids));

	HidNpadIdType npad_id_type = padIsHandheld(&pad) ? HidNpadIdType_Handheld : HidNpadIdType_No1;
	Result rc = hidsysGetUniquePadsFromNpad(npad_id_type, g_unique_pad_ids, 2, &g_total_entries);

	if (R_SUCCEEDED(rc) && g_total_entries > 0) {
		for (int i = 0; i < g_total_entries; i++) {
			hidsysSetNotificationLedPattern(&pattern, g_unique_pad_ids[i]);
		}
		g_led_state = false;
	}
}

void toggle_led(LedState patternType = LED_SOLID) {
	if (g_led_state) {
		turn_led_off();
	}
	else {
		turn_led_on(patternType);
	}
}

bool print_ip_local() {
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		printf("Cannot create socket\n");
		return false;
	}

	// Try connecting to local broadcast address (works even without internet)
	struct sockaddr_in remote = {};
	remote.sin_family = AF_INET;
	remote.sin_port = htons(9);
	remote.sin_addr.s_addr = inet_addr("255.255.255.255");

	// Enable broadcast
	int broadcast = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
		printf("Setsockopt failed\n");
		close(sock);
		return false;
	}

	if (connect(sock, (struct sockaddr*)&remote, sizeof(remote)) < 0) {
		printf("Local network not available\n");
		close(sock);
		return false;
	}

	// Get the local address
	struct sockaddr_in local;
	socklen_t len = sizeof(local);
	if (getsockname(sock, (struct sockaddr*)&local, &len) == 0) {
		printf("Your Local IP Address: %s\n", inet_ntoa(local.sin_addr));
		close(sock);
		return true;
	}
	else {
		printf("Could not get IP address\n");
	}

	close(sock);
	return false;
}
// === Rescan functions ===
// === Docked / handheld ===
// Heavy shaders go in a folder whose name starts with "Docked" (e.g. Docked, Docked-intensive).
// They are only listed while the console is docked: the GPU runs much slower in handheld mode.
static bool g_docked = false;               // true in TV mode
static bool g_forceShaderScan = true;       // rescan the shader list on the next frame
static bool g_modeToastPending = false;

static bool isDockedOnlyShader(const std::string& path) {
	const size_t fileStart = path.find_last_of('/');
	if (fileStart == std::string::npos) return false;
	size_t pos = 0;
	while (pos < fileStart) { // every folder component, not the file name
		size_t slash = path.find('/', pos);
		if (slash == std::string::npos || slash > fileStart) break;
		if (toLower(path.substr(pos, slash - pos)).compare(0, 6, "docked") == 0) return true;
		pos = slash + 1;
	}
	return false;
}

// Scans one folder and drops the docked-only shaders while handheld
static std::vector<std::string> scanShaderFoldersForMode(const char* dir) {
	std::vector<std::string> list = scanShaderFolders(dir);
	if (!g_docked) {
		list.erase(std::remove_if(list.begin(), list.end(), isDockedOnlyShader), list.end());
	}
	return list;
}

// Same folder precedence as always: test, then shaders, then the built-in romfs ones
static std::vector<std::string> buildShaderList() {
	std::vector<std::string> list = scanShaderFoldersForMode("sdmc:/switch/shaderfun/test");
	if (list.empty()) list = scanShaderFoldersForMode("sdmc:/switch/shaderfun/shaders");
	if (list.empty()) list = scanShaderFoldersForMode("romfs:/shaders");
	return list;
}

// Reads the dock state. Returns true if it changed since the last call.
static bool updateOperationMode() {
	const bool docked = (appletGetOperationMode() == AppletOperationMode_Console);
	const bool changed = (docked != g_docked);
	g_docked = docked;
	return changed;
}
// === end docked ===

void rescanShaders(std::vector<std::string>& shaderFiles, int& currentShader) {
	printf("Rescanning shaders...\n");

	// Clear current list
	shaderFiles.clear();

	shaderFiles = buildShaderList();

	// Reset to first shader if current is out of bounds
	if (currentShader >= (int)shaderFiles.size()) {  // Cast to int for comparison
		currentShader = 0;
	}

	printf("Rescan complete: Found %zu shaders\n", shaderFiles.size());
}

int main(int argc, char* argv[]) {
	nxlinkStdio(); // log to nxlink if running
	appletSetMediaPlaybackState(true); // Set media playback state to prevent switch sleeping

	//Toggle FTP Server Button vars
	Uint32 startTicks = SDL_GetTicks();
	bool running = true;
	Uint32 lastShaderChange = 0;
	int frameCount = 0;
	//

	// Initialize ROMFS
	Result rc = romfsInit();
	if (R_FAILED(rc)) {
		printf("Failed to initialize ROMFS: 0x%x\n", rc);
	}
	else {
		printf("ROMFS initialized successfully\n");
	}


	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	padInitializeDefault(&pad);
	hidInitializeTouchScreen();
	hidsysInitialize();

	bool network_available = false;

	if (R_FAILED(socketInitializeDefault())) {
		printf("Network unavailable\n");
	}
	else {
		network_available = print_ip_local();
		socketExit();
	}


	// Execute appropriate function based on network status
	if (network_available) {
		//turn_led_on(); //uncomment to test
		// Initialize FTP server
		if (!ftp_init()) {
			printf("Failed to initialize FTP server\n");
		}
		else {
			printf("FTP server initialized - Press Minus to start/stop\n");
		}
	}

	// Initialize SDL with audio support
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		printf("SDL_Init failed: %s\n", SDL_GetError());
		return -1;
	}

	if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG)) == 0) {
		printf("IMG_Init failed: %s\n", IMG_GetError());
	}
	if (TTF_Init() < 0) {
		printf("TTF_Init failed: %s\n", TTF_GetError());
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); // needed for 3D models

	SDL_Window* window = SDL_CreateWindow("Shaderfun Switch - Audio Reactive",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

	if (!window) {
		printf("Failed to create window: %s\n", SDL_GetError());
		return -1;
	}

	SDL_GLContext glContext = SDL_GL_CreateContext(window);
	if (!glContext) {
		printf("Failed to create OpenGL context: %s\n", SDL_GetError());
		return -1;
	}

	// Initialize audio system
	bool audioInitialized = initAudio();
	printf("Audio system %s\n", audioInitialized ? "initialized successfully" : "failed to initialize");

	// Register audio effect callback
	Mix_RegisterEffect(MIX_CHANNEL_POST, audioEffectCallback, NULL, NULL);

	// Test OpenGL functionality
	printf("OpenGL vendor: %s\n", glGetString(GL_VENDOR));
	printf("OpenGL renderer: %s\n", glGetString(GL_RENDERER));
	printf("OpenGL version: %s\n", glGetString(GL_VERSION));

	// Critical OpenGL setup
	glViewport(0, 0, 1280, 720);

	// Test OpenGL rendering with a simple color
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f); // Red
	glClear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SwapWindow(window);
	SDL_Delay(500); // Show red for half second

	glClearColor(0.0f, 1.0f, 0.0f, 1.0f); // Green
	glClear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SwapWindow(window);
	SDL_Delay(500); // Show green for half second

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Back to black
	glClear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SwapWindow(window);

	printf("OpenGL state test completed.\n");

	// Quad vertices
	GLfloat vertices[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		-1.0f,  1.0f,
		 1.0f,  1.0f
	};

	// Create VBO (Vertex Buffer Object)
	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	// Initialize audio textures
	glGenTextures(1, &audioTexWaveform);
	glBindTexture(GL_TEXTURE_2D, audioTexWaveform);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &audioTexSpectrum);
	glBindTexture(GL_TEXTURE_2D, audioTexSpectrum);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Check directories first
	checkDirectories();

	// Image textures for iChannel2/iChannel3 (must run before the first shader loads)
	initImageTextures();

	// Sinus scroller overlay: font from the SD card first (so you can swap it), then the built-in one
	{
		const char* fontCandidates[] = { "sdmc:/switch/shaderfun/font.ttf", "romfs:/font.ttf" };
		bool scrollerOk = false;
		for (const char* p : fontCandidates) {
			if (!scrollerOk) scrollerOk = scroller_init(p, 1280, 720);
		}
		if (scrollerOk) scroller_poll_text_file("sdmc:/switch/shaderfun/scroller.txt");
		else printf("Scroller disabled: no usable font found\n");
	}

	// Music player core + overlay UI
	mkdir("sdmc:/switch/shaderfun", 0777); // make sure the config folder exists
	player::init("sdmc:/switch/shaderfun/config.ini");
	library::init("sdmc:/switch/shaderfun/library.idx");
	library::startScan(player::musicFolder()); // indexes the music folder in the background (cached after the first run)
	bool plReady = false;
	ui::addFontFile("sdmc:/switch/shaderfun/ui_font.ttf"); // optional user font
	ui::addFontFile("romfs:/ui_font.ttf");                 // bundled font (Latin, Greek, Cyrillic)
	// The Switch system fonts supply characters the bundled font doesn't have (Japanese, Korean, Chinese...)
	if (R_SUCCEEDED(plInitialize(PlServiceType_User))) {
		plReady = true;
		const PlSharedFontType fontTypes[] = { PlSharedFontType_Standard, PlSharedFontType_ChineseSimplified,
			PlSharedFontType_ChineseTraditional, PlSharedFontType_KO };
		for (PlSharedFontType ft : fontTypes) {
			PlFontData fd;
			if (R_SUCCEEDED(plGetSharedFontByType(&fd, ft)) && fd.address) ui::addFontMemory(fd.address, fd.size);
		}
	}
	if (!ui::init(1280, 720)) printf("Player UI disabled\n");

	// 3D model layer (OBJ files from sdmc:/switch/shaderfun/models/)
	if (!model3d::init()) printf("3D model support disabled\n");

	// Scan shader directories with fallback logic
	std::vector<std::string> shaderFiles;

	g_docked = (appletGetOperationMode() == AppletOperationMode_Console);
	printf("Console is %s\n", g_docked ? "docked (TV mode)" : "in handheld mode");
	shaderFiles = buildShaderList();
	g_forceShaderScan = false;

	// If both directories are empty or don't exist, use fallback
	if (shaderFiles.empty()) {
		printf("No .frag files found in any directory. Using fallback shader.\n");
	}

	int currentShader = 0;

	ShaderProgram shader = shaderFiles.empty() ?
		loadShaderProgram(fallbackFragmentShader) :
		loadShaderFromFile(shaderFiles[currentShader]);

	// Add FTP state variable
	bool ftpEnabled = false;
	Uint32 lastFtpToggle = 0;

	while (running) {
		// Lets libnx process system messages (dock/undock, HOME menu, close request)
		if (!appletMainLoop()) break;
		if (updateOperationMode()) {
			printf("Operation mode changed: now %s\n", g_docked ? "docked" : "handheld");
			g_forceShaderScan = true;
			g_modeToastPending = true;
		}

		frameCount++;

		padUpdate(&pad);
		u64 kDown = padGetButtonsDown(&pad);

		// Music player overlay: it takes the buttons/touches it uses; the rest continue below
		ui::Result uiRes;
		{
			static const struct { u64 pad; uint32_t ui; } uiMap[] = {
				{ HidNpadButton_Up, ui::BTN_UP }, { HidNpadButton_Down, ui::BTN_DOWN },
				{ HidNpadButton_Left, ui::BTN_LEFT }, { HidNpadButton_Right, ui::BTN_RIGHT },
				{ HidNpadButton_A, ui::BTN_A }, { HidNpadButton_B, ui::BTN_B },
				{ HidNpadButton_X, ui::BTN_X }, { HidNpadButton_Y, ui::BTN_Y },
				{ HidNpadButton_L, ui::BTN_L }, { HidNpadButton_R, ui::BTN_R },
				{ HidNpadButton_ZL, ui::BTN_ZL }, { HidNpadButton_ZR, ui::BTN_ZR },
				{ HidNpadButton_StickR, ui::BTN_R3 }, { HidNpadButton_StickL, ui::BTN_L3 },
				{ HidNpadButton_StickLUp, ui::BTN_LS_UP }, { HidNpadButton_StickLDown, ui::BTN_LS_DOWN },
				{ HidNpadButton_StickLLeft, ui::BTN_LS_LEFT }, { HidNpadButton_StickLRight, ui::BTN_LS_RIGHT } };
			static Uint32 lastUiTicks = SDL_GetTicks();
			const Uint32 nowUiTicks = SDL_GetTicks();
			const u64 kHeld = padGetButtons(&pad);
			ui::Input uin;
			uin.dt = (nowUiTicks - lastUiTicks) / 1000.0f;
			lastUiTicks = nowUiTicks;
			for (const auto& m : uiMap) {
				if (kDown & m.pad) uin.down |= m.ui;
				if (kHeld & m.pad) uin.held |= m.ui;
			}
			HidTouchScreenState uiTouch = {};
			hidGetTouchScreenStates(&uiTouch, 1);
			if (uiTouch.count > 0) {
				uin.touching = true;
				uin.touchX = (float)uiTouch.touches[0].x;
				uin.touchY = (float)uiTouch.touches[0].y;
			}
			uiRes = ui::update(uin);
			for (const auto& m : uiMap) {
				if (uiRes.consumedButtons & m.ui) kDown &= ~m.pad;
			}
		}

		if (kDown & HidNpadButton_Plus) running = false;

		// FTP server toggle with Minus button (with debouncing)
		if (network_available) {

			/*
			if (kDown & HidNpadButton_X && ftpEnabled) {
				static LedState testPattern = LED_SOLID;
				toggle_led(testPattern);
				testPattern = static_cast<LedState>((testPattern + 1) % 5); // Cycle through patterns
				printf("LED pattern: %d\n", testPattern);
			}
			*/

			if ((kDown & HidNpadButton_Minus) && (SDL_GetTicks() - lastFtpToggle > 500)) {
				ftpEnabled = !ftpEnabled;
				lastFtpToggle = SDL_GetTicks();

				if (ftpEnabled) {
					ftp_start(&pad);
					printf("FTP server started\n");
					turn_led_on(LED_BREATHING);  // Start with breathing pattern
					wasClientConnected = false;   // Reset connection state
					currentLedPatternSet = true;
					ftpEnabled = true;
				}
				else {
					ftp_stop(&pad);   // Pass the main program's pad
					printf("FTP server stopped\n");
					turn_led_off();
					ftpEnabled = false;
					ftp_init(); //reinitialse as we cleaned the socket...
				}
			}

			// Update FTP server if running
			if (ftp_is_running()) {
				ftp_update();
			}


			if (ftpEnabled && ftp_is_running()) {
				bool isClientConnected = user_connected();

				// Only change LED pattern when connection state changes
				if (isClientConnected != wasClientConnected) {
					if (isClientConnected) {
						// Client just connected - set solid pattern
						turn_led_on(LED_SOLID);
						printf("FTP client connected - LED set to SOLID\n");
					}
					else {
						// Client just disconnected - set breathing pattern
						turn_led_on(LED_BREATHING);
						printf("FTP client disconnected - LED set to BREATHING\n");
					}
					wasClientConnected = isClientConnected;
					currentLedPatternSet = true;
				}
			}
			else if (currentLedPatternSet) {
				// FTP is not running, ensure LED is off and reset state
				if (g_led_state) {
					turn_led_off();
				}
				wasClientConnected = false;
				currentLedPatternSet = false;
			}
		}

		// B: show/hide the sinus scroller
		if ((kDown & HidNpadButton_B) && scroller_ready()) g_scrollerEnabled = !g_scrollerEnabled;

		// X: cycle the image shown on iChannel2
		if ((kDown & HidNpadButton_X) && !g_imageFiles.empty()) {
			g_imageIndex = (g_imageIndex + 1) % (int)g_imageFiles.size();
			loadImageTexture(g_imageFiles[g_imageIndex], g_img[0]);
		}
		
		// 3D model controls (right stick)
		//  - Horizontal flick: cycle models
		//  - Vertical: zoom (or free-look pitch when ZL held)
		//  - Hold ZL + right stick: free-look orbit (disables auto-rotate)
		//  - Hold ZR + horizontal flick: cycle material preset
		{
			static Uint32 lastModelSwitch = 0;
			static Uint32 lastMatSwitch = 0;
			static Uint32 lastCamTicks = SDL_GetTicks();
			const Uint32 nowCam = SDL_GetTicks();
			const float camDt = (nowCam - lastCamTicks) / 1000.0f;
			lastCamTicks = nowCam;

			HidAnalogStickState stick = padGetStickPos(&pad, 1); // right stick
			const s32 DEAD = 18000;
			const u64 kHeld = padGetButtons(&pad);
			const bool lookHeld = (kHeld & HidNpadButton_ZL) != 0;
			const bool matHeld  = (kHeld & HidNpadButton_ZR) != 0;

			float sx = 0.0f, sy = 0.0f;
			if (abs(stick.x) > DEAD) sx = stick.x / 32767.0f;
			if (abs(stick.y) > DEAD) sy = stick.y / 32767.0f;

			model3d::updateCamera(camDt, sx, sy, lookHeld);

			if (!lookHeld && !matHeld && SDL_GetTicks() - lastModelSwitch > 350) {
				if (stick.x > DEAD && model3d::count() > 0) {
					std::string name = model3d::next();
					if (!name.empty()) ui::toast("Model: " + name);
					lastModelSwitch = SDL_GetTicks();
				} else if (stick.x < -DEAD && model3d::count() > 0) {
					std::string name = model3d::prev();
					if (!name.empty()) ui::toast("Model: " + name);
					lastModelSwitch = SDL_GetTicks();
				}
			}

			if (matHeld && SDL_GetTicks() - lastMatSwitch > 350) {
				if (stick.x > DEAD) {
					ui::toast("Material: " + model3d::nextMaterial());
					lastMatSwitch = SDL_GetTicks();
				} else if (stick.x < -DEAD) {
					ui::toast("Material: " + model3d::prevMaterial());
					lastMatSwitch = SDL_GetTicks();
				}
			}
		}

		// Periodic model folder rescan (FTP uploads of new .obj files)
		{
			static Uint32 lastModelScan = 0;
			if (SDL_GetTicks() - lastModelScan > 4000) {
				lastModelScan = SDL_GetTicks();
				int prev = model3d::count();
				model3d::rescan();
				if (model3d::count() != prev) {
					ui::toast("Models: " + std::to_string(model3d::count()) + " found");
				}
			}
		}

		// Auto-reload the current shader when its file changes (e.g. after an FTP upload)
		{
			static Uint32 lastMtimeCheck = 0;
			if (!shaderFiles.empty() && SDL_GetTicks() - lastMtimeCheck > 1000) {
				lastMtimeCheck = SDL_GetTicks();
				scroller_poll_text_file("sdmc:/switch/shaderfun/scroller.txt");
				struct stat st;
				if (stat(shaderFiles[currentShader].c_str(), &st) == 0 && st.st_mtime != g_shaderMtime) {
					printf("Shader file changed, reloading\n");
					glDeleteProgram(shader.prog);
					shader = loadShaderFromFile(shaderFiles[currentShader]);
				}
			}
		}

		// Touchscreen -> iMouse (Shadertoy convention: xy = position, zw = click position,
		// zw negative when not pressed; y is measured from the bottom)
		static float mouse[4] = { 0, 0, 0, 0 };
		static bool wasTouching = false;
		{
			HidTouchScreenState touch = {};
			hidGetTouchScreenStates(&touch, 1);
			if (touch.count > 0 && !uiRes.consumedTouch) {
				float tx = (float)touch.touches[0].x;
				float ty = 720.0f - (float)touch.touches[0].y;
				if (!wasTouching) { mouse[2] = tx; mouse[3] = ty; }
				mouse[0] = tx;
				mouse[1] = ty;
				wasTouching = true;
			}
			else if (wasTouching) {
				mouse[2] = -fabsf(mouse[2]);
				mouse[3] = -fabsf(mouse[3]);
				wasTouching = false;
			}
		}

		// Music player: moves on to the next track when one ends
		player::update();

		// Show the current track in the scroller whenever the track changes
		{
			static uint32_t lastTrackChange = 0;
			if (player::trackChangeCounter() != lastTrackChange) {
				lastTrackChange = player::trackChangeCounter();
				if (scroller_ready()) scroller_set_now_playing(player::nowPlayingText());
			}
		}

		// Keep the shader list current: picks up shaders added/removed over FTP, and follows docking/undocking
		{
			static Uint32 lastShaderScan = 0;
			if (g_forceShaderScan || SDL_GetTicks() - lastShaderScan > 3000) {
				g_forceShaderScan = false;
				lastShaderScan = SDL_GetTicks();
				g_scanQuiet = true;
				std::vector<std::string> fresh = buildShaderList();
				g_scanQuiet = false;
				const bool listChanged = (fresh != shaderFiles);
				if (listChanged) {
					const std::string curPath = shaderFiles.empty() ? std::string() : shaderFiles[currentShader];
					shaderFiles = fresh;
					bool stillThere = false;
					for (size_t i = 0; i < shaderFiles.size(); i++) {
						if (shaderFiles[i] == curPath) { currentShader = (int)i; stillThere = true; break; }
					}
					if (!stillThere) { // e.g. the current shader is docked-only and we just undocked
						currentShader = 0;
						if (!shaderFiles.empty()) {
							glDeleteProgram(shader.prog);
							shader = loadShaderFromFile(shaderFiles[0]);
						}
					}
					printf("Shader list updated: %zu shaders\n", shaderFiles.size());
				}
				if (g_modeToastPending) {
					g_modeToastPending = false;
					ui::toast(std::string(g_docked ? "Docked: " : "Handheld: ") + std::to_string(shaderFiles.size()) +
						(g_docked ? " shaders (including Docked ones)" : " shaders (Docked ones hidden)"));
				}
				else if (listChanged) {
					ui::toast("Shader list updated: " + std::to_string(shaderFiles.size()) + " shaders");
				}
			}
		}
		
		// Auto-change the visualisation every N seconds (overlay chip or library menu; 0 = stay on this shader)
		{
			static int lastAutoSecs = -1;
			const int autoSecs = ui::autoCycleSeconds();
			if (autoSecs != lastAutoSecs) { // setting changed: restart the countdown
				lastAutoSecs = autoSecs;
				lastShaderChange = SDL_GetTicks();
			}
			if (autoSecs > 0 && shaderFiles.size() > 1 && SDL_GetTicks() - lastShaderChange > (Uint32)autoSecs * 1000u) {
				currentShader = (currentShader + 1) % (int)shaderFiles.size();
				glDeleteProgram(shader.prog);
				shader = loadShaderFromFile(shaderFiles[currentShader]);
				lastShaderChange = SDL_GetTicks(); // a manual L/R switch also restarts the countdown
				ui::toast("Shader: " + player::baseName(shaderFiles[currentShader]));
			}
		}

		// Switch shaders with shoulder buttons
		if (SDL_GetTicks() - lastShaderChange > 200) {
			bool changed = false;

			if (kDown & HidNpadButton_L) {  // Left shoulder button - Previous shader
				if (!shaderFiles.empty()) {
					currentShader = (currentShader - 1 + shaderFiles.size()) % shaderFiles.size();
					changed = true;
					printf("Previous shader: %s\n", shaderFiles[currentShader].c_str());
				}
			}
			if (kDown & HidNpadButton_R) {  // Right shoulder button - Next shader
				if (!shaderFiles.empty()) {
					currentShader = (currentShader + 1) % shaderFiles.size();
					changed = true;
					printf("Next shader: %s\n", shaderFiles[currentShader].c_str());
				}
			}

			if (changed) {
				if (kDown & (HidNpadButton_L | HidNpadButton_R)) {
					glDeleteProgram(shader.prog);
					shader = loadShaderFromFile(shaderFiles[currentShader]);
					ui::toast("Shader: " + player::baseName(shaderFiles[currentShader]));
				}
				lastShaderChange = SDL_GetTicks();
			}
		}

		float time = (SDL_GetTicks() - startTicks) / 1000.0f;

		// Process audio data
		computeFFT();
		uploadAudioTextures();

		// Debug output every 5 seconds
		if (frameCount % 300 == 0) {
			printf("Frame: %d, Time: %.2f, Shader: %d/%zu, Track: %d/%d %s (%.1fs)\n",
				frameCount, time, currentShader + 1, shaderFiles.size(),
				player::queueIndex() + 1, player::queueSize(),
				player::isPlaying() ? "(Playing)" : "(Stopped)", player::position());
		}

		glUseProgram(shader.prog);
		glUniform3f(shader.iResolutionLoc, 1280.0f, 720.0f, 1.0f);
		glUniform1f(shader.iTimeLoc, time);

		// Optional Shadertoy uniforms (only set if the shader declares them)
		static float prevTime = 0.0f;
		if (shader.iTimeDeltaLoc != -1) glUniform1f(shader.iTimeDeltaLoc, time - prevTime);
		prevTime = time;
		// NOTE: declare as "uniform int iFrame;" in the shader
		if (shader.iFrameLoc != -1) glUniform1i(shader.iFrameLoc, frameCount);
		if (shader.iMouseLoc != -1) glUniform4f(shader.iMouseLoc, mouse[0], mouse[1], mouse[2], mouse[3]);
		if (shader.iChannelResolutionLoc != -1) {
			float res[12] = {
				(float)FFT_SIZE, 1.0f, 1.0f,
				(float)(FFT_SIZE / 2), 1.0f, 1.0f,
				(float)g_img[0].w, (float)g_img[0].h, 1.0f,
				(float)g_img[1].w, (float)g_img[1].h, 1.0f
			};
			glUniform3fv(shader.iChannelResolutionLoc, 4, res);
		}

		// Bind audio (0,1) and image (2,3) textures to the shader channels
		const GLuint channelTex[4] = { audioTexWaveform, audioTexSpectrum, g_img[0].id, g_img[1].id };
		for (int c = 0; c < 4; c++) {
			if (shader.iChannelLoc[c] != -1) {
				glActiveTexture(GL_TEXTURE0 + c);
				glBindTexture(GL_TEXTURE_2D, channelTex[c]);
				glUniform1i(shader.iChannelLoc[c], c);
			}
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Re-bind the vertex attributes before drawing
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// 3D model layer (on top of the background shader, under the scroller)
		{
			float bass = 0.0f, mid = 0.0f, treble = 0.0f;
			const int half = FFT_SIZE / 2;
			for (int b = 1; b <= 6 && b < half; b++) bass += audioSpectrum[b];
			bass = fminf(1.0f, bass / 3.0f);
			for (int b = 8; b <= 24 && b < half; b++) mid += audioSpectrum[b];
			mid = fminf(1.0f, mid / 8.0f);
			for (int b = 28; b <= 48 && b < half; b++) treble += audioSpectrum[b];
			treble = fminf(1.0f, treble / 10.0f);
			model3d::draw(time, bass, mid, treble);
		}

		// Sinus scroller overlay
		if (g_scrollerEnabled && scroller_ready()) {
			static Uint32 lastScrollTicks = SDL_GetTicks();
			const Uint32 nowTicks = SDL_GetTicks();
			const float sdt = (nowTicks - lastScrollTicks) / 1000.0f;
			lastScrollTicks = nowTicks;
			float bass = 0.0f; // low spectrum bins -> 0..1
			for (int b = 1; b <= 4; b++) bass += audioSpectrum[b];
			bass = fminf(1.0f, bass); // (sum of 4 bins == average * 4)
			scroller_update(sdt, wasTouching, mouse[0], 720.0f - mouse[1], bass);
			scroller_draw();
		}

		// Music player overlay (drawn last, on top of the shader and the scroller)
		ui::draw();

		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			printf("OpenGL error: %d\n", error);
		}

		SDL_GL_SwapWindow(window);

		// Small delay to prevent excessive CPU usage
		SDL_Delay(16);
	}

	// Cleanup
	if (g_led_state) {
		turn_led_off();
	}
	// Reset when done
	appletSetMediaPlaybackState(false); //allow switch to go back to sleep
	ftp_cleanup(&pad);  // Pass the pad parameter
	ui::shutdown();
	if (plReady) plExit();
	library::shutdown();
	player::shutdown();
	cleanupAudio();
	glDeleteTextures(1, &audioTexWaveform);
	glDeleteTextures(1, &audioTexSpectrum);
	scroller_shutdown();
	model3d::shutdown();
	cleanupImageTextures();
	TTF_Quit();
	IMG_Quit();
	glDeleteProgram(shader.prog);
	glDeleteBuffers(1, &vbo);
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();
	// Cleanup ROMFS
	romfsExit();
	socketExit();
	return 0;
}