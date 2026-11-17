// Soft flying triangular plates – muted blended colour
precision mediump float;
//!scroller off

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel1;
varying vec2 vUV;

vec3 hash33(vec3 p) {
    float n = sin(dot(p, vec3(7.0, 157.0, 113.0)));
    return fract(vec3(2097152.0, 262144.0, 32768.0) * n);
}

float sdTriPlate(vec3 p, float r) {
    const float k = 1.7320508;
    vec2 q = p.xy;
    float d = max(q.y, max(abs(q.x) * k + q.y * 0.5, -q.y) - r * 0.55);
    d = max(d, abs(p.z) - 0.035);
    return d;
}

float map(vec3 p) {
    vec3 id = floor(p);
    vec3 o = hash33(id) * 0.3;
    p = fract(p + o) - 0.5;
    float ang = hash33(id + 3.1).x * 6.28318;
    float ca = cos(ang), sa = sin(ang);
    p.xy = mat2(ca, sa, -sa, ca) * p.xy;
    float tilt = (hash33(id + 7.7).x - 0.5) * 0.8;
    float ct = cos(tilt), st = sin(tilt);
    p.yz = mat2(ct, st, -st, ct) * p.yz;
    float r = 0.22 + hash33(id + 1.0).x * 0.08;
    return sdTriPlate(p, r);
}

void main() {
    vec2 uv = (vUV * iResolution.xy - iResolution.xy * 0.5) / iResolution.y;
    vec3 rd = normalize(vec3(uv, (1.0 - dot(uv, uv) * 0.5) * 0.5));
    vec3 ro = vec3(0.0, 0.0, iTime * 2.4);
    vec3 col = vec3(0.0);
    float cs = cos(iTime * 0.28), si = sin(iTime * 0.28);
    rd.xz = mat2(cs, si, -si, cs) * rd.xz;
    rd.xy = mat2(cs, si, -si, cs) * rd.xy;
    rd *= 0.985 + hash33(rd) * 0.03;
    float t = 0.0, layers = 0.0, d, aD;
    float thD = 0.038;
    float bass = texture2D(iChannel1, vec2(0.02, 0.0)).r * 0.12;
    for (int i = 0; i < 48; i++) {
        if (layers > 14.0 || col.x > 1.0 || t > 10.0) break;
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
    col = mix(col, pow(col.x*vec3(1.4, 0.8, 1.0), vec3(1.0, 3.5, 14.0)), 
              dot(sin(rd.yzx*8.0 + sin(rd.zxy*8.0)), vec3(.1666)) + .4);
    
    // Color mixing - second pass
    col = mix(col, vec3(col.x*col.x*.85, col.x, col.x*col.x*.3), 
             dot(sin(rd.yzx*4.0 + sin(rd.zxy*4.0)), vec3(.1666)) + .25);
    
    // Final output
    gl_FragColor = vec4(max(col, 0.0), 1.0);
}
