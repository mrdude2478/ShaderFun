precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float tanh_manual(float x) {
    float ex = exp(x);
    float emx = exp(-x);
    return (ex - emx) / (ex + emx);
}

vec4 tanh_manual(vec4 x) {
    return vec4(tanh_manual(x.x), tanh_manual(x.y), tanh_manual(x.z), tanh_manual(x.w));
}

void main() {
    vec2 u = vUV * iResolution.xy;
    vec3 q, p = iResolution;
    
    float i, s, d = 0.1 * rand(u), t = iTime * 0.7;
    vec4 o = vec4(0.0);
    
    u = (u + u - p.xy) / p.y;
    
    for(i = 0.0; i < 50.0; i++) {
        if(i > 49.0) break;
        
        q = p = vec3(u * d, d - 16.0);
        
        float angle = t + p.z * 0.2;
        float cosA = cos(angle);
        float sinA = sin(angle);
        vec2 rotated = vec2(
            p.x * cosA - p.y * sinA,
            p.x * sinA + p.y * cosA
        );
        p.xy = rotated;
        
        float ss;
        for (ss = 1.0; ss < 4.0; ss += 1.0) {
            q += sin(0.3 * t + p.xzy * ss * 0.3) * 0.2;  // Was 0.3
            p += sin(0.4 * t + q.yzx * ss * 0.4) * 0.2;  // Was 0.3
        }
        
        vec3 p_floor = p - floor(p) - 0.5;
        s = abs(min(dot(abs(p_floor), vec3(1.0)),
                   max(length(p) - 6.0, length(q) - 6.0)));
        
        s = 0.005 + abs(mix(s, 0.001 / abs(p.y), length(u)));
        d += s * 1.5;  // Was just s (adds 50% larger steps)
        
        o += (1.0 + cos(p.z + vec4(1.0, 2.0, 2.0, 0.0))) / s;
    }
    
    o = o * o / 8e7;
    o = o / (1.0 + o);  // Simple Reinhard instead of tanh
    gl_FragColor = o;
}