// Abstract Glassy Field
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// - texture() replaced with triplanar procedural noise
// - accum global removed (returned from trace)
// - float loop counters -> int
// - iteration counts reduced for Switch

precision highp float;

uniform vec3  iResolution;
uniform float iTime;

#define FAR 50.0

// --- Procedural 3D noise ---
float hash3(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 14.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise3(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(mix(hash3(p + vec3(0,0,0)), hash3(p + vec3(1,0,0)), f.x),
            mix(hash3(p + vec3(0,1,0)), hash3(p + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash3(p + vec3(0,0,1)), hash3(p + vec3(1,0,1)), f.x),
            mix(hash3(p + vec3(0,1,1)), hash3(p + vec3(1,1,1)), f.x), f.y),
        f.z);
}

// --- 2x2 rotation (Fabrice's trick) ---
mat2 rot2(float a) {
    vec2 v = sin(vec2(1.570796, 0.0) - a);
    return mat2(v, -v.y, v.x);
}

// --- Triplanar grayscale noise (for bump mapping) ---
vec3 tpl(vec3 p, vec3 n, float scale) {
    n = max(abs(n) - 0.2, 0.001);
    n /= dot(n, vec3(1.0));
    float vx = noise3(vec3(p.zy * scale, 1.0));   // yz plane
    float vy = noise3(vec3(p.xz * scale, 2.0));   // xz plane
    float vz = noise3(vec3(p.xy * scale, 3.0));   // xy plane
    return vec3(vx*vx*n.x + vy*vy*n.y + vz*vz*n.z);
}

// --- Triplanar colored noise (for environment) ---
vec3 envTpl(vec3 p, vec3 n, float scale) {
    n = max(abs(n) - 0.2, vec3(0.001));
    n /= dot(n, vec3(1.0));
    float vx = noise3(vec3(p.zy * scale, 1.0));
    float vy = noise3(vec3(p.xz * scale, 2.0));
    float vz = noise3(vec3(p.xy * scale, 3.0));
    float v = vx*vx*n.x + vy*vy*n.y + vz*vz*n.z;
    
    // Muted palette: deep blue (cool) to warm amber (warm)
    vec3 cool = vec3(0.98, 0.60, 0.15);
    vec3 warm = vec3(0.85, 0.55, 0.1);
    return mix(cool, warm, v);
}
// --- Camera path ---
vec3 camPath(float t) {
    float a = sin(t * 0.188);
    float b = cos(t * 0.14);
    float c = cos(t * 0.2);
    return vec3(a*c - b*1.0, b*1.0 + a*1.5, t);
}

// --- SDF ---
float map(vec3 p) {
    p.xy -= camPath(p.z).xy;
    const float TAU = 6.2831853;
    p = cos(mod(p * 0.315 * 1.25 + sin(mod(p.zxy * 0.875 * 1.25, TAU)), TAU));
    return (length(p) - 1.025) * 1.33;
}

// --- Ambient occlusion ---
float cao(in vec3 p, in vec3 n) {
    float sca = 1.0, occ = 0.0;
    for (int i = 0; i < 5; i++) {
        float fi = float(i);
        float hr = 0.01 + fi * 0.35 / 4.0;
        float dd = map(n * hr + p);
        occ += (hr - dd) * sca;
        sca *= 0.7;
    }
    return clamp(1.0 - occ, 0.0, 1.0);
}

// --- Normal ---
vec3 nr(vec3 p) {
    const vec2 e = vec2(0.002, 0.0);
    return normalize(vec3(
        map(p + e.xyy) - map(p - e.xyy),
        map(p + e.yxy) - map(p - e.yxy),
        map(p + e.yyx) - map(p - e.yyx)
    ));
}

// --- Raymarch (returns t and accum) ---
vec2 trace(in vec3 ro, in vec3 rd) {
    float accum = 0.0;
    float t = 0.0, h;
    for (int i = 0; i < 96; i++) {
        h = map(ro + rd * t);
        if (abs(h) < 0.001 * (t * 0.25 + 1.0) || t > FAR) break;
        t += h;
        if (abs(h) < 0.35) accum += (0.35 - abs(h)) / 24.0;
    }
    return vec2(min(t, FAR), accum);
}

// --- Soft shadow ---
float sha(in vec3 ro, in vec3 rd, in float start, in float end, in float k) {
    float shade = 1.0;
    const int maxIterationsShad = 12;
    float dist = start;
    float stepDist = end / float(maxIterationsShad);
    for (int i = 0; i < maxIterationsShad; i++) {
        float h = map(ro + rd * dist);
        shade = min(shade, smoothstep(0.0, 1.0, k * h / dist));
        dist += clamp(h, 0.01, 0.2);
        if (abs(h) < 0.001 || dist > end) break;
    }
    return min(max(shade, 0.0) + 0.4, 1.0);
}

// --- Bump mapping ---
vec3 db(in vec3 p, in vec3 n, float bf) {
    const vec2 e = vec2(0.001, 0.0);
    const float scale = 3.0;
    mat3 m = mat3(
        tpl(p - e.xyy, n, scale),
        tpl(p - e.yxy, n, scale),
        tpl(p - e.yyx, n, scale)
    );
    vec3 gw = vec3(0.299, 0.587, 0.114);
    vec3 g = gw * m;
    float center = dot(tpl(p, n, scale), gw);
    g = (g - vec3(center)) / e.x;
    g -= n * dot(n, g);
    return normalize(n + g * bf);
}

// --- 3D value noise (for purple electric charge) ---
float n3D(vec3 p) {
    const vec3 s = vec3(7.0, 157.0, 113.0);
    vec3 ip = floor(p);
    p -= ip;
    vec4 h = vec4(0.0, s.yz, s.y + s.z) + dot(ip, s);
    p = p * p * (3.0 - 2.0 * p);
    h = mix(fract(sin(mod(h, 6.231589)) * 43758.5453),
            fract(sin(mod(h + s.x, 6.231589)) * 43758.5453), p.x);
    h.xy = mix(h.xz, h.yw, p.y);
    return mix(h.x, h.y, p.z);
}

// --- Environment mapping ---
vec3 envMap(vec3 rd, vec3 n) {
    vec3 col = envTpl(rd * 4.0, n, 1.0);
    return smoothstep(0.0, 1.0, col);
}

// --- Main ---
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    
    vec2 u = (fragCoord - iResolution.xy * 0.5) / iResolution.y;
    
    float speed = 4.0;
    vec3 o = camPath(iTime * speed);
    vec3 lk = camPath(iTime * speed + 0.25);
    vec3 l = camPath(iTime * speed + 2.0) + vec3(0.0, 1.0, 0.0);
    
    float FOV = 3.14159 / 2.0;
    vec3 fwd = normalize(lk - o);
    vec3 rgt = normalize(vec3(fwd.z, 0.0, -fwd.x));
    vec3 up = cross(fwd, rgt);
    
    // Lens distortion
    vec3 r = fwd + FOV * (u.x * rgt + u.y * up);
    r = normalize(vec3(r.xy, (r.z - length(r.xy) * 0.125)));
    
    vec2 tr = trace(o, r);
    float t = tr.x;
    float accum = tr.y;
    
    vec3 col = vec3(0.0);
    
    if (t < FAR) {
        vec3 p = o + r * t;
        vec3 n = nr(p);
        vec3 svn = n;
        
        float sz = 1.0 / 3.0;
        n = db(p * sz, n, 0.1 / (1.0 + t * 0.25 / FAR));
        
        l -= p;
        float d = max(length(l), 0.001);
        l /= d;
        
        float at = 1.0 / (1.0 + d * 0.05 + d * d * 0.0125);
        
        float ao = cao(p, n);
        float sh = sha(p, l, 0.04, d, 16.0);
        
        float di = max(dot(l, n), 0.0);
        float sp = pow(max(dot(reflect(r, n), l), 0.0), 64.0);
        float fr = clamp(1.0 + dot(r, n), 0.0, 1.0);
        
        // Dark glass body
        vec3 tx = vec3(0.05);
        
        col = tx * (di * 0.1 + ao * 0.25)
            + vec3(0.5, 0.7, 1.0) * sp * 2.0
            + vec3(1.0, 0.7, 0.4) * pow(fr, 8.0) * 0.25;
        
        // Fake reflection and refraction
        vec3 blendedNormal = svn * 0.5 + n * 0.5;
        vec3 refl = envMap(normalize(reflect(r, blendedNormal)), blendedNormal);
        vec3 refr = envMap(normalize(refract(r, blendedNormal, 1.0 / 1.35)), blendedNormal);
        
        vec3 refCol = mix(refr, refl, pow(fr, 5.0));
        col += refCol * ((di * di * 0.25 + 0.75) + ao * 0.25) * 1.5;
        
        // Beer's law fake
        col = mix(col.xzy, col, di * 0.85 + 0.15);
        
        // Glow from accumulated distance
        vec3 accCol = vec3(1.0, 0.3, 0.1) * accum;
        vec3 gc = pow(min(vec3(1.5, 1.0, 1.0) * accum, 1.0), vec3(1.0, 2.5, 12.0)) * 0.5 + accCol * 0.5;
        col += col * gc * 8.0;
        
        // Purple electric charge
        float hi = abs(mod(t / 1.0 + iTime / 3.0, 8.0) - 8.0 / 2.0) * 2.0;
        vec3 cCol = vec3(0.01, 0.05, 1.0) * col / (0.001 + hi * hi * 0.2);
        col += mix(cCol.yxz, cCol, n3D(p * 3.0));
        
        col *= ao * sh * at;
    }
    
    // Atmospheric fog
    vec3 fog = vec3(0.125, 0.04, 0.05) * (r.y * 0.5 + 0.5);
    col = mix(col, fog, smoothstep(0.0, 0.95, t / FAR));
    
    // Vignette
    u = fragCoord / iResolution.xy;
    col = mix(vec3(0.0), col, pow(16.0 * u.x * u.y * (1.0 - u.x) * (1.0 - u.y), 0.125) * 0.5 + 0.5);
    
    gl_FragColor = vec4(sqrt(clamp(col, 0.0, 1.0)), 1.0);
}