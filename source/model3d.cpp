// model3d.cpp – Phase 2: textured OBJ + material presets + ground + camera
#include "model3d.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <vector>
#include <cctype>

namespace model3d {

// ----------------------------------------------------------------- matrix helpers (column-major)
struct Mat4 {
	float m[16];
	Mat4() { memset(m, 0, sizeof(m)); m[0]=m[5]=m[10]=m[15]=1.0f; }
	float& operator()(int r, int c) { return m[c*4 + r]; }
	const float& operator()(int r, int c) const { return m[c*4 + r]; }
};

static Mat4 mul(const Mat4& a, const Mat4& b) {
	Mat4 r;
	for (int c = 0; c < 4; c++)
		for (int row = 0; row < 4; row++)
			r(row,c) = a(row,0)*b(0,c) + a(row,1)*b(1,c) + a(row,2)*b(2,c) + a(row,3)*b(3,c);
	return r;
}

static Mat4 perspective(float fovyDeg, float aspect, float znear, float zfar) {
	Mat4 r;
	float f = 1.0f / tanf(fovyDeg * 0.5f * 3.14159265f / 180.0f);
	r.m[0] = f / aspect; r.m[5] = f;
	r.m[10] = (zfar + znear) / (znear - zfar);
	r.m[11] = -1.0f;
	r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
	r.m[15] = 0.0f;
	return r;
}

static Mat4 lookAt(float eyex, float eyey, float eyez,
                   float cx, float cy, float cz,
                   float upx, float upy, float upz) {
	float fx = cx-eyex, fy = cy-eyey, fz = cz-eyez;
	float len = sqrtf(fx*fx+fy*fy+fz*fz);
	if (len > 1e-6f) { fx/=len; fy/=len; fz/=len; }
	float sx = fy*upz - fz*upy, sy = fz*upx - fx*upz, sz = fx*upy - fy*upx;
	len = sqrtf(sx*sx+sy*sy+sz*sz);
	if (len > 1e-6f) { sx/=len; sy/=len; sz/=len; }
	float ux = sy*fz - sz*fy, uy = sz*fx - sx*fz, uz = sx*fy - sy*fx;
	Mat4 r;
	r.m[0]=sx;  r.m[4]=sy;  r.m[8]=sz;  r.m[12]=0;
	r.m[1]=ux;  r.m[5]=uy;  r.m[9]=uz;  r.m[13]=0;
	r.m[2]=-fx; r.m[6]=-fy; r.m[10]=-fz; r.m[14]=0;
	r.m[3]=0;   r.m[7]=0;   r.m[11]=0;   r.m[15]=1;
	r.m[12] = -(sx*eyex + sy*eyey + sz*eyez);
	r.m[13] = -(ux*eyex + uy*eyey + uz*eyez);
	r.m[14] = -(-fx*eyex - fy*eyey - fz*eyez);
	return r;
}

static Mat4 rotationY(float rad) {
	Mat4 r; float c=cosf(rad), s=sinf(rad);
	r.m[0]=c; r.m[8]=s; r.m[2]=-s; r.m[10]=c; return r;
}
static Mat4 rotationX(float rad) {
	Mat4 r; float c=cosf(rad), s=sinf(rad);
	r.m[5]=c; r.m[9]=-s; r.m[6]=s; r.m[10]=c; return r;
}
static Mat4 rotationZ(float rad) {
	Mat4 r; float c=cosf(rad), s=sinf(rad);
	r.m[0]=c; r.m[4]=-s; r.m[1]=s; r.m[5]=c; return r;
}
static Mat4 scaleMat(float s) {
	Mat4 r; r.m[0]=s; r.m[5]=s; r.m[10]=s; return r;
}

// ----------------------------------------------------------------- GL helpers
static GLuint compile(GLenum type, const char* src) {
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char buf[1024];
		glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
		printf("model3d shader error: %s\n", buf);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

static GLuint linkProgram(const char* vsSrc, const char* fsSrc) {
	GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
	GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
	if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return 0; }
	GLuint p = glCreateProgram();
	glAttachShader(p, vs); glAttachShader(p, fs);
	glBindAttribLocation(p, 0, "aPos");
	glBindAttribLocation(p, 1, "aNormal");
	glBindAttribLocation(p, 2, "aUV");
	glLinkProgram(p);
	glDeleteShader(vs); glDeleteShader(fs);
	GLint linked = 0;
	glGetProgramiv(p, GL_LINK_STATUS, &linked);
	if (!linked) {
		char buf[1024];
		glGetProgramInfoLog(p, sizeof(buf), nullptr, buf);
		printf("model3d link error: %s\n", buf);
		glDeleteProgram(p);
		return 0;
	}
	return p;
}

// ----------------------------------------------------------------- shaders (model)
static const char* kVertSrc = R"(
attribute vec3 aPos;
attribute vec3 aNormal;
attribute vec2 aUV;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;
varying vec3 vN;
varying vec3 vWorldPos;
varying vec2 vUV;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vN = normalize(uNormalMat * aNormal);
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kFragSrc = R"(
precision mediump float;
uniform float uTime;
uniform float uBass;
uniform float uMid;
uniform float uTreble;
uniform vec3  uCamPos;
uniform float uMaterial;
uniform float uHasTex;
uniform sampler2D uTex;
varying vec3 vN;
varying vec3 vWorldPos;
varying vec2 vUV;

vec3 palette(float t) {
    return 0.5 + 0.5 * cos(6.28318 * (t + vec3(0.0, 0.33, 0.67)));
}

void main() {
    vec3 N = normalize(vN);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = normalize(vec3(0.55, 1.05, 0.4));
    vec3 L2 = normalize(vec3(-0.6, 0.3, -0.5));

    float diff = max(dot(N, L), 0.0) * 0.85 + max(dot(N, L2), 0.0) * 0.25;
    float fre  = pow(1.0 - max(dot(N, V), 0.0), 2.6);
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);

    vec3 texCol = vec3(1.0);
    if (uHasTex > 0.5) {
        texCol = texture2D(uTex, vUV).rgb;
    }

    vec3 col;
    float matId = uMaterial;

