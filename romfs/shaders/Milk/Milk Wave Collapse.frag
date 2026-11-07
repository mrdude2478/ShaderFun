// Collapsing interference waves + spectrum rings
precision mediump float;
//!scroller off
//!audio off

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
varying vec2 vUV;

void main() {
    vec2 uv = (vUV - 0.5) * 2.0;
    uv.x *= iResolution.x / iResolution.y;
    float t = iTime;

    float waveAmp = texture2D(iChannel0, vec2(vUV.x, 0.0)).r * 2.0 - 1.0;

    // many circular wave sources
    float sum = 0.0;
    for (int i = 0; i < 9; i++) {
        float fi = float(i);
        float ang = fi * 0.698 + t * 0.15;
        float rad = 0.35 + 0.15 * sin(t * 0.4 + fi);
        vec2 src = vec2(cos(ang), sin(ang)) * rad;
        float d = length(uv - src);
        float freq = 12.0 + fi * 1.5;
        sum += sin(d * freq - t * (3.0 + fi * 0.2));
    }
    sum += waveAmp * sin(length(uv) * 10.0 - t * 5.0);

    float field = 0.5 + 0.5 * (sum / 8.0);
    field = pow(field, 1.2);

    // spectrum rings
    float rings = 0.0;
    for (int i = 0; i < 12; i++) {
        float fi = float(i);
        float s = texture2D(iChannel1, vec2(fi / 24.0, 0.0)).r;
        float rr = 0.15 + fi * 0.07 + s * 0.12;
        rings += smoothstep(0.025, 0.0, abs(length(uv) - rr)) * s;
    }

    vec3 col = mix(vec3(0.02, 0.05, 0.12), vec3(0.3, 0.9, 0.8), field);
    col = mix(col, vec3(1.0, 0.3, 0.7), rings * 1.2);
    col += vec3(0.8, 0.9, 1.0) * rings * 0.5;

    col *= 1.0 - 0.35 * length(uv);
    col = col / (1.0 + col * 0.35);
    gl_FragColor = vec4(col, 1.0);
}
