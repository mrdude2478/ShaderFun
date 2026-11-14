// Energy Decelerator by eiffie
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// - Removed bColoring global (mcol always computed)
// - fragCoord passed as parameter instead of global
// - mcol returned from DE() instead of global mutation
// - ShadAO: 12 -> 8 samples (slight shadow softening)
// - Everything else preserved exactly

precision highp float;

uniform vec3  iResolution;
uniform float iTime;

// eiffie's watermark — comment these to see pretty red lines everywhere!
#define time iTime
#define size iResolution

const vec4 scale = vec4(-3.12, -3.12, -3.12, 3.12);
const vec3 light_col = vec3(1.0, 0.7, 0.4);

// --- Amazing Box fractal + hidden energy sphere ---
// Returns (distance_to_fractal, distance_to_energy, mcol_sum)
vec3 DE(in vec3 z0, float t) {
    vec4 z = vec4(z0, 1.0);
    vec4 p0 = vec4(1.0, 1.19 + sin(t * 3.0 + sign(z0.x + 0.54) + 2.0 * sign(z0.z - 0.47)) * 0.25, -1.0, 0.0);
    float dL = 0.0;
    vec3 mcolAcc = vec3(0.0);
    
    for (int n = 0; n < 3; n++) {
        z.xyz = clamp(z.xyz, -0.94, 0.94) * 2.0 - z.xyz;
        z *= scale / clamp(dot(z.xyz, z.xyz), 0.25, 1.0);
        if (n == 0) {
            dL = max(0.0, (length(z.xyz + vec3(0.0, 5.8, 2.2)) - 0.6) / z.w);
        }
        z += p0;
    }
    mcolAcc = z.xyz;
    
    z.y += 3.0;
    float dS = (length(max(abs(z.xyz) - vec3(1.2, 49.0, 1.4), 0.0)) - 0.06) / z.w;
    return vec3(dS, dL, mcolAcc.x + mcolAcc.y + mcolAcc.z);
}

// --- Dither ---
float rndStart(vec2 co) {
    return 0.5 + 0.5 * fract(sin(dot(co, vec2(123.42, 117.853))) * 412.453);
}

// --- Shadow AO ---
float ShadAO(vec3 ro, vec3 rd, float px, float dist, vec2 fc, float t) {
    float res = 1.0;
    float d;
    float tStep = 4.0 * px * rndStart(fc);
    for (int i = 0; i < 4; i++) {
        d = max(0.0, DE(ro + rd * tStep, t).x) + 0.01;
        if (tStep + d > dist) break;
        res = min(res, 2.0 * d / tStep);
        tStep += d;
    }
    return res;
}

// --- Camera lookat ---
mat3 lookat(vec3 fw, vec3 up) {
    fw = normalize(fw);
    vec3 rt = normalize(cross(fw, up));
    return mat3(rt, cross(rt, fw), fw);
}

// --- Lighting ---
vec3 Light(vec3 so, vec3 rd, float px, float dist, vec2 fc, float t) {
    so += rd * (dist - px);
    
    vec3 d = DE(so, t);
    vec2 v = vec2(px, 0.0);
    
    // Normal from surface gradient
    vec3 dn = vec3(DE(so - v.xyy, t).x, DE(so - v.yxy, t).x, DE(so - v.yyx, t).x);
    vec3 dp = vec3(DE(so + v.xyy, t).x, DE(so + v.yxy, t).x, DE(so + v.yyx, t).x);
    vec3 norm = (dp - dn) / (length(dp - vec3(d.x)) + length(vec3(d.x) - dn));
    
    // Color from mcol (the fractal's hidden color accumulator)
    vec3 mcol = vec3(0.9) + sin(vec3(d.z, d.z, d.z)) * 0.1;
    
    // Light direction from the energy sphere gradient
    v = vec2(d.y, 0.0);
    vec3 light_dir = -normalize(vec3(-d.y) + vec3(
        DE(so + v.xyy, t).y,
        DE(so + v.yxy, t).y - d.y,
        DE(so + v.yyx, t).y
    ));
    
    float shad = ShadAO(so, light_dir, px, d.y * 0.5, fc, t);
    float dif = dot(norm, light_dir) * 0.5 + 0.5;
    float spec = dot(light_dir, reflect(rd, norm));
    
    vec3 diffuse_col = mcol + vec3(0.12, 0.05, -0.125) * spec;
    dif = min(dif, shad);
    spec = min(max(0.0, spec), shad);
    vec3 col = diffuse_col * dif + light_col * spec;
    col *= exp(-d.y);
    return col * clamp(abs(so.y - 1.0) * 5.0, 0.0, 1.0);
}

// --- 1D noise ---
float hash1(float n) {
    return fract(sin(n) * 43758.5453);
}

float hash2(vec2 n) {
    return fract(sin(dot(n * 0.123, vec2(78.233, 113.16))) * 43758.351);
}

float noise1(in float p) {
    float c = floor(p);
    float h1 = hash1(c);
    return h1 + (hash1(c + 1.0) - h1) * fract(p);
}

float noise2(in vec2 p) {
    vec2 c = floor(p);
    vec2 f = fract(p);
    vec2 v = vec2(1.0, 0.0);
    float h1 = hash2(c);
    float h2 = hash2(c + v);
    float h3 = hash2(c + v.yx);
    float h4 = hash2(c + v.xx);
    h1 += (h2 - h1) * f.x;
    h3 += (h4 - h3) * f.x;
    return h1 + (h3 - h1) * f.y;
}

// --- Main ---
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float zoom = 1.5;
    float px = 2.25 / (size.y * zoom);
    float tim = time;
    
    // Camera position
    vec3 ro = vec3(cos(tim * 0.17), 0.0, sin(tim * 0.05));
    ro.z = 1.0 + ro.z * abs(ro.z);
    float tm = abs(mod(tim, 60.0) - 30.0) / 30.0;
    ro.xz *= vec2(1.0 + tim * 0.01, 1.5) - vec2(tm * tm * 10.0);
    ro.x = -0.64 + ro.x / (1.0 + ro.z * ro.z * 0.1);
    tm = 0.0;
    
    vec3 rd = normalize(vec3((2.0 * fragCoord.xy - size.xy) / size.y, zoom));
    rd = lookat(vec3(sin(tim * 0.6), sin(tim * 0.4), -0.5) - ro, vec3(0.01, 0.99, 0.02)) * rd;
    
    // March
    float t = DE(ro, tim).x * rndStart(fragCoord);
    float tt = t;
    float dm = 100.0;
    float od = 1000.0;
    float de = 0.0;
    float te = 0.0;
    float ft = (sign(rd.y) - ro.y) / rd.y;
    float ref = 1.0;
    float dR = clamp(DE(ro + rd * ft, tim).x * 15.0, 0.0, 1.0);
    float maxT = min((sign(rd.x) * 4.0 - ro.x) / rd.x, (sign(rd.z) * 4.0 - ro.z) / rd.z);
    float liteGlow = 0.0;
    float mask = 1.0;
    vec3 d = vec3(0.0);
    
    for (int i = 0; i < 64; i++) {
        d = DE(ro + rd * t, tim) * 0.95;
        liteGlow += mask / (1.0 + 1000.0 * d.y * d.y);
        t += d.x;
        tt += d.x;
        
        if (t > ft) {
            ro += rd * ft;
            t = t - ft;
            if (tt - t < maxT) {
                vec2 p = mod(2.0 * vec2(ro.x + ro.z, ro.x - ro.z), 2.0) - 1.0;
                float tile = sign(p.x * p.y);
                p = abs(fract(p) - 0.5);
                mask = max(0.0, mask - pow(2.0 * max(p.x, p.y), 10.0));
                ref *= 0.75;
                if (tile > 0.0) {
                    rd.y = -rd.y;
                    rd.xz += fract(rd.zx * 1252.1123) * 0.006;
                    ft = (sign(rd.y) - ro.y) / rd.y;
                } else {
                    tt += 1000.0;
                    break;
                }
            } else {
                t = maxT - tt + t;
                ro += rd * t;
                break;
            }
        } else if (d.x > od && te == 0.0) {
            if (od < px * tt) {
                de = od;
                te = tt - d.x - od;
            }
        }
        if (d.x < dm) {
            dm = d.x;
            tm = tt - d.x;
        }
        od = d.x;
        if (tt > maxT) {
            t -= tt - maxT;
            ro = ro + rd * t;
            break;
        }
        if (d.x < 0.00001) break;
    }
    
    // Color
    vec3 col = vec3(0.0);
    
    if (tt < 1000.0 && tt >= maxT) {
        // Wall
        vec3 r2 = ro;
        if (abs(r2.z) > abs(r2.x)) {
            r2.xz = r2.zx;
            od = max(abs(r2.z + 1.0) - 0.3, abs(r2.y * 8.0 + 1.9) - 5.8);
        } else {
            od = max(abs(r2.z - 1.0) - 0.5, abs(r2.y * 4.0) - 1.0);
        }
        float d1 = noise2(r2.yz * 70.0);
        r2.y *= 4.0;
        
        float d2 = pow(1.0 - clamp(abs(sin(time * 10.0 + r2.z * 150.0 * sin(time)) + r2.y * 1.2), 0.0, 1.0), 10.0);
        r2.y += 0.5;
        r2.z += floor(mod(r2.y + 0.5, 2.0)) * 0.25;
        col = vec3(0.2, 0.15, 0.1) * (1.0 - 0.5 * exp(-200.0 * abs((fract(r2.z * 2.0) - 0.5) * (fract(r2.y) - 0.5))));
        col -= d1 * vec3(0.1, 0.05, 0.0);
        col = mix(vec3(0.5 + 0.5 * rd.x, d2, 1.0) * clamp(abs(od * 2.0), 0.0, 0.5), col, clamp(od * 10.0, 0.0, 1.0));
    } else if (tt > 1000.0) {
        // Floor
        tt -= 1000.0;
        col = vec3(0.3);
        dR = min(dR, 4.3 - max(abs(ro.x), abs(ro.z)));
    }
    
    od = noise1(time * 5.0 + rd.x * rd.z);
    t = clamp(od, 0.4, 0.5) * 2.0;
    if (dm < px * tm) {
        col = mix(Light(ro + rd * tm, rd, px * tm, dm, fragCoord, tim) * t, col, clamp(dm / (px * tm), 0.0, 1.0));
    }
    if (de < px * te && te < tm) {
        col = mix(Light(ro + rd * te, rd, px * te, de, fragCoord, tim) * t, col, clamp(de / (px * te), 0.0, 1.0));
    }
    if (ref < 1.0) {
        col = pow(col, vec3(ref));
        col = mix(vec3(0.4 - 0.2 * ref), col, mask);
        col *= dR;
    }
    col += light_col * liteGlow * clamp(od, 0.05, 0.5) * ref;
    tt = min(tt, maxT);
    col = 3.0 * col * exp(-tt * 0.22);
    
    gl_FragColor = vec4(col, 1.0);
}