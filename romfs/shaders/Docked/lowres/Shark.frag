// Near miss! - by David Hoskins 2013
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// Textures replaced with procedural noise.

precision highp float;

uniform vec3  iResolution;
uniform float iTime;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;

#define time (iTime + 32.2)
#define swim ((time*.3 + sin(time*.5+5.0)*.3)*3.0)

const vec2 sun = vec2(-0.1, 0.6);
vec3 lightDir = normalize(vec3(.15, .4, .3));
float height;
float pupilPos;

// --- Noise (replaces texture lookups) ---
float hash(float n) {
    return fract(sin(n) * 43758.5453123);
}

float noise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f*f*(3.0 - 2.0*f);
    return mix(mix(hash(i.x + i.y*57.0), hash(i.x+1.0 + i.y*57.0), f.x),
               mix(hash(i.x + (i.y+1.0)*57.0), hash(i.x+1.0 + (i.y+1.0)*57.0), f.x), f.y);
}

// Replacement for the water/sand textures
vec3 texWater(vec2 p) {
    float n = noise2(p * 8.0) * 0.5 + noise2(p * 16.0) * 0.3 + noise2(p * 32.0) * 0.2;
    return vec3(n, n * 1.05, n * 1.15);
}

vec3 texSand(vec2 p) {
    float n = noise2(p * 4.0) * 0.6 + noise2(p * 10.0) * 0.4;
    return vec3(n * 1.0, n * 0.85, n * 0.55);
}

// --- Utilities ---
vec3 Rotate_Y(vec3 v, float angle) {
    vec3 vo = v; float cosa = cos(angle); float sina = sin(angle);
    v.x = cosa*vo.x - sina*vo.z;
    v.z = sina*vo.x + cosa*vo.z;
    return v;
}

vec3 Rotate_Z(vec3 v, float angle) {
    vec3 vo = v; float cosa = cos(angle); float sina = sin(angle);
    v.x = cosa*vo.x - sina*vo.y;
    v.y = sina*vo.x + cosa*vo.y;
    return v;
}

float _union(float a, float b) { return min(a, b); }

float intersect(float a, float b) { return max(a, b); }
float difference(float a, float b) { return max(a, -b); }

