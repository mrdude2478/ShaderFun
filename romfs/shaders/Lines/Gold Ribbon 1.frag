precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define S smoothstep

vec4 Line(vec2 uv, float speed, float height, vec3 col) {
    // Reduced height multiplier for longer waves
    uv.y += S(1.0, 0.0, abs(uv.x)) * sin(iTime * speed + uv.x * height * 0.3) * 0.2;
    float line = S(0.06 * S(0.2, 0.9, abs(uv.x)), 0.0, abs(uv.y) - 0.004);
    // Wider fade area to extend lines further
    float fade = S(1.5, 0.5, abs(uv.x));
    return vec4(line * col * fade, 1.0);
}

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    vec4 O = vec4(0.0);
    
    // More lines with slower speeds
    for (float i = 0.0; i <= 7.0; i += 1.0) {
        float t = i / 7.0;
        O += Line(uv, 0.8 + t * 0.4, 3.0 + t * 1.5, vec3(0.2 + t * 0.7, 0.2 + t * 0.4, 0.3));
    }
    
    gl_FragColor = O;
}