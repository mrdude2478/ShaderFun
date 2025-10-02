// Author @kyndinfo - 2016
// http://www.kynd.info
// Title: Displacement
// Converted for Nintendo Switch Shadertoy

precision mediump float;

uniform vec3 iResolution;  // Shadertoy-style resolution (z-component is pixel aspect ratio)
uniform float iTime;       // Shadertoy-style time

float random (in vec2 st) {
    return fract(sin(dot(st.xy,
                         vec2(12.9898,78.233)))*
        43758.5453123);
}

vec2 random2(vec2 p) {
    return fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453);
}

float cellular(vec2 p) {
    vec2 i_st = floor(p);
    vec2 f_st = fract(p);
    float m_dist = 10.;
    for (int j=-1; j<=1; j++ ) {
        for (int i=-1; i<=1; i++ ) {
            vec2 neighbor = vec2(float(i),float(j));
            vec2 point = random2(i_st + neighbor);
            point = 0.5 + 0.5*sin(6.2831*point);
            vec2 diff = neighbor + point - f_st;
            float dist = length(diff);
            if( dist < m_dist ) {
                m_dist = dist;
            }
        }
    }
    return m_dist;
}

void main() {
    // Convert to Shadertoy-style coordinates
    vec2 st = gl_FragCoord.xy / iResolution.xy;
    st.x *= iResolution.x / iResolution.y;
    st *= 5.0;
    
    float r = cellular(st);
    float b = cellular(st - vec2(0.0, sin(iTime * 0.5) * 0.5));
    gl_FragColor = vec4(r, 0.0, b, 1.0);
}

/*
Key changes made:
Removed GL_ES precision directive - Your Switch app already sets precision in the shader

Replaced uniforms:

u_resolution → iResolution.xy (using only x and y components)

u_time → iTime

Removed u_mouse (not used in your Switch app)

Adjusted coordinate calculation to use gl_FragCoord.xy and iResolution like Shadertoy
*/