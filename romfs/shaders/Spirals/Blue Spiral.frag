precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

void main()
{
    vec2 uv = (vUV - 0.5) * 2.0;
    uv.x *= iResolution.x / iResolution.y;
    
    // Simple spiral galaxy
    float angle = atan(uv.y, uv.x);
    float radius = length(uv);
    
    // Create spiral arms
    float spiral = sin(angle * 5.0 + log(radius) * 10.0 - iTime * 2.0);
    spiral = smoothstep(0.3, 0.7, spiral);
    
    // Core glow
    float core = 1.0 - smoothstep(0.0, 0.3, radius);
    
    // Stars
    float stars = sin(uv.x * 100.0 + iTime) * sin(uv.y * 100.0);
    stars = max(0.0, stars) * 0.3;
    
    // Combine effects
    vec3 color = vec3(0.1, 0.2, 0.8) * spiral +    // Blue spiral arms
                 vec3(0.1, 0.2, 1.0) * core * 0.3 + // Small Blue core
                 vec3(1.0) * stars;                 // White stars
    
    gl_FragColor = vec4(color, 1.0);
}