// Box Grid Animation
// Raymarched repeated boxes with shifting grid animation.
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)

precision highp float;

uniform vec3  iResolution;
uniform float iTime;

#define PI 3.14159265

mat3 xrot(float t) {
    return mat3(1.0, 0.0, 0.0,
                0.0, cos(t), -sin(t),
                0.0, sin(t), cos(t));
}

mat3 yrot(float t) {
    return mat3(cos(t), 0.0, -sin(t),
                0.0, 1.0, 0.0,
                sin(t), 0.0, cos(t));
}

float sdBox(vec3 p, vec3 b) {
    vec3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

float map(vec3 pos) {
    float speed = 1.0;
    vec3 grid = floor(pos);
    vec3 gmod = mod(grid, 2.0);
    vec3 rmod = mod(grid, 4.0) - 2.0;
    float tm = fract(iTime * speed);
    rmod *= (cos(tm * PI) - 1.0);
    
    // FIX: use float arithmetic instead of int comparison
    float g = floor(mod(iTime * speed, 3.0));
    
    if (g > 1.5) {
        // g == 2: shift along z, driven by x*y
        if (gmod.y * gmod.x > 0.5) {
            pos.z += rmod.y * rmod.x * 0.5;
        }
    } else if (g < 0.5) {
        // g == 0: shift along x, driven by y*z
        if (gmod.y * gmod.z > 0.5) {
            pos.x += rmod.y * rmod.z * 0.5;
        }
    } else {
        // g == 1: shift along y, driven by z*x
        if (gmod.z * gmod.x > 0.5) {
            pos.y += rmod.z * rmod.x * 0.5;
        }
    }
    
    grid = floor(pos);
    pos = pos - grid;
    pos = pos * 2.0 - 1.0;
    
    float len = 0.9;
    float d = sdBox(pos, vec3(len));
    
    // FIX: use float skip instead of bool (more reliable on ES 2.0)
    float skip = 0.0;
    
    if (mod(grid.x, 2.0) < 0.5 && mod(grid.y, 2.0) < 0.5) {
        skip = 1.0;
    }
    if (mod(grid.x, 2.0) < 0.5 && mod(grid.z, 2.0) < 0.5) {
        skip = 1.0;
    }
    if (mod(grid.y, 2.0) < 0.5 && mod(grid.z, 2.0) < 0.5) {
        skip = 1.0;
    }
    
    if (skip > 0.5) {
        // Expanded: 3 extra boxes along axes
        d = 100.0;
        vec3 off = vec3(2.0, 0.0, 0.0);
        for (int i = 0; i < 3; i++) {
            float a = sdBox(pos + off, vec3(len));
            float b = sdBox(pos - off, vec3(len));
            d = min(d, min(a, b));
            off = off.zxy;
        }
        d *= 0.5;
    } else {
        d *= 0.8;
    }
    
    return d;
}

vec3 surfaceNormal(vec3 pos) {
    vec3 delta = vec3(0.01, 0.0, 0.0);
    vec3 normal;
    normal.x = map(pos + delta.xyz) - map(pos - delta.xyz);
    normal.y = map(pos + delta.yxz) - map(pos - delta.yxz);
    normal.z = map(pos + delta.zyx) - map(pos - delta.zyx);
    return normalize(normal);
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    
    vec2 uv = fragCoord.xy / iResolution.xy;
    uv = uv * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;
    
    vec3 eye = normalize(vec3(uv, 1.0 - dot(uv, uv) * 0.33));
    vec3 origin = vec3(0.0);
    
    eye = eye * yrot(iTime) * xrot(iTime);
    
    float speed = 0.5;
    float j = iTime * speed;
    float f = fract(j);
    float g = 1.0 - f;
    f = f * f * g + (1.0 - g * g) * f;
    f = f * 2.0 - 1.0;
    float a = floor(j) + f * floor(mod(j, 2.0));
    float b = floor(j) + f * floor(mod(j + 1.0, 2.0));
    
    origin.x += 0.5 + a;
    origin.y += 0.5;
    origin.z += -0.5 + b;
    
    float t = 0.0;
    float d = 0.0;
    
    for (int i = 0; i < 32; i++) {
        vec3 pos = origin + eye * t;
        d = map(pos);
        t += d;
    }
    
    vec3 worldPos = origin + eye * t;
    vec3 norm = surfaceNormal(worldPos);
    
    float prod = max(0.0, dot(norm, -eye));
    prod = pow(1.0 - prod, 2.0);
    
    vec3 col = vec3(0.1, 0.3, 0.5);
    
    // Fog based on distance
    float fog = 1.0 / (1.0 + t * t * 0.2);
    vec3 final = mix(col, vec3(1.0), fog);
    
    fog = 1.0 / (1.0 + t * t * 0.1);
    
    gl_FragColor = vec4(final * fog, 1.0);
}