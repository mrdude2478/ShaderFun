precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

//https://iquilezles.org/articles/palettes/
vec3 palette(float t) {
    vec3 a = vec3(1.0, 0.7, 0.2);
    vec3 b = vec3(0.6, 1.0, 1.0);
    vec3 c = vec3(0.6, 0.8, 1.0);
    vec3 d = vec3(0.263, 0.616, 0.557);

    return a + b * cos(6.28318 * (c * t + d));
}

// 2D rotation function
mat2 rotate(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv;
    
    // Apply horizontal flip with more noticeable timing
    float flipFrequency = 2.0; // Flips every 2 seconds
    float flip = sin(iTime * flipFrequency) > 0.0 ? 1.0 : -1.0;
    
    // Apply flip FIRST, then rotation (more noticeable)
    uv.x *= flip;
    uv0.x *= flip;
    
    // Then apply rotation
    float rotationAngle = iTime * 0.5;
    uv = uv * rotate(rotationAngle);
    uv0 = uv0 * rotate(rotationAngle);
    
    vec3 finalColor = vec3(0.0);
    
    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;

        float d = length(uv) * exp(-length(uv0));

        vec3 col = palette(length(uv0) + i * 0.4 + iTime * 0.4);

        d = sin(d * 8.0 + iTime) / 5.0;
        d = abs(d);

        d = pow(0.01 / d, 1.2);

        finalColor += col * d;
    }
        
    gl_FragColor = vec4(finalColor, 1.0);
}

/*
For faster rotation: Increase the multiplier:
float rotationAngle = iTime * 1.0; // Faster rotation

For slower flip frequency: Adjust the flip speed:
float flip = sin(iTime * 0.3) > 0.0 ? 1.0 : -1.0; // Slower flipping

For continuous flip (no back-and-forth):
float flip = -1.0; // Always flipped
// or
float flip = 1.0; // Never flipped
*/

