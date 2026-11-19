// Cellular Tiled Tunnel
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// - texture(iChannel0) replaced with procedural noise
// - calculateAO float loop → int counter
// - tex3D returns vec3 (procedural noise)
// - All else preserved

precision highp float;

uniform vec3  iResolution;
uniform float iTime;

#define PI 3.14159265358979
#define FAR 50.0

// --- Procedural noise (replaces texture) ---
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

// --- Greyscale ---
float getGrey(vec3 p) { return p.x * 0.299 + p.y * 0.587 + p.z * 0.114; }

// --- Hash33 ---
vec3 hash33(vec3 p) {
    float n = sin(dot(p, vec3(7.0, 157.0, 113.0)));
    return fract(vec3(2097152.0, 262144.0, 32768.0) * n);
}

// --- Rotation ---
mat2 rot2(float a) {
    float c = cos(a); float s = sin(a);
    return mat2(c, s, -s, c);
}

// --- Triplanar texture (procedural) ---
vec3 tex3D(in vec3 p, in vec3 n, float scale) {
    n = max((abs(n) - 0.2) * 7.0, vec3(0.001));
    n /= (n.x + n.y + n.z);
    
    // Three noise samples, one per axis plane
    float a = noise3(vec3(p.yz * scale, 1.0));
    float b = noise3(vec3(p.zx * scale, 2.0));
    float c = noise3(vec3(p.xy * scale, 3.0));
    
    vec3 tx = vec3(a * n.x + b * n.y + c * n.z);
    return tx * tx;
}

// --- Sphere distance (tile helper) ---
float drawSphere(in vec3 p) {
    p = fract(p) - 0.5;
    return dot(p, p);
}

// --- Cellular tile (fast Voronoi emulation) ---
float cellTile(in vec3 p) {
    vec4 v, d;
    d.x = drawSphere(p - vec3(0.81, 0.62, 0.53));
    p.xy = vec2(p.y - p.x, p.y + p.x) * 0.7071;
    d.y = drawSphere(p - vec3(0.39, 0.2, 0.11));
    p.yz = vec2(p.z - p.y, p.z + p.y) * 0.7071;
    d.z = drawSphere(p - vec3(0.62, 0.24, 0.06));
    p.xz = vec2(p.z - p.x, p.z + p.x) * 0.7071;
    d.w = drawSphere(p - vec3(0.2, 0.82, 0.64));
    
    v.xy = min(d.xz, d.yw);
    v.z = min(max(d.x, d.y), max(d.z, d.w));
    v.w = max(v.x, v.y);
    
    d.x = min(v.z, v.w) - min(v.x, v.y);
    return d.x * 2.66;
}

// --- Cell ID for coloring ---
int cellTileID(in vec3 p) {
    int cellID = 0;
    vec3 d = vec3(0.75);
    
    d.z = drawSphere(p - vec3(0.81, 0.62, 0.53));
    if (d.z < d.x) cellID = 1;
    d.y = max(d.x, min(d.y, d.z)); d.x = min(d.x, d.z);
    
    p.xy = vec2(p.y - p.x, p.y + p.x) * 0.7071;
    d.z = drawSphere(p - vec3(0.39, 0.2, 0.11));
    if (d.z < d.x) cellID = 2;
    d.y = max(d.x, min(d.y, d.z)); d.x = min(d.x, d.z);
    
    p.yz = vec2(p.z - p.y, p.z + p.y) * 0.7071;
    d.z = drawSphere(p - vec3(0.62, 0.24, 0.06));
    if (d.z < d.x) cellID = 3;
    d.y = max(d.x, min(d.y, d.z)); d.x = min(d.x, d.z);
    
    p.xz = vec2(p.z - p.x, p.z + p.x) * 0.7071;
    d.z = drawSphere(p - vec3(0.2, 0.82, 0.64));
    if (d.z < d.x) cellID = 4;
    d.y = max(d.x, min(d.y, d.z)); d.x = min(d.x, d.z);
    
    return cellID;
}

// --- Tunnel path ---
vec2 path(in float z) {
    float s = sin(z / 24.0) * cos(z / 16.0);
    return vec2(s * 9.0, 0.0);
}

// --- Tunnel SDF ---
float map(vec3 p) {
    float sf = cellTile(p / 2.5);
    p.xy -= path(p.z);
    return 1.0 - length(p.xy * vec2(0.5, 0.7071)) + (0.5 - sf) * 0.35;
}

// --- Raymarch ---
float trace(in vec3 ro, in vec3 rd) {
    float t = 0.0, h;
    for (int i = 0; i < 96; i++) {
        h = map(ro + rd * t);
        if (abs(h) < 0.002 * (t * 0.125 + 1.0) || t > FAR) break;
        t += h * 0.8;
    }
    return min(t, FAR);
}

// --- Texture bump mapping (FIXED: same pattern as Winding Menger) ---
vec3 doBumpMap(in vec3 p, in vec3 n, float bf) {
    const vec2 e = vec2(0.001, 0.0);
    const float scale = 4.0;
    
    mat3 m = mat3(
        tex3D(p - e.xyy, n, scale),
        tex3D(p - e.yxy, n, scale),
        tex3D(p - e.yyx, n, scale)
    );
    
    vec3 gw = vec3(0.299, 0.587, 0.114);
    vec3 g = gw * m;
    
    float center = dot(tex3D(p, n, scale), gw);
    g = (g - vec3(center)) / e.x;
    g -= n * dot(n, g);
    
    return normalize(n + g * bf);
}

// --- Normal (tetrahedral) ---
vec3 calcNormal(in vec3 p) {
    vec2 e = vec2(0.0025, -0.0025);
    return normalize(
        e.xyy * map(p + e.xyy) +
        e.yyx * map(p + e.yyx) +
        e.yxy * map(p + e.yxy) +
        e.xxx * map(p + e.xxx)
    );
}

// --- Ambient occlusion (int loop) ---
float calculateAO(in vec3 p, in vec3 n) {
    float ao = 0.0, l;
    const float maxDist = 2.0;
    const int nbIte = 6;
    
    for (int i = 1; i <= nbIte; i++) {
        float fi = float(i);
        l = (fi * 0.75 + fract(cos(fi) * 45758.5453) * 0.25) / float(nbIte) * maxDist;
        ao += (l - map(p + n * l)) / (1.0 + l);
    }
    
    return clamp(1.0 - ao / float(nbIte), 0.0, 1.0);
}

// --- Curvature (Nimitz) ---
float curve(in vec3 p, in float w) {
    vec2 e = vec2(-1.0, 1.0) * w;
    float t1 = map(p + e.yxx), t2 = map(p + e.xxy);
    float t3 = map(p + e.xyx), t4 = map(p + e.yyy);
    return 0.125 / (w * w) * (t1 + t2 + t3 + t4 - 4.0 * map(p));
}

// --- Main ---
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    
    vec2 uv = (fragCoord - iResolution.xy * 0.5) / iResolution.y;
    
    // Camera setup
    vec3 lookAt = vec3(0.0, 0.0, iTime * 6.0);
    vec3 camPos = lookAt + vec3(0.0, 0.1, -0.5);
    
    vec3 light_pos = camPos + vec3(0.0, 0.125, 4.125);
    vec3 light_pos2 = camPos + vec3(0.0, 0.0, 8.0);
    
    lookAt.xy += path(lookAt.z);
    camPos.xy += path(camPos.z);
    light_pos.xy += path(light_pos.z);
    light_pos2.xy += path(light_pos2.z);
    
    float FOV = PI / 3.0;
    vec3 forward = normalize(lookAt - camPos);
    vec3 right = normalize(vec3(forward.z, 0.0, -forward.x));
    vec3 up = cross(forward, right);
    
    vec3 rd = normalize(forward + FOV * uv.x * right + FOV * uv.y * up);
    
    // Camera swivel (rotation based on path)
    rd.xy = rot2(path(lookAt.z).x / 32.0) * rd.xy;
    
    // Trace
    float t = trace(camPos, rd);
    vec3 sceneCol = vec3(0.0);
    
    if (t < FAR) {
        vec3 sp = t * rd + camPos;
        vec3 sn = calcNormal(sp);
        
        const float tSize0 = 1.0 / 1.0;
        
        // Bump mapping
        sn = doBumpMap(sp * tSize0, sn, 0.02);
        
        // AO
        float ao = calculateAO(sp, sn);
        
        // Lights
        vec3 ld = light_pos - sp;
        vec3 ld2 = light_pos2 - sp;
        float lDdist = max(length(ld), 0.001);
        float lDdist2 = max(length(ld2), 0.001);
        ld /= lDdist;
        ld2 /= lDdist2;
        
        float atten = 1.0 / (1.0 + lDdist * 0.125 + lDdist * lDdist * 0.05);
        float atten2 = 1.0 / (1.0 + lDdist2 * 0.125 + lDdist2 * lDdist2 * 0.05);
        
        float ambience = 0.75;
        float diff = max(dot(sn, ld), 0.0);
        float diff2 = max(dot(sn, ld2), 0.0);
        float spec = pow(max(dot(reflect(-ld, sn), -rd), 0.0), 32.0);
        float spec2 = pow(max(dot(reflect(-ld2, sn), -rd), 0.0), 32.0);
        
        float crv = clamp(curve(sp, 0.125) * 0.5 + 0.5, 0.0, 1.0);
        float fre = pow(clamp(dot(sn, rd) + 1.0, 0.0, 1.0), 1.0);
        
        vec3 texCol = tex3D(sp * tSize0, sn, 4.0);
        texCol = min(texCol * 1.5, vec3(1.0));
        
        int id = cellTileID(sp / 2.5);
        if (id == 4) texCol = texCol * vec3(0.8, 0.6, 0.4);
        if (id == 3) texCol = texCol * 0.5 + texCol * vec3(0.5, 0.35, 0.2);
        
        float shading = crv * 0.75 + 0.25;
        
        sceneCol = (texCol * (diff + ambience + spec) + spec * vec3(0.7, 0.9, 1.0)) * atten;
        sceneCol += (texCol * (diff2 + ambience + spec2) + spec2 * vec3(0.7, 0.9, 1.0)) * atten2;
        
        sceneCol *= shading * ao;
        
        // Surface lines (curvature-based)
        sceneCol *= clamp(abs(curve(sp, 0.035)), 0.0, 1.0) * 0.5 + 1.0;
        sceneCol *= 1.0 - smoothstep(0.0, 4.0, abs(curve(sp, 0.0125))) * vec3(0.82, 0.85, 0.88);
    }
    
    gl_FragColor = vec4(sqrt(clamp(sceneCol, 0.0, 1.0)), 1.0);
}