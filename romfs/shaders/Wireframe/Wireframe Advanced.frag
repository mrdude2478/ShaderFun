// Optimized 3D Wireframes for Switch
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// Simple line function
float line(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    float dist = length(pa - ba * h);
    return 0.01 / (dist * dist + 0.001);
}

// Rotation matrix
mat2 rot(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    vec3 col = vec3(0.1);
    
    float time = iTime * 0.5;
    
    // Create a simple 3D cube projection
    for(int i = 0; i < 4; i++) {
        float scale = 0.8 + float(i) * 0.3;
        float angle = time * (0.5 + float(i) * 0.2);
        
        // Cube corners in 2D projection
        vec2 corners[8];
        mat2 r = rot(angle);
        
        // Front face
        corners[0] = r * vec2(-scale, -scale);
        corners[1] = r * vec2(-scale,  scale);
        corners[2] = r * vec2( scale,  scale);
        corners[3] = r * vec2( scale, -scale);
        
        // Back face (slightly smaller and offset)
        corners[4] = r * vec2(-scale*0.7, -scale*0.7) + vec2(0.2, 0.1);
        corners[5] = r * vec2(-scale*0.7,  scale*0.7) + vec2(0.2, 0.1);
        corners[6] = r * vec2( scale*0.7,  scale*0.7) + vec2(0.2, 0.1);
        corners[7] = r * vec2( scale*0.7, -scale*0.7) + vec2(0.2, 0.1);
        
        // Draw cube edges
        vec3 cubeColor = 0.5 + 0.5 * cos(vec3(0.0, 2.0, 4.0) + float(i));
        
        // Front face
        col += cubeColor * line(uv, corners[0], corners[1]);
        col += cubeColor * line(uv, corners[1], corners[2]);
        col += cubeColor * line(uv, corners[2], corners[3]);
        col += cubeColor * line(uv, corners[3], corners[0]);
        
        // Back face
        col += cubeColor * 0.7 * line(uv, corners[4], corners[5]);
        col += cubeColor * 0.7 * line(uv, corners[5], corners[6]);
        col += cubeColor * 0.7 * line(uv, corners[6], corners[7]);
        col += cubeColor * 0.7 * line(uv, corners[7], corners[4]);
        
        // Connecting edges
        col += cubeColor * 0.5 * line(uv, corners[0], corners[4]);
        col += cubeColor * 0.5 * line(uv, corners[1], corners[5]);
        col += cubeColor * 0.5 * line(uv, corners[2], corners[6]);
        col += cubeColor * 0.5 * line(uv, corners[3], corners[7]);
    }
    
    // Add some pulsing background
    col += vec3(0.05) * (0.5 + 0.5 * sin(time * 3.0));
    
    gl_FragColor = vec4(col, 1.0);
}