#pragma once
// Lightweight 3D model support for ShaderFun (GLES 2.0) – Phase 2+
// OBJ loader + textures + materials + ground + camera + frag directives.

#include <string>
#include <vector>
#include <GLES2/gl2.h>

namespace model3d {

enum MaterialPreset {
	MAT_METALLIC = 0,
	MAT_CHROME,
	MAT_NEON,
	MAT_MATTE,
	MAT_COUNT
};

struct Mesh {
	GLuint vbo = 0;
	int vertexCount = 0;
	float radius = 1.0f;
	std::string path;
	std::string name;
	GLuint texId = 0;
	bool hasTex = false;
};

bool init();
void shutdown();
void rescan();

int count();
int currentIndex();
std::string next();
std::string prev();
std::string setIndex(int idx);
std::string currentFileName(); // file name of the selected model ("" if none) - used to restore it on the next launch

// Select by filename (basename or full path). Returns name or empty on failure.
// Example: selectByName("spaceship.obj") or selectByName("spaceship")
std::string selectByName(const std::string& name);

// Enable / disable drawing (directives and manual control)
void setEnabled(bool on);
bool isEnabled();

// Audio-reactive material / pulse (//!model_audio off)
void setAudioReactive(bool on);
bool isAudioReactive();

// Material
int materialIndex();
std::string materialName();
std::string nextMaterial();
std::string prevMaterial();
bool setMaterialByName(const std::string& name); // "metallic","chrome","neon","matte"

// Transform overrides (from directives; 0 / defaults = auto)
void setRotateSpeed(float radPerSec);   // model spin speed (default ~0.55)
void setCamDist(float dist);            // zoom (default ~3.8)
void setCamYaw(float rad);              // fixed yaw offset
void setCamPitch(float rad);
void setAutoRotate(bool on);
void setPosOffset(float x, float y, float z); // world offset of the model

// Parse //!model / //!material / //!model_* lines from a fragment shader source.
// Call when a shader is loaded. Safe to call with empty string (resets to defaults).
void applyShaderDirectives(const std::string& fragSource);

// If the shader has no //!model directive, optionally auto-pick a model whose
// basename matches the shader file basename (e.g. Tunnel.frag -> Tunnel.obj).
// shaderPath = full path of the loaded .frag/.glsl file.
void applyAutoMatch(const std::string& shaderPath);

void updateCamera(float dt, float stickX, float stickY, bool lookHeld);
void draw(float timeSeconds, float bass, float mid, float treble);
bool hasModel();

} // namespace model3d
