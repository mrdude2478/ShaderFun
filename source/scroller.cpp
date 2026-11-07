/*
Rainbow sinus scroller overlay.
Based on MrDude's original "Rainbow Sinus Scroller With Speed Controls", ported from SDL_Renderer to
OpenGL ES 2.0 so it can be drawn on top of the shaders.

Special characters in the message (same as the original):
  #  pauses the scroller for 2 seconds when it enters the screen
  $  toggles double speed (resets when the text loops)

Touchscreen:
  - letters near your finger swell up and are pulled towards it
  - the text stops auto-scrolling while you hold the screen, and drags along with your finger
*/

#include "scroller.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <GLES2/gl2.h>
#include <sys/stat.h>
#include <math.h>
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace {

// ---------------------------------------------------------------- tunables
const int   FONT_SIZE = 100;
const float WAVE_AMPLITUDE = 50.0f;        // pixels
const float WAVE_FREQUENCY = 0.20f;        // radians per character
const float PHASE_SPEED = 0.07f * 60.0f;   // radians per second (wave animation)
const float SCROLL_SPEED = 2.5f * 60.0f;   // pixels per second
const float COLOR_SPEED = 0.5f * 60.0f;    // degrees per second (rainbow)
const float HUE_STEP = 5.0f;               // degrees per character
const float GRADIENT_SPEED = 2.0f * 60.0f; // degrees per second (glow colour)
const float GRADIENT_SCALE = 0.8f;         // glow radius relative to half the glyph width
const float GLOW_ALPHA = 190.0f / 255.0f;
const float PAUSE_SECONDS = 2.0f;

const float AUDIO_BOUNCE = 20.0f;          // extra wave amplitude at full bass level (pixels)
const float TOUCH_RADIUS = 160.0f;         // how far the finger's influence reaches (pixels)
const float TOUCH_SWELL = 1.2f;            // extra amplitude next to the finger (1.0 = double)
const float TOUCH_MAGNET = 0.35f;          // 0..1, how strongly letters are pulled towards the finger

const char* DEFAULT_TEXT =
	"OldSchool rainbow scrolling effect for the nintendo switch created by"
	"$               *** MrDude ***       #"
	"   Touch the screen to play with the letters - drag to move the text        #"
	"   Press B to hide or show this scroller, and put your own message in"
	" sdmc:/switch/shaderfun/scroller.txt   "
	"                      $"
	"Normal speed has resumed now so carry on with the messages";

// ---------------------------------------------------------------- state
struct Glyph {
	bool valid = false;  // has pixels in the atlas
	int advance = 0;     // horizontal advance (what the original used as the character width)
	int w = 0, h = 0;    // size of the rendered glyph in the atlas
	float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
};

Glyph g_glyph[256];
GLuint g_atlas = 0;
GLuint g_prog = 0;
GLuint g_vbo = 0;
GLint g_uAtlas = -1;
int g_screenW = 1280, g_screenH = 720;
bool g_ready = false;

std::string g_text;
std::string g_base;    // the message (default text or scroller.txt), as Latin-1
std::string g_prefix;  // "Now playing: ..." shown in front of it
std::vector<float> g_offsets;    // x offset of every character from the start of the text
std::vector<char> g_processed;   // '#'/'$' already handled this loop
float g_totalLength = 0.0f;

float g_xPos = 0.0f;
float g_phase = 1.5f;
float g_colorHue = 0.0f;
float g_gradientHue = 0.0f;
float g_scrollMul = 1.0f;        // 1 or 2 ('$')
float g_pauseTimer = 0.0f;
bool g_speedIncreased = false;

float g_touchAmt = 0.0f;         // 0..1, eased in/out
float g_touchX = 0.0f, g_touchY = 0.0f;
float g_prevTouchX = 0.0f;
bool g_wasTouching = false;
float g_audio = 0.0f;

std::vector<float> g_verts;

// ---------------------------------------------------------------- helpers
void hsvToRgb(float h, float s, float v, float out[3]) {
	h = fmodf(h, 360.0f);
	if (h < 0) h += 360.0f;
	if (s == 0) { out[0] = out[1] = out[2] = v; return; }
	h /= 60.0f;
	int i = (int)h;
	float f = h - i;
	float p = v * (1.0f - s);
	float q = v * (1.0f - s * f);
	float t = v * (1.0f - s * (1.0f - f));
	switch (i) {
	case 0: out[0] = v; out[1] = t; out[2] = p; break;
	case 1: out[0] = q; out[1] = v; out[2] = p; break;
	case 2: out[0] = p; out[1] = v; out[2] = t; break;
	case 3: out[0] = p; out[1] = q; out[2] = v; break;
	case 4: out[0] = t; out[1] = p; out[2] = v; break;
	default: out[0] = v; out[1] = p; out[2] = q; break;
	}
}

bool isControl(char c) { return c == '#' || c == '$' || c == ' '; }

GLuint compile(GLenum type, const char* src) {
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024];
		glGetShaderInfoLog(s, sizeof(log), NULL, log);
		printf("Scroller shader compile error: %s\n", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

const char* VS = R"(
attribute vec2 aPos;
attribute vec2 aUV;
attribute vec3 aLocal;
attribute vec4 aCol;
varying vec2 vUV;
varying vec3 vLocal;
varying vec4 vCol;
void main() {
    vUV = aUV;
    vLocal = aLocal;
    vCol = aCol;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

// aCol.w = 1 -> radial glow behind a letter (normal alpha blend),
// aCol.w = 0 -> the letter itself (additive). Both use premultiplied blending (ONE, ONE_MINUS_SRC_ALPHA).
const char* FS = R"(
precision mediump float;
uniform sampler2D uAtlas;
varying vec2 vUV;
varying vec3 vLocal;
varying vec4 vCol;
void main() {
    if (vCol.w > 0.5) {
        float inten = 1.0 - length(vLocal.xy) / vLocal.z;
        if (inten <= 0.0) discard;
        float a = inten * 0.745;
        gl_FragColor = vec4(vCol.rgb * a, a);
    } else {
        float a = texture2D(uAtlas, vUV).a;
        gl_FragColor = vec4(vCol.rgb * a, 0.0);
    }
}
)";

void pushVertex(float px, float py, float u, float v, float lx, float ly, float rad,
	const float rgb[3], float kind) {
	g_verts.push_back(px / g_screenW * 2.0f - 1.0f);
	g_verts.push_back(1.0f - py / g_screenH * 2.0f);
	g_verts.push_back(u); g_verts.push_back(v);
	g_verts.push_back(lx); g_verts.push_back(ly); g_verts.push_back(rad);
	g_verts.push_back(rgb[0]); g_verts.push_back(rgb[1]); g_verts.push_back(rgb[2]);
	g_verts.push_back(kind);
}

void pushQuad(float x, float y, const Glyph& g, const float rgb[3], float kind) {
	const float w = (float)g.w, h = (float)g.h;
	const float cx = floorf(w / 2.0f), cy = floorf(h / 2.0f);
	const float rad = (kind > 0.5f) ? fmaxf(1.0f, cx * GRADIENT_SCALE) : 1.0f;
	// corners: TL, TR, BL, BR
	struct C { float dx, dy, u, v; };
	const C c[4] = {
		{ 0, 0, g.u0, g.v0 }, { w, 0, g.u1, g.v0 },
		{ 0, h, g.u0, g.v1 }, { w, h, g.u1, g.v1 }
	};
	const int order[6] = { 0, 1, 2, 1, 3, 2 };
	for (int k = 0; k < 6; k++) {
		const C& p = c[order[k]];
		pushVertex(x + p.dx, y + p.dy, p.u, p.v, p.dx - cx, p.dy - cy, rad, rgb, kind);
	}
}

// UTF-8 -> Latin-1 (what the glyph atlas covers); anything else becomes '?'
std::string toLatin1(const std::string& in) {
	std::string out;
	size_t i = 0;
	while (i < in.size()) {
		unsigned char c = (unsigned char)in[i];
		unsigned cp = c;
		size_t len = 1;
		if (c >= 0xC0 && c < 0xE0 && i + 1 < in.size() && ((unsigned char)in[i + 1] & 0xC0) == 0x80) {
			cp = ((c & 0x1Fu) << 6) | ((unsigned char)in[i + 1] & 0x3Fu);
			len = 2;
		}
		else if (c >= 0xE0) {            // 3/4-byte sequences are never Latin-1
			len = (c >= 0xF0) ? 4 : 3;
			cp = '?';
		}
		else if (c >= 0x80 && c < 0xC0) { // stray byte: assume it already is Latin-1
			cp = c;
		}
		if (cp == '\n' || cp == '\r' || cp == '\t') cp = ' ';
		if (cp > 255 || (cp >= 127 && cp < 160)) cp = '?';
		out.push_back((char)cp);
		i += len;
	}
	return out;
}

void layoutText() {
	const size_t n = g_text.size();
	g_offsets.assign(n, 0.0f);
	g_processed.assign(n, 0);
	float x = 0.0f;
	for (size_t i = 0; i < n; i++) {
		g_offsets[i] = x;
		x += (float)g_glyph[(unsigned char)g_text[i]].advance;
	}
	g_totalLength = x;
}

} // namespace

// ---------------------------------------------------------------- public API
bool scroller_ready() { return g_ready; }

bool scroller_init(const char* fontPath, int screenW, int screenH) {
	if (g_ready) return true;
	g_screenW = screenW;
	g_screenH = screenH;

	TTF_Font* font = TTF_OpenFont(fontPath, FONT_SIZE);
	if (!font) {
		printf("Scroller: could not open font %s: %s\n", fontPath, TTF_GetError());
		return false;
	}

	// --- measure
	const int FIRST = 32, LAST = 255, COUNT = LAST - FIRST + 1, COLS = 16; // ASCII + Latin-1
	const int ROWS = (COUNT + COLS - 1) / COLS;
	int maxW = 1;
	for (int c = FIRST; c <= LAST; c++) {
		if (c >= 127 && c < 160) continue; // control characters
		char s[2] = { (char)c, 0 };
		int w = 0, h = 0;
		TTF_SizeText(font, s, &w, &h);
		g_glyph[c].advance = w;
		if (w > maxW) maxW = w;
	}
	const int cellW = maxW + 8;                 // padding for glyph overhang + filtering
	const int cellH = TTF_FontHeight(font) + 4;
	const int atlasW = COLS * cellW, atlasH = ROWS * cellH;

	GLint maxTex = 2048;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
	if (atlasW > maxTex || atlasH > maxTex) {
		printf("Scroller: font atlas %dx%d too large (max %d)\n", atlasW, atlasH, (int)maxTex);
		TTF_CloseFont(font);
		return false;
	}

	// --- render glyphs into one surface
	SDL_Surface* atlas = SDL_CreateRGBSurfaceWithFormat(0, atlasW, atlasH, 32, SDL_PIXELFORMAT_ABGR8888);
	if (!atlas) {
		TTF_CloseFont(font);
		return false;
	}
	SDL_FillRect(atlas, NULL, 0);
	const SDL_Color white = { 255, 255, 255, 255 };
	for (int c = FIRST; c <= LAST; c++) {
		if (c >= 127 && c < 160) continue;
		char s[2] = { (char)c, 0 };
		SDL_Surface* raw = TTF_RenderText_Blended(font, s, white);
		if (!raw) continue; // e.g. nothing to draw for a blank glyph; advance is still known
		SDL_Surface* conv = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ABGR8888, 0);
		SDL_FreeSurface(raw);
		if (!conv) continue;
		SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_NONE); // copy RGBA as-is

		const int idx = c - FIRST;
		const int col = idx % COLS, row = idx / COLS;
		const int gw = conv->w < cellW - 4 ? conv->w : cellW - 4;
		const int gh = conv->h < cellH - 4 ? conv->h : cellH - 4;
		SDL_Rect src = { 0, 0, gw, gh };
		SDL_Rect dst = { col * cellW + 2, row * cellH + 2, gw, gh };
		SDL_BlitSurface(conv, &src, atlas, &dst);
		SDL_FreeSurface(conv);

		Glyph& g = g_glyph[c];
		g.valid = true;
		g.w = gw;
		g.h = gh;
		g.u0 = (float)dst.x / atlasW;
		g.v0 = (float)dst.y / atlasH;
		g.u1 = (float)(dst.x + gw) / atlasW;
		g.v1 = (float)(dst.y + gh) / atlasH;
	}
	TTF_CloseFont(font); // everything we need is in the atlas now

	// --- upload
	glGenTextures(1, &g_atlas);
	glBindTexture(GL_TEXTURE_2D, g_atlas);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	{
		std::vector<unsigned char> alpha((size_t)atlasW * atlasH);
		for (int y = 0; y < atlasH; y++) {
			const Uint32* row = (const Uint32*)((const unsigned char*)atlas->pixels + (size_t)y * atlas->pitch);
			for (int x = 0; x < atlasW; x++) alpha[(size_t)y * atlasW + x] = (unsigned char)(row[x] >> 24); // ABGR8888: alpha is the top byte
		}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlasW, atlasH, 0, GL_ALPHA, GL_UNSIGNED_BYTE, alpha.data());
	}
	SDL_FreeSurface(atlas);

	// --- program
	GLuint vs = compile(GL_VERTEX_SHADER, VS);
	GLuint fs = vs ? compile(GL_FRAGMENT_SHADER, FS) : 0;
	if (!vs || !fs) {
		if (vs) glDeleteShader(vs);
		if (fs) glDeleteShader(fs);
		glDeleteTextures(1, &g_atlas);
		g_atlas = 0;
		return false;
	}
	g_prog = glCreateProgram();
	glAttachShader(g_prog, vs);
	glAttachShader(g_prog, fs);
	glBindAttribLocation(g_prog, 0, "aPos");
	glBindAttribLocation(g_prog, 1, "aUV");
	glBindAttribLocation(g_prog, 2, "aLocal");
	glBindAttribLocation(g_prog, 3, "aCol");
	glLinkProgram(g_prog);
	glDeleteShader(vs);
	glDeleteShader(fs);
	GLint linked = 0;
	glGetProgramiv(g_prog, GL_LINK_STATUS, &linked);
	if (!linked) {
		printf("Scroller: program link failed\n");
		glDeleteProgram(g_prog);
		g_prog = 0;
		glDeleteTextures(1, &g_atlas);
		g_atlas = 0;
		return false;
	}
	g_uAtlas = glGetUniformLocation(g_prog, "uAtlas");
	glGenBuffers(1, &g_vbo);

	scroller_set_text(DEFAULT_TEXT);
	g_ready = true;
	printf("Scroller ready (atlas %dx%d)\n", atlasW, atlasH);
	return true;
}

