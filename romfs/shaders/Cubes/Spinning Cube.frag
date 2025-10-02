precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

void main() {
    vec2 C = vUV * iResolution.xy;
    vec2 r = iResolution.xy;
    
    float t = iTime;
    float angle = 0.3 * t;
    float ca = cos(angle);
    float sa = sin(angle);
    
    vec3 o = vec3(0.0);
    float z = 0.0;
    bool hit = false;
    
    for (int i = 0; i < 40; i++) {
        if (i > 0) z += 0.15;
        
        vec3 rd = normalize(vec3(C - 0.5 * r, r.y));
        vec3 p = rd * z;
        p.z -= 4.0;
        
        p.xy = vec2(p.x * ca - p.y * sa, p.x * sa + p.y * ca);
        p.xz = vec2(p.x * ca - p.z * sa, p.x * sa + p.z * ca);
        p.yz = vec2(p.y * ca - p.z * sa, p.y * sa + p.z * ca);
        
        vec3 q = abs(p) - 0.7;
        float d = length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
        
        if (d < 0.08) {
            // Simple RGB cycle
            vec3 color = 0.5 + 0.5 * sin(vec3(1.0, 2.0, 3.0) + t * 2.0);
            float edge = 1.0 - smoothstep(0.0, 0.08, d);
            o = color * edge;
            hit = true;
            break;
        }
    }
    
    if (!hit) o = vec3(0.0);
    
    gl_FragColor = vec4(o, 1.0);
}