precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// HSV to RGB color conversion
vec3 hsv(float h, float s, float v) {
    vec4 t = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(vec3(h) + t.xyz) * 6.0 - vec3(t.w));
    return v * mix(vec3(t.x), clamp(p - vec3(t.x), 0.0, 1.0), s);
}

void main() {
    // Convert UV coordinates to Shadertoy-style coordinates
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    
    vec4 color = vec4(0.0);
    float t = iTime;
    
    // Simplified ray marching that works with your coordinate system
    for(float i = 0.0; i < 32.0; i++) {
        vec3 pos = vec3(uv, i * 0.1);
        
        // Animate over time
        pos.x += sin(t * 0.5) * 0.5;
        pos.y += cos(t * 0.3) * 0.3;
        pos.z += t * 0.2;
        
        // Create procedural patterns
        float pattern = sin(pos.x * 8.0 + t) * 
                       cos(pos.y * 6.0) * 
                       sin(pos.z * 4.0 - t * 0.5);
        
        // HSV coloring
        vec3 hsvColor = hsv(length(pos) * 0.3 + t * 0.1, 0.8, 0.9);
        
        // Accumulate with fading
        color.rgb += hsvColor * pattern * exp(-i * 0.15) * 0.2;
    }
    
    gl_FragColor = vec4(color.rgb, 1.0);
}