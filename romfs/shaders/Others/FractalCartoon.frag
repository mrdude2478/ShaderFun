// "Fractal Cartoon" - former "DE edge detection" by Kali
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// Nyan Cat removed (requires external image).
// Sky, Sun, and Edge detection preserved.

precision highp float;

uniform vec3  iResolution;
uniform float iTime;

#define RAY_STEPS 100
#define BRIGHTNESS 1.2
#define GAMMA 1.4
#define SATURATION 0.65
#define detail 0.001

const vec3 origin = vec3(-1.0, 0.7, 0.0);

// 2D rotation function
mat2 rot(float a) {
    return mat2(cos(a), sin(a), -sin(a), cos(a));	
}

// "Amazing Surface" fractal
vec4 formula(vec4 p) {
    p.xz = abs(p.xz + 1.0) - abs(p.xz - 1.0) - p.xz;
    p.y -= 0.25;
    p.xy *= rot(0.610865); // 35 degrees in radians
    p = p * 2.0 / clamp(dot(p.xyz, p.xyz), 0.2, 1.0);
    return p;
}

// Distance function
float de(vec3 pos) {
    float t = iTime * 0.5;
    // Waves
    pos.y += sin(pos.z - t * 6.0) * 0.15;
    
    vec3 tpos = pos;
    tpos.z = abs(3.0 - mod(tpos.z, 6.0));
    vec4 p = vec4(tpos, 1.0);
    
    for (int i = 0; i < 4; i++) {
        p = formula(p);
    }
    
    float fr = (length(max(vec2(0.0), p.yz - 1.5)) - 1.0) / p.w;
    float ro = max(abs(pos.x + 1.0) - 0.3, pos.y - 0.35);
    ro = max(ro, -max(abs(pos.x + 1.0) - 0.1, pos.y - 0.5));
    pos.z = abs(0.25 - mod(pos.z, 0.5));
    ro = max(ro, -max(abs(pos.z) - 0.2, pos.y - 0.3));
    ro = max(ro, -max(abs(pos.z) - 0.01, -pos.y + 0.32));
    
    float d = min(fr, ro);
    return d;
}

// Camera path
vec3 path(float ti) {
    ti *= 1.5;
    vec3 p = vec3(sin(ti), (1.0 - sin(ti * 2.0)) * 0.5, -ti * 5.0) * 0.5;
    return p;
}

// Global edge variable (replaces the original global)
float edge = 0.0;

// Calc normals, and edge detection
vec3 normal(vec3 p) { 
    vec3 e = vec3(0.0, detail * 5.0, 0.0);

    float d1 = de(p - e.yxx), d2 = de(p + e.yxx);
    float d3 = de(p - e.xyx), d4 = de(p + e.xyx);
    float d5 = de(p - e.xxy), d6 = de(p + e.xxy);
    float d = de(p);
    
    edge = abs(d - 0.5 * (d2 + d1)) + abs(d - 0.5 * (d4 + d3)) + abs(d - 0.5 * (d6 + d5));
    edge = min(1.0, pow(edge, 0.55) * 15.0);
    
    return normalize(vec3(d1 - d2, d3 - d4, d5 - d6));
}

// Raymarching and 2D graphics
vec3 raymarch(in vec3 from, in vec3 dir) {
    edge = 0.0;
    vec3 p;
    vec3 norm;
    float d = 100.0;
    float totdist = 0.0;
    float det = 0.0;
    
    for (int i = 0; i < RAY_STEPS; i++) {
        if (d > det && totdist < 25.0) {
            p = from + totdist * dir;
            d = de(p);
            det = detail * exp(0.13 * totdist);
            totdist += d; 
        }
    }
    
    vec3 col = vec3(0.0);
    p -= (det - d) * dir;
    norm = normal(p);
    
    // Set normal as color with dark edges
    col = (1.0 - abs(norm)) * max(0.0, 1.0 - edge * 0.8);
    
    totdist = clamp(totdist, 0.0, 26.0);
    dir.y -= 0.02;
    
    // Responsive sun size (using waveform for audio reactivity)
    float sunsize = 7.0; // - max(0.0, texture2D(iChannel0, vec2(0.6, 0.2)).x) * 5.0;
    
    // Angle for drawing and rotating sun
    float an = atan(dir.x, dir.y) + iTime * 1.5;
    
    // Sun and sky
    float s = pow(clamp(1.0 - length(dir.xy) * sunsize - abs(0.2 - mod(an, 0.4)), 0.0, 1.0), 0.1);
    float sb = pow(clamp(1.0 - length(dir.xy) * (sunsize - 0.2) - abs(0.2 - mod(an, 0.4)), 0.0, 1.0), 0.1);
    float sg = pow(clamp(1.0 - length(dir.xy) * (sunsize - 4.5) - 0.5 * abs(0.2 - mod(an, 0.4)), 0.0, 1.0), 3.0);
    
    float y = mix(0.45, 1.2, pow(smoothstep(0.0, 1.0, 0.75 - dir.y), 2.0)) * (1.0 - sb * 0.5);
    
    // Set up background with sky and sun
    vec3 backg = vec3(0.5, 0.0, 1.0) * ((1.0 - s) * (1.0 - sg) * y + (1.0 - sb) * sg * vec3(1.0, 0.8, 0.15) * 3.0);
    backg += vec3(1.0, 0.9, 0.1) * s;
    backg = max(backg, sg * vec3(1.0, 0.9, 0.5));
    
    // Distant fading to sun color
    col = mix(vec3(1.0, 0.9, 0.3), col, exp(-0.004 * totdist * totdist));
    
    if (totdist > 25.0) col = backg; // hit background
    
    // Tone mapping
    col = pow(col, vec3(GAMMA)) * BRIGHTNESS;
    col = mix(vec3(length(col)), col, SATURATION);
    col *= vec3(1.0, 0.9, 0.85);
    
    return col;
}

// Get camera position
vec3 move(inout vec3 dir) {
    float t = iTime * 0.5;
    vec3 go = path(t);
    vec3 adv = path(t + 0.7);
    vec3 advec = normalize(adv - go);
    
    float an = adv.x - go.x;
    an *= min(1.0, abs(adv.z - go.z)) * sign(adv.z - go.z) * 0.7;
    dir.xy *= rot(an);
    
    an = advec.y * 1.7;
    dir.yz *= rot(an);
    
    an = atan(advec.x, advec.z);
    dir.xz *= rot(an);
    
    return go;
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord.xy / iResolution.xy * 2.0 - 1.0;
    vec2 oriuv = uv;
    uv.y *= iResolution.y / iResolution.x;
    
    // Mouse handling (defaults to looking forward)
    vec2 mouse = vec2(0.0, -0.05);
    
    float fov = 0.9 - max(0.0, 0.7 - iTime * 0.3);
    vec3 dir = normalize(vec3(uv * fov, 1.0));
    dir.yz *= rot(mouse.y);
    dir.xz *= rot(mouse.x);
    
    vec3 from = origin + move(dir);
    vec3 color = raymarch(from, dir); 
    
    // Border vignette
    //color = mix(vec3(0.0), color, pow(max(0.0, 0.95 - length(oriuv * oriuv * oriuv * vec2(1.05, 1.1))), 0.3));
    
    gl_FragColor = vec4(color, 1.0);
}