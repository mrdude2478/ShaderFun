precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

void main() {
    // Convert from [0,1] to [-1,1] and adjust aspect ratio
    vec2 uv = (vUV - 0.5) * 2.0;
    uv.x *= iResolution.x / iResolution.y;
    
    // Simple laser beam effect
    float time = iTime;
    
    // Create multiple laser beams
    vec3 color = vec3(0.0);
    
    for (int i = 0; i < 6; i++) {
        float beam = float(i) * 0.8;
        
        // Animated laser positions - centered around (0,0)
        float x = sin(time * 1.5 + beam * 2.0) * 0.8;
        float y = cos(time * 1.2 + beam * 1.5) * 0.6;
        
        // Laser beam with glow
        float dist = length(uv - vec2(x, y));
        float intensity = 0.03 / (dist * dist + 0.005);
        
        // Color cycling
        vec3 beamColor = 0.5 + 0.5 * cos(time * 2.0 + beam * 3.0 + vec3(0.0, 2.0, 4.0));
        color += beamColor * intensity;
    }
    
    // Add some background grid
    float grid = 0.1 + 0.1 * sin(uv.x * 15.0 + time) * sin(uv.y * 15.0 + time);
    color += grid * 0.05;
    
    // Add pulsing center
    float pulse = 0.15 / (length(uv) * length(uv) + 0.2);
    color += pulse * (0.5 + 0.5 * sin(time * 2.0)) * 0.4;
    
    gl_FragColor = vec4(color, 1.0);
}