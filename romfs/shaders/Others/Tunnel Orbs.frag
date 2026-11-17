// i tried to make it run better on non-mega-gpus
// let me know how performance is if you don't mind :D
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)

precision mediump float;

uniform vec3  iResolution;
uniform float iTime;

#define T (iTime*6.)
#define P(z) vec3(cos((z) *  .05) * 16., \
                 cos((z) *  .1) * 8., (z))
#define R(a) mat2(cos(a+vec4(0,33,11,0)))
#define N normalize

float orb(vec3 p) {
    vec3 q = P(p.z);
    return length(p - vec3(
            q.x+(sin(p.z*.4) * .4),
            q.y+sin(sin(p.z*.3)+T) * .5,
           5.+T+tan(cos(T*.2)*.5)*3.2));
}

#define MENGERLAYER(scale, minmax, hole)\
    s /= (scale), \
    p = abs(fract(q/s)*s - s*.5), \
    d = minmax(d, min(max(p.x, p.y), \
                  min(max(p.y, p.z), \
                  max(p.x, p.z))) - s/(hole))

float fractal(vec3 q){
    vec3 p;
    float s = 4., d = 9e9;
    MENGERLAYER(1., min, 6.);
    MENGERLAYER(4., max, 3.5);
    return d;
}

// Returns (distance, lightContribution) instead of using a global.
// ES 2.0 does not reliably support writing a global from a function
// called inside a loop, which produced the "TV static" artifact.
vec2 map(vec3 p) {
    float s, o;
    vec3 q = p;
    p.xy -= P(p.z).xy;
    p.y += .1;
    s = max(1. - abs(p.x), 1. - abs(p.y));
    s = min(s, fractal(p));
    o = orb(q) - .01;
    s = min(s, o);
    float lightContribution = 1.0 / max(o, 0.001);
    return vec2(min(o, max(-q.y - 5.35, s)), lightContribution);
}

vec4 tanh4(vec4 x) {
    // Stable tanh: never overflows, and all channels saturate identically.
    vec4 ax = abs(x);
    vec4 e  = exp(-2.0 * ax);            // e in (0,1], no overflow
    vec4 t  = (1.0 - e) / (1.0 + e);     // = tanh(|x|)
    return sign(x) * t;
}

void main() {
    float s, d, a;
    vec3 r = iResolution;
    vec2 u = gl_FragCoord.xy;

    vec3 p = P(T), ro = p,
         Z = N( P(T+3.) - p),
         X = N(vec3(Z.z, 0.0, -Z)),
         D = vec3(R(sin(T*.2)*.3)*(u-r.xy/2.)/r.y, 1.0)
             * mat3(-X, cross(X, Z), Z);

    vec4 o = vec4(0.0);
    float light = 0.0;

    // Explicit initialisation of d and s (ES 2.0 safe).
    d = 0.0;
    s = 0.01;

    for (int i = 0; i < 50; i++) {
        if (d >= 50.0) break;

        p = ro + D * d;
        vec2 m = map(p);
        light += m.y;

        s = 0.01 + 0.65 * abs(m.x);
        d += s;

        o += vec4(2.0, 12.0, 12.0, 0.0) / s
           + 60.0 * vec4(2.0, 0.1, 6.0, 0.0) * light / d;
    }

    o = tanh4(o * o / 4e8);
    // Preserve hue on very bright pixels so the bouncing orb stays white
    // instead of tipping into yellow when red/green saturate before blue.
    float peak = max(o.r, max(o.g, o.b));
    if (peak > 0.85) {
    	o.rgb *= 0.85 / peak;   // pull back, keeping colour ratios
    }

    gl_FragColor = o;
}