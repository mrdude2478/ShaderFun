#pragma once
// Music player overlay for ShaderFun: an auto-hiding "now playing" bar (transport buttons, progress,
// volume, play mode) plus a slide-in library browser. Drawn with OpenGL ES 2.0 on top of the visualiser.
// Gamepad and touch controlled. Independent of libnx: main.cpp maps the pad to the BTN_* bits below.

#include <stdint.h>
#include <stddef.h>
#include <string>

namespace ui {

enum : uint32_t {
	BTN_UP = 1u << 0, BTN_DOWN = 1u << 1, BTN_LEFT = 1u << 2, BTN_RIGHT = 1u << 3,
	BTN_A = 1u << 4, BTN_B = 1u << 5, BTN_X = 1u << 6, BTN_Y = 1u << 7,
	BTN_L = 1u << 8, BTN_R = 1u << 9, BTN_ZL = 1u << 10, BTN_ZR = 1u << 11,
	BTN_R3 = 1u << 12, BTN_L3 = 1u << 13,
	BTN_LS_UP = 1u << 14, BTN_LS_DOWN = 1u << 15, BTN_LS_LEFT = 1u << 16, BTN_LS_RIGHT = 1u << 17, // left stick as digital directions
	BTN_RS_LEFT = 1u << 18, BTN_RS_RIGHT = 1u << 19  // right stick left/right
};

struct Input {
	uint32_t down = 0;      // buttons pressed this frame
	uint32_t held = 0;      // buttons currently held
	bool touching = false;  // touchscreen state (screen pixels, origin top-left)
	float touchX = 0, touchY = 0;
	float dt = 0.016f;      // seconds since the last update
};

struct Result {
	uint32_t consumedButtons = 0; // buttons the UI used this frame - main should ignore them
	bool consumedTouch = false;   // this touch belongs to the UI - don't forward it to shaders/scroller
	bool requestQuit = false;     // the sleep timer wants the app to close - main should leave its loop
};

// Fonts are tried in the order they were added, per character (so a later font can supply CJK etc.).
// Call these before init().
void addFontFile(const char* path);
void addFontMemory(const void* data, size_t size); // data must stay valid until shutdown()

bool init(int screenW, int screenH); // needs a current GL context and TTF_Init()
void shutdown();
bool ready();

Result update(const Input& in);
void draw();

void toast(const std::string& text); // small message in the top-left corner
bool libraryOpen();
bool lyricsAvailable();        // the current song has lyrics (a .lrc file or embedded in its tags)
void lyricsShow(bool show);    // show / hide them (main.cpp: right stick right / left)
int autoCycleSeconds();   // auto-change visualisation interval in seconds, 0 = off
bool infoOpen();   // the track info panel (L3, or tap the title) is showing

} // namespace ui
