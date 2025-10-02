// Blue Waves with Dark Background
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

void main() {
    vec2 uv = vUV;
    
    // Pure dark background
    vec3 bg_color = vec3(0.00, 0.00, 0.00);
    
    // Wave effect - now blue
    vec3 wave_color = vec3(0.0);
    vec2 wave_uv = -1.0 + 2.0 * uv;
    wave_uv.y += 0.1;
    
    for(float i = 0.0; i < 8.0; i++) {
        wave_uv.y += (0.07 * sin(wave_uv.x + i/7.0 + iTime));
        float wave_width = abs(1.0 / (150.0 * wave_uv.y));
        // Changed to blue: reduce red, keep some green, boost blue
        wave_color += vec3(wave_width * 0.3, wave_width * 0.8, wave_width * 2.5);
        //deep blue
        //wave_color += vec3(wave_width * 0.1, wave_width * 0.3, wave_width * 3.0);
        
        //electric blue
        //wave_color += vec3(wave_width * 0.0, wave_width * 1.0, wave_width * 3.0);
        
        //cyan blue
        //wave_color += vec3(wave_width * 0.0, wave_width * 1.5, wave_width * 2.0); 
    }
    
    // Enhanced wave colors
    wave_color *= 0.5; // Adjust brightness as needed
    
    vec3 final_color = bg_color + wave_color;
    
    gl_FragColor = vec4(final_color, 1.0);
}