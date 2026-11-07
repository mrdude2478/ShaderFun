// Hexagonal cells that pulse and shift hue with the beat
precision mediump float;
//!scroller off
//!audio off

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel1;
varying vec2 vUV;

vec2 hexCenter(vec2 p) {
    // approximate hex grid id
    vec2 s = vec2(1.0, 1.7320508);
    vec2 a = mod(p, s) - s * 0.5;
    vec2 b = mod(p + s * 0.5, s) - s * 0.5;
    return (dot(a, a) < dot(b, b)) ? a : b;
}

void main() {
    vec2 uv = (vUV - 0.5) * 2.0;
    uv.x *= iResolution.x / iResolution.y;
    float t = iTime;

    float bass = 0.0;
    for (int i = 0; i < 6; i++)
        bass += texture2D(iChannel1, vec2(float(i) / 48.0, 0.0)).r;
    bass *= 0.15;

    float scale = 4.0 + bass * 1.5;
    vec2 p = uv * scale;
    // slow drift
    p += vec2(sin(t * 0.2), cos(t * 0.15)) * 0.5;

    vec2 h = hexCenter(p);
    float d = max(abs(h.x), abs(h.x + h.y * 1.732) * 0.5);
    d = max(d, abs(h.y * 0.866));

    float edge = smoothstep(0.48, 0.42, d);
    float glow = exp(-d * 6.0);

    // per-cell id-ish from position
    float id = sin(floor(p.x * 1.3) * 12.1 + floor(p.y * 1.1) * 4.7);
    float pulse = 0.5 + 0.5 * sin(t * 3.0 + id * 6.0 + bass * 8.0);

    vec3 base = 0.5 + 0.5 * cos(vec3(0.0, 2.0, 4.0) + id + t * 0.5 + bass);
    vec3 col = base * edge * (0.35 + pulse * 0.65);
    col += base * glow * (0.2 + bass * 0.5);

    // darken outside a soft circle
    col *= smoothstep(1.6, 0.5, length(uv));
    gl_FragColor = vec4(col, 1.0);
}