    if (matId < 0.5) {
        float spec = pow(NdotH, 40.0);
        vec3 base = mix(vec3(0.20, 0.26, 0.36), vec3(0.9, 0.55, 0.18), uBass * 0.9);
        base = mix(base, vec3(0.25, 0.55, 0.95), uMid * 0.5);
        base *= texCol;
        col = base * (0.15 + 0.85 * diff);
        col += vec3(1.0, 0.96, 0.9) * spec * (0.85 + uTreble * 1.5);
        col += base * fre * (0.35 + uBass * 0.6);
        col += base * (0.1 + uBass * 0.5) * (0.55 + 0.45 * sin(uTime * 5.5));
    }
    else if (matId < 1.5) {
        float spec = pow(NdotH, 80.0);
        vec3 env = palette(fre * 0.5 + uTime * 0.05 + uBass * 0.2);
        vec3 base = mix(vec3(0.7), env, 0.55) * texCol;
        col = base * (0.25 + 0.5 * diff);
        col += vec3(1.0) * spec * 1.4;
        col += env * fre * 0.9;
        col += base * uBass * 0.35;
    }
    else if (matId < 2.5) {
        float spec = pow(NdotH, 24.0);
        vec3 neon = palette(uTime * 0.12 + uBass * 0.4 + uMid * 0.2);
        neon = mix(neon, vec3(0.2, 0.9, 1.0), 0.3);
        vec3 base = neon * texCol;
        float pulse = 0.55 + 0.45 * sin(uTime * 8.0 + uBass * 12.0);
        col = base * (0.15 + 0.4 * diff);
        col += base * (0.7 + uBass * 1.8) * pulse;
        col += vec3(1.0) * spec * (0.4 + uTreble);
        col += base * fre * 0.8;
    }
    else {
        float spec = pow(NdotH, 12.0);
        vec3 base = vec3(0.45, 0.42, 0.40) * texCol;
        base = mix(base, palette(uMid * 0.3 + uTime * 0.02), 0.15 + uBass * 0.25);
        col = base * (0.3 + 0.7 * diff);
        col += vec3(0.9) * spec * 0.25;
        col += base * fre * 0.15;
    }

    col = col / (1.0 + col * 0.38);
    col = pow(col, vec3(0.92));
    gl_FragColor = vec4(col, 1.0);
}
)";

static const char* kGroundVert = R"(
attribute vec3 aPos;
uniform mat4 uMVP;
varying vec2 vXZ;
void main() {
    vXZ = aPos.xz;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kGroundFrag = R"(
precision mediump float;
uniform float uBass;
uniform float uTime;
varying vec2 vXZ;
void main() {
    float r = length(vXZ);
    float shadow = exp(-r * r * 1.8);
    float ring = smoothstep(0.02, 0.0, abs(r - 0.85 - uBass * 0.15));
    vec3 col = vec3(0.04, 0.06, 0.12) * shadow;
    col += vec3(0.2, 0.45, 0.9) * ring * (0.25 + uBass * 0.6);
    float grid = abs(sin(vXZ.x * 8.0)) * abs(sin(vXZ.y * 8.0));
    grid = smoothstep(0.92, 1.0, grid);
    col += vec3(0.15, 0.25, 0.4) * grid * shadow * 0.35;
    float alpha = shadow * 0.75 + ring * 0.4;
    gl_FragColor = vec4(col, alpha);
}
)";

// ----------------------------------------------------------------- state
static GLuint g_prog = 0, g_groundProg = 0;
static GLint loc_aPos=-1, loc_aNormal=-1, loc_aUV=-1;
static GLint loc_uMVP=-1, loc_uModel=-1, loc_uNormalMat=-1;
static GLint loc_uTime=-1, loc_uBass=-1, loc_uMid=-1, loc_uTreble=-1, loc_uCamPos=-1;
static GLint loc_uMaterial=-1, loc_uHasTex=-1, loc_uTex=-1;
static GLint gnd_uMVP=-1, gnd_uBass=-1, gnd_uTime=-1;

static GLuint g_groundVbo = 0;
static std::vector<std::string> g_modelFiles;
static int g_current = -1;
static Mesh g_mesh;
static time_t g_meshMtime = 0;
static time_t g_texMtime = 0;
static bool g_ready = false;
static int g_material = MAT_METALLIC;

static float g_camYaw = 0.4f;
static float g_camPitch = 0.25f;
static float g_camDist = 3.8f;
static bool g_autoRotate = true;
static float g_autoYaw = 0.0f;
static bool g_enabled = false;         // off unless //!model on / "file.obj"
static float g_rotateSpeed = 0.55f;    // //!model_rotate (Y / yaw)
static float g_rotateSpeedX = 0.0f;    // //!model_rotate_x (vertical / pitch)
static float g_rotateSpeedZ = 0.0f;    // //!model_rotate_z (roll)
static float g_posX = 0, g_posY = 0, g_posZ = 0; // //!model_pos
static bool g_directiveSetsModel = false; // true if //!model was in the shader
static bool g_audioReactive = true;    // //!model_audio off
static bool g_groundEnabled = true;    // //!model_ground off – hide shadow disc

// ----------------------------------------------------------------- texture override (//!model "skin.jpg")
static std::string g_texOverride; // from the current shader's directive; "" = use the picture named like the model, "none" = no texture

static std::string lowerStr(std::string s) {
	for (char& c : s) c = (char)tolower((unsigned char)c);
	return s;
}

static bool isImageName(const std::string& name) {
	const std::string l = lowerStr(name);
	const char* exts[] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif" };
	for (const char* e : exts) {
		const size_t n = strlen(e);
		if (l.size() > n && l.compare(l.size() - n, n, e) == 0) return true;
	}
	return false;
}

static bool pathExists(const std::string& p) {
	struct stat st;
	return stat(p.c_str(), &st) == 0;
}

