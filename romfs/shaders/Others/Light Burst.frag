precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// Cosine-based palette function
vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263, 0.416, 0.557);
    return a + b * cos(6.28318 * (c * t + d));
}

void main() {
    // Normalize UV coordinates to range ~[-1,1]
    vec2 uv = (vUV * 2.0 - 1.0) * (iResolution.x / iResolution.y);
    vec2 uv0 = uv;
    vec3 col = vec3(0.0);
    vec3 finalColor = vec3(0.0);

    // Only one iteration here, but kept for structure
    uv = fract(uv * 0.4) - 0.5;
    float d = length(uv / 0.2) * exp(-length(uv0));
    col = palette(length((uv * iTime) / 100.0));

    d = tan(d * 2.0 + iTime) / 2.0;
    finalColor += d * col;

    gl_FragColor = vec4(finalColor, 1.0);
}
