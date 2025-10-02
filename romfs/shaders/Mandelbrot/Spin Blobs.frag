// resources:
// - https://iquilezles.org/articles/msetsmooth
// - http://colorpalettes.net/color-palette-3885/

precision mediump float;

uniform vec3 iResolution;
uniform float iTime;

varying vec2 vUV;

// Color palettes
struct palette {
    vec3 c0, c1, c2, c3, c4;
};

palette palette1() {
    palette p;
    p.c0 = vec3(0);
    p.c1 = vec3(190.0,220.0,227.0)/255.0;
    p.c2 = vec3(243.0,243.0,246.0)/255.0;
    p.c3 = vec3(227.0,220.0,213.0)/255.0;
    p.c4 = vec3(218.0,112.0, 21.0)/255.0;
    return p;
}

palette palette2() {
    palette p; 
    p.c0 = vec3(0.0,2.0,5.0)/255.0;
    p.c1 = vec3(8.0,45.0,58.0)/255.0;
    p.c2 = vec3(38.0,116.0,145.0)/255.0;
    p.c3 = vec3(0.0,0.0,181.0)/255.0;
    p.c4 = vec3(207.0,197.0,188.0)/255.0;
    return p;    
}

// Complex math
vec2 cpow ( vec2 z ) { return mat2(z, -z.y, z.x) * z; }

vec2 cmul( vec2 z1, vec2 z2 ) { 
    return mat2(z1, -z1.y, z1.x) * z2; 
}

vec2 cpown (vec2 z, int n) {
    mat2 m = mat2(z, -z.y, z.x);
    for(int i=0; i<n-1; i++) {
        z = m * z;
    }
    return z;
}

float cmod( vec2 z ) {
    return dot(z,z);
}

// Mapping
#define ZOOM
vec2 map ( vec2 uv, float time ) {
    #ifdef ZOOM
    return 1.0/exp(mod(time/2.0,80.0))*uv;
    #else
    return 2.0*uv;
    #endif
}

vec3 cmap( float t, palette p ) {
    vec3 col = vec3(0.0);
    col = mix( p.c0,  p.c1, smoothstep(0.0 , 0.2, t));
    col = mix( col, p.c2, smoothstep(0.2, 0.4 , t));
    col = mix( col, p.c3, smoothstep(0.4 , 0.6, t));
    col = mix( col, p.c4, smoothstep(0.6,  0.8, t));
    col = mix( col, vec3(0.0), smoothstep(0.8, 1.0,  t));
    return col;
}

// Polynomials
vec2 fMandelbrot( vec2 z, vec2 c) { return cpow(z) + c; }
vec2 fCPoly1 ( vec2 z, vec2 c ) { 
    return cpown(z,11) + cmul((vec2(1.0,0.0)-c),cpown(z,5)) + cmul((c+1.0+vec2(0.0,1.0)),z) + c; 
}

vec2 fCPoly2 ( vec2 z, vec2 c ) { 
    return cpown(z,5) + cmul((vec2(1.0,0.0)-c),cpown(z,3)) + cmul((c+1.0+vec2(0.0,1.0)),z) + c; 
}

void main() {
    palette p = palette2(); // palette1
    
    float t = iTime/4.0;
    vec2 fragCoord = vUV * iResolution.xy;
    vec2 R = iResolution.xy;
    vec2 uv = (2.0*fragCoord-R)/iResolution.y;
    
    #define ROTATE
    #ifdef ROTATE
    float angle = -2.0*t;
    mat2 rot = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
    uv *= rot;
    #endif
    
    vec3 col = vec3(0.0);
    vec2 z = vec2(0.0);
    vec2 c = map(uv, iTime);
    float n = 0.0;
    float threshold = 4.0;
    float maxIter = 200.0;
    
    for(int i = 0; i < int(maxIter); i++) {
        z = fCPoly1(z, c);
        n += 1.0;
        if(cmod(z) > threshold) break;
    }
    
    // Smooth coloring using continuous iteration count
    float smoothIter = n - log(log(cmod(z))/log(11.0))/log(11.0);
    col = cmap(fract(smoothIter/50.0 + t), p);
    
    gl_FragColor = vec4(col, 1.0);
}