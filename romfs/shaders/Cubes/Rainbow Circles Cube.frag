precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

void main() {
    vec2 C = vUV * iResolution.xy;
    vec2 r = iResolution.xy;
    
    float t = iTime;
    float B = t * 1.9;
    float F = sqrt(fract(B));
    float T = floor(B) + F;
    
    vec3 o = vec3(0.0);
    vec3 U = vec3(1.0, 2.0, 3.0);
    
    // Simple rotation
    float angle = 0.3 * t;
    float ca = cos(angle);
    float sa = sin(angle);
    
    float z = 0.0;
    bool hit = false;
    
    // Rainbow color cycle
    float hue = t * 0.5; // Speed of color cycling
    
    for (int i = 0; i < 40; i++) {
        if (i > 0) z += 0.15;
        
        vec3 rd = normalize(vec3(C - 0.5 * r, r.y));
        vec3 p = rd * z;
        p.z -= 4.0;
        
        // Apply rotation to all axes
        p.xy = vec2(p.x * ca - p.y * sa, p.x * sa + p.y * ca);
        p.xz = vec2(p.x * ca - p.z * sa, p.x * sa + p.z * ca);
        p.yz = vec2(p.y * ca - p.z * sa, p.y * sa + p.z * ca);
        
        // Larger cube with proper distance function
        vec3 q = abs(p) - 0.7;
        float d = length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
        
        if (d < 0.08) {
            // Rainbow colors based on position and time
            vec3 rainbow;
            
            // Convert HSV to RGB for rainbow effect
            float hueOffset = length(p) * 2.0 + hue; // Rainbow based on distance + time
            hueOffset = fract(hueOffset); // Keep in 0-1 range
            
            // HSV to RGB conversion
            vec3 rgb = clamp(abs(mod(hueOffset * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
            rainbow = rgb * rgb * (3.0 - 2.0 * rgb); // Smooth gradient
            
            // Smooth edge with better falloff
            float edge = 1.0 - smoothstep(0.0, 0.08, d);
            o = rainbow * edge;
            hit = true;
            break;
        }
    }
    
    // Pure black background if no hit
    if (!hit) {
        o = vec3(0.0);
    }
    
    gl_FragColor = vec4(o, 1.0);
}