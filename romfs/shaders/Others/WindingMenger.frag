// Winding Menger Tunnel
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// - texture(iChannel0) replaced with procedural noise
// - objID global removed (saved locally)
// - iMouse hardcoded to zero
// - 96 raymarch steps reduced to 72

precision highp float;

uniform vec3  iResolution;
uniform float iTime;

// --- Procedural 3D noise (replaces texture lookups) ---
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

// --- Triplanar texture (replaces tex3D with texture2D) ---
vec3 tex3D(in vec3 p, in vec3 n, float scale) {
    n = max(abs(n), vec3(0.001));
    n /= (n.x + n.y + n.z);
    float a = noise3(vec3(p.yz * scale, 1.0));
    float b = noise3(vec3(p.zx * scale, 2.0));
    float c = noise3(vec3(p.xy * scale, 3.0));
    float v = a * n.x + b * n.y + c * n.z;
    return vec3(v);
}

// --- Simple hash ---
float hash(float n) { return fract(cos(n) * 45758.5453); }

// --- Path displacement ---
vec2 path(in float x) {
    return vec2(cos(x * 0.25) * 1.8 + cos(x * 0.15) * 1.5,
                sin(x * 0.25) * 1.2 + sin(x * 0.15));
}

vec3 camPath(float t) {
    return vec3(path(t), t);
}

// --- Smooth min ---
float sminP(float a, float b, float s) {
    float h = clamp(0.5 + 0.5 * (b - a) / s, 0.0, 1.0);
    return mix(b, a, h) - s * h * (1.0 - h);
}

// --- Menger sponge (3 layers) ---
float Menger(in vec3 q) {
    float s = 4.0;
    vec3 p = abs(fract(q / s) * s - s * 0.5);
    float d = min(max(p.x, p.y), min(max(p.y, p.z), max(p.x, p.z))) - s / 3.0;
    
    s /= 2.0;
    p = abs(fract(q / s) * s - s * 0.5);
    d = max(d, min(max(p.x, p.y), min(max(p.y, p.z), max(p.x, p.z))) - s / 3.0);
    
    s /= 3.0;
    p = abs(fract(q / s) * s - s * 0.5);
    d = max(d, min(max(p.x, p.y), min(max(p.y, p.z), max(p.x, p.z))) - s / 3.0);
    
    return min(d, q.y + 0.8);
}

// --- Tubing lattice ---
float tubing(in vec3 p) {
    p = fract(p / 2.0) * 2.0 - 1.0;
    float x1 = sminP(length(p.xy), sminP(length(p.yz), length(p.xz), 0.25), 0.25) - 0.5;
    
    p = abs(fract(p * 2.0) * 0.5 - 0.25);
    float x2 = min(p.x, min(p.y, p.z)) - 0.025;
    
    return max(abs(x1), abs(x2)) - 0.0175;
}

// --- Map: returns (distance, objID) as vec2 ---
// objID: 1 = metal tubing, 2 = walls, 3 = screens
vec2 map(in vec3 p) {
    p.xy -= path(p.z);
    
    float tube = tubing(p);
    float walls = Menger(p);
    
    // Screens
    p += vec3(sign(p.x) * (-0.11 + (sin(p.z * 3.14159 * 2.0 + 1.57 / 1.0)) * 0.05), 0.0, 0.0);
    vec3 q = abs(mod(p + vec3(0.0, 0.5, 0.0), vec3(1.0, 1.0, 2.0)) - vec3(0.5, 0.5, 1.0));
    float screen = max(max(q.y, q.z) - 0.22, q.x - 0.05);
    screen = max(screen, max(abs(p.x) - 0.5, abs(p.y) - 0.22));
    
    // Object ID determination
    float id = 1.0 + step(tube, walls) + step(screen, tube) * step(screen, walls) * 2.0;
    
    return vec2(min(min(tube, walls), screen), id);
}

// --- Distance only (for normal, AO) ---
float mapD(in vec3 p) {
    return map(p).x;
}

// --- Normal (tetrahedral) ---
vec3 calcNormal(in vec3 p) {
    vec2 e = vec2(0.0025, -0.0025);
    return normalize(
        e.xyy * mapD(p + e.xyy) +
        e.yyx * mapD(p + e.yyx) +
        e.yxy * mapD(p + e.yxy) +
        e.xxx * mapD(p + e.xxx)
    );
}

// --- Ambient occlusion ---
float calcAO(in vec3 pos, in vec3 nor) {
    float sca = 2.0, occ = 0.0;
    for (int i = 0; i < 5; i++) {
        float hr = 0.01 + float(i) * 0.5 / 4.0;
        float dd = mapD(nor * hr + pos);
        occ += (hr - dd) * sca;
        sca *= 0.7;
    }
    return clamp(1.0 - occ, 0.0, 1.0);
}

// --- Texture bump mapping (4 triplanar lookups) ---
vec3 texBump(in vec3 p, in vec3 n, float bf) {
    const vec2 e = vec2(0.002, 0.0);
    
    // Build the 3 column vectors from tex3D (each returns vec3)
    mat3 m = mat3(
        tex3D(p - e.xyy, n, 12.0),
        tex3D(p - e.yxy, n, 12.0),
        tex3D(p - e.yyx, n, 12.0)
    );
    
    // Convert to greyscale
    vec3 gw = vec3(0.299, 0.587, 0.114);
    vec3 g = vec3(dot(gw, m[0]), dot(gw, m[1]), dot(gw, m[2]));
    
    // Subtract the center sample and divide by epsilon
    g = (g - vec3(dot(tex3D(p, n, 12.0), gw))) / e.x;
    g -= n * dot(n, g);
    
    return normalize(n + g * bf);
}

// --- Hue rotation ---
vec3 rotHue(vec3 p, float a) {
    vec2 cs = sin(vec2(1.570796, 0.0) + a);
    mat3 hr = mat3(0.299, 0.587, 0.114, 0.299, 0.587, 0.114, 0.299, 0.587, 0.114)
            + mat3(0.701, -0.587, -0.114, -0.299, 0.413, -0.114, -0.300, -0.588, 0.886) * cs.x
            + mat3(0.168, 0.330, -0.497, -0.328, 0.035, 0.292, 1.250, -1.050, -0.203) * cs.y;
    return clamp(p * hr, 0.0, 1.0);
}

// --- Dot pattern for screens ---
float dotPattern(vec2 p) {
    vec2 fp = abs(fract(p) - 0.5) * 2.0;
    fp = pow(fp, vec2(8.0));
    float r = max(1.0 - pow(fp.x + fp.y, 1.0), 0.0);
    p = floor(p);
    float c = dot(sin(p / 4.0 - cos(p.yx / 0.2 + iTime / 4.0)), vec2(0.5));
    c = fract(c * 7.0);
    return c * r;
}

// --- Main ---
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    
    vec2 u = (fragCoord - iResolution.xy * 0.5) / iResolution.y;
    
    // Camera
    vec3 ro = camPath(iTime * 1.5);
    vec3 lk = camPath(iTime * 1.5 + 0.1);
    vec3 lp = camPath(iTime * 1.5 + 2.0) + vec3(0.0, 2.0, 0.0);
    
    float FOV = 1.57;
    vec3 fwd = normalize(lk - ro);
    vec3 rgt = normalize(vec3(fwd.z, 0.0, -fwd.x));
    vec3 up = cross(fwd, rgt);
    
    vec3 rd = normalize(fwd + FOV * (u.x * rgt + u.y * up));
    
    // Roll based on camera path
    vec2 a = sin(vec2(1.5707963, 0.0) - camPath(lk.z).x / 12.0);
    mat2 rM = mat2(a, -a.y, a.x);
    rd.xy = rd.xy * rM;
    
    // Raymarch
    const float FAR = 50.0;
    float t = 0.0;
    float h = 0.0;
    vec2 hit = vec2(FAR, 0.0);
    
    for (int i = 0; i < 72; i++) {
        hit = map(ro + rd * t);
        h = hit.x;
        if (abs(h) < 0.001 * (t * 0.75 + 0.25) || t > FAR) break;
        t += h * 0.75;
    }
    
    vec3 col = vec3(0.0);
    
    if (t < FAR) {
        // SAVE objID BEFORE anything else calls map()
        float saveObjID = hit.y;
        
        vec3 pos = ro + rd * t;
        vec3 pOffs = pos - vec3(camPath(pos.z).xy, 0.0);
        vec3 nor = calcNormal(pos);
        
        // Bump mapping on walls and metal
        if (saveObjID < 2.5) {
            nor = texBump(pOffs * 2.0, nor, 0.002 + step(saveObjID, 1.5) * 0.012);
        }
        
        // Base texture
        col = tex3D(pOffs * 2.0, nor, 12.0);
        col = smoothstep(-0.3, 0.8, col) * vec3(1.0, 0.8, 0.7);
        
        // Spotlight
        float spot = max(2.0 - length(pOffs.xy - vec2(0.0, 1.0)), 0.0)
                   * (sin(pOffs.z * 3.14159 + 1.57) * 0.5 + 0.5);
        spot = smoothstep(0.25, 1.0, spot);
        
        float occ = calcAO(pos, nor);
        vec3 li = normalize(lp - pos);
        float dif = clamp(dot(nor, li), 0.0, 1.0);
        float spe = pow(max(dot(reflect(-li, nor), -rd), 0.0), 8.0);
        float spe2 = 0.0;
        
        vec3 rCol = vec3(0.0);
        
        if (saveObjID > 1.5) {
            // Metal or screen
            col = vec3(1.0) * dot(col * 0.7 + 0.2, vec3(0.299, 0.587, 0.114));
            rCol = tex3D((pOffs + reflect(rd, nor)) * 2.0, nor, 12.0);
            col += rCol * 0.25 + spot * 0.125;
            spe2 = spe * spe * 0.25;
            if (saveObjID < 2.5) dif = pow(dif, 2.0) * 1.25;
        }
        
        if (saveObjID > 2.5) {
            // Screen
            float c = dotPattern(pOffs.zy * 36.0 + 0.5);
            col = vec3(min(c * 1.5, 1.0), pow(c, 2.5), pow(c, 12.0));
            col = mix(col.zyx, col, sin(dot(pos, vec3(0.333)) * 3.14159 * 6.0) * 0.34 + 0.66);
            
            float id = hash(dot(floor(pOffs + vec3(0.0, 0.5, 0.5)), vec3(7.0, 157.0, 113.0)));
            col = rotHue(col, floor(id * 12.0) / 12.0 * 6.283 / 2.0);
            col += rCol * rCol * 0.5;
            dif += 0.5;
            spe += 0.25;
        }
        
        col *= (dif + 0.25 + spot * 0.5 + vec3(0.25, 0.3, 0.5) * spe) + spe2;
        col *= occ;
    }
    
    // Fog
    col = mix(min(col, 1.0), vec3(0.0), 1.0 - exp(-t * t / FAR / FAR * 15.0));
    
    gl_FragColor = vec4(col, 1.0);
}