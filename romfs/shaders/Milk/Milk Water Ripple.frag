// Watery ripple pool driven by spectrum
precision mediump float;
//!scroller off

uniform vec3 iResolution;
uniform float iTime;
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
    float t = iTime;
    float b = bass();

    // multi-source ripples
    float d1 = length(uv - vec2(-0.3, 0.2));
    float d2 = length(uv - vec2(0.4, -0.15));
    float d3 = length(uv);
    float wave = sin(d1 * 18.0 - t * 4.0) * 0.5
               + sin(d2 * 14.0 - t * 3.2 + 1.0) * 0.35
               + sin(d3 * 10.0 - t * 2.5) * 0.4;
    wave += b * sin(d3 * 8.0 - t * 6.0);

    vec3 deep = vec3(0.02, 0.08, 0.18);
    vec3 light = vec3(0.3, 0.7, 0.9);
    vec3 col = mix(deep, light, 0.5 + 0.5 * wave * 0.5);
    // specular sparkle
    float spec = pow(max(0.0, wave), 4.0);
    col += vec3(0.8, 0.95, 1.0) * spec * 0.35;
    col += vec3(0.2, 0.4, 0.6) * b * 0.3;

    col *= 1.0 - 0.3 * length(uv);
    gl_FragColor = vec4(col, 1.0);
}
