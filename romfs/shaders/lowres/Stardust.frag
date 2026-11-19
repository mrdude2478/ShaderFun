// starDust - shadertoy intro
// Created by Dmitry Andreev - and'2014
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)

precision highp float;

uniform vec3  iResolution;
uniform float iTime;

#define SPEED           (1.7)
#define WARMUP_TIME     (0.0)
#define SOUND_OFFSET    (16.0)

// Color constants (moved out of loops for ES 2.0 safety)
const vec3 ORANGE = vec3(1.0, 0.7, 0.4);
const vec3 BLUE   = vec3(0.4, 0.7, 1.0);

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

float isectPlane(vec3 n, float d, vec3 org, vec3 dir) {
    float t = -(dot(org, n) + d) / dot(dir, n);
    return t;
}

float drawLogo(in vec2 fragCoord) {
    float res = max(iResolution.x, iResolution.y);
    vec2 pos = vec2(floor((fragCoord.xy / res) * 128.0));

    float val = 0.0;

    // AND'14 bitmap (converted to range comparisons)
    if (pos.y > 1.5 && pos.y < 2.5) val = 4873761.5;
    if (pos.y > 2.5 && pos.y < 3.5) val = 8049199.5;
    if (pos.y > 3.5 && pos.y < 4.5) val = 2839721.5;
    if (pos.y > 4.5 && pos.y < 5.5) val = 1726633.5;
    if (pos.x > 125.0) val = 0.0;

    float bit = floor(val * exp2(pos.x - 125.0));

    return (bit - floor(bit / 2.0) * 2.0) > 0.5 ? 1.0 : 0.0;
}

vec3 drawEffect(vec2 coord, float time) {
    vec3 clr = vec3(0.0);
    const float far_dist = 10000.0;

    float mtime = SOUND_OFFSET + time * 2.0 / SPEED;
    vec2 uv = coord.xy / iResolution.xy;

    vec3 org = vec3(0.0);
    vec3 dir = vec3(uv.xy * 2.0 - 1.0, 1.0);

    // Animate tilt
    float ang = sin(time * 0.2) * 0.2;
    vec3 odir = dir;
    dir.x = cos(ang) * odir.x + sin(ang) * odir.y;
    dir.y = sin(ang) * odir.x - cos(ang) * odir.y;

    // Animate FOV and aspect ratio
    dir.x *= 1.5 + 0.5 * sin(time * 0.125);
    dir.y *= 1.5 + 0.5 * cos(time * 0.25 + 0.5);

    // Animate view direction
    dir.x += 0.25 * sin(time * 0.3);
    dir.y += 0.25 * sin(time * 0.7);

    // Bend it like this
    dir.xy = mix(vec2(dir.x + 0.2 * cos(dir.y) - 0.1, dir.y), dir.xy,
        smoothstep(0.0, 1.0, saturate(0.5 * abs(mtime - 50.0))));

    // Bend it like that
    dir.xy = mix(vec2(dir.x + 0.1 * sin(4.0 * (dir.x + time)), dir.y), dir.xy,
        smoothstep(0.0, 1.0, saturate(0.5 * abs(mtime - 58.0))));

    // Cycle between long blurry and short sharp particles
    vec2 param = mix(vec2(60.0, 0.8), vec2(800.0, 3.0),
        pow(0.5 + 0.5 * sin(time * 0.2), 2.0));

    float lt = fract(mtime / 4.0) * 4.0;
    vec2 mutes = vec2(0.0);
    
    if (mtime >= 32.0 && mtime < 48.0) {
        mutes = max(vec2(0.0), 1.0 - 4.0 * abs(lt - vec2(3.25, 3.50)));
    }
    
    for (int k = 0; k < 2; k++) {
        for (int i = 0; i < 64; i++) {
            // Prologue: only 16 layers
            if (mtime < 16.0 && i >= 16) break;

            vec3 pn = vec3(k > 0 ? -1.0 : 1.0, 0.0, 0.0);
            float t = isectPlane(pn, 100.0 + float(i) * 20.0, org, dir);

            if (t <= 0.0 || t >= far_dist) continue;

            vec3 p = org + dir * t;
            vec3 vdir = normalize(-p);

            // Particle lanes
            vec3 pp = ceil(p / 100.0) * 100.0;

            // Pseudo-random
            float n = pp.y + float(i) + float(k) * 123.0;
            float q = fract(sin(n * 123.456) * 234.345);
            float q2 = fract(sin(n * 234.123) * 345.234);

            q = sin(p.z * 0.0003 + time * (0.25 + 0.75 * q2) + q * 12.0);

            // Smooth particle edges
            q = saturate(q * param.x - param.x + 1.2) * param.y;
            q *= saturate(4.0 - 8.0 * abs(-50.0 + pp.y - p.y) / 100.0);

            // Fade by distance
            q *= 1.0 - saturate(pow(t / far_dist, 5.0));

            // Fade by view angle
            float fn = 1.0 - pow(1.0 - dot(vdir, pn), 2.0);
            q *= 2.0 * smoothstep(0.0, 1.0, fn);

            // Flash fade left or right
            float m = (k < 1) ? mutes.x : mutes.y;
            q *= 1.0 - 0.9 * m;

            // Color
            clr += q * mix(ORANGE, BLUE, 0.5 + 0.5 * sin(time * 0.5 + q2));

            // Bass-drum particle flashes
            float population = (mtime < 16.0) ? 0.0 : 0.97;

            if (mtime >= 8.0 && q2 > population) {
                float a = (mtime >= 62.0) ? 8.0 : 1.0;
                float b = (mtime < 16.0) ? 2.0 : a;
                clr += q * ((mtime < 16.0) ? 2.0 : 8.0)
                    * max(0.0, fract(-mtime * b) * 2.0 - 1.0);
            }
        }
    }

    clr *= 0.2;

    // Gamma cycle
    clr.r = pow(clr.r, 0.75 + 0.35 * sin(time * 0.5));
    clr.b = pow(clr.b, 0.75 - 0.35 * sin(time * 0.5));

    // Fade in
    clr *= pow(min(mtime / 4.0, 1.0), 2.0);

    // Fade out before drums
    if (mtime < 8.0) clr *= 1.0 - saturate((mtime - 5.0) / 3.0);

    // Snare drum horizon flash
    if (mtime >= 15.0) {
        float h = normalize(dir).x;
        clr *= 1.0 + 2.0 * pow(saturate(1.0 - abs(h)), 8.0)
            * max(0.0, fract(-mtime + 0.5) * 4.0 - 3.0);
    }

    // The end
    if (mtime >= 64.0) clr = vec3(0.0);

    // Initial flash
    if (mtime >= 16.0) clr += max(0.0, 1.0 - (mtime - 16.0));

    // Final flash
    if (mtime >= 64.0) clr += max(0.0, 1.0 - (mtime - 64.0) * 0.5) * vec3(0.8, 0.9, 1.0);

    // Desaturate prologue
    if (mtime < 16.0) {
        float lum = dot(clr, vec3(0.33));
        clr = mix(vec3(lum), clr, min(1.0, mtime / 32.0));
    }

    // Vignette in linear space
    clr *= clr;
    clr *= 1.4;
    clr *= 1.0 - 1.5 * dot(uv - 0.5, uv - 0.5);
    clr = sqrt(max(vec3(0.0), clr));

    return clr;
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord.xy / iResolution.xy;
    
    vec3 org = vec3(0.0);
    vec3 dir = vec3(uv.xy * 2.0 - 1.0, 1.0);

    // Animate
    float ang = sin(iTime * 0.2) * 0.2;
    vec3 odir = dir;
    dir.x = cos(ang) * odir.x + sin(ang) * odir.y;
    dir.y = sin(ang) * odir.x - cos(ang) * odir.y;

    dir.x *= 1.5 + 0.5 * sin(iTime * 0.125);
    dir.y *= 1.5 + 0.5 * cos(iTime * 0.25 + 0.5);
    dir.x += 0.25 * sin(iTime * 0.3);
    dir.y += 0.25 * sin(iTime * 0.7);

    vec2 param = mix(vec2(60.0, 0.8), vec2(800.0, 3.0),
        pow(0.5 + 0.5 * sin(iTime * 0.2), 2.0));

    vec3 clr = vec3(0.0);
    const float far_dist = 10000.0;

    for (int k = 0; k < 2; k++) {
        for (int i = 0; i < 32; i++) {
            vec3 pn = vec3(k > 0 ? -1.0 : 1.0, 0.0, 0.0);
            float t = -(dot(org, pn) + (100.0 + float(i) * 20.0)) / dot(dir, pn);

            if (t <= 0.0 || t >= far_dist) continue;

            vec3 p = org + dir * t;
            vec3 vdir = normalize(-p);
            vec3 pp = ceil(p / 100.0) * 100.0;

            float n = pp.y + float(i) + float(k) * 123.0;
            float q = fract(sin(n * 123.456) * 234.345);
            float q2 = fract(sin(n * 234.123) * 345.234);

            q = sin(p.z * 0.0003 + iTime * (0.25 + 0.75 * q2) + q * 12.0);

            q = saturate(q * param.x - param.x + 1.2) * param.y;
            q *= saturate(4.0 - 8.0 * abs(-50.0 + pp.y - p.y) / 100.0);
            q *= 1.0 - saturate(pow(t / far_dist, 5.0));

            float fn = 1.0 - pow(1.0 - dot(vdir, pn), 2.0);
            q *= 2.0 * smoothstep(0.0, 1.0, fn);

            // HARD CUTOFF: don't add dim particles
            if (q < 0.02) continue;

            clr += q * mix(vec3(1.0, 0.7, 0.4), vec3(0.4, 0.7, 1.0),
                           0.5 + 0.5 * sin(iTime * 0.5 + q2));
        }
    }

    clr *= 0.2;

    clr.r = pow(clr.r, 0.75 + 0.35 * sin(iTime * 0.5));
    clr.b = pow(clr.b, 0.75 - 0.35 * sin(iTime * 0.5));

    // KILL THE BRIGHT CENTER STAR
    float centerDist = length(uv - 0.5) * 2.0;
    clr *= smoothstep(0.0, 0.3, centerDist);

    // Vignette
    clr *= clr;
    clr *= 1.4;
    clr *= 1.0 - 1.5 * dot(uv - 0.5, uv - 0.5);
    clr = sqrt(max(vec3(0.0), clr));

    gl_FragColor = vec4(clr, 1.0);
}