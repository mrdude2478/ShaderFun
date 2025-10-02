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

// Generate random palette base colors
vec3 randomPalette(float seed) {
    vec3 a = vec3(hash(seed), hash(seed + 1.0), hash(seed + 2.0));
    vec3 b = vec3(hash(seed + 3.0), hash(seed + 4.0), hash(seed + 5.0));
    vec3 c = vec3(1.0, 1.0, 1.0); // Keep frequencies consistent
    vec3 d = vec3(hash(seed + 6.0), hash(seed + 7.0), hash(seed + 8.0));
    
    return a + b * cos(6.28318 * (c * 1.0 + d));
}

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv;
    
    // Simple continuous rotation
    float rotationAngle = iTime * 0.5;
    uv = uv * rotate(rotationAngle);
    uv0 = uv0 * rotate(rotationAngle);
    
    // Palette cycle every 4 seconds with smooth fade
    float cycleTime = 4.0;
    float cycleProgress = fract(iTime / cycleTime);
    float currentCycle = floor(iTime / cycleTime);
    float nextCycle = currentCycle + 1.0;
    
    // Smooth fade between palettes
    float fade = smoothstep(0.0, 1.0, cycleProgress);
    
    // Generate random palettes for current and next cycle
    vec3 palette1 = randomPalette(currentCycle * 123.456);
    vec3 palette2 = randomPalette(nextCycle * 123.456);
    
    vec3 finalColor = vec3(0.0);
    
    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;

        float d = length(uv) * exp(-length(uv0));

        // Smoothly blend between two random palettes
        float t = length(uv0) + i * 0.4 + iTime * 0.4;
        
        // Generate colors from both palettes and blend
        vec3 col1 = palette1 * (0.7 + 0.3 * cos(6.28318 * t + vec3(0.0, 0.333, 0.667)));
        vec3 col2 = palette2 * (0.7 + 0.3 * cos(6.28318 * t + vec3(0.0, 0.333, 0.667)));
        vec3 col = mix(col1, col2, fade);

        d = sin(d * 8.0 + iTime) / 5.0;
        d = abs(d);
        d = pow(0.01 / d, 1.2);

        finalColor += col * d;
    }
        
    gl_FragColor = vec4(finalColor, 1.0);
}