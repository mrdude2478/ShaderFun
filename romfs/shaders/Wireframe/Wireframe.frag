// Simple rotating wireframes for Switch
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    
    float time = iTime;
    vec3 color = vec3(0.05);
    
    // Create rotating grid pattern
    for(int i = 0; i < 4; i++) {
        float angle = time + float(i) * 1.57;
        vec2 dir = vec2(cos(angle), sin(angle));
        
        // Grid lines
        float grid1 = 0.02 / abs(uv.x * dir.x + uv.y * dir.y - sin(time * 2.0) * 0.5);
        float grid2 = 0.02 / abs(uv.x * dir.y - uv.y * dir.x - cos(time * 1.5) * 0.3);
        
        vec3 lineColor = 0.5 + 0.5 * cos(vec3(0.0, 2.0, 4.0) + float(i));
        color += lineColor * min(grid1 + grid2, 1.0) * 0.3;
    }
    
    gl_FragColor = vec4(color, 1.0);
}