void scroller_shutdown() {
	if (g_vbo) glDeleteBuffers(1, &g_vbo);
	if (g_prog) glDeleteProgram(g_prog);
	if (g_atlas) glDeleteTextures(1, &g_atlas);
	g_vbo = g_prog = g_atlas = 0;
	g_ready = false;
}

static void rebuildText() {
	g_text = g_prefix + g_base;
	layoutText();
	g_xPos = (float)g_screenW; // restart from the right edge
	g_pauseTimer = 0.0f;
	g_scrollMul = 1.0f;
	g_speedIncreased = false;
}

void scroller_set_text(const std::string& text) {
	g_base = toLatin1(text);
	rebuildText();
}

void scroller_set_now_playing(const std::string& nowPlaying) {
	if (nowPlaying.empty()) {
		g_prefix.clear();
	}
	else {
		std::string t = toLatin1(nowPlaying);
		for (char& c : t) if (c == '#' || c == '$') c = ' '; // those are control characters in the message
		g_prefix = "Now playing: " + t + "        ";
	}
	rebuildText();
}

bool scroller_poll_text_file(const char* path) {
	static time_t lastMtime = 0;
	static bool loadedOnce = false;
	struct stat st;
	if (stat(path, &st) != 0) return false;
	if (loadedOnce && st.st_mtime == lastMtime) return false;

	std::ifstream f(path);
	if (!f.is_open()) return false;
	std::stringstream ss;
	ss << f.rdbuf();
	std::string t = ss.str();
	lastMtime = st.st_mtime;
	loadedOnce = true;
	if (t.empty() || !g_ready) return false;
	scroller_set_text(t);
	printf("Scroller text loaded from %s (%zu chars)\n", path, t.size());
	return true;
}

void scroller_update(float dt, bool touching, float touchX, float touchY, float audioLevel) {
	if (!g_ready || g_text.empty()) return;
	if (dt > 0.1f) dt = 0.1f;

	g_audio += (audioLevel - g_audio) * fminf(1.0f, dt * 10.0f); // smooth the audio level

	// ---- touch
	g_touchAmt += ((touching ? 1.0f : 0.0f) - g_touchAmt) * fminf(1.0f, dt * 8.0f);
	if (touching) {
		if (g_wasTouching) g_xPos += touchX - g_prevTouchX; // drag the text with the finger
		g_touchX = touchX;
		g_touchY = touchY;
		g_prevTouchX = touchX;
	}
	g_wasTouching = touching;

	// ---- scroll / pause (effects keep animating while paused, like the original)
	if (g_pauseTimer > 0.0f) {
		g_pauseTimer -= dt;
	}
	else if (!touching) {
		g_xPos -= SCROLL_SPEED * g_scrollMul * dt;
	}
	if (g_xPos > (float)g_screenW) g_xPos = (float)g_screenW; // can't drag past the start

	// ---- wrap around
	if (g_xPos < -g_totalLength) {
		g_xPos = (float)g_screenW;
		std::fill(g_processed.begin(), g_processed.end(), 0);
		g_scrollMul = 1.0f;
		g_speedIncreased = false;
	}

	// ---- '#' and '$' take effect the first time they come on screen
	for (size_t i = 0; i < g_text.size(); i++) {
		const char c = g_text[i];
		if ((c != '#' && c != '$') || g_processed[i]) continue;
		const float x = g_xPos + g_offsets[i];
		const float w = (float)g_glyph[(unsigned char)c].advance;
		if (x + w > 0 && x < (float)g_screenW) {
			if (c == '#') {
				g_pauseTimer = PAUSE_SECONDS;
			}
			else {
				g_speedIncreased = !g_speedIncreased;
				g_scrollMul = g_speedIncreased ? 2.0f : 1.0f;
			}
			g_processed[i] = 1;
		}
	}

	// ---- animation (colours run faster while touched)
	const float boost = 1.0f + 3.0f * g_touchAmt;
	g_colorHue = fmodf(g_colorHue + COLOR_SPEED * boost * dt, 360.0f);
	g_gradientHue = fmodf(g_gradientHue + GRADIENT_SPEED * dt, 360.0f);
	g_phase += PHASE_SPEED * dt;
}

void scroller_draw() {
	if (!g_ready) return;

	g_verts.clear();
	const float baseAmp = WAVE_AMPLITUDE + g_audio * AUDIO_BOUNCE;

	for (size_t i = 0; i < g_text.size(); i++) {
		const char c = g_text[i];
		const Glyph& g = g_glyph[(unsigned char)c];
		if (!g.valid || isControl(c)) continue;

		const float x = floorf(g_xPos + g_offsets[i]);
		if (x + g.w < 0 || x > (float)g_screenW) continue;

		// sine wave, with a swell + magnet effect around the finger
		float amp = baseAmp;
		float centre;
		float swell = 0.0f;
		if (g_touchAmt > 0.001f) {
			const float d = ((x + g.w * 0.5f) - g_touchX) / TOUCH_RADIUS;
			swell = expf(-d * d) * g_touchAmt;
			amp *= 1.0f + TOUCH_SWELL * swell;
		}
		centre = g_screenH * 0.5f + sinf((float)i * WAVE_FREQUENCY + g_phase) * amp;
		if (swell > 0.0f) centre += (g_touchY - centre) * TOUCH_MAGNET * swell;
		const float y = floorf(centre - g.h * 0.5f);

		float glow[3], rainbow[3];
		hsvToRgb(g_gradientHue + (float)i * 15.0f, 1.0f, 1.0f, glow);
		hsvToRgb(g_colorHue + (float)i * HUE_STEP, 1.0f, 1.0f, rainbow);

		pushQuad(x, y, g, glow, 1.0f);    // radial glow behind the letter
		pushQuad(x, y, g, rainbow, 0.0f); // the letter, additive
	}
	if (g_verts.empty()) return;

	const GLsizei stride = 11 * sizeof(float);
	glUseProgram(g_prog);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied: glow = normal blend, letters = additive
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_atlas);
	glUniform1i(g_uAtlas, 0);

	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glBufferData(GL_ARRAY_BUFFER, g_verts.size() * sizeof(float), g_verts.data(), GL_STREAM_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)(0 * sizeof(float)));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
	for (int a = 0; a < 4; a++) glEnableVertexAttribArray(a);

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(g_verts.size() / 11));

	for (int a = 1; a < 4; a++) glDisableVertexAttribArray(a); // attribute 0 stays enabled for the shader quad
	glDisable(GL_BLEND);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
