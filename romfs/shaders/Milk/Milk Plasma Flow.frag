// Soft plasma flow (Milkdrop watercolor feel)
precision mediump float;
//!scroller off

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
varying vec2 vUV;

float bass() {
    float s = 0.0;
    for (int i = 0; i < 5; i++)
        s += texture2D(iChannel1, vec2(float(i) / 64.0, 0.0)).r;
    return s * 0.2;
}

void main() {
    vec2 uv = (vUV - 0.5) * 2.0;
    uv.x *= iResolution.x / iResolution.y;
    float t = iTime * 0.25;
    float b = bass();

    // domain warp
    float a = sin(uv.x * 2.5 + t) + sin(uv.y * 2.2 - t * 1.3);
    float c = sin(uv.x * 3.1 - t * 0.7 + b) + cos(uv.y * 2.8 + t);
    vec2 w = uv + 0.35 * vec2(a, c);

    float p1 = sin(w.x * 3.0 + t * 2.0) * sin(w.y * 2.5 - t);
    float p2 = sin(length(w) * 4.0 - t * 3.0 - b * 2.0);
    float plasma = 0.5 + 0.5 * (p1 + p2) * 0.5;

    vec3 c1 = vec3(0.1, 0.2, 0.6);
    vec3 c2 = vec3(0.9, 0.2, 0.5);
    vec3 c3 = vec3(0.2, 0.9, 0.8);
    vec3 col = mix(c1, c2, plasma);
    col = mix(col, c3, 0.5 + 0.5 * sin(plasma * 6.28 + t + b));

    // soft vignette
    col *= 0.7 + 0.3 * (1.0 - length(uv) * 0.45);
    col += vec3(0.15, 0.1, 0.2) * b * 0.5;
    gl_FragColor = vec4(col, 1.0);
}
