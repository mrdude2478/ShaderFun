precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// Simple pseudo-random function
float hash(vec2 p){
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Simple 2D noise
float noise(vec2 p){
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f*f*(3.0-2.0*f);
    return mix(a, b, u.x) + (c - a)*u.y*(1.0 - u.x) + (d - b)*u.x*u.y;
}

// Fractional Brownian Motion (simplified for Switch)
float fbm(vec2 uv){
    float v = 0.0;
    float a = 0.5;
    for(int i=0; i<3; i++){
        v += a * noise(uv);
        uv = uv * 2.0 + vec2(5.0, 5.0); // simple shift instead of rotation
        a *= 0.5;
    }
    return v;
}

// Color palette
vec3 palette(float t){
    return vec3(0.2,0.4,0.6) + 0.5*vec3(sin(6.28*t), sin(6.28*t+1.0), sin(6.28*t+2.0));
}

void main(){
    vec2 uv = vUV * 3.0; // scale for more detail
    uv.x *= iResolution.x / iResolution.y;

    // Layered FBM for flow
    float n = fbm(uv + iTime*0.2);
    n += 0.5*fbm(uv*2.0 - iTime*0.1);

    // Animate flow
    n = sin(n*6.2831 + iTime*0.5);

    // Map to color
    vec3 col = palette(n);

    gl_FragColor = vec4(col, 1.0);
}
