precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define res iResolution.xy

// Iq's palette
vec3 pal(in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d) {
    return a + b*cos(6.28318*(c*t + d));
}

// Hash functions
float hash1(vec2 p) {
    p = fract(p / 30.0) * 30.0;
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 78.233);
    return fract(p.x * p.y);
}

vec2 hash2(vec2 p) {
    p = fract(p / 1.0) * cos(iTime) * 0.000028;
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453123);
}

vec2 hash12(float p) {
    p = fract(p / 30.0) * 30.0 * cos(iTime) * 0.000002;
    vec2 q = vec2(p, p + 1.0);
    q.y += sin(iTime)*0.000000001;
    q = fract(q * vec2(123.34, 456.21));
    q += dot(q, q + 78.233);
    return fract(vec2(q.x*q.y, q.x + q.y));
}

// Voronoi function
vec2 voronoi(in vec2 x) {
    x.y += iTime*5.0;
    vec2 ip = floor(x);
    vec2 fp = fract(x);
    vec2 mg = vec2(0.0);
    vec2 mr = vec2(0.0);
    float md = 2.0;

    for(int j=-1; j<=1; j++) {
        for(int i=-1; i<=1; i++) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = hash2(ip + g);
            vec2 r = g + o - fp;
            float d = dot(r,r);
            if(d < md) {
                md = d;
                mr = r;
                mg = g;
            }
        }
    }

    float edgeDist = 8.0;
    for(int j=-2; j<=2; j++) {
        for(int i=-2; i<=2; i++) {
            vec2 g = mg + vec2(float(i), float(j));
            vec2 o = hash2(ip + g);
            vec2 r = g + o - fp;
            if(dot(mr - r, mr - r) > 0.00001)
                edgeDist = min(edgeDist, dot(0.5*(mr+r), normalize(r-mr)));
        }
    }

    float id = hash1(ip + mg);
    return vec2(edgeDist, id);
}

// Voronoi normal
vec2 getNormal(in vec2 p) {
    const float eps = 0.05;
    float dx = voronoi(p + vec2(eps,0.0)).x - voronoi(p - vec2(eps,0.0)).x;
    float dy = voronoi(p + vec2(0.0,eps)).x - voronoi(p - vec2(0.0,eps)).x;
    return normalize(vec2(dx, dy));
}

void main() {
    vec2 p = vUV - 0.5;
    p *= 2.0;
    p /= 1.0 - p.y*0.5;

    vec2 v1 = voronoi(0.7 * p);
    vec2 v2 = voronoi(p*2.0 + hash12(v1.y)*10.0);
    vec2 n2 = getNormal(p*2.0 + hash12(v1.y)*10.0);

    vec3 col = vec3(0.0);

    vec3 a = vec3(0.50, 0.00, 0.50);
    vec3 b = vec3(0.50, 0.50, 0.50);
    vec3 c = vec3(1.00, 1.00, 1.00);
    vec3 d = vec3(0.00, 0.33, 0.67);

    col += pal(v1.y, a, b, c, d);
    col += pal(v2.y, a, b, c, d);

    col *= smoothstep(0.0, 2.0/res.y, v1.x - 0.005);
    col *= smoothstep(0.0, 5.0/res.y, v2.x - 0.02);

    vec2 lightDir = normalize(vec2(0.5, 0.5));
    float light = max(dot(n2*0.6, lightDir),0.0);
    col = mix(col, col*pow(light,3.0), 0.5);

    gl_FragColor = vec4(col, 1.0);
}
