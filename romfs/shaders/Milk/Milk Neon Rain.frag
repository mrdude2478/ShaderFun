// Neon digital rain / streak field, audio reactive speed
precision mediump float;
//!scroller off
//!audio off

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel1;
varying vec2 vUV;

float hash(float n) {
    return fract(sin(n) * 43758.5453);
}

void main() {
    vec2 uv = vUV;
    float t = iTime;

    float bass = 0.0;
    for (int i = 0; i < 5; i++)
        bass += texture2D(iChannel1, vec2(float(i) / 64.0, 0.0)).r;
    bass *= 0.2;

    float speed = 0.25 + bass * 0.5;
    vec3 col = vec3(0.01, 0.02, 0.04);

    // columns of falling glyphs-ish streaks
    float cols = 36.0;
    float cx = floor(uv.x * cols);
    float x = (cx + 0.5) / cols;
    float rnd = hash(cx * 13.7);

    float fall = fract(uv.y + t * speed * (0.5 + rnd) + rnd);
    // head of streak
    float head = smoothstep(0.08, 0.0, fall) * step(0.0, fall);
    float trail = exp(-fall * 8.0) * (1.0 - fall);

    // thickness
    float w = abs(uv.x - x) * cols;
    float line = smoothstep(0.45, 0.05, w);

    float mid = texture2D(iChannel1, vec2(cx / cols * 0.8, 0.0)).r;
    vec3 neon = mix(vec3(0.1, 1.0, 0.4), vec3(0.4, 0.8, 1.0), rnd);
    neon = mix(neon, vec3(1.0, 0.3, 0.8), mid);

    col += neon * trail * line * (0.6 + mid);
    col += vec3(0.8, 1.0, 0.9) * head * line * (1.0 + bass);

    // horizontal glitch flashes on bass
    float glitch = step(0.92, hash(floor(t * 8.0) + cx)) * bass;
    col += vec3(1.0) * glitch * line * 0.5;

    gl_FragColor = vec4(col, 1.0);
}
