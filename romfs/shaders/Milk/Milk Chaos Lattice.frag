// Dense lattice / crystal grid, heavily warped by spectrum
precision mediump float;
//!scroller off
//!audio off

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel1;
varying vec2 vUV;

float spec(float x) {
    return texture2D(iChannel1, vec2(clamp(x, 0.001, 0.99), 0.0)).r;
}

void main() {
    vec2 uv = (vUV - 0.5) * 2.0;
    uv.x *= iResolution.x / iResolution.y;
    float t = iTime;

    float b = 0.0;
    for (int i = 0; i < 8; i++)
        b += spec(float(i) / 32.0);
    b *= 0.12;

    // multi-pass domain warp
    vec2 p = uv;
    for (int i = 0; i < 4; i++) {
        float fi = float(i);
        p = vec2(
            p.x + 0.18 * sin(p.y * (3.0 + fi) + t * (0.7 + fi * 0.15) + b * 2.0),
            p.y + 0.18 * cos(p.x * (2.5 + fi) - t * (0.5 + fi * 0.1) - b)
        );
    }

    float grid = abs(sin(p.x * 10.0)) * abs(sin(p.y * 10.0));
    grid = pow(grid, 4.0);
    float diag = abs(sin((p.x + p.y) * 8.0 + t)) * abs(sin((p.x - p.y) * 8.0 - t));
    diag = pow(diag, 3.0);

    vec3 col = vec3(0.01, 0.02, 0.05);
    col += vec3(0.2, 0.6, 1.0) * grid * (1.5 + b * 2.0);
    col += vec3(1.0, 0.3, 0.7) * diag * (0.8 + b);
    col += vec3(0.4, 0.9, 0.8) * exp(-length(p) * 1.5) * b;

    // spectral tint by angle
    float a = atan(uv.y, uv.x);
    col *= 0.7 + 0.3 * cos(vec3(0.0, 2.1, 4.2) + a + t * 0.5);

    col = col / (1.0 + col * 0.4);
    gl_FragColor = vec4(col, 1.0);
}
