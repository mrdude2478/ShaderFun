// Milkdrop-style spectrum bars + waveform
precision mediump float;
//!scroller off

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
varying vec2 vUV;

float spectrum(float x) {
    return texture2D(iChannel1, vec2(clamp(x, 0.0, 0.99), 0.0)).r;
}

void main() {
    vec2 uv = vUV;
    vec2 p = (uv - 0.5) * 2.0;
    p.x *= iResolution.x / iResolution.y;

    // soft dark bg
    vec3 col = vec3(0.02, 0.03, 0.06);

    // mirrored spectrum bars
    float bars = 48.0;
    float bx = floor(uv.x * bars) / bars;
    float h = spectrum(bx * 0.85 + 0.02);
    h = pow(h, 0.85) * 0.85;
    float bar = smoothstep(0.0, 0.02, h - abs(uv.y - 0.5) * 2.0);
    // gap between bars
    float gap = step(0.15, fract(uv.x * bars));
    vec3 barCol = mix(vec3(0.1, 0.4, 1.0), vec3(1.0, 0.3, 0.8), bx);
    col += barCol * bar * gap * 0.95;

    // center waveform line
    float wave = texture2D(iChannel0, vec2(uv.x, 0.0)).r * 2.0 - 1.0;
    float wy = 0.5 + wave * 0.12;
    float line = smoothstep(0.012, 0.0, abs(uv.y - wy));
    col += vec3(1.0, 0.95, 0.8) * line * 0.8;

    // glow under bars
    col += barCol * exp(-abs(uv.y - 0.5) * 6.0) * h * 0.25;

    col *= 1.0 - 0.25 * length(p);
    gl_FragColor = vec4(col, 1.0);
}
