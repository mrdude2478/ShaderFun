precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
uniform float iAudioLevel;
uniform float iAudioBass;
uniform float iAudioMid;
uniform float iAudioHigh;
varying vec2 vUV;

// 2D rotation function
mat2 rotate(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

// Random hash function
float hash(float n) {
    return fract(sin(n) * 3758.5453);
}

// Generate vibrant random palette
vec3 randomPalette(float seed) {
    vec3 a = 0.3 + 0.7 * vec3(hash(seed), hash(seed + 1.0), hash(seed + 2.0));
    vec3 b = 0.3 + 0.7 * vec3(hash(seed + 3.0), hash(seed + 4.0), hash(seed + 5.0));
    vec3 c = vec3(0.8, 0.9, 1.0);
    vec3 d = 2.0 * vec3(hash(seed + 6.0), hash(seed + 7.0), hash(seed + 8.0)) - 1.0;
    
    return a + b * cos(6.28318 * (c + d));
}

// Simple low-pass filter using time-based smoothing
float smoothAudio(float audioValue, float seed) {
    float slowTime = iTime * 0.3;
    return audioValue * (0.3 + 0.7 * sin(slowTime + seed) * 0.5 + 0.5);
}

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv;
    
    // Apply slower smoothing to audio values
    float slowLevel = smoothAudio(iAudioLevel, 1.0);
    float slowBass = smoothAudio(iAudioBass, 2.0);
    float slowMid = smoothAudio(iAudioMid, 3.0);
    float slowHigh = smoothAudio(iAudioHigh, 4.0);
    
    // BEAT ZOOM EFFECT - Add this section
    float beatZoom = 1.0 + slowBass * 0.8; // Zoom factor based on bass
    uv *= beatZoom;                        // Apply zoom to coordinates
    uv0 *= beatZoom;                       // Apply zoom to both UV sets
    
    // Much slower rotation
    float rotationSpeed = 0.3 + slowBass * 0.3;
    float rotationAngle = iTime * 0.2 * rotationSpeed;
    float rotationAngle2 = iTime * 0.15 + slowMid * 0.1;
    uv = uv * rotate(rotationAngle);
    uv0 = uv0 * rotate(rotationAngle2);
    
    // Much slower palette cycling
    float cycleDuration = 12.0 - slowLevel * 2.0;
    float cyclePos = iTime * 0.3 / cycleDuration;
    float currentCycle = floor(cyclePos);
    float nextCycle = currentCycle + 1.0;
    float blend = smoothstep(0.0, 1.0, fract(cyclePos));
    
    // Slower palette changes
    vec3 pal1 = randomPalette(currentCycle * 15.0 + slowBass * 3.0);
    vec3 pal2 = randomPalette(nextCycle * 15.0 + slowHigh * 3.0);
    
    vec3 finalColor = vec3(0.0);
    
    // Reduced audio influence on random range
    float minValue = 0.6 + slowBass * 0.1;
    float maxValue = 1.0 + slowHigh * 0.2;
    
    // Stable iteration count
    float iterations = 2.0 + slowLevel * 1.0;
    
    for (float i = 0.0; i < iterations; i++) {
        // Much slower random changes
        float audioSeed = i + iTime * 0.02 + slowMid * 0.05 + 123.45;
        float randomScale = minValue + (maxValue - minValue) * hash(audioSeed);
        
        uv = fract(uv * randomScale) - 0.5; //use this for a bouncier music effect
        //uv = fract(uv * 1.5) - 0.5; //use this for a smoother grid effect

        // Reduced audio distortion
        float audioDistortion = slowLevel * 0.2;
        float d = length(uv) * exp(-length(uv0) * (2.0 - audioDistortion)); //circles

        // Slower color pattern changes
        float pattern = length(uv0) + i * 0.3;
        vec3 col1 = 0.5 + 0.5 * cos(4.28318 * (pattern + pal1));
        vec3 col2 = 0.5 + 0.5 * cos(6.28318 * (pattern + pal2));
        vec3 col = mix(col1, col2, blend);
        
        /*
        The sin() function creates the oscillating ring patterns
        abs() makes sure the values are positive
        pow(0.01 / d, 1.2) creates the bright outlines and glow effect
        */

        // Slower wave distortion
        float waveFreq = 6.0 + slowHigh * 5.0;
        float waveSpeed = iTime * 0.5;
        d = sin(d * waveFreq + waveSpeed) / (5.0 - slowLevel * 1.0);
        d = abs(d);
        
        // Reduced intensity variation
        float intensity = 1.1 + slowLevel * 0.3;
        d = pow(0.01 / d, intensity);

        finalColor += col * d;
    }
    
    // Minimal brightness boost
    finalColor *= (1.0 + slowLevel * 0.1);
        
    gl_FragColor = vec4(finalColor, 1.0);
}