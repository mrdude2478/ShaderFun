// srtuss, 2013
// "Endless Sci-Fi Tunnel" - modified for continuous smooth flight.
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// - Removed camera switching (single seamless camera)
// - Removed white flash blend
// - Added smooth banking, sway, and speed pulse
// - Reduced raymarch to 64 steps for Switch performance

precision highp float;

uniform vec3  iResolution;
uniform float iTime;

#define PI 3.14159265358979323

vec2 rotate(vec2 p, float a) {
    return vec2(p.x * cos(a) - p.y * sin(a), p.x * sin(a) + p.y * cos(a));
}

// --- Procedural 3D noise (replaces textureLod) ---
float hash3(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise3d(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    
    float n000 = hash3(p + vec3(0.0, 0.0, 0.0));
    float n100 = hash3(p + vec3(1.0, 0.0, 0.0));
    float n010 = hash3(p + vec3(0.0, 1.0, 0.0));
    float n110 = hash3(p + vec3(1.0, 1.0, 0.0));
    float n001 = hash3(p + vec3(0.0, 0.0, 1.0));
    float n101 = hash3(p + vec3(1.0, 0.0, 1.0));
    float n011 = hash3(p + vec3(0.0, 1.0, 1.0));
    float n111 = hash3(p + vec3(1.0, 1.0, 1.0));
    
    return mix(
        mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
        mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y),
        f.z
    );
}

float fbm3(vec3 p) {
    return noise3d(p) * 0.5 + noise3d(p * 2.02) * 0.25 + noise3d(p * 4.01) * 0.125;
}

float hash(float x) {
    return fract(484.982 * sin(x)) * 2.0 - 1.0;
}

float noise(float x) {
    float fl = floor(x);
    return mix(hash(fl), hash(fl + 1.0), smoothstep(0.0, 1.0, fract(x)));
}

float fbm(float x) {
    return noise(x) * 0.5 + noise(x * 2.2) * 0.1 + noise(x * 4.1) * 0.005;
}

// animated 3d fbm (time passed in)
float fbm3a(vec3 p, float time) {
    vec2 t = vec2(time * 0.4, 0.0);
    return noise3d(p + t.xyy) * 0.5 + noise3d(p * 2.02 - t.xyy) * 0.25 + noise3d(p * 4.01 + t.yxy) * 0.125;
}

float fbm3a_(vec3 p, float time) {
    vec2 t = vec2(time * 0.4, 0.0);
    return noise3d(p + t.xyy) * 0.5 + noise3d(p * 2.02 - t.xyy) * 0.25 + noise3d(p * 4.01 + t.yxy) * 0.125 + noise3d(p * 8.03 + t.yxy) * 0.0625;
}

vec2 rand2(in vec2 p) {
    return fract(vec2(sin(p.x * 591.32 + p.y * 154.077), cos(p.x * 391.32 + p.y * 49.077)));
}

float voronoi(in vec2 x) {
    vec2 p = floor(x);
    vec2 f = fract(x);
    vec2 res = vec2(8.0);
    for(int j = -1; j <= 1; j++) {
        for(int i = -1; i <= 1; i++) {
            vec2 b = vec2(float(i), float(j));
            vec2 r = b - f + rand2(p + b);
            float d = max(abs(r.x), abs(r.y));
            if(d < res.x) {
                res.y = res.x;
                res.x = d;
            } else if(d < res.y) {
                res.y = d;
            }
        }
    }
    return res.y - res.x;
}

// tunnel path
vec2 path(float z) {
    return vec2(sin(z * 0.1333) * 2.0, cos(z * 0.2) * 2.0);
}

// rectangular tech detail
void technify(vec3 p, inout float d1) {
    d1 *= 0.3;
    p *= 0.3;

    vec4 h = vec4(0.0);
    
    float p1 = 0.06;
    float p2 = 0.0;
    
    for(float i = 0.0; i < 4.0; i++) {
        vec3 pp = p;
        vec3 q = 1.0 + i * i * 0.18 * (1.0 + 4.0 * (1.0 + 0.3 * p2) * sin(vec3(5.7, 6.4, 7.3) * i * 1.145 + 0.3 * sin(h.w * 0.015) * (3.0 + i)));
        vec3 g = (fract(pp * q) - 0.5) / q;
        
        d1 = min(d1 + 0.03 + p1, max(d1, max(abs(g.x), max(abs(g.y), abs(g.z))) - 0.148));
    }
    
    d1 = d1 / 0.28;
}

float scene(vec3 p) {
    // NOTE: We now use iTime directly, no warped "time" variable,
    // so there are no speed jumps — just a smooth, constant flow.
    float t = iTime;
    
    float v;
    p.xy += path(p.z);
    
    // Slightly slower rotation so the tunnel feels like a steady drift
    p.xy = rotate(p.xy, t * 0.4);
    
    float s = sin(p.z * 0.4) * 0.5;
    float l = length(p.xy) + s;
    
    v = max(3.0 - l, l - 5.0);
    
    technify(p, v);
    
    return v;
}

vec3 normal(vec3 p) {
    float c = scene(p);
    vec2 h = vec2(0.01, 0.0);
    vec3 nml;
    nml.x = scene(p + h.xyy) - c;
    nml.y = scene(p + h.yxy) - c;
    nml.z = scene(p + h.yyx) - c;
    return normalize(nml);
}

vec3 sky(vec3 p, float time) {
    vec3 col;
    float v = 1.0 - abs(fbm3a(p * 4.0, time) * 2.0 - 1.0);
    float n = fbm3a_(p * 7.0 - 104.042, time);
    v = mix(v, pow(n, 0.3), 0.5);
    col = vec3(pow(v, 13.0), pow(v, 7.0), pow(v, 6.0)) * 0.8;
    return col;
}

vec3 shade(vec3 p, vec3 dir, vec3 nml, vec3 ref, float dist, float time) {
    vec3 col;
    
    vec3 sun = normalize(vec3(0.2, 1.0, 0.3));
    float diff = dot(nml, sun) * 0.5 + 0.5;
    float spec = max(dot(ref, sun), 0.0);
    spec = pow(spec, 32.0);
    col = vec3(0.25, 0.4, 0.5) * diff + spec;
    
    vec3 q = p;
    q.xy += path(p.z);
    float a = atan(q.x, q.y) - time * 0.4;
    float l = q.z;
    
    float li = smoothstep(0.4, 1.0, sin(p.z * 0.2 + time * 3.0));
    float vo = smoothstep(0.90, 1.0, 1.0 - voronoi(1.5 * vec2(a * 3.0, l)));
    
    col = mix(mix(col, vec3(0.5, 0.8, 1.0) * 0.2, vo), col + vec3(vo) * 0.6, li);
    col = mix(col, sky(ref, time), 0.3);
    
    return col;
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    
    // Smooth, unbounded time (no warp/segments)
    float time = iTime;
    
    vec2 uv = fragCoord.xy / iResolution.xy;
    uv = uv * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;
    
        // --- CAMERA: travelling down the tunnel ---
    
    // Forward speed (you can adjust the 0.4 if you want it a bit faster/slower)
    float speed = 2.6;
    
    // The camera's current distance along the tunnel
    float camZ = time * speed;
    
    // 1. Position: Follow the tunnel's winding path perfectly
    vec3 eye = vec3(0.0, 0.0, camZ);
    eye.xy -= path(camZ); 
    
    // 2. Direction: Look ahead down the tunnel
    // We get the point 2 units ahead on the path and look at it
    vec3 target = vec3(0.0, 0.0, camZ + 2.0);
    target.xy -= path(camZ + 2.0);
    
    vec3 forward = normalize(target - eye);
    
    // 3. Build the camera matrix
    vec3 right = normalize(vec3(forward.z, 0.0, -forward.x));
    vec3 up = cross(forward, right);
    
    // 4. Create the ray direction from the screen UVs
    vec3 dir = normalize(forward + uv.x * right + uv.y * up);
    
    // 5. Add a subtle "rollercoaster" banking effect based on the path curve
    float curve = path(camZ + 0.5).x - path(camZ - 0.5).x;
    float bank = -curve * 0.5; 
    
    // Apply the bank rotation to the ray direction
    dir.xz = rotate(dir.xz, bank);
    
    // --- RAYMARCH ---
    vec3 col = sky(dir, time);
    
    float d = 0.0;
    for(int i = 0; i < 64; i++) {
        d += scene(eye + dir * d) * 0.7;
    }
    vec3 ray = eye + dir * d;
    
    d = distance(eye, ray);
    d = clamp(d, 0.0, 100.0);
    
    if(d < 100.0) {
        vec3 nml = normal(ray);
        col = shade(ray, dir, nml, reflect(dir, nml), d, time);
    }
    
    // Dramatize colors
    col = pow(col, vec3(1.5)) * 2.0;
    
    // Vignette
    vec2 q = fragCoord.xy / iResolution.xy;
    col *= 0.2 + 0.8 * pow(16.0 * q.x * q.y * (1.0 - q.x) * (1.0 - q.y), 0.1);
    
    gl_FragColor = vec4(col, 1.0);
}