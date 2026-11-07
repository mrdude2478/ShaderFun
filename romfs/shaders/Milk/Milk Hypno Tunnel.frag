// Hypnotic tunnel (classic Milkdrop energy tunnel)
precision mediump float;
//!scroller off
//!audio off

uniform vec3 iResolution;
uniform float iTime;
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
    // travel into tunnel
    float z = 1.0 / (r + 0.15) + t * (1.2 + b * 0.8);
    float rings = sin(z * 6.0 + a * 3.0);
    float spokes = sin(a * 8.0 + t * 2.0 + b);

    float pat = 0.5 + 0.5 * rings * 0.7 + 0.25 * spokes;
    vec3 col = mix(vec3(0.02, 0.0, 0.08), vec3(0.2, 0.6, 1.0), pat);
    col = mix(col, vec3(1.0, 0.4, 0.9), 0.5 + 0.5 * sin(z + a + t));

    // center bloom on bass
    col += vec3(0.5, 0.7, 1.0) * exp(-r * (3.0 - b * 1.5)) * (0.3 + b);
    col *= smoothstep(1.4, 0.2, r);
    gl_FragColor = vec4(col, 1.0);
}
