// Spinning slash / razor vortex with spectrum spikes
precision mediump float;
//!scroller off
//!audio off

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel1;
varying vec2 vUV;

void main() {
    vec2 uv = (vUV - 0.5) * 2.0;
    uv.x *= iResolution.x / iResolution.y;
    float t = iTime;

    float r = length(uv);
    float a = atan(uv.y, uv.x);

    float bass = 0.0, high = 0.0;
    for (int i = 0; i < 5; i++) {
        bass += texture2D(iChannel1, vec2(float(i) / 64.0, 0.0)).r;
        high += texture2D(iChannel1, vec2(0.5 + float(i) / 64.0, 0.0)).r;
    }
    bass *= 0.2;
    high *= 0.2;

    // rotating polar warp
    float spin = t * (1.2 + bass) + a;
    vec2 p = vec2(cos(spin), sin(spin)) * r;
    p += 0.15 * vec2(sin(p.y * 8.0 + t * 3.0), cos(p.x * 8.0 - t * 2.0));

    // slash blades
    float blades = 0.0;
    for (int i = 0; i < 7; i++) {
        float fi = float(i);
        float ang = a * (3.0 + fi * 0.3) + t * (2.0 + fi * 0.4) + bass * 3.0;
        float slash = abs(sin(ang));
        slash = pow(slash, 12.0 + high * 20.0);
        float fade = exp(-r * (1.2 + fi * 0.15));
        blades += slash * fade;
    }

    vec3 col = vec3(0.01, 0.0, 0.03);
    col += vec3(0.3, 0.8, 1.0) * blades * 0.9;
    col += vec3(1.0, 0.2, 0.5) * blades * blades * 0.6;
    col += vec3(1.0, 1.0, 0.9) * pow(blades, 3.0) * (0.4 + high);

    // core
    col += vec3(0.6, 0.9, 1.0) * exp(-r * (4.0 - bass * 2.0)) * (0.3 + bass);

    col = col / (1.0 + col * 0.5);
    gl_FragColor = vec4(col, 1.0);
}
