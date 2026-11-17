// Companion Cube Remix - @christinacoffin (c) All Rights Reserved.
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// - Removed iMouse dependency (hardcoded default)
// - sdfsw passed as parameter instead of global
// - All ES 2.0 compatibility fixes

precision highp float;

uniform vec3  iResolution;
uniform float iTime;

#define PARALLAX_SCALE 0.2
#define USE_OFFSET_SCALING
#define OFFSET_SCALE 4.0
#define BUMP_STRENGTH 0.21
#define BUMP_WIDTH 0.004

const float texscl = 2.5;

#define ITR 70
#define FAR 15.0
#define time iTime

// --- Helpers ---
mat2 mm2(in float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

float hash(vec2 x) {
    return fract(cos(dot(x.xy, vec2(2.31, 53.21)) * 124.123) * 412.0);
}

// length4 (4-norm), used for the "squircle" companion cube shape
float length4(in vec3 p) {
    p = p * p;
    p = p * p;
    return pow(p.x + p.y + p.z, 1.0 / 4.0);
}

// --- SDF (sdfsw is now a parameter) ---
float map(vec3 p, float sdfsw) {
    float d = mix(length(p) - 1.1, length4(p) - 1.0, sdfsw - 0.3);
    d = min(d, -(length4(p) - 4.0));
    return d * 0.95;
}

// --- March ---
float march(in vec3 ro, in vec3 rd, float sdfsw) {
    float precis = 0.001;
    float h = precis * 2.0;
    float d = 0.0;
    for (int i = 0; i < ITR; i++) {
        if (abs(h) < precis || d > FAR) break;
        d += h;
        float res = map(ro + rd * d, sdfsw);
        h = res;
    }
    return d;
}

// --- Normal ---
vec3 normal(in vec3 p, float sdfsw) {
    vec2 e = vec2(-1.0, 1.0) * 0.005;
    return normalize(
        e.yxx * map(p + e.yxx, sdfsw) +
        e.xxy * map(p + e.xxy, sdfsw) +
        e.xyx * map(p + e.xyx, sdfsw) +
        e.yyy * map(p + e.yyy, sdfsw)
    );
}

// --- Cells pattern (from TekF) ---
float cells(in vec3 p) {
    p = fract(p / 2.0) * 2.0;
    p = min(p, 2.0 - p);
    return 1.0 - min(length(p), length(p - 1.0));
}

// --- Heart mapping (from iq) ---
vec4 heartMapping(in vec2 p) {
    p.y -= 0.25;
    
    vec3 bcol = vec3(0.0);  // black background (so only the heart shows)
    
    float tt = mod(iTime, 1.5) / 1.5;
    float ss = pow(tt, 0.2) * 0.5 + 0.5;
    ss = 1.0 + ss * 0.5 * sin(tt * 6.2831 * 3.0 + p.y * 0.5) * exp(-tt * 4.0);
    p *= vec2(0.5, 1.5) + ss * vec2(0.5, -0.5);
    
    float a = atan(p.x, p.y) / 3.141593;
    float r = length(p);
    float h = abs(a);
    float d = (13.0 * h - 22.0 * h * h + 10.0 * h * h * h) / (6.0 - 5.0 * h);
    
    float s = 0.5;
    s = 0.5 + 0.6 * s;
    s *= 0.5 + 0.5 * pow(1.0 - clamp(r / d, 0.0, 1.0), 0.1);
    
    vec3 hcol = vec3(1.0, 0.0, 0.3) * s;
    
    vec3 col = mix(bcol, hcol, smoothstep(-0.01, 0.01, d - r));
    
    col.x *= ss;
    col.y = ss;
    
    return vec4(col, 1.0);
}

// --- Main texture function (triplanar heart + cells) ---
float tex(vec3 p) {
    p *= texscl;
    float rz = 0.0;
    float z = 1.0;
    
    // Triplanar heart mapping
    float heartScaleFactor = 1.1;
    vec4 heart0 = heartMapping(p.xy * heartScaleFactor);
    vec4 heart1 = heartMapping(p.zy * heartScaleFactor);
    vec4 heart2 = heartMapping(p.xz * heartScaleFactor);
    
    heart0 = max(heart0, heart1);
    heart0 = max(heart0, heart2);
    float heartMask = (heart0.x + heart1.x + heart2.x * 0.333);
    heartMask = clamp(heartMask, 0.0, 1.0);
    
    // Cells pattern
    for (int i = 0; i < 2; i++) {
        rz += cells(p) / z;
        p *= 2.5 * 0.15;
        z *= -1.1 * 0.15;
    }
    
    return clamp(heartMask + (rz * rz) * 4.95, 0.0, 1.0) * 2.73 - 1.0 - heartMask;
}

// --- Parallax position ---
vec3 prlpos(in vec3 p, in vec3 n, in vec3 rd) {
    vec3 tgt = n * dot(rd, n) - rd;
    tgt /= (abs(dot(tgt, rd))) + OFFSET_SCALE;
    p += tgt * tex(p) * PARALLAX_SCALE;
    return p;
}

// --- Bump texture ---
float btex(in vec3 p) {
    float rz = tex(p);
    rz += tex(p * 20.0) * 0.01;
    return rz;
}

// --- Bump mapping ---
vec3 bump(in vec3 p, in vec3 n, in float ds) {
    vec2 e = vec2(BUMP_WIDTH * sqrt(ds) * 0.5, 0.0);
    float n0 = btex(p);
    vec3 d = vec3(
        btex(p + e.xyy) - n0,
        btex(p + e.yxy) - n0,
        btex(p + e.yyx) - n0
    ) / e.x;
    vec3 tgd = d - n * dot(n, d);
    n = normalize(n - tgd * BUMP_STRENGTH * 2.0 / ds);
    return n;
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    
    vec2 bp = fragCoord.xy / iResolution.xy * 2.0 - 1.0;
    vec2 p = bp;
    p.x *= iResolution.x / iResolution.y;
    
    // Hardcoded "mouse" position (no iMouse on ShaderFun)
    // The original defaults to vec2(0.4, -0.25) when iMouse is untouched
    vec2 mo = vec2(0.4, -0.25);
    mo.x *= iResolution.x / iResolution.y;
    
    mo.y = 0.425;
    mo.y += (0.5 * abs(sin(iTime))) - 0.15;
    
    p.x += mo.x;
    float sdfsw = mo.y * 4.0;
    
    vec3 ro = vec3(0.0, 0.0, 4.0);
    vec3 rd = normalize(vec3(p, -3.0 + sin(time * 0.9 + sin(time))));
    
    mat2 mx = mm2(time * 0.1 + sin(time * 0.4) - 0.2);
    mat2 my = mm2(time * 0.07 + cos(time * 0.33) - 0.1);
    ro.xz *= mx; rd.xz *= mx;
    ro.xy *= my; rd.xy *= my;
    
    float rz = march(ro, rd, sdfsw);
    
    vec3 col = vec3(0.0);
    
    if (rz < FAR) {
        vec3 pos = ro + rz * rd;
        vec3 nor = normal(pos, sdfsw);
        pos = prlpos(pos, nor, rd);
        float d = distance(ro, pos);
        nor = bump(pos, nor, d);
        
        vec3 ligt = normalize(vec3(-0.5, 0.5, -0.3));
        float dif = clamp(dot(nor, ligt), 0.0, 1.0);
        float bac = clamp(dot(nor, normalize(vec3(-ligt))), 0.0, 1.0);
        float spe = pow(clamp(dot(reflect(rd, nor), ligt), 0.0, 1.0), 10.0);
        float fre = pow(clamp(1.0 + dot(nor, rd), 0.0, 1.0), 2.0);
        
        vec3 brdf = vec3(0.3);
        brdf += bac * vec3(0.3);
        brdf += dif * 0.5;
        
        float tx = tex(pos);
        col = sin(vec3(1.5 + mo.x * 0.4, 2.2 + mo.x * 0.25, 2.7) + tx * 1.2 + 4.2) * 0.6 + 0.55;
        col = col * brdf + spe * 0.5 / sqrt(rz) + 0.25 * fre;
        
        col = mix(col, vec3(0.0), clamp(exp(rz * 0.43 - 4.0), 0.0, 1.0));
    }
    
    col.x *= col.z;
    col.xyz = col.zxy;
    
    col = clamp(col * 1.05, 0.0, 1.0);
    col *= pow(smoothstep(0.0, 0.2, (bp.x + 1.0) * (bp.y + 1.0) * (bp.x - 1.0) * (bp.y - 1.0)), 0.3);
    col *= smoothstep(3.9, 0.5, sin(p.y * 0.5 * iResolution.y + time * 10.0)) + 0.1;
    col -= hash(col.xy + p.xy) * 0.025;
    
    gl_FragColor = vec4(col, 1.0);
}