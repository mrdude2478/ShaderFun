// Simple LED Tunnel Effect
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    
    // Create tunnel coordinates
    float r = length(uv);
    float angle = atan(uv.y, uv.x);
    
    // Animated tunnel effect
    float tunnel = sin(1.0 / (r + 0.1) * 10.0 + angle * 5.0 + iTime * 2.0);
    
    // LED grid pattern
    vec2 grid = fract(uv * 15.0) - 0.5;
    float led = 1.0 - dot(grid, grid) * 10.0;
    led = max(led, 0.0);
    
    // Color variation
    vec3 color = mix(vec3(0.1, 0.8, 0.2), vec3(0.8, 0.2, 0.8), 0.5 + 0.5 * sin(iTime + angle));
    
    // Combine effects
    vec3 finalColor = color * led * tunnel * (1.0 - r);
    finalColor += vec3(0.05); // Background
    
    gl_FragColor = vec4(finalColor, 1.0);
}