// Finds the override picture: next to the .obj first, then the models folder, then the images folder.
// A name with a drive or a leading slash ("sdmc:/...") is used as it is.
static std::string resolveTextureOverride(const std::string& name, const std::string& objPath) {
	if (name.empty()) return "";
	if (name.find(':') != std::string::npos || name[0] == '/') return pathExists(name) ? name : "";
	std::vector<std::string> dirs;
	const size_t slash = objPath.find_last_of("/\\");
	if (slash != std::string::npos) dirs.push_back(objPath.substr(0, slash));
	dirs.push_back("sdmc:/switch/shaderfun/models");
	dirs.push_back("sdmc:/switch/shaderfun/images");
	dirs.push_back("romfs:/models");
	dirs.push_back("romfs:/images");
	for (const std::string& d : dirs) {
		const std::string candidate = d + "/" + name;
		if (pathExists(candidate)) return candidate;
	}
	return "";
}

// ----------------------------------------------------------------- texture load
static bool loadTextureFile(const std::string& path, GLuint& outId) {
	SDL_Surface* surf = IMG_Load(path.c_str());
	if (!surf) return false;
	SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(surf);
	if (!rgba) return false;

	// Cap very large textures so GPU bandwidth stays reasonable on Switch
	const int kMaxDim = 2048;
	if (rgba->w > kMaxDim || rgba->h > kMaxDim) {
		float scale = (float)kMaxDim / (float)std::max(rgba->w, rgba->h);
		int nw = std::max(1, (int)(rgba->w * scale));
		int nh = std::max(1, (int)(rgba->h * scale));
		SDL_Surface* scaled = SDL_CreateRGBSurfaceWithFormat(0, nw, nh, 32, SDL_PIXELFORMAT_ABGR8888);
		if (scaled) {
			SDL_BlitScaled(rgba, nullptr, scaled, nullptr);
			SDL_FreeSurface(rgba);
			rgba = scaled;
			printf("model3d: texture %s downscaled to %dx%d\n", path.c_str(), nw, nh);
		}
	}

	const int rowBytes = rgba->w * 4;
	std::vector<unsigned char> pixels((size_t)rowBytes * rgba->h);
	const unsigned char* base = (const unsigned char*)rgba->pixels;
	for (int y = 0; y < rgba->h; y++)
		memcpy(&pixels[(size_t)(rgba->h - 1 - y) * rowBytes], base + (size_t)y * rgba->pitch, rowBytes);

	if (outId == 0) glGenTextures(1, &outId);
	glBindTexture(GL_TEXTURE_2D, outId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	printf("model3d: texture %s (%dx%d)\n", path.c_str(), rgba->w, rgba->h);
	SDL_FreeSurface(rgba);
	return true;
}

static void tryLoadTextureForModel(Mesh& m) {
	if (m.texId) { glDeleteTextures(1, &m.texId); m.texId = 0; }
	m.hasTex = false;
	m.texKey = g_texOverride;

	// a picture chosen by the shader (//!model "skin.jpg" or //!model_texture "skin.jpg")
	if (!g_texOverride.empty()) {
		const std::string low = lowerStr(g_texOverride);
		if (low == "none" || low == "off") return; // plain colour on purpose
		const std::string found = resolveTextureOverride(g_texOverride, m.path);
		if (!found.empty() && loadTextureFile(found, m.texId)) { m.hasTex = true; return; }
		printf("model3d: texture '%s' not found or unreadable, using the model's own picture\n", g_texOverride.c_str());
	}

	std::string base = m.path;
	size_t dot = base.find_last_of('.');
	if (dot != std::string::npos) base = base.substr(0, dot);

	const char* exts[] = { ".png", ".jpg", ".jpeg", ".PNG", ".JPG", ".JPEG", ".bmp", ".BMP" };
	for (const char* e : exts) {
		std::string candidate = base + e;
		if (loadTextureFile(candidate, m.texId)) {
			m.hasTex = true;
			return;
		}
	}
	size_t slash = m.name.find_last_of('.');
	std::string nameOnly = (slash != std::string::npos) ? m.name.substr(0, slash) : m.name;
	for (const char* e : exts) {
		std::string candidate = std::string("sdmc:/switch/shaderfun/images/") + nameOnly + e;
		if (loadTextureFile(candidate, m.texId)) {
			m.hasTex = true;
			return;
		}
	}
}

// ----------------------------------------------------------------- OBJ loader (with UVs)
// Optimised for large files: fast parse, triangle budget, optional decimation.
struct Vertex {
	float x, y, z, nx, ny, nz, u, v;
};

// Switch-friendly budget. Above this we keep a uniform subset of triangles.
static const int kMaxTriangles = 80000;

static bool loadOBJ(const std::string& path, Mesh& out) {
	FILE* fp = fopen(path.c_str(), "rb");
	if (!fp) {
		printf("model3d: cannot open %s\n", path.c_str());
		return false;
	}

	// Estimate size for reserves
	fseek(fp, 0, SEEK_END);
	long fsz = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	std::vector<float> pos, norms, uvs;
	pos.reserve((size_t)std::max(1024L, fsz / 40));
	norms.reserve(pos.capacity());
	uvs.reserve(pos.capacity() / 2);

	// First pass: count faces roughly while loading attributes, store face index records
	struct FaceTri { int v[3], t[3], n[3]; };
	std::vector<FaceTri> faces;
	faces.reserve((size_t)std::max(1024L, fsz / 50));

	char line[2048];
	while (fgets(line, sizeof(line), fp)) {
		char* s = line;
		while (*s == ' ' || *s == '\t') s++;
		if (*s == 0 || *s == '#' || *s == '\n' || *s == '\r') continue;

		if (s[0] == 'v' && s[1] == ' ') {
			float x, y, z;
			if (sscanf(s + 2, "%f %f %f", &x, &y, &z) >= 3) {
				pos.push_back(x); pos.push_back(y); pos.push_back(z);
			}
		} else if (s[0] == 'v' && s[1] == 'n' && s[2] == ' ') {
			float x, y, z;
			if (sscanf(s + 3, "%f %f %f", &x, &y, &z) >= 3) {
				norms.push_back(x); norms.push_back(y); norms.push_back(z);
			}
		} else if (s[0] == 'v' && s[1] == 't' && s[2] == ' ') {
			float u, v;
			if (sscanf(s + 3, "%f %f", &u, &v) >= 2) {
				uvs.push_back(u); uvs.push_back(v);
			}
		} else if (s[0] == 'f' && s[1] == ' ') {
			// Parse up to 8 corners, triangulate as fan
			int vi[8], ti[8], ni[8];
			int ncorn = 0;
			const char* p = s + 2;
			while (*p && ncorn < 8) {
				while (*p == ' ' || *p == '\t') p++;
				if (*p == 0 || *p == '\n' || *p == '\r') break;
				int v = 0, t = 0, n = 0;
				// formats: v, v/t, v//n, v/t/n
				if (sscanf(p, "%d/%d/%d", &v, &t, &n) == 3) {
					/* ok */
				} else if (sscanf(p, "%d//%d", &v, &n) == 2) {
					t = 0;
				} else if (sscanf(p, "%d/%d", &v, &t) == 2) {
					n = 0;
				} else if (sscanf(p, "%d", &v) == 1) {
					t = n = 0;
				} else {
					break;
				}
				if (v < 0) v = (int)(pos.size() / 3) + v + 1;
				if (t < 0) t = (int)(uvs.size() / 2) + t + 1;
				if (n < 0) n = (int)(norms.size() / 3) + n + 1;
				vi[ncorn] = v; ti[ncorn] = t; ni[ncorn] = n;
				ncorn++;
				while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
			}
			for (int i = 1; i + 1 < ncorn; i++) {
				FaceTri ft;
				ft.v[0] = vi[0]; ft.v[1] = vi[i]; ft.v[2] = vi[i + 1];
				ft.t[0] = ti[0]; ft.t[1] = ti[i]; ft.t[2] = ti[i + 1];
				ft.n[0] = ni[0]; ft.n[1] = ni[i]; ft.n[2] = ni[i + 1];
				faces.push_back(ft);
			}
		}
	}
	fclose(fp);

	if (faces.empty() || pos.empty()) {
		printf("model3d: no triangles in %s\n", path.c_str());
		return false;
	}

	const int totalTris = (int)faces.size();
	int step = 1;
	if (totalTris > kMaxTriangles) {
		step = (totalTris + kMaxTriangles - 1) / kMaxTriangles;
		printf("model3d: %s has %d tris – decimating 1/%d to stay under %d (Switch performance)\n",
			path.c_str(), totalTris, step, kMaxTriangles);
	}

	std::vector<Vertex> verts;
	verts.reserve((size_t)std::min(totalTris, kMaxTriangles) * 3);

	const int npos = (int)(pos.size() / 3);
	const int nnorm = (int)(norms.size() / 3);
	const int nuv = (int)(uvs.size() / 2);

	for (int fi = 0; fi < totalTris; fi += step) {
		const FaceTri& f = faces[fi];
		Vertex tri[3];
		for (int k = 0; k < 3; k++) {
			Vertex vert = {};
			int pi = f.v[k] - 1;
			if (pi >= 0 && pi < npos) {
				vert.x = pos[pi * 3]; vert.y = pos[pi * 3 + 1]; vert.z = pos[pi * 3 + 2];
			}
			int nii = f.n[k] - 1;
			if (nii >= 0 && nii < nnorm) {
				vert.nx = norms[nii * 3]; vert.ny = norms[nii * 3 + 1]; vert.nz = norms[nii * 3 + 2];
			}
			int tii = f.t[k] - 1;
			if (tii >= 0 && tii < nuv) {
				vert.u = uvs[tii * 2]; vert.v = uvs[tii * 2 + 1];
			}
			tri[k] = vert;
		}
		// flat normal if missing
		if (tri[0].nx == 0 && tri[0].ny == 0 && tri[0].nz == 0) {
			float ax = tri[1].x - tri[0].x, ay = tri[1].y - tri[0].y, az = tri[1].z - tri[0].z;
			float bx = tri[2].x - tri[0].x, by = tri[2].y - tri[0].y, bz = tri[2].z - tri[0].z;
			float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
			float len = sqrtf(nx * nx + ny * ny + nz * nz);
			if (len > 1e-8f) { nx /= len; ny /= len; nz /= len; }
			for (int k = 0; k < 3; k++) { tri[k].nx = nx; tri[k].ny = ny; tri[k].nz = nz; }
		}
		verts.push_back(tri[0]);
		verts.push_back(tri[1]);
		verts.push_back(tri[2]);
	}

	// Free face list early
	faces.clear();
	faces.shrink_to_fit();
	pos.clear(); pos.shrink_to_fit();
	norms.clear(); norms.shrink_to_fit();
	uvs.clear(); uvs.shrink_to_fit();

	if (verts.empty()) {
		printf("model3d: no triangles kept in %s\n", path.c_str());
		return false;
	}

	// Auto UVs only for smaller meshes (expensive on huge ones)
	if (verts.size() <= 120000) {
		bool anyUV = false;
		for (const auto& v : verts) {
			if (v.u != 0.0f || v.v != 0.0f) { anyUV = true; break; }
		}
		if (!anyUV) {
			float bminx=1e9,bminy=1e9,bminz=1e9,bmaxx=-1e9,bmaxy=-1e9,bmaxz=-1e9;
			for (const auto& v : verts) {
				bminx=std::min(bminx,v.x); bmaxx=std::max(bmaxx,v.x);
				bminy=std::min(bminy,v.y); bmaxy=std::max(bmaxy,v.y);
				bminz=std::min(bminz,v.z); bmaxz=std::max(bmaxz,v.z);
			}
			float dx = std::max(1e-6f, bmaxx-bminx);
			float dy = std::max(1e-6f, bmaxy-bminy);
			float dz = std::max(1e-6f, bmaxz-bminz);
			for (auto& v : verts) {
				float ax = fabsf(v.nx), ay = fabsf(v.ny), az = fabsf(v.nz);
				if (ax >= ay && ax >= az) { v.u = (v.z - bminz) / dz; v.v = (v.y - bminy) / dy; }
				else if (ay >= ax && ay >= az) { v.u = (v.x - bminx) / dx; v.v = (v.z - bminz) / dz; }
				else { v.u = (v.x - bminx) / dx; v.v = (v.y - bminy) / dy; }
			}
		}
	}

	float minx=1e9,miny=1e9,minz=1e9,maxx=-1e9,maxy=-1e9,maxz=-1e9;
	for (const auto& v : verts) {
		minx=std::min(minx,v.x); maxx=std::max(maxx,v.x);
		miny=std::min(miny,v.y); maxy=std::max(maxy,v.y);
		minz=std::min(minz,v.z); maxz=std::max(maxz,v.z);
	}
	float cx=0.5f*(minx+maxx), cy=0.5f*(miny+maxy), cz=0.5f*(minz+maxz);
	float maxDim = std::max(maxx-minx, std::max(maxy-miny, maxz-minz));
	float s = (maxDim > 1e-6f) ? (1.8f / maxDim) : 1.0f;
	float bottom = (miny - cy) * s;

	for (auto& v : verts) {
		v.x = (v.x - cx) * s;
		v.y = (v.y - cy) * s - bottom;
		v.z = (v.z - cz) * s;
	}

	float rad = 0.0f;
	for (const auto& v : verts) {
		float r2 = v.x*v.x + v.y*v.y + v.z*v.z;
		if (r2 > rad) rad = r2;
	}
	out.radius = sqrtf(rad);

	if (out.vbo) glDeleteBuffers(1, &out.vbo);
	glGenBuffers(1, &out.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	out.vertexCount = (int)verts.size();
	out.path = path;
	size_t slash = path.find_last_of("/\\");
	out.name = (slash == std::string::npos) ? path : path.substr(slash + 1);

	tryLoadTextureForModel(out);

	printf("model3d: loaded %s (%d tris from %d, r=%.2f, tex=%s)\n",
		out.name.c_str(), out.vertexCount/3, totalTris, out.radius, out.hasTex ? "yes" : "no");
	return true;
}

static void freeMesh(Mesh& m) {
	if (m.vbo) { glDeleteBuffers(1, &m.vbo); m.vbo = 0; }
	if (m.texId) { glDeleteTextures(1, &m.texId); m.texId = 0; }
	m.vertexCount = 0; m.hasTex = false;
	m.texKey.clear();
	m.path.clear(); m.name.clear();
}

static void scanRecursive(const std::string& dirPath, std::vector<std::string>& files) {
	DIR* dir = opendir(dirPath.c_str());
	if (!dir) return;
	struct dirent* ent;
	while ((ent = readdir(dir)) != nullptr) {
		std::string name = ent->d_name;
		if (name == "." || name == "..") continue;
		std::string full = dirPath + "/" + name;
		if (ent->d_type == DT_DIR) {
			scanRecursive(full, files);
		} else if (name.size() > 4) {
			std::string ext = name.substr(name.size() - 4);
			for (char& c : ext) c = (char)tolower(c);
			if (ext == ".obj") files.push_back(full);
		}
	}
	closedir(dir);
}


static time_t fileMtime(const std::string& p) {
	struct stat st;
	if (stat(p.c_str(), &st) != 0) return 0;
	return st.st_mtime;
}

static time_t textureMtimeFor(const std::string& objPath) {
	std::string base = objPath;
	size_t dot = base.find_last_of('.');
	if (dot != std::string::npos) base = base.substr(0, dot);
	const char* exts[] = { ".png", ".jpg", ".jpeg", ".PNG", ".JPG", ".JPEG", ".bmp", ".BMP" };
	time_t best = 0;
	for (const char* e : exts) {
		time_t t = fileMtime(base + e);
		if (t > best) best = t;
	}
	if (!g_texOverride.empty()) {
		const std::string ov = resolveTextureOverride(g_texOverride, objPath);
		if (!ov.empty()) { time_t t = fileMtime(ov); if (t > best) best = t; }
	}
	// also images/ folder by basename
	size_t slash = objPath.find_last_of("/\\");
	std::string name = (slash == std::string::npos) ? objPath : objPath.substr(slash + 1);
	dot = name.find_last_of('.');
	if (dot != std::string::npos) name = name.substr(0, dot);
	for (const char* e : exts) {
		time_t t = fileMtime(std::string("sdmc:/switch/shaderfun/images/") + name + e);
		if (t > best) best = t;
	}
	return best;
}

static bool loadCurrent() {
	freeMesh(g_mesh);
	if (g_current < 0 || g_current >= (int)g_modelFiles.size()) return false;
	const std::string& p = g_modelFiles[g_current];
	bool ok = loadOBJ(p, g_mesh);
	if (ok) {
		g_meshMtime = fileMtime(p);
		g_texMtime = textureMtimeFor(p);
	} else {
		g_meshMtime = 0;
		g_texMtime = 0;
	}
	return ok;
}

static void createGround() {
	const float s = 3.5f;
	float verts[] = {
		-s, 0, -s,  s, 0, -s,  s, 0,  s,
		-s, 0, -s,  s, 0,  s, -s, 0,  s,
	};
	glGenBuffers(1, &g_groundVbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_groundVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool init() {
	g_prog = linkProgram(kVertSrc, kFragSrc);
	g_groundProg = linkProgram(kGroundVert, kGroundFrag);
	if (!g_prog || !g_groundProg) {
		printf("model3d: failed to create shaders\n");
		return false;
	}

	loc_aPos      = glGetAttribLocation(g_prog, "aPos");
	loc_aNormal   = glGetAttribLocation(g_prog, "aNormal");
	loc_aUV       = glGetAttribLocation(g_prog, "aUV");
	loc_uMVP      = glGetUniformLocation(g_prog, "uMVP");
	loc_uModel    = glGetUniformLocation(g_prog, "uModel");
	loc_uNormalMat= glGetUniformLocation(g_prog, "uNormalMat");
	loc_uTime     = glGetUniformLocation(g_prog, "uTime");
	loc_uBass     = glGetUniformLocation(g_prog, "uBass");
	loc_uMid      = glGetUniformLocation(g_prog, "uMid");
	loc_uTreble   = glGetUniformLocation(g_prog, "uTreble");
	loc_uCamPos   = glGetUniformLocation(g_prog, "uCamPos");
	loc_uMaterial = glGetUniformLocation(g_prog, "uMaterial");
	loc_uHasTex   = glGetUniformLocation(g_prog, "uHasTex");
	loc_uTex      = glGetUniformLocation(g_prog, "uTex");

	gnd_uMVP  = glGetUniformLocation(g_groundProg, "uMVP");
	gnd_uBass = glGetUniformLocation(g_groundProg, "uBass");
	gnd_uTime = glGetUniformLocation(g_groundProg, "uTime");

	createGround();

	g_ready = true;
	printf("model3d: ready (models + materials + optional ground)\n");

	mkdir("sdmc:/switch/shaderfun", 0777);
	mkdir("sdmc:/switch/shaderfun/models", 0777);

	rescan();
	return true;
}

void shutdown() {
	freeMesh(g_mesh);
	if (g_groundVbo) { glDeleteBuffers(1, &g_groundVbo); g_groundVbo = 0; }
	if (g_prog) { glDeleteProgram(g_prog); g_prog = 0; }
	if (g_groundProg) { glDeleteProgram(g_groundProg); g_groundProg = 0; }
	g_modelFiles.clear();
	g_current = -1;
	g_ready = false;
}

void rescan() {
	std::vector<std::string> found;
	scanRecursive("sdmc:/switch/shaderfun/models", found);
	if (found.empty()) scanRecursive("romfs:/models", found);
	std::sort(found.begin(), found.end());

	std::string prevPath = (g_current >= 0 && g_current < (int)g_modelFiles.size())
		? g_modelFiles[g_current] : std::string();

	const bool listChanged = (found != g_modelFiles);
	g_modelFiles = found;

	if (listChanged)
		printf("model3d: found %zu model(s)\n", g_modelFiles.size());

	if (g_modelFiles.empty()) {
		g_current = -1;
		freeMesh(g_mesh);
		g_meshMtime = g_texMtime = 0;
		return;
	}

	int newIdx = 0;
	bool stillThere = false;
	for (size_t i = 0; i < g_modelFiles.size(); i++) {
		if (g_modelFiles[i] == prevPath) { newIdx = (int)i; stillThere = true; break; }
	}
	g_current = newIdx;

	// Only reload from disk if the selection changed, or the .obj / texture was updated
	const std::string& cur = g_modelFiles[g_current];
	const time_t objT = fileMtime(cur);
	const time_t texT = textureMtimeFor(cur);
	const bool needReload =
		!stillThere ||
		g_mesh.vertexCount == 0 ||
		g_mesh.path != cur ||
		objT != g_meshMtime ||
		texT != g_texMtime;

	if (needReload) {
		printf("model3d: reloading %s\n", cur.c_str());
		loadCurrent();
	}
}

int count() { return (int)g_modelFiles.size(); }
int currentIndex() { return g_current; }
bool hasModel() { return g_ready && g_mesh.vertexCount > 0; }

std::string setIndex(int idx) {
	if (g_modelFiles.empty()) return "";
	g_current = idx % (int)g_modelFiles.size();
	if (g_current < 0) g_current += (int)g_modelFiles.size();
	loadCurrent();
	g_autoRotate = true; // reset to auto when changing model
	return g_mesh.name;
}
std::string currentFileName() {
	if (g_modelFiles.empty() || g_current < 0 || g_current >= (int)g_modelFiles.size()) return "";
	const std::string& fn = g_modelFiles[g_current];
	const size_t slash = fn.find_last_of("/\\");
	return slash == std::string::npos ? fn : fn.substr(slash + 1);
}
std::string next() { return g_modelFiles.empty() ? "" : setIndex(g_current + 1); }
std::string prev() { return g_modelFiles.empty() ? "" : setIndex(g_current - 1); }

int materialIndex() { return g_material; }
std::string materialName() {
	static const char* names[] = { "Metallic", "Chrome", "Neon", "Matte" };
	return names[g_material % MAT_COUNT];
}
std::string nextMaterial() {
	g_material = (g_material + 1) % MAT_COUNT;
	return materialName();
}
std::string prevMaterial() {
	g_material = (g_material + MAT_COUNT - 1) % MAT_COUNT;
	return materialName();
}


std::string selectByName(const std::string& name) {
	if (name.empty() || g_modelFiles.empty()) return "";
	std::string needle = name;
	// strip path
	size_t slash = needle.find_last_of("/\\");
	if (slash != std::string::npos) needle = needle.substr(slash + 1);
	std::string needleLower = needle;
	for (char& c : needleLower) c = (char)tolower(c);
	// strip extension for flexible match
	std::string base = needleLower;
	size_t dot = base.find_last_of('.');
	if (dot != std::string::npos) base = base.substr(0, dot);

	for (size_t i = 0; i < g_modelFiles.size(); i++) {
		std::string fn = g_modelFiles[i];
		size_t s = fn.find_last_of("/\\");
		std::string bn = (s == std::string::npos) ? fn : fn.substr(s + 1);
		std::string bnLower = bn;
		for (char& c : bnLower) c = (char)tolower(c);
		std::string bnBase = bnLower;
		size_t d = bnBase.find_last_of('.');
		if (d != std::string::npos) bnBase = bnBase.substr(0, d);
		if (bnLower == needleLower || bnBase == base) {
			return setIndex((int)i);
		}
	}
	printf("model3d: no model matching '%s'\n", name.c_str());
	return "";
}

void setEnabled(bool on) { g_enabled = on; }
bool isEnabled() { return g_enabled; }
void setAudioReactive(bool on) { g_audioReactive = on; }
bool isAudioReactive() { return g_audioReactive; }

bool setMaterialByName(const std::string& name) {
	std::string n = name;
	for (char& c : n) c = (char)tolower(c);
	if (n.find("metal") != std::string::npos) { g_material = MAT_METALLIC; return true; }
	if (n.find("chrome") != std::string::npos || n.find("mirror") != std::string::npos) { g_material = MAT_CHROME; return true; }
	if (n.find("neon") != std::string::npos || n.find("emissive") != std::string::npos) { g_material = MAT_NEON; return true; }
	if (n.find("matte") != std::string::npos || n.find("diffuse") != std::string::npos) { g_material = MAT_MATTE; return true; }
	return false;
}

void setRotateSpeed(float radPerSec) { g_rotateSpeed = radPerSec; }
void setRotateSpeedX(float radPerSec) { g_rotateSpeedX = radPerSec; }
void setRotateSpeedZ(float radPerSec) { g_rotateSpeedZ = radPerSec; }
void setCamDist(float dist) { g_camDist = std::max(1.2f, std::min(12.0f, dist)); }
void setCamYaw(float rad) { g_camYaw = rad; g_autoYaw = 0; }
void setCamPitch(float rad) { g_camPitch = std::max(-1.3f, std::min(1.3f, rad)); }
void setAutoRotate(bool on) { g_autoRotate = on; }
void setPosOffset(float x, float y, float z) { g_posX = x; g_posY = y; g_posZ = z; }

static std::string extractQuoted(const std::string& line, size_t from) {
	size_t q1 = line.find('"', from);
	if (q1 == std::string::npos) return "";
	size_t q2 = line.find('"', q1 + 1);
	if (q2 == std::string::npos) return "";
	return line.substr(q1 + 1, q2 - q1 - 1);
}

static float extractFloat(const std::string& line, size_t from, float fallback) {
	// skip keyword, read next number
	size_t i = from;
	while (i < line.size() && (isalpha((unsigned char)line[i]) || line[i]=='_' || line[i]=='!' || line[i]=='/')) i++;
	while (i < line.size() && isspace((unsigned char)line[i])) i++;
	if (i >= line.size()) return fallback;
	return (float)atof(line.c_str() + i);
}

// every "quoted" string on the line after 'from'
static std::vector<std::string> quotedStrings(const std::string& line, size_t from) {
	std::vector<std::string> out;
	size_t i = from;
	while (true) {
		const size_t q1 = line.find('"', i);
		if (q1 == std::string::npos) break;
		const size_t q2 = line.find('"', q1 + 1);
		if (q2 == std::string::npos) break;
		if (q2 > q1 + 1) out.push_back(line.substr(q1 + 1, q2 - q1 - 1));
		i = q2 + 1;
	}
	return out;
}

// After the directives are read: if the model on screen wears a different skin than this shader asks for, change it
static void syncTextureOverride() {
	if (g_mesh.vertexCount > 0 && g_mesh.texKey != g_texOverride) {
		tryLoadTextureForModel(g_mesh);
		g_texMtime = textureMtimeFor(g_mesh.path);
	}
}

void applyShaderDirectives(const std::string& fragSource) {
	// Reset each time a shader loads – model is OFF unless the frag opts in
	g_enabled = false;
	g_rotateSpeed = 0.55f;
	g_rotateSpeedX = 0.0f;
	g_rotateSpeedZ = 0.0f;
	g_posX = g_posY = g_posZ = 0;
	g_directiveSetsModel = false;
	g_audioReactive = true;
	g_groundEnabled = true;
	g_texOverride.clear(); // each shader chooses its own skin (or none)
	// leave camera yaw/pitch/dist and material as-is unless overridden
	// (so manual free-look isn't wiped every shader change unless directed)

	if (fragSource.empty()) { syncTextureOverride(); return; }

	std::istringstream in(fragSource);
	std::string line;
	int lineNo = 0;
	while (std::getline(in, line) && lineNo++ < 60) {
		// //!model off | on | "file.obj"
		size_t p = line.find("//!model");
		if (p != std::string::npos) {
			// avoid matching //!model_rotate etc. unless exact-ish
			std::string rest = line.substr(p + 8);
			// trim
			size_t rs = rest.find_first_not_of(" \t");
			if (rs == std::string::npos) continue;
			rest = rest.substr(rs);
			std::string restLower = rest;
			for (char& c : restLower) c = (char)tolower(c);

			if (restLower.compare(0, 8, "_texture") == 0 || restLower.compare(0, 5, "_skin") == 0) {
				// //!model_texture "skin.jpg"   (or: //!model_texture none)
				std::string q = extractQuoted(line, p);
				if (q.empty() && (restLower.find("none") != std::string::npos || restLower.find("off") != std::string::npos)) q = "none";
				if (!q.empty()) g_texOverride = q;
				continue;
			}
			if (restLower.compare(0, 9, "_rotate_x") == 0) {
				g_rotateSpeedX = extractFloat(line, p + 8, g_rotateSpeedX);
				continue;
			}
			if (restLower.compare(0, 9, "_rotate_z") == 0) {
				g_rotateSpeedZ = extractFloat(line, p + 8, g_rotateSpeedZ);
				continue;
			}
			if (restLower.compare(0, 7, "_rotate") == 0) {
				g_rotateSpeed = extractFloat(line, p + 8, g_rotateSpeed);
				continue;
			}
			if (restLower.compare(0, 5, "_zoom") == 0 || restLower.compare(0, 5, "_dist") == 0) {
				setCamDist(extractFloat(line, p + 8, g_camDist));
				continue;
			}
			if (restLower.compare(0, 4, "_yaw") == 0) {
				setCamYaw(extractFloat(line, p + 8, g_camYaw));
				g_autoRotate = false;
				continue;
			}
			if (restLower.compare(0, 6, "_pitch") == 0) {
				setCamPitch(extractFloat(line, p + 8, g_camPitch));
				g_autoRotate = false;
				continue;
			}
			if (restLower.compare(0, 4, "_pos") == 0) {
				// //!model_pos x y z
				float x=0,y=0,z=0;
				size_t i = p + 8;
				while (i < line.size() && (isalpha((unsigned char)line[i]) || line[i]=='_' || line[i]=='!' || line[i]=='/')) i++;
				if (i < line.size()) {
					sscanf(line.c_str() + i, "%f %f %f", &x, &y, &z);
					setPosOffset(x, y, z);
				}
				continue;
			}
			if (restLower.compare(0, 5, "_auto") == 0) {
				setAutoRotate(restLower.find("off") == std::string::npos && restLower.find("0") == std::string::npos);
				continue;
			}
			if (restLower.compare(0, 6, "_audio") == 0) {
				g_audioReactive = (restLower.find("off") == std::string::npos && restLower.find("0") == std::string::npos);
				continue;
			}
			if (restLower.compare(0, 7, "_ground") == 0) {
				g_groundEnabled = (restLower.find("off") == std::string::npos && restLower.find("0") == std::string::npos);
				continue;
			}

			// plain //!model ...   (a quoted name ending in .obj picks the model, one ending in .jpg/.png/.bmp picks its skin)
			const bool hasQuote = rest.find('"') != std::string::npos;
			if (!hasQuote && restLower.find("off") != std::string::npos) {
				g_enabled = false;
				g_directiveSetsModel = true;
			} else if (!hasQuote && restLower.find("on") != std::string::npos) {
				g_enabled = true;
				g_directiveSetsModel = true;
			} else {
				for (const std::string& q : quotedStrings(line, p)) {
					if (isImageName(q)) {
						g_texOverride = q;
						g_enabled = true;
					}
					else {
						g_enabled = true;
						g_directiveSetsModel = true;
						selectByName(q);
					}
				}
			}
			continue;
		}

		// //!material neon
		p = line.find("//!material");
		if (p != std::string::npos) {
			std::string q = extractQuoted(line, p);
			std::string arg = q;
			if (arg.empty()) {
				size_t i = p + 11;
				while (i < line.size() && isspace((unsigned char)line[i])) i++;
				arg = line.substr(i);
				// trim trailing
				while (!arg.empty() && isspace((unsigned char)arg.back())) arg.pop_back();
			}
			if (!arg.empty()) setMaterialByName(arg);
			continue;
		}
	}
	syncTextureOverride();
}

void applyAutoMatch(const std::string& shaderPath) {
	// Basename auto-match is disabled: models only show with //!model on or //!model "file.obj"
	(void)shaderPath;
}

void updateCamera(float dt, float stickX, float stickY, bool lookHeld) {
	if (lookHeld) {
		g_autoRotate = false;
		g_camYaw   += stickX * dt * 2.2f;
		g_camPitch += stickY * dt * 1.6f;
		g_camPitch = std::max(-1.2f, std::min(1.2f, g_camPitch));
	} else {
		if (fabsf(stickY) > 0.15f) {
			g_camDist -= stickY * dt * 2.8f;
			g_camDist = std::max(1.6f, std::min(9.0f, g_camDist));
		}
		if (g_autoRotate) {
			g_autoYaw += dt * 0.35f;
		}
	}
}

void draw(float timeSeconds, float bass, float mid, float treble) {
	if (!g_enabled || !hasModel()) return;
	if (!g_audioReactive) { bass = 0.0f; mid = 0.0f; treble = 0.0f; }

	GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
	GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
	GLboolean blendWas = glIsEnabled(GL_BLEND);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glClear(GL_DEPTH_BUFFER_BIT);

	float yaw = g_autoRotate ? (g_autoYaw + g_camYaw) : g_camYaw;
	float pitch = g_camPitch;
	float dist = g_camDist + g_mesh.radius * 0.25f;

	float camX = sinf(yaw) * cosf(pitch) * dist;
	float camY = sinf(pitch) * dist + 0.6f;
	float camZ = cosf(yaw) * cosf(pitch) * dist;

	Mat4 proj = perspective(45.0f, 1280.0f/720.0f, 0.1f, 40.0f);
	Mat4 view = lookAt(camX, camY, camZ, 0, 0.35f, 0, 0, 1, 0);
	Mat4 viewProj = mul(proj, view);

	// ground / shadow disc (optional)
	if (g_groundEnabled) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
		glUseProgram(g_groundProg);
		glUniformMatrix4fv(gnd_uMVP, 1, GL_FALSE, viewProj.m);
		glUniform1f(gnd_uBass, bass);
		glUniform1f(gnd_uTime, timeSeconds);
		glBindBuffer(GL_ARRAY_BUFFER, g_groundVbo);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisableVertexAttribArray(0);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	// model
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

	glUseProgram(g_prog);

	float pulse = 1.0f + (g_audioReactive ? bass * 0.07f : 0.0f);
	Mat4 model;
	float rotY = g_autoRotate ? timeSeconds * g_rotateSpeed : 0.0f;
	// Continuous vertical spin if //!model_rotate_x set; else slight idle nod
	float rotX = (g_rotateSpeedX != 0.0f)
		? timeSeconds * g_rotateSpeedX
		: (g_autoRotate ? (0.12f + sinf(timeSeconds * 0.35f) * 0.06f) : 0.0f);
	float rotZ = timeSeconds * g_rotateSpeedZ;
	model = scaleMat(pulse);
	if (rotX != 0.0f) model = mul(rotationX(rotX), model);
	if (rotZ != 0.0f) model = mul(rotationZ(rotZ), model);
	if (rotY != 0.0f) model = mul(rotationY(rotY), model);
	model.m[12] += g_posX;
	model.m[13] += g_posY;
	model.m[14] += g_posZ;
	Mat4 mvp = mul(viewProj, model);

	float nmat[9] = {
		model.m[0], model.m[1], model.m[2],
		model.m[4], model.m[5], model.m[6],
		model.m[8], model.m[9], model.m[10]
	};

	glUniformMatrix4fv(loc_uMVP, 1, GL_FALSE, mvp.m);
	glUniformMatrix4fv(loc_uModel, 1, GL_FALSE, model.m);
	glUniformMatrix3fv(loc_uNormalMat, 1, GL_FALSE, nmat);
	glUniform1f(loc_uTime, timeSeconds);
	glUniform1f(loc_uBass, bass);
	glUniform1f(loc_uMid, mid);
	glUniform1f(loc_uTreble, treble);
	glUniform3f(loc_uCamPos, camX, camY, camZ);
	glUniform1f(loc_uMaterial, (float)g_material);
	glUniform1f(loc_uHasTex, g_mesh.hasTex ? 1.0f : 0.0f);

	if (g_mesh.hasTex) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, g_mesh.texId);
		glUniform1i(loc_uTex, 0);
	}

	glBindBuffer(GL_ARRAY_BUFFER, g_mesh.vbo);
	const int stride = 8 * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

	glDrawArrays(GL_TRIANGLES, 0, g_mesh.vertexCount);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	if (!depthWas) glDisable(GL_DEPTH_TEST);
	if (!cullWas)  glDisable(GL_CULL_FACE);
	if (blendWas)  glEnable(GL_BLEND);
	else           glDisable(GL_BLEND);
}

} // namespace model3d