float box(vec3 p, vec3 b) {
    vec3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

float boxFin(vec3 p, vec3 b) {
    p = Rotate_Y(p, -.8);
    vec3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

float sphere(vec3 p, float r) { return length(p) - r; }

float prism(vec3 p, vec2 h) {
    vec3 q = abs(p);
    return max(q.z - h.y, max(q.x*0.866025 + p.y*0.5, -p.y) - h.x*0.5);
}

// --- Scene ---
vec2 Scene(vec3 p) {
    float f;
    p += vec3(-0.5, 0.0, 3.0 - swim);
    float d;
    float mat = 1.0;
    p.x += sin(p.z*2.0 + swim*5.33333 + 2.1)*.07;
    p.x = abs(p.x);
    
    // Bounding box
    d = box(p + vec3(0.0, -0.14, 0.0), vec3(.92, .4, 1.3));
    if (d > 0.0) return vec2(d, 0.0);
    
    // Body
    float s = 4.0 + smoothstep(0.5, -1.0, p.z)*4.0;
    s = max(s, 4.0 + smoothstep(.3, .75, p.z/2.0)*4.0);
    d = sphere(p*vec3(s, s, 1.0), 1.0) / 6.0;
    
    // Eyes
    float eye = sphere(p + vec3(-.138, 0.01, -0.77), 0.025);
    if (eye < 0.008) {
        d = _union(d, eye);
        mat = 2.0;
        eye = sphere(p + vec3(-.168+pupilPos, 0.01, -0.77-pupilPos), 0.01);
        if (eye < 0.008) mat = 4.0;
    }
    
    // Top fin
    vec3 fin = p + vec3(0.0, -.15, 0.2);
    f = box(fin, vec3(0.005, .3, 1.8));
    f = intersect(f, sphere(fin - vec3(0.0, -.29, -.4), .85));
    f = difference(f, sphere(fin - vec3(0, 0.5, -2.5), 2.7));
    d = min(d, f);
    
    // Side fins
    fin = Rotate_Z(Rotate_Y(p, .4), 0.4) + vec3(-0.1, 0.05, -.4);
    f = box(fin, vec3(.5, .005, .082));
    f = intersect(f, sphere(fin - vec3(0.0, -.2, -.52), .65));
    d = min(d, f);
    
    // Tail
    f = box(p + vec3(0.0, 0.0, 1.1), vec3(0.002, 0.28, 0.15));
    f = intersect(f, sphere(p*vec3(1.0, 1.0, 0.78) + vec3(0, 0, 1.1), 0.34));
    f = difference(f, sphere(p*p*vec3(1.0, 1.0, 0.22) + vec3(0, -0.08, -.21), 0.084));
    f = difference(f, sphere(p + vec3(0, 0.0, 1.68), 0.54));
    d = min(d, f);
    
    // Gills
    f = boxFin(p + vec3(-0.223, 0.0, -.48), vec3(0.0004, 0.04, 0.005));
    f = min(f, boxFin(p + vec3(-0.219, 0.0, -.50), vec3(0.0004, 0.04, 0.005)));
    f = min(f, boxFin(p + vec3(-0.216, 0.0, -.52), vec3(0.0004, 0.04, 0.005)));
    f = min(f, boxFin(p + vec3(-0.214, 0.0, -.54), vec3(0.0004, 0.04, 0.005)));
    d = mix(d, f, smoothstep(-0.006, .003, d - f));
    
    // Mouth
    f = box(p + vec3(0.0, 0.11 + height*.5, -1.), vec3(1.5, 0.0 + height, 0.37));
    if (f > 0.0) return vec2(d, mat);
    
    // Teeth
    mat = 3.0;
    d = difference(d, f);
    if (p.x < .144 && p.x > -.144) {
        vec3 teeth = p;
        teeth.z += sin(p.x*p.x*7.6);
        teeth += vec3(0.05, .12 + height, -.84 + height*3.0);
        teeth.x = mod(p.x, .015);
        f = prism(teeth, vec2(.01, .005));
        if (f < 0.005) mat = 2.0;
        d = min(d, f);
        
        teeth = p;
        teeth.z += sin(p.x*p.x*7.5);
        teeth += vec3(0.025, .11 - height*.2, -.8);
        teeth.x = mod(teeth.x, .015);
        teeth.y = -teeth.y;
        f = prism(teeth, vec2(.01, .005));
        if (f < 0.005) mat = 2.0;
        d = min(d, f);
    }
    
    return vec2(d, mat);
}

// --- Trace ---
vec4 Trace(vec3 ro, vec3 rd, out float hit) {
    hit = 0.0;
    vec2 ret = vec2(0.0, 0.0);
    vec3 pos = ro;
    float dist = 0.0;
    for (int i = 0; i < 90; i++) {
        if (hit != 0.0 || pos.y < -.30 || pos.y > .46 || dist > 7.0) continue;
        pos = ro + dist * rd;
        ret = Scene(pos);
        if (ret.x < 0.005) hit = ret.y;
        if (ret.y >= 2.0) dist += ret.x * .35;
        else dist += ret.x * .7;
    }
    return vec4(pos, ret.y);
}

// --- Normal ---
vec3 GetNormal(vec3 p) {
    vec3 eps = vec3(0.001, 0.0, 0.0);
    return normalize(vec3(Scene(p+eps.xyy).x - Scene(p-eps.xyy).x,
                          Scene(p+eps.yxy).x - Scene(p-eps.yxy).x,
                          Scene(p+eps.yyx).x - Scene(p-eps.yyx).x));
}

// --- Bubble ---
float Bubble(vec2 loc, vec2 pos, float size) {
    vec2 v2 = loc - pos;
    float d = dot(v2, v2) / size;
    if (d > 1.0) return pow(max(0.0, 1.5 - d), 3.0) * 5.0;
    d = pow(d, 6.0) * .85;
    v2 = loc - pos + vec2(-size*7.0, +size*7.0);
    d += .8 / max(sqrt((dot(v2, v2)) / size*8.0), .3);
    v2 = loc - pos + vec2(+size*7.0, -size*7.0);
    d += .2 / max((dot(v2, v2) / size*4.0), .3);
    return d;
}

// --- Shadow ---
float Shadow(vec3 ro, vec3 rd) {
    float res = 1.0;
    float t = .01;
    for (int i = 0; i < 10; i++) {
        if (t < .15) {
            float h = Scene(ro + rd * t).x;
            res = min(res, 2.2*h / t);
            t += .005;
        }
    }
    return clamp(res, 0.0, 1.0);
}

// --- Colour ---
vec3 GetColour(vec4 p, vec3 n, vec3 org, vec3 dir) {
    vec3 colour = vec3(0.0);
    float lum = clamp(dot(n, lightDir), 0.0, 1.0);
    
    if (p.w < 1.5) {
        float v = clamp(-(n.y - .1)*6.2, 0.3, 1.0);
        v += .35;
        colour = vec3(v*.8, v*.9, v*1.0) * lum;
    } else if (p.w < 2.5) {
        colour = vec3(.2) + vec3(.34, .34, .2) * lum;
    } else if (p.w < 3.5) {
        colour = vec3(.5, .1, .0) * lum;
    } else {
        colour = vec3(.15, .15, .15);
    }
    
    colour += vec3(0.0, .01, .13) * abs(n.y);
    
    // Procedural water pattern on top (replaces iChannel0/1 lookups)
    vec2 wat = p.xz * 5.3;
    float i = noise2(wat * 0.1) * 0.5;
    i = min(pow(max(0.0, i - .2), 1.0) * 1.0, .6) * .3;
    colour += vec3(i*.5, i, i) * max(n.y, 0.0);
    
    float shad = Shadow(p.xyz, lightDir);
    colour = mix(vec3(0.1), colour, min(shad + .4, 1.0));
    
    float dis = length(org - p.xyz);
    float fogAmount = clamp(max((dis - .5), 0.0)*.15, 0.0, 1.0);
    return mix(colour, vec3(.05, .31, .49), fogAmount);
}

// --- Main ---
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec3 col;
    vec2 uv = (fragCoord.xy / iResolution.xy) - vec2(.5);
    uv.x *= iResolution.x / iResolution.y;
    vec3 dir = normalize(vec3(uv, -1.4));
    
    vec3 pos = vec3(1.3, sin(time + 4.3)*.18 - .05, sin(-time*.15)*5.0 - 1.35);
    float rot = max(-pos.z - .4, 0.0)*.4;
    
    float f = smoothstep(-2.9, -1.5, pos.z)*.046;
    f -= smoothstep(-1.5, 1.5, pos.z)*.05;
    height = max(f, sin(time*4.0 + 1.4)*0.03);
    
    pupilPos = hash(floor(time*3.0) + 1.0)*0.02;
    f = hash(floor(time*3.0) + 2.0)*0.02;
    float s = pow(fract(time*3.0), 4.0);
    pupilPos = mix(pupilPos, f, s);
    
    pos.x -= smoothstep(-1.15, .0, pos.z);
    rot = smoothstep(0.0, 1.5, rot)*2.9;
    dir = Rotate_Y(dir, -rot);
    pos.z += swim;
    
    // Sun
    float i = max(0.0, 1.0 - length(sun - uv));
    col = vec3(pow(i, 1.9), pow(i, 1.0), pow(i, .8)) * 1.3;
    col = mix(col, vec3(0.0, .25, .45), ((1.0 - uv.y)*.45) * 1.8);
    
    // Water surface / floor
    if (uv.y >= 0.0) {
        float d = (3.0 - pos.y) / -uv.y;
        vec2 wat = (dir * d).xz - pos.xz;
        vec3 wtex = texWater(wat * 0.03);
        i = wtex.x;
        col += vec3(i) * max(abs(uv.y), 0.0);
    } else {
        float d = (-1.0 - pos.y) / uv.y;
        vec2 wat = (dir * d).xz + pos.xz;
        vec3 sand = texSand(wat * 0.1) * 1.5 + texSand(wat * 0.6);
        sand -= clamp(.5 - length((wat + vec2(-0.5, 3.2 - swim))*vec2(1.4, .6)), 0.0, 1.0)*3.0;
        f = ((-uv.y - .1)*2.45) * .4;
        f = clamp(f, 0.0, 1.0);
        col = mix(col, sand, f);
    }
    
    float hit = 0.0;
    vec4 loc = Trace(pos, dir, hit);
    if (hit > 0.0) {
        vec3 norm = GetNormal(loc.xyz);
        col = GetColour(loc, norm, pos, dir);
    }
    
    // Light beams
    vec2 beam = dir.xy;
    beam.x *= (-beam.y - .6)*.8;
    float bright =
        - sin(beam.y * 12.0 + beam.x * 13.0 + time*.530)*.1
        - sin(beam.y + beam.x * 17.0 + time*.60)*.1
        - cos(beam.x * 13.0 - time*.40)*.1
        - sin(-beam.x * 52.23 + time*1.8)*.1;
    col += vec3(clamp(bright, 0.0, 1.0)) * .6;
    
    // Bubbles
    for (int i = 0; i < 30; i++) {
        float fi = float(i);
        float t = time + 1.27;
        float f = floor((t + 2.0) / 4.0);
        vec2 bpos = vec2(.4, -.9) + vec2(0.0, mod(t + (fi/30.0) + hash(fi + f)*.7, 4.0));
        bpos.x += hash(fi)*.7 * (uv.y + .6);
        float d = Bubble(bpos, uv, .002*hash(fi - f) + .00015);
        d *= hash(fi + f + 399.0)*.3 + .08;
        col = mix(col, vec3(.6 + hash(f*323.1 + fi)*.4, 1.0, 1.0), d);
    }
    
    // Contrast and vignette
    col = mix(col, vec3(.0), abs(fragCoord.x/iResolution.x * 2.0 - 1.0) * abs(fragCoord.y/iResolution.y * 2.0 - 1.0));
    col *= smoothstep(0.0, 2.5, iTime);
    
    gl_FragColor = vec4(col, 1.0);
}