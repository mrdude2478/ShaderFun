// Soft zoom / spin warp (Milkdrop feedback vibe without a buffer)
precision mediump float;
//!scroller off

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
varying vec2 vUV;

float bass() {
    float s = 0.0;
    for (int i = 0; i < 4; i++)
        s += texture2D(iChannel1, vec2(float(i) / 64.0, 0.0)).r;
    return s * 0.25;
}

void main() {
    vec2 uv = (vUV - 0.5) * 2.0;
    uv.x *= iResolution.x / iResolution.y;
    float t = iTime;
    float b = bass();

    float r = length(uv);
    float a = atan(uv.y, uv.x);
    // spin + zoom layers
    float zoom = 0.7 + 0.3 * sin(t * 0.4) + b * 0.15;
    float ang = a + t * 0.3 + b * 0.5;

    vec3 col = vec3(0.0);
    for (int i = 0; i < 6; i++) {
        float fi = float(i);
        float zr = r * (zoom + fi * 0.08);
        float za = ang + fi * 0.15;
        vec2 p = vec2(cos(za), sin(za)) * zr;
        float n = sin(p.x * 5.0 + t) * sin(p.y * 4.0 - t * 1.2);
        float w = texture2D(iChannel0, vec2(fract(za * 0.1), 0.0)).r;
        vec3 c = 0.5 + 0.5 * cos(vec3(0.0, 2.0, 4.0) + n * 3.0 + fi + t + w);
        col += c * exp(-fi * 0.35) * (0.15 + w * 0.1);
    }

    col *= 0.9 + 0.3 * b;
    col *= smoothstep(1.5, 0.3, r);
    gl_FragColor = vec4(col, 1.0);
}
