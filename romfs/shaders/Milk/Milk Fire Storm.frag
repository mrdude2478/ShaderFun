// Rising fire / ember field driven by bass
precision mediump float;
//!scroller off
//!audio off

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
varying vec2 vUV;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * noise(p);
        p *= 2.05;
        a *= 0.5;
    }
    return v;
}

void main() {
    vec2 uv = vUV;
    vec2 p = (uv - 0.5) * 2.0;
    p.x *= iResolution.x / iResolution.y;

    float bass = 0.0;
    for (int i = 0; i < 6; i++)
        bass += texture2D(iChannel1, vec2(float(i) / 48.0, 0.0)).r;
    bass *= 0.16;

    float t = iTime;
    // rise upward, sway
    vec2 q = vec2(p.x * 1.2 + sin(p.y * 3.0 + t) * 0.15, p.y * 0.9 - t * (0.35 + bass * 0.4));
    float n = fbm(q * 2.5 + vec2(0.0, t * 0.2));
    n += 0.35 * fbm(q * 5.0 - vec2(t * 0.15, 0.0));

    float flame = n - (p.y + 0.7) * 0.55 + bass * 0.35;
    flame = clamp(flame, 0.0, 1.0);
    flame = pow(flame, 1.4);

    vec3 c_cool = vec3(0.05, 0.0, 0.02);
    vec3 c_mid  = vec3(0.9, 0.25, 0.02);
    vec3 c_hot  = vec3(1.0, 0.9, 0.4);
    vec3 col = mix(c_cool, c_mid, flame);
    col = mix(col, c_hot, pow(flame, 2.5));

    // embers
    float ember = noise(vec2(p.x * 20.0 + t, p.y * 10.0 - t * 2.0));
    ember = pow(ember, 12.0) * step(p.y, 0.8);
    col += vec3(1.0, 0.7, 0.3) * ember * 2.0;

    // waveform heat shimmer at bottom
    float w = texture2D(iChannel0, vec2(uv.x, 0.0)).r;
    col += vec3(1.0, 0.4, 0.1) * exp(-(p.y + 1.0) * 3.0) * w * 0.5;

    gl_FragColor = vec4(col, 1.0);
}
