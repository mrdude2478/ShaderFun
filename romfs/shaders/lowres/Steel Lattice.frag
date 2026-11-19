// Steel Lattice
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// - texture(iChannel0/1) replaced with procedural noise
// - blackbodyPalette preserved exactly (the soul of the shader)
// - AO/shadow loops converted to int counters
// - raymarch reduced 128 -> 96 for Switch

precision highp float;

uniform vec3  iResolution;
uniform float iTime;

#define FIRE_REFLECTION
#define sEPS 0.005
#define FAR 20.0

// --- Procedural 3D noise (replaces texture) ---
float hash3(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
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

// --- Triplanar noise (replaces tex3D with texture lookups) ---
// We use noise3 sampled at p + direction to simulate surface texture.
// Three samples blended by the normal — same logic as the original tex3D.
vec3 tex3D(in vec3 p, in vec3 n, float scale) {
    n = max((abs(n) - 0.2) * 7.0, vec3(0.001));
    n /= (n.x + n.y + n.z);
    
    // Sample noise along each axis plane (roughly equivalent to texture lookup in each)
    float a = noise3(vec3(p.yz * scale, 1.0));
    float b = noise3(vec3(p.zx * scale, 2.0));
    float c = noise3(vec3(p.xy * scale, 3.0));
    
    float v = a * n.x + b * n.y + c * n.z;
    return vec3(v);
}

// --- Grey scale ---
float getGrey(vec3 p) {
    return p.x * 0.299 + p.y * 0.587 + p.z * 0.114;
}

// --- Smooth min ---
float sminP(float a, float b, float smoothing) {
    float h = clamp(0.5 + 0.5 * (b - a) / smoothing, 0.0, 1.0);
    return mix(b, a, h) - smoothing * h * (1.0 - h);
}

// --- 2D rotation ---
mat2 rot(float th) {
    float cs = cos(th), si = sin(th);
    return mat2(cs, -si, si, cs);
}

// --- Blackbody palette (preserved exactly) ---
// The physics here is real: Planck's law + CIE chromacity + XYZ to RGB.
// This is the single most important function in the shader.
vec3 blackbodyPalette(float t) {
    t *= 4000.0; // Temperature range
    
    float cx = (0.860117757 + 1.54118254e-4 * t + 1.28641212e-7 * t * t)
             / (1.0 + 8.42420235e-4 * t + 7.08145163e-7 * t * t);
    float cy = (0.317398726 + 4.22806245e-5 * t + 4.20481691e-8 * t * t)
             / (1.0 - 2.89741816e-5 * t + 1.61456053e-7 * t * t);
    
    float d = (2.0 * cx - 8.0 * cy + 4.0);
    vec3 XYZ = vec3(3.0 * cx / d, 2.0 * cy / d, 1.0 - (3.0 * cx + 2.0 * cy) / d);
    
    vec3 RGB = mat3(3.240479, -0.969256, 0.055648,
                    -1.537150, 1.875992, -0.204043,
                    -0.498535, 0.041556, 1.057311) * vec3(1.0 / XYZ.y * XYZ.x, 1.0, 1.0 / XYZ.y * XYZ.z);
    
    return max(RGB, 0.0) * pow(t * 0.0004, 4.0);
}

// --- Bump surface ---
float bumpSurf3D(in vec3 p, in vec3 n) {
    p = abs(mod(p, 0.0625) - 0.03125);
    float x = min(p.x, min(p.y, p.z)) / 0.03125;
    
    p = sin(p * 380.0 + sin(p.yzx * 192.0 + 64.0));
    float surfaceNoise = (p.x * p.y * p.z);
    
    return clamp(x + surfaceNoise * 0.05, 0.0, 1.0);
}

// --- Bump mapping ---
vec3 doBumpMap(in vec3 p, in vec3 nor, float bumpfactor) {
    const float eps = 0.001;
    float ref = bumpSurf3D(p, nor);
    vec3 grad = vec3(
        bumpSurf3D(vec3(p.x - eps, p.y, p.z), nor) - ref,
        bumpSurf3D(vec3(p.x, p.y - eps, p.z), nor) - ref,
        bumpSurf3D(vec3(p.x, p.y, p.z - eps), nor) - ref
    ) / eps;
    grad -= nor * dot(nor, grad);
    return normalize(nor + bumpfactor * grad);
}

// --- Lattice SDF ---
float map(vec3 p) {
    // SECTION 1: Lattice of tubes (smooth min combination)
    p = mod(p, 2.0) - 1.0;
    float x1 = sminP(length(p.xy), sminP(length(p.yz), length(p.xz), 0.25), 0.25) - 0.5;
    
    // SECTION 2: Abstract box pattern
    p = abs(mod(p, 0.5) - 0.25);
    float x2 = min(p.x, min(p.y, p.z));
    
    // SECTION 3: Combine
    return sqrt(x1 * x1 + x2 * x2) - 0.05;
}

// --- Raymarch ---
float raymarch(vec3 ro, vec3 rd) {
    float d, t = 0.0;
    for (int i = 0; i < 96; i++) {
        d = map(ro + rd * t);
        if (d < sEPS || t > FAR) break;
        t += d * 0.75;
    }
    return t;
}

// --- Ambient occlusion ---
float calculateAO(vec3 p, vec3 n) {
    const int AO_SAMPLES = 4;
    float r = 0.0;
    float w = 1.0;
    for (int i = 1; i <= AO_SAMPLES; i++) {
        float fi = float(i);
        float d = fi / float(AO_SAMPLES);
        r += w * (d - map(p + n * d));
        w *= 0.5;
    }
    return 1.0 - clamp(r, 0.0, 1.0);
}

// --- Soft shadow ---
float softShadow(vec3 ro, vec3 rd, float start, float end, float k) {
    float shade = 1.0;
    const int maxIterationsShad = 12;
    float dist = start;
    float stepDist = end / float(maxIterationsShad);
    
    for (int i = 0; i < maxIterationsShad; i++) {
        float h = map(ro + rd * dist);
        shade = min(shade, k * h / dist);
        dist += clamp(h, 0.0005, stepDist * 2.0);
        if (h < 0.001 || dist > end) break;
    }
    return min(max(shade, 0.0) + 0.4, 1.0);
}

// --- Normal ---
vec3 getNormal(in vec3 p) {
    const float eps = 0.001;
    return normalize(vec3(
        map(vec3(p.x + eps, p.y, p.z)) - map(vec3(p.x - eps, p.y, p.z)),
        map(vec3(p.x, p.y + eps, p.z)) - map(vec3(p.x, p.y - eps, p.z)),
        map(vec3(p.x, p.y, p.z + eps)) - map(vec3(p.x, p.y, p.z - eps))
    ));
}

// --- Curvature (Nimitz) ---
float curve(in vec3 p) {
    vec2 e = vec2(-1.0, 1.0) * 0.05;
    float t1 = map(p + e.yxx), t2 = map(p + e.xxy);
    float t3 = map(p + e.xyx), t4 = map(p + e.yyy);
    return 7.0 * (t1 + t2 + t3 + t4 - 4.0 * map(p));
}

// --- Main ---
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    
    vec2 uv = (fragCoord - iResolution.xy * 0.5) / iResolution.y;
    
    vec3 rd = normalize(vec3(uv, 0.5));
    rd.xy *= rot(iTime * 0.5);
    rd.xz *= rot(iTime * 0.25);
    
    vec3 ro = vec3(0.0, 0.0, iTime * 1.0);
    
    vec3 lp = vec3(0.0, 0.125, -0.125);
    lp.xy *= rot(iTime * 0.5);
    lp.xz *= rot(iTime * 0.25);
    lp += ro + vec3(0.0, 1.0, 0.0);
    
    vec3 sceneCol = vec3(0.0);
    
    float dist = raymarch(ro, rd);
    
    if (dist < FAR) {
        vec3 sp = ro + rd * dist;
        vec3 sn = getNormal(sp);
        sn = doBumpMap(sp, sn, 0.01);
        
        vec3 ld = lp - sp;
        
        // Surface texture: procedural noise instead of steel texture
        vec3 objCol = tex3D(sp, sn, 12.0);
        objCol *= bumpSurf3D(sp, sn) * 0.5 + 0.5;
        
        float lDist = max(length(ld), 0.001);
        ld /= lDist;
        float atten = min(1.0 / (lDist * 0.5 + lDist * lDist * 0.1), 1.0);
        
        float ambient = 0.25;
        float diffuse = max(0.0, dot(sn, ld));
        float specular = max(0.0, dot(reflect(-ld, sn), -rd));
        specular = pow(specular, 8.0);
        
        float shadow = softShadow(sp, ld, sEPS * 2.0, lDist, 32.0);
        float ao = calculateAO(sp, sn) * 0.5 + 0.5;
        
        #ifdef FIRE_REFLECTION
        vec3 sf = reflect(rd, sn);
        float crv = clamp(curve(sp), 0.0, 1.0);
        
        // Fire reflection: procedural noise sampled at sp/4 + time/64, scaled by curvature
        float refShade = getGrey(tex3D(sp / 4.0 + iTime / 64.0, sf, 8.0));
        refShade = refShade * 0.4 + max(dot(sf, vec3(0.166)), 0.0);
        vec3 refCol = blackbodyPalette(refShade * (crv * 0.5 + 0.5));
        #endif
        
        sceneCol = objCol * (vec3(1.0, 0.97, 0.92) * diffuse + ambient)
                 + vec3(1.0, 0.9, 0.92) * specular * 0.75;
        
        #ifdef FIRE_REFLECTION
        sceneCol += refCol;
        #endif
        
        sceneCol *= atten * ao * shadow;
    }
    
    gl_FragColor = vec4(clamp(sceneCol, 0.0, 1.0), 1.0);
}