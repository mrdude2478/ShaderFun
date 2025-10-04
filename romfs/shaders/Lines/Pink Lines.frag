// Clean Waves without Blocks - converted for Nintendo Switch
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

vec3 COLOR1 = vec3(0.0, 0.0, 0.3);
vec3 COLOR2 = vec3(0.5, 0.0, 0.0);

void main() {
    vec2 uv = vUV;
    
    // Simple dark background instead of blocks
    vec3 bg_color = mix(COLOR1, COLOR2, uv.x) * 0.3; // Subtle gradient
    
    // To create the waves
    vec3 wave_color = vec3(0.0);
    float wave_width = 0.01;
    
    // Convert to normalized coordinates for waves
    vec2 wave_uv = -1.0 + 2.0 * uv;
    wave_uv.y += 0.1;
    
    for(float i = 0.0; i < 10.0; i++) {
        wave_uv.y += (0.07 * sin(wave_uv.x + i/7.0 + iTime));
        wave_width = abs(1.0 / (150.0 * wave_uv.y));
        wave_color += vec3(wave_width * 1.9, wave_width, wave_width * 1.5);
    }
    
    
    // Enhanced wave colors
    wave_color *= 0.2; // Brighten the waves
    
    vec3 final_color = bg_color + wave_color;
    
    gl_FragColor = vec4(final_color, 1.0);
}