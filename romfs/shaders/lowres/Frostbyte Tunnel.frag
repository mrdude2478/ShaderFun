// SPDX-License-Identifier: CC-BY-NC-SA-4.0
// Copyright (c) 2026 @Frostbyte
// Original shader: https://fragcoord.xyz/s/tbe1g319
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)

precision mediump float;

uniform vec3  iResolution;
uniform float iTime;

#define T iTime

// Path
#define P(z) (vec3(cos((z) * .02) *20., cos((z) * .05) * 3., z))

// 2d rotation matrix golfed
#define R(a) mat2(cos(a+vec4(0,33,11,0)))

// Xor's Dot Noise
mat3 G = mat3(-.57,.81,.1, -.28,-.3,.9, .77,.49,.4);
#define n(p) dot( cos(G*(p)), sin(1.6*(p)*G) )

// Inigo Quilez (MIT)
vec3 c(float t) {
    return clamp(vec3(.455, .322, .216) + vec3(-.073, .119, .150) * cos(6.28318 * (vec3(20.000, 1.000, 1.000) * t + vec3(.100, -.256, -.231))), 0., 1.);
}

// tanh does not exist in GLSL ES 2.0 -- stable version
vec4 tanh4(vec4 x) {
    vec4 ax = abs(x);
    vec4 e  = exp(-2.0 * ax);
    vec4 t  = (1.0 - e) / (1.0 + e);
    return sign(x) * t;
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;

    float d = 0.0;
    float s = 0.0;
    
    vec3 g;
    vec3 p = P(T * 1e1);
    vec3 Z = normalize( P(T * 1e1 + 1.0) - p );
    vec3 X = normalize(vec3(Z.z, 0.0, -Z));
    vec3 rd = vec3((fragCoord - .5 * iResolution.xy) / iResolution.y, 2.0) * mat3(-X, cross(X, Z), Z);

    vec4 o = vec4(0.0);

    // OPTIMIZATION 1: Early exit. If we travel further than 40 units, stop.
    // The rest of the screen is just empty space and costs GPU time for nothing.
    for (int i = 0; i < 80; i++) {
        if (d > 40.0) break;

        s = .001 + abs(s) * .1;
        d += s;
        o.rgb += c(p.z*.1 + float(i)*2.) / s;
        p = rd * d + P(T * 1e1);
        g = p;
        p.xy *= R(length(P(T).xy) * .05);
        
        s = (sin(p.z + p.y) * .1) - .2;
        float y = abs(n(p) + n(p / 8.0) * 4.0);
        s += y + y * .2;
        s = max(1.5 + sin(p.z*.2 + p.y*.4) - length((g - P(p.z)).xy), s);
    }
    
    // OPTIMIZATION 2: Brightness Compensation
    // Since we dropped from 150 to 80 steps, we multiply the final 
    // color by (150/80) = 1.875 to keep it looking bright and vivid.
    vec4 finalColor = tanh4(o * o / 2e8) * 1.5;
    
    // We multiply the RGB by 1.875 to replace the missing steps
    gl_FragColor = vec4(finalColor.rgb * 1.875, 1.0);
}