#pragma once
// Rainbow sinus scroller overlay for ShaderFun (OpenGL ES 2.0, drawn on top of the current shader).
// Re-implementation of the original SDL_Renderer scroller using a glyph atlas, so it can share
// the GL context with the shaders. Requires TTF_Init() to have been called, and a current GL context.

#include <string>

// Builds the glyph atlas from a TTF font. Returns false if the font/GL setup failed.
bool scroller_init(const char* fontPath, int screenW, int screenH);
bool scroller_ready();
void scroller_shutdown();

// Replace the scrolling message. '#' pauses the text for 2 seconds, '$' toggles double speed.
void scroller_set_text(const std::string& text);

// Shows "Now playing: <text>" in front of the message and restarts the scroll (empty string clears it).
// UTF-8 is accepted; characters outside Latin-1 are shown as '?'.
void scroller_set_now_playing(const std::string& nowPlaying);

// Loads the message from a text file when the file's modification time changes
// (so a message uploaded over FTP shows up by itself). Returns true if the text was (re)loaded.
bool scroller_poll_text_file(const char* path);

// dt = seconds since the last call, touching/touchX/touchY = touchscreen state in screen pixels
// (origin top-left), audioLevel = 0..1 bass level that makes the wave bounce a little harder.
void scroller_update(float dt, bool touching, float touchX, float touchY, float audioLevel);
void scroller_draw();
