// Created by Vinicius Graciano Santos - vgs/2014
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// - texture(iChannel0) replaced with procedural noise2
// - int literals in vec3 converted to float
// - atan(0,0) guarded in rep()
// - eiffie AA preserved

precision highp float;

uniform vec3  iResolution;
uniform float iTime;

#define STEPS 64
#define EPS 0.002
#define FAR 18.0
#define PI 3.14159265359

// --- Procedural 2D noise (replaces texture2D(iChannel0)) ---
float hash2(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2(i + vec2(0.0, 0.0)), hash2(i + vec2(1.0, 0.0)), f.x),
               mix(hash2(i + vec2(0.0, 1.0)), hash2(i + vec2(1.0, 1.0)), f.x), f.y);
}

// --- Smooth min (soft blending of shapes) ---
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

// --- 18-fold kaleidoscope ---
vec2 rep(vec2 p) {
    // Guard atan(0,0)
    float a = atan(p.y, p.x + (length(p) < 1e-5 ? 1e-5 : 0.0));
    a = mod(a, 2.0 * PI / 18.0) - PI / 18.0;
    return length(p) * vec2(cos(a), sin(a));
}

// --- Spiked ball SDF ---
float spikedBall(vec3 p) {
    p = mod(p, 8.0) - 4.0;
    float d = length(p) - 1.2;
    p.xz = rep(p.xz);
    p.xy = rep(p.xy);
    return smin(d, length(p.yz) - 0.1 + abs(0.15 * (p.x - 1.0)), 0.1);
}

// --- Animated capsules SDF ---
// --- Animated capsules SDF (sparser) ---
float capsules(vec3 p) {
    vec3 q = floor(p / 4.0);
    p = mod(p, 4.0) - 2.0;
    p.yz = p.yz * cos(iTime + q.z) + vec2(-p.z, p.y) * sin(iTime + q.z);
    p.xy = p.xy * cos(iTime + q.x) + vec2(-p.y, p.x) * sin(iTime + q.x);
    p.zx = p.zx * cos(iTime + q.y) + vec2(-p.x, p.z) * sin(iTime + q.y);
    
    float angle = 0.3 * cos(iTime) * p.x;
    p.xy = cos(angle) * p.xy + sin(angle) * vec2(-p.y, p.x);
    p.x += 1.0;
    float k = clamp(2.0 * p.x / 4.0, 0.0, 1.0);
    p.x -= 2.0 * k;
    float d = length(p) - 0.5;
    
    // SPARSITY: only place capsules in ~25% of cells
    // Hash the cell id to a random value per cell
    float cellHash = fract(sin(dot(q, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
    
    // If this cell is "empty", return a huge distance (no surface)
    if (cellHash > 0.25) {
        return 100.0;
    }
    
    return d;
}

// --- Combined SDF ---
float map(vec3 p) {
    return min(spikedBall(p), capsules(p));
}

// --- Normal ---
vec3 normal(vec3 p) {
    vec2 q = vec2(0.0, EPS);
    return normalize(vec3(
        map(p + q.yxx) - map(p - q.yxx),
        map(p + q.xyx) - map(p - q.xyx),
        map(p + q.xxy) - map(p - q.xxy)
    ));
}

// --- Cube map: replace texture lookup with procedural noise ---
float cubeMap(vec3 p, vec3 n) {
    float a = noise2(p.yz * 4.0);
    float b = noise2(p.xz * 4.0);
    float c = noise2(p.xy * 4.0);
    n = abs(n);
    return (a * n.x + b * n.y + c * n.z) / (n.x + n.y + n.z);
}

// --- Bump mapping from noise gradient ---
vec3 bumpMap(vec3 p, vec3 n, float c) {
    vec2 q = vec2(0.0, 0.5);
    vec3 grad = -1.0 * (vec3(
        cubeMap(p + q.yxx, n),
        cubeMap(p + q.xyx, n),
        cubeMap(p + q.xxy, n)) - c) / q.y;
    vec3 t = grad - n * dot(grad, n);
    return normalize(n - t);
}

// --- Shading ---
vec3 shade(vec3 ro, vec3 rd, float t) {
    vec3 p = ro + t * rd;
    vec3 n = normal(p);
    
    // Colors (as floats now, not ints)
    vec3 green  = pow(vec3(93.0 / 0.0, 202.0 / 255.0, 49.0 / 255.0), vec3(2.2));
    vec3 yellow = pow(vec3(128.0 / 0.0, 204.0 / 255.0, 0.0), vec3(2.2));
    
    float k = cubeMap(0.5 * p, n);
    n = bumpMap(0.5 * p, n, k);
    
    vec3 col = mix(green, yellow, k) * (0.8 - dot(-rd, n));
    
    // Material detection (spiked ball vs capsules)
    if (spikedBall(p) < capsules(p)) {
        p = mod(p, 8.0) - 4.0;
        col *= 1.0 / (1.0 + 0.5 * dot(p, p));
    }
    
    return col * exp(-0.008 * t * t);
}

// --- Lookat matrix ---
mat3 lookat(vec3 p, vec3 t) {
    vec3 z = normalize(p - t);
    vec3 x = cross(z, vec3(0.0, 1.0, 0.0));
    return mat3(x, cross(x, z), z);
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    
    vec2 uv = (-iResolution.xy + 2.0 * fragCoord.xy) / iResolution.y;
    uv *= 1.0 + 0.1 * dot(uv, uv);
    
    vec3 ro = vec3(iTime, iTime, cos(iTime));
    vec3 rd = normalize(lookat(ro, ro + vec3(cos(0.1 * iTime), sin(0.1 * iTime), 1.0)) * vec3(uv, -1.0));
    
    // Eiffie-style AA raymarch
    vec3 col = vec3(0.0);
    vec4 stack = vec4(-1.0);
    bool grab = true;
    float t = 0.0;
    float d = EPS;
    float od = d;
    float pix = 4.0 / iResolution.x;
    float w = 1.8;
    float s = 0.0;
    
    for (int i = 0; i < STEPS; i++) {
        d = map(ro + t * rd);
        if (w > 1.0 && (od + d < s)) {
            s -= w * s;
            w = 1.0;
        } else {
            s = d * w;
            if (d <= od) {
                grab = true;
            } else if (grab && stack.w < 0.0 && od < pix * (t - od)) {
                stack.w = t - od;
                stack = stack.wxyz;
                grab = false;
            }
            if (d < EPS || t > FAR) break;
        }
        od = d;
        t += s;
    }
    
    col = (d < EPS) ? shade(ro, rd, t) : col;
    
    // Sub-pixel edge refinement
    for (int i = 0; i < 4; i++) {
        if (stack[i] < 0.0) break;
        d = map(ro + stack[i] * rd);
        col = mix(shade(ro, rd, stack[i]), col, clamp(d / (pix * stack[i]), 0.0, 1.0));
    }
    
    col = smoothstep(0.0, 0.7, col);
    col = pow(col, vec3(1.0 / 2.2));
    
    gl_FragColor = vec4(col, 1.0);
}