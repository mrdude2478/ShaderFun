precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// Simple cosine palette
vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.4, 0.2);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.2, 0.3, 0.5);
    return a + b * cos(6.28318 * (c * t + d));
}

// 2D noise function
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f*f*(3.0-2.0*f);
    return mix(a, b, u.x) + (c - a)*u.y*(1.0 - u.x) + (d - b)*u.x*u.y;
}

void main() {
    vec2 uv = vUV * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;

    // Animate coordinates
    vec2 p = uv * 3.0 + vec2(iTime*0.2, iTime*0.3);

    // Layered noise
    float n = 0.0;
    n += noise(p);
    n += 0.5 * noise(p * 2.0 - iTime * 0.1);
    n += 0.25 * noise(p * 4.0 + iTime * 0.2);

    // Flow effect
    n = sin(n*6.28318 + iTime);

    // Map to color
    vec3 col = palette(n);

    gl_FragColor = vec4(col, 1.0);
}
