/*
Created By MrDude
=================
Controls:
D-pad Up/Down: Volume control
D-pad Left/Right: Seek backward/forward 10 seconds
ZL/ZR: Previous/Next song
L/R: Switch shaders
Plus: Exit
Minus: Start/Stop FTP Sever
A: Play/Pause
X: Toggle network status LED - disabled, just use for testing...
Y: Rescan music and shader folders.
Left Stick Down: Rescan only shader files.
Right Stick Down: Rescane only music files.
*/

#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
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
#include "ftp.h"

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

static GLuint compileShader(GLenum type, const char* src) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char buffer[1024];
		glGetShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
		printf("Shader compile error: %s\n", buffer);
	}
	return shader;
}

// === Shader state ===
struct ShaderProgram {
	GLuint prog;
	GLint iResolutionLoc;
	GLint iTimeLoc;
};

ShaderProgram loadShaderProgram(const char* fragSrc) {
	GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
	GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);

	// Check if shaders compiled successfully
	GLint vsStatus, fsStatus;
	glGetShaderiv(vs, GL_COMPILE_STATUS, &vsStatus);
	glGetShaderiv(fs, GL_COMPILE_STATUS, &fsStatus);

	if (!vsStatus || !fsStatus) {
		printf("Shader compilation failed! Using fallback.\n");
		// Clean up and use fallback
		if (vs) glDeleteShader(vs);
		if (fs) glDeleteShader(fs);
		return loadShaderProgram(fallbackFragmentShader);
	}

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "aPos");
	glLinkProgram(prog);

	// Check link status
	GLint linkStatus;
	glGetProgramiv(prog, GL_LINK_STATUS, &linkStatus);
	if (!linkStatus) {
		char buffer[1024];
		glGetProgramInfoLog(prog, sizeof(buffer), NULL, buffer);
		printf("Program link error: %s\n", buffer);
		glDeleteProgram(prog);
		return loadShaderProgram(fallbackFragmentShader);
	}

	// Clean up shaders after linking
	glDeleteShader(vs);
	glDeleteShader(fs);

	ShaderProgram sp;
	sp.prog = prog;
	sp.iResolutionLoc = glGetUniformLocation(prog, "iResolution");
	sp.iTimeLoc = glGetUniformLocation(prog, "iTime");

	printf("Shader loaded successfully. iResolution loc: %d, iTime loc: %d\n",
		sp.iResolutionLoc, sp.iTimeLoc);

	return sp;
}

// === Load fragment shader file (with fallback) ===
ShaderProgram loadShaderFromFile(const std::string& path) {
	printf("Loading shader from: %s\n", path.c_str());
	std::string fragSource = loadFile(path.c_str());
	if (fragSource.empty()) {
		printf("Shader file is empty, using fallback\n");
	}
	const char* fragSrcCStr = fragSource.empty() ? fallbackFragmentShader : fragSource.c_str();
	return loadShaderProgram(fragSrcCStr);
}

// === Recursively scan shader folder and subfolders ===
void scanShaderFolderRecursive(const std::string& dirPath, std::vector<std::string>& files) {
	DIR* dir = opendir(dirPath.c_str());
	if (!dir) {
		printf("Could not open directory: %s\n", dirPath.c_str());
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
			printf("Found shader: %s\n", fullPath.c_str());
		}
	}
	closedir(dir);
}

// === Scan shader folders (wrapper function) ===
std::vector<std::string> scanShaderFolders(const char* dirPath) {
	std::vector<std::string> files;
	scanShaderFolderRecursive(dirPath, files);
	printf("Found %zu shader files in %s\n", files.size(), dirPath);
	return files;
}

// === Recursively scan music folder and subfolders ===
void scanMusicFolderRecursive(const std::string& dirPath, std::vector<std::string>& files) {
	DIR* dir = opendir(dirPath.c_str());
	if (!dir) {
		printf("Could not open directory: %s\n", dirPath.c_str());
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
			scanMusicFolderRecursive(fullPath, files);
		}
		// Check if it's a music file (expanded list)
		else {
			std::string ext = "";
			if (name.size() > 4) ext = name.substr(name.size() - 4);
			std::string ext5 = "";
			if (name.size() > 5) ext5 = name.substr(name.size() - 5);
			std::string ext3 = "";
			if (name.size() > 3) ext3 = name.substr(name.size() - 3);

			if (ext == ".mp3" || ext == ".wav" || ext == ".ogg" || ext == ".mod" ||
				ext == ".xm" || ext == ".s3m" || ext5 == ".flac" || ext3 == ".it" ||
				ext == ".aif" || ext == ".mid") {
				files.push_back(fullPath);
				printf("Found music: %s\n", fullPath.c_str());
			}
		}
	}
	closedir(dir);
}

// === Scan music folders (wrapper function) ===
std::vector<std::string> scanMusicFolders(const char* dirPath) {
	std::vector<std::string> files;
	scanMusicFolderRecursive(dirPath, files);
	printf("Found %zu music files in %s\n", files.size(), dirPath);
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

// Music object
Mix_Music* music = nullptr;

// Effect callback to capture PCM
void audioEffectCallback(int chan, void* stream, int len, void* udata) {
	int16_t* samples = (int16_t*)stream;
	int count = len / 2; // 16-bit samples
	static int idx = 0;
	for (int i = 0; i < count; i += 2) { // left channel
		float sample = samples[i] / 32768.0f;
		audioWaveform[idx % FFT_SIZE] = sample;
		idx++;
	}
}

// Compute FFT from waveform
void computeFFT() {
	for (int i = 0; i < FFT_SIZE; i++) {
		fftIn[i].r = audioWaveform[i];
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
	// Waveform -> iChannel0
	glBindTexture(GL_TEXTURE_2D, audioTexWaveform);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, FFT_SIZE, 1, 0, GL_LUMINANCE, GL_FLOAT, audioWaveform);

	// Spectrum -> iChannel1
	glBindTexture(GL_TEXTURE_2D, audioTexSpectrum);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, FFT_SIZE / 2, 1, 0, GL_LUMINANCE, GL_FLOAT, audioSpectrum);
}

// Load and play a specific music file
bool loadAndPlayMusic(const std::string& musicPath) {
	if (music) {
		Mix_FreeMusic(music);
		music = nullptr;
	}

	music = Mix_LoadMUS(musicPath.c_str());
	if (!music) {
		printf("Failed to load music: %s\n", musicPath.c_str());
		return false;
	}

	if (Mix_PlayMusic(music, 0) == -1) { // Play once (no loop)
		printf("Failed to play music: %s\n", Mix_GetError());
		return false;
	}

	printf("Now playing: %s\n", musicPath.c_str());
	return true;
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
	if (music) {
		Mix_HaltMusic(); // Stop music playback
		Mix_FreeMusic(music);
		music = nullptr;
	}
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
void rescanShaders(std::vector<std::string>& shaderFiles, int& currentShader) {
	printf("Rescanning shaders...\n");

	// Clear current list
	shaderFiles.clear();

	// Rescan using your existing logic
	shaderFiles = scanShaderFolders("sdmc:/switch/shaderfun/test");
	if (shaderFiles.empty()) {
		printf("No shaders found in test folder, trying shaders folder...\n");
		shaderFiles = scanShaderFolders("sdmc:/switch/shaderfun/shaders");
	}
	if (shaderFiles.empty()) {
		printf("No shaders found in SDMC, trying romfs...\n");
		shaderFiles = scanShaderFolders("romfs:/shaders");
	}

	// Reset to first shader if current is out of bounds
	if (currentShader >= (int)shaderFiles.size()) {  // Cast to int for comparison
		currentShader = 0;
	}

	printf("Rescan complete: Found %zu shaders\n", shaderFiles.size());
}

void rescanMusic(std::vector<std::string>& musicFiles, int& currentMusic, bool& musicPlaying) {
	printf("Rescanning music...\n");

	// Store current music state
	bool wasPlaying = musicPlaying;
	std::string currentFile = "";
	if (currentMusic < (int)musicFiles.size()) {  // Cast to int for comparison
		currentFile = musicFiles[currentMusic];
	}

	// Stop music if playing
	if (musicPlaying) {
		Mix_HaltMusic();
		musicPlaying = false;
	}

	// Clear current list
	musicFiles.clear();

	// Rescan using your existing logic
	musicFiles = scanMusicFolders("sdmc:/switch/shaderfun/music");
	if (musicFiles.empty()) {
		printf("No music found in SDMC, trying romfs...\n");
		musicFiles = scanMusicFolders("romfs:/music");
	}

	// Try to find and resume the previously playing file
	if (!currentFile.empty()) {
		for (size_t i = 0; i < musicFiles.size(); i++) {
			if (musicFiles[i] == currentFile) {
				currentMusic = (int)i;  // Cast to int
				printf("Resumed previous music file: %s\n", currentFile.c_str());
				break;
			}
		}
	}

	// Reset to first music if current is out of bounds
	if (currentMusic >= (int)musicFiles.size()) {  // Cast to int for comparison
		currentMusic = 0;
	}

	// Restart music if it was playing
	if (wasPlaying && !musicFiles.empty()) {
		if (loadAndPlayMusic(musicFiles[currentMusic])) {
			musicPlaying = true;
			printf("Music resumed after rescan\n");
		}
	}

	printf("Rescan complete: Found %zu music files\n", musicFiles.size());
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

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

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

	// Scan shader directories with fallback logic
	std::vector<std::string> shaderFiles;

	// First try SDMC directory
	shaderFiles = scanShaderFolders("sdmc:/switch/shaderfun/test");

	if (shaderFiles.empty()) {
		printf("No shaders found in sdmc:/switch/shaderfun/test, trying shaders folder...\n");
		shaderFiles = scanShaderFolders("sdmc:/switch/shaderfun/shaders");
	}

	// If SDMC directory doesn't exist or is empty, try romfs
	if (shaderFiles.empty()) {
		printf("No shaders found in sdmc:/switch/shaderfun/shaders or sdmc:/switch/shaderfun/test, try romfs...\n");
		shaderFiles = scanShaderFolders("romfs:/shaders");
	}

	// If both directories are empty or don't exist, use fallback
	if (shaderFiles.empty()) {
		printf("No .frag files found in any directory. Using fallback shader.\n");
	}

	// Scan music directories
	std::vector<std::string> musicFiles;

	// First try SDMC directory
	musicFiles = scanMusicFolders("sdmc:/switch/shaderfun/music");

	if (musicFiles.empty()) {
		printf("No music found in sdmc:/switch/shaderfun/music, trying romfs...\n");
		musicFiles = scanMusicFolders("romfs:/music");
	}

	if (musicFiles.empty()) {
		printf("No music files found in any directory. Audio will be silent.\n");
	}

	int currentShader = 0;
	int currentMusic = 0;
	bool musicPlaying = false;

	// Music control variables
	int volume = MIX_MAX_VOLUME / 2; // Start at 50% volume
	double musicPosition = 0.0;
	Uint32 lastMusicUpdate = 0;
	bool musicSeeking = false;

	Mix_VolumeMusic(volume); // Set initial volume

	ShaderProgram shader = shaderFiles.empty() ?
		loadShaderProgram(fallbackFragmentShader) :
		loadShaderFromFile(shaderFiles[currentShader]);

	// Add FTP state variable
	bool ftpEnabled = false;
	Uint32 lastFtpToggle = 0;

	while (running) {
		frameCount++;

		padUpdate(&pad);
		u64 kDown = padGetButtonsDown(&pad);

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

		// Press Y to rescan music and shader folders.
		if (kDown & HidNpadButton_Y) {
			bool rescanned = false;

			// Rescan shaders
			rescanShaders(shaderFiles, currentShader);
			rescanned = true;

			// Rescan music  
			rescanMusic(musicFiles, currentMusic, musicPlaying);
			rescanned = true;

			if (rescanned) {
				printf("Rescanned folders - Shaders: %zu, Music: %zu\n",
					shaderFiles.size(), musicFiles.size());

				// Reload current shader if we have shaders
				if (!shaderFiles.empty()) {
					glDeleteProgram(shader.prog);
					shader = loadShaderFromFile(shaderFiles[currentShader]);
					printf("Reloaded current shader: %s\n", shaderFiles[currentShader].c_str());
				}

				lastShaderChange = SDL_GetTicks();
			}
		}

		// For shaders only (maybe L3 button?)
		if (kDown & HidNpadButton_StickL) {
			rescanShaders(shaderFiles, currentShader);
			if (!shaderFiles.empty()) {
				glDeleteProgram(shader.prog);
				shader = loadShaderFromFile(shaderFiles[currentShader]);
				printf("Reloaded shaders: %zu found\n", shaderFiles.size());
			}
		}

		// For music only (maybe R3 button?)  
		if (kDown & HidNpadButton_StickR) {
			rescanMusic(musicFiles, currentMusic, musicPlaying);
			printf("Reloaded music: %zu found\n", musicFiles.size());
		}

		// Update music position tracking
		Uint32 currentTime = SDL_GetTicks();
		if (musicPlaying && !musicSeeking && currentTime - lastMusicUpdate > 100) {
			// Update position every 100ms when music is playing normally
			musicPosition += 0.1; // Add 0.1 seconds
			lastMusicUpdate = currentTime;
		}

		// Check if music ended naturally
		if (musicPlaying && !Mix_PlayingMusic()) {
			printf("Song ended, moving to next\n");
			currentMusic = (currentMusic + 1) % musicFiles.size();
			musicPosition = 0.0;
			if (loadAndPlayMusic(musicFiles[currentMusic])) {
				printf("Now playing: %s\n", musicFiles[currentMusic].c_str());
			}
			else {
				musicPlaying = false;
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

			// Music controls
			if (kDown & HidNpadButton_ZL) {  // Left Trigger - Previous song
				if (!musicFiles.empty()) {
					currentMusic = (currentMusic - 1 + musicFiles.size()) % musicFiles.size();
					musicPosition = 0.0; // Reset position for new song
					if (musicPlaying) {
						loadAndPlayMusic(musicFiles[currentMusic]);
					}
					else {
						printf("Selected previous song: %s\n", musicFiles[currentMusic].c_str());
					}
					changed = true;
				}
			}
			if (kDown & HidNpadButton_ZR) {  // Right Trigger - Next song
				if (!musicFiles.empty()) {
					currentMusic = (currentMusic + 1) % musicFiles.size();
					musicPosition = 0.0; // Reset position for new song
					if (musicPlaying) {
						loadAndPlayMusic(musicFiles[currentMusic]);
					}
					else {
						printf("Selected next song: %s\n", musicFiles[currentMusic].c_str());
					}
					changed = true;
				}
			}
			if (kDown & HidNpadButton_A) {  // A button - Play/Pause
				if (musicPlaying) {
					Mix_PauseMusic();
					musicPlaying = false;
					printf("Music paused at %.1f seconds\n", musicPosition);
				}
				else if (!musicFiles.empty()) {
					if (!Mix_PlayingMusic()) {
						// If starting a new playback, ensure position is reset if needed
						if (musicPosition > 0) {
							musicSeeking = true;
							loadAndPlayMusic(musicFiles[currentMusic]);
							if (Mix_SetMusicPosition(musicPosition) != 0) {
								printf("Note: Resume position not supported for this format\n");
							}
							musicSeeking = false;
						}
						else {
							loadAndPlayMusic(musicFiles[currentMusic]);
						}
					}
					else {
						Mix_ResumeMusic();
					}
					musicPlaying = true;
					printf("Music playing: %s at %.1f seconds\n", musicFiles[currentMusic].c_str(), musicPosition);
				}
				changed = true;
			}

			// Volume and Seek controls
			if (kDown & HidNpadButton_Up) {    // Volume Up
				volume = (volume + 10 > MIX_MAX_VOLUME) ? MIX_MAX_VOLUME : volume + 10;
				Mix_VolumeMusic(volume);
				printf("Volume: %d/%d\n", volume, MIX_MAX_VOLUME);
				changed = true;
			}
			if (kDown & HidNpadButton_Down) {  // Volume Down
				volume = (volume - 10 < 0) ? 0 : volume - 10;
				Mix_VolumeMusic(volume);
				printf("Volume: %d/%d\n", volume, MIX_MAX_VOLUME);
				changed = true;
			}
			if (kDown & HidNpadButton_Left) {  // Seek backward 10 seconds
				if (musicPlaying && music) {
					musicPosition = (musicPosition - 10.0 < 0) ? 0 : musicPosition - 10.0;
					musicSeeking = true;

					// Stop current music and restart from new position
					Mix_HaltMusic();
					if (loadAndPlayMusic(musicFiles[currentMusic])) {
						// Set the position by using Mix_SetMusicPosition if supported
						if (Mix_SetMusicPosition(musicPosition) != 0) {
							printf("Note: Precise seeking not supported for this format\n");
						}
						printf("Seeked to: %.1f seconds\n", musicPosition);
					}
					musicSeeking = false;
					changed = true;
				}
			}
			if (kDown & HidNpadButton_Right) { // Seek forward 10 seconds
				if (musicPlaying && music) {
					musicPosition += 10.0;
					musicSeeking = true;

					// Stop current music and restart from new position
					Mix_HaltMusic();
					if (loadAndPlayMusic(musicFiles[currentMusic])) {
						// Set the position by using Mix_SetMusicPosition if supported
						if (Mix_SetMusicPosition(musicPosition) != 0) {
							printf("Note: Precise seeking not supported for this format\n");
						}
						printf("Seeked to: %.1f seconds\n", musicPosition);
					}
					musicSeeking = false;
					changed = true;
				}
			}

			if (changed) {
				if (kDown & (HidNpadButton_L | HidNpadButton_R)) {
					glDeleteProgram(shader.prog);
					shader = loadShaderFromFile(shaderFiles[currentShader]);
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
			printf("Frame: %d, Time: %.2f, Shader: %d/%zu, Music: %d/%zu %s (Pos: %.1fs)\n",
				frameCount, time, currentShader + 1, shaderFiles.size(),
				currentMusic + 1, musicFiles.size(),
				musicPlaying ? "(Playing)" : "(Stopped)", musicPosition);
		}

		glUseProgram(shader.prog);
		glUniform3f(shader.iResolutionLoc, 1280.0f, 720.0f, 1.0f);
		glUniform1f(shader.iTimeLoc, time);

		// Bind audio textures to shader channels
		GLint loc0 = glGetUniformLocation(shader.prog, "iChannel0");
		if (loc0 != -1) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, audioTexWaveform);
			glUniform1i(loc0, 0);
		}

		GLint loc1 = glGetUniformLocation(shader.prog, "iChannel1");
		if (loc1 != -1) {
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, audioTexSpectrum);
			glUniform1i(loc1, 1);
		}

		glClear(GL_COLOR_BUFFER_BIT);

		// Re-bind the vertex attributes before drawing
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

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
	cleanupAudio();
	glDeleteTextures(1, &audioTexWaveform);
	glDeleteTextures(1, &audioTexSpectrum);
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