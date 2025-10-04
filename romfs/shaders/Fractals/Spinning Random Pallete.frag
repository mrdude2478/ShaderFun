precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// 2D rotation function
mat2 rotate(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

// Random hash function
float hash(float n) {
    return fract(sin(n) * 43758.5453);
}

// Generate vibrant random palette
vec3 randomPalette(float seed) {
    // Create more vibrant colors by expanding the range
    vec3 a = 0.3 + 0.7 * vec3(hash(seed), hash(seed + 1.0), hash(seed + 2.0));
    vec3 b = 0.3 + 0.7 * vec3(hash(seed + 3.0), hash(seed + 4.0), hash(seed + 5.0));
    vec3 c = vec3(0.8, 0.9, 1.0); // Slightly different frequencies for each channel
    vec3 d = 2.0 * vec3(hash(seed + 6.0), hash(seed + 7.0), hash(seed + 8.0)) - 1.0;
    
    return a + b * cos(6.28318 * (c + d));
}

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv;
    
    // Rotation
    float rotationAngle = iTime * 0.5;
    uv = uv * rotate(rotationAngle);
    uv0 = uv0 * rotate(rotationAngle);
    
    // Palette cycling with smooth transitions
    float cycleDuration = 5.0; // Change palette every 5 seconds
    float cyclePos = iTime / cycleDuration;
    float currentCycle = floor(cyclePos);
    float nextCycle = currentCycle + 1.0;
    float blend = smoothstep(0.0, 1.0, fract(cyclePos));
    
    // Get current and next palettes
    vec3 pal1 = randomPalette(currentCycle * 99.9);
    vec3 pal2 = randomPalette(nextCycle * 99.9);
    
    vec3 finalColor = vec3(0.0);
    
    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;

        float d = length(uv) * exp(-length(uv0));

        // Create color with smooth palette transition
        float pattern = length(uv0) + i * 0.4;
        vec3 col1 = 0.5 + 0.5 * cos(6.28318 * (pattern + pal1));
        vec3 col2 = 0.5 + 0.5 * cos(6.28318 * (pattern + pal2));
        vec3 col = mix(col1, col2, blend);

        d = sin(d * 8.0 + iTime) / 5.0;
        d = abs(d);
        d = pow(0.01 / d, 1.2);

        finalColor += col * d;
    }
        
    gl_FragColor = vec4(finalColor, 1.0);
}