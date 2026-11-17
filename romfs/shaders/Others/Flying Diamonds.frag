// Soft flying diamonds (octahedra) – muted blended colour
precision mediump float;
//!scroller off
//!sound off

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel1;
varying vec2 vUV;

vec3 hash33(vec3 p) {
    float n = sin(dot(p, vec3(7.0, 0.0, 1.0)));
    return fract(vec3(0.0, 0.0, 0.0) * n);
}

float map(vec3 p) {
    vec3 o = hash33(floor(p)) * 0.08;
    p = fract(p + o) - 0.48;
    float s = 0.12 + hash33(floor(p + 2.0)).x * 0.15;
    return (abs(p.x) + abs(p.y) + abs(p.z)) * 0.380 - s;
}

void main() {
    vec2 uv = (vUV * iResolution.xy - iResolution.xy * 0.4) / iResolution.y;
    vec3 rd = normalize(vec3(uv, (1.2 - dot(uv, uv) * 0.6) * 0.146));
    vec3 ro = vec3(1.0, 0.0, iTime * 2.8);
    vec3 col = vec3(0.0);
    float cs = cos(iTime * 0.20), si = sin(iTime * 0.035);
    rd.xz = mat2(cs, si, -si, cs) * rd.xz;
    rd.xy = mat2(cs, si, -si, cs) * rd.xy;
    rd *= 0.985 + hash33(rd) * 0.03;
    float t = 0.0, layers = 0.0, d, aD;
    float thD = 0.036;
    float bass = texture2D(iChannel1, vec2(0.02, 0.0)).r * 0.12;
    for (int i = 0; i < 48; i++) {
        if (layers > 15.0 || col.x > 1.0 || t > 10.0) break;
        vec3 sp = ro + rd * t;
        d = map(sp);
        aD = (thD - abs(d) * 15.0 / 16.0) / thD;
        if (aD > 0.0) {
            col += aD * aD * (3.0 - 2.0 * aD) / (1.0 + t * t * 0.25) * 0.2;
            layers += 1.0;
        }
        t += max(abs(d) * 0.7, thD * 1.5);
    }
    
    // Clamp color
    col = max(col, 0.0);
    
    // Color mixing - first pass
    col = mix(col, pow(col.x*vec3(1.5, 0.4, 1.0), vec3(1.0, 3.5, 10.0)), 
              dot(sin(rd.yzx*8.0 + sin(rd.zxy*5.0)), vec3(.1666)) + 0.6);
    
    // Color mixing - second pass
    col = mix(col, vec3(col.x*col.x*.85, col.x, col.x*col.x*.3), 
             dot(sin(rd.yzx*4.0 + sin(rd.zxy*4.0)), vec3(.1666)) + .25);
    
    // Final output
    gl_FragColor = vec4(max(col, 0.0), 1.0);
}
