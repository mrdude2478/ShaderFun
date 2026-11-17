// Coloured Lines (Optimized)
precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// --- Rotations ---
mat2 mm2(float a) { float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }
vec3 roty(vec3 p, float a) { float s = sin(a), c = cos(a); return vec3(c*p.x + s*p.z, p.y, -s*p.x + c*p.z); }
vec3 rotz(vec3 p, float a) { float s = sin(a), c = cos(a); return vec3(c*p.x - s*p.y, s*p.x + c*p.y, p.z); }

vec3 texcub(vec2 p, float idx) {
    float siz = iResolution.x * 0.007;
    float rz = sin(clamp(max(abs(p.x), abs(p.y)) * 24.0, 0.0, 25.0)) * siz + siz;
    vec3 col = (sin(vec3(4.0, 3.0, 5.0) * idx * 0.9) + 2.0) * (rz + 0.25);
    float a = atan(p.y, p.x + 1e-6);
    col -= sin(a * 15.0 + iTime * 11.0) * 0.15 - 0.15;
    return col;
}

vec3 cub(vec3 p) {
    vec3 col = vec3(1.0);
    col = min(texcub(vec2(p.z, p.y) / p.x * 1.01, 15.0), col);
    p = rotz(p, 1.5708);
    col = min(texcub(vec2(p.z, p.y) / p.x * 1.5, 4.0), col);
    p = roty(p, 1.5708);
    col = min(texcub(vec2(p.z, p.y) / p.x * 1.01, 5.0), col);
    return 1.0 - col;
}

// --- Dispatch (replaces the two long if/else chains) ---
vec3 getTiling(float sel, vec3 dir) {
    if (sel < 2.5) return cub(dir) * 1.2;
    return cub(dir) * 1.2;
}

// --- Main ---
void main() {
    vec2 p = vUV * iResolution.xy - 0.5 * iResolution.xy;
    p.x *= iResolution.x / iResolution.y;

    // Camera
    vec3 ro = vec3(0.0, 0.0, 3.5);
    vec3 rd = normalize(vec3(p, -1.4));
    mat2 my = mm2(iTime * 0.27);
    ro.xy *= my; rd.xy *= my;

    // --- Cross-fade phase (replaces hard `sel` snapping) ---
    float phase  = (iTime + 10.0) * 0.2;
    float frac   = fract(phase);
    float selCur = mod(floor(phase), 4.0);
    float selNxt = mod(selCur + 1.0, 4.0);
    float blend  = smoothstep(0.8, 1.0, frac);  // last 20% of each 5s window

    // Base tiling (cross-fade)
    vec3 col = getTiling(selCur, rd);
    if (blend > 0.201) {
        col = mix(col, getTiling(selNxt, rd), blend);
    }

    gl_FragColor = vec4(col, 1.2);
}