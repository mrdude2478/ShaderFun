// Blue Waves with Dark Background - Audio Reactive
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel0; // Waveform
uniform sampler2D iChannel1; // Spectrum
varying vec2 vUV;

void main() {
    vec2 uv = vUV;
    
    // Pure dark background
    vec3 bg_color = vec3(0.00, 0.00, 0.00);
    
    // Sample audio data for reactivity
    float waveform = texture2D(iChannel0, vec2(uv.x, 0.0)).r;
    float spectrum = texture2D(iChannel1, vec2(uv.x * 0.5, 0.0)).r;
    
    // Audio-reactive parameters
    float audioBoost = 1.0 + spectrum * 3.0; // Boost effect with spectrum
    float waveIntensity = 0.5 + waveform * 2.0; // Wave intensity reacts to waveform
    float timeFactor = iTime * (1.0 + spectrum * 2.0); // Time speed reacts to spectrum
    
    // Wave effect - now audio reactive blue
    vec3 wave_color = vec3(0.0);
    vec2 wave_uv = -1.0 + 2.0 * uv;
    wave_uv.y += 0.1;
    
    for(float i = 0.0; i < 2.0; i++) {
        // Audio-reactive wave distortion
        float audioDistortion = spectrum * 0.5 * sin(iTime * 2.0 + i);
        wave_uv.y += (0.07 * sin(wave_uv.x + i/7.0 + timeFactor + audioDistortion));
        
        // Audio-reactive wave width
        float baseWidth = 80.0;
        float audioWidth = baseWidth * (1.0 + waveform * 0.5);
        float wave_width = abs(1.0 / (audioWidth * wave_uv.y));
        
        // Audio-reactive blue colors
        float blueIntensity = 2.5 * audioBoost;
        float greenIntensity = 0.8 * (1.0 + spectrum * 0.5);
        float redIntensity = 0.3 * (1.0 + waveform * 0.3);
        
        wave_color += vec3(wave_width * redIntensity, 
                          wave_width * greenIntensity, 
                          wave_width * blueIntensity);
    }
    
    // Additional audio-reactive elements
    
    // Pulsing center glow based on bass frequencies (low spectrum)
    float bass = texture2D(iChannel1, vec2(0.1, 0.0)).r;
    vec2 center = uv - 0.5;
    float dist = length(center);
    float glow = (1.0 - dist * 2.0) * bass * 2.0;
    vec3 glow_color = vec3(0.0, 0.3, 0.8) * glow;
    
    // High frequency sparkles
    float highs = texture2D(iChannel1, vec2(0.8, 0.0)).r;
    float sparkle = sin(uv.x * 50.0 + iTime * 10.0) * sin(uv.y * 30.0 + iTime * 8.0);
    sparkle = max(0.0, sparkle) * highs * 0.3;
    vec3 sparkle_color = vec3(0.2, 0.6, 1.0) * sparkle;
    
    // Rhythm pulses based on overall volume
    float rhythm = waveform * 0.5 + spectrum * 0.5;
    float pulse = sin(iTime * 5.0) * 0.5 + 0.5;
    float rhythm_pulse = rhythm * pulse * 0.3;
    vec3 pulse_color = vec3(0.1, 0.4, 0.9) * rhythm_pulse;
    
    // Enhanced wave colors with audio reactivity
    wave_color *= waveIntensity;
    
    // Combine all elements
    vec3 final_color = bg_color + wave_color + glow_color + sparkle_color + pulse_color;
    
    // Audio-reactive brightness boost
    final_color *= (1.0 + rhythm * 0.2);
    
    gl_FragColor = vec4(final_color, 1.0);
}

/*
You can change the number of lines by modifying the loop counter in the for loop. Look for this line:
for(float i = 0.0; i < 8.0; i++)

You might also want to adjust the wave_width and spacing parameters to make the lines look good with your chosen count:
For fewer lines (3-5): Increase wave width for thicker lines:
float baseWidth = 80.0; // Instead of 150.0

For more lines (15+): Decrease wave width for thinner lines:
float baseWidth = 200.0; // Instead of 150.0
*/