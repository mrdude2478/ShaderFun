// Radial beat flower / soft kaleidoscope
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

    float r = length(uv);
    float a = atan(uv.y, uv.x);
    // fold into petals
    float petals = 6.0;
    float af = abs(fract((a / 6.28318) * petals) - 0.5);
    float shape = smoothstep(0.15 + b * 0.1, 0.0, af - r * 0.15);
    float pulse = 0.5 + 0.5 * sin(r * 12.0 - t * 4.0 - b * 6.0);

    vec3 col = vec3(0.02, 0.02, 0.05);
    vec3 petalCol = mix(vec3(0.9, 0.2, 0.5), vec3(0.3, 0.6, 1.0), pulse);
    col += petalCol * shape * (0.6 + b * 0.8);
    col += vec3(1.0, 0.8, 0.5) * exp(-r * 4.0) * (0.2 + b * 0.6);

    // outer rings
    float ring = smoothstep(0.03, 0.0, abs(r - 0.55 - b * 0.15));
    col += petalCol * ring * 0.5;

    gl_FragColor = vec4(col, 1.0);
}
