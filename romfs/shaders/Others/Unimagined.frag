// Unimagined
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)

precision mediump float;

uniform vec3  iResolution;
uniform float iTime;

// tanh does not exist in GLSL ES 2.0
vec4 tanh4(vec4 x) {
    vec4 e = exp(2.0 * x);
    return (e - 1.0) / (e + 1.0);
}

void main() {
    vec2 I = gl_FragCoord.xy;
    
    // Initialize O to zero (prevents static)
    vec4 O = vec4(0.0);
    
    // Iterator and distance travelled
    float t = 0.0;
    float v = 0.0;

    // Raymarching loop (using int counter for ES 2.0)
    for (int idx = 0; idx < 50; idx++) {
        float i = float(idx);

        // Ray direction
        vec3 p = t * normalize(vec3(I + I, 1.0) - iResolution.xyy);
        
        // Rotation of xy coordinates based on distance travelled
        p.xy *= mat2(cos(t * .15 + vec4(0.0, 11.0, 33.0, 0.0)));
        
        // Move forward
        p.z -= iTime;
        
        // Coordinate repetition
        p = mod(p, 4.0) - 2.0;
        
        // Density from mix between sphere and line sdf based on distance travelled
        v = mix(abs(length(p) - 1.0), length(p.xz), .5 - .5 * cos(t)) + .01;
        
        // Travel forwards based on density
        t += v * .3;
        
        // Color accumulation based on density and distance travelled
        O += exp(sin(t + vec4(0.0, 2.0, 4.0, 0.0))) / v;
    }
    
    // Tone mapping
    gl_FragColor = tanh4(O / 2e2);
}