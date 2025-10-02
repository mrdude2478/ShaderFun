// Colorful Checkerboard Tunnel
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define PI  3.14159265359
#define PI2 6.28318530718

// Color definitions - different colors for the checkerboard
// https://ohmycolor.app/color-picker/vec3-color-picker/
#define COLOR1 vec3(1.0, 1.0, 1.0)   // white
#define COLOR2 vec3(0.0, 0.0, 0.0)   // black
#define COLOR3 vec3(0.271, 0.271, 0.271)   // dark grey
#define COLOR4 vec3(0.580, 0.580, 0.580)   // grey

#define LIGHTCOLOR  vec3(0.039, 0.039, 0.035)  // very dark grey

#define ARMS 10.
#define DENSITY 3.
#define ANGLE_SPEED 5.
#define ANGLE_TEMPO 4.
#define SPEED 2.
#define PULSE_SPEED 1.
#define SMOOTH 0.04

float smoothstepCheckerboard(in vec2 uv) {
    uv = fract(uv + 0.25);
    float sm2 = SMOOTH * 0.5;
    vec2 p01 =
        smoothstep(0.25 - sm2, 0.25 + sm2, uv) -
        smoothstep(0.75 - sm2, 0.75 + sm2, uv);
    vec2 pn11 = (p01 - 0.5) * 2.;
    return 0.5 - 0.5 * pn11.x * pn11.y;
}

void main() {
    vec2
        R = iResolution.xy,
        uv = (2. * vUV * R - R) / min(R.x, R.y);
    
    float
        T = iTime,
        a = atan(uv.x, uv.y),
        r = length(uv),
        w = a - sin(1. / r) * r * ANGLE_SPEED * sin((T - 1./r) / ANGLE_TEMPO);

    vec2 polar = vec2(
        w * ARMS / PI2,
        1. / r * DENSITY + T * SPEED 
    );

    // Get checkerboard pattern (0 to 1)
    float pattern = smoothstepCheckerboard(polar);
    
    // Create colorful checkerboard by alternating between multiple colors
    vec3 checker;
    
    // Use position to determine which color pair to use
    float colorSelector = mod(floor(polar.y * 2.0), 4.0);
    
    if (colorSelector < 1.0) {
        checker = mix(COLOR1, COLOR2, pattern); // Red/Green
    } else if (colorSelector < 2.0) {
        checker = mix(COLOR3, COLOR4, pattern); // Blue/Yellow
    } else if (colorSelector < 3.0) {
        checker = mix(COLOR2, COLOR3, pattern); // Green/Blue
    } else {
        checker = mix(COLOR4, COLOR1, pattern); // Yellow/Red
    }
    
    // Light in the center
    float light = smoothstep(0.7 + 0.3 * sin(T * PULSE_SPEED), 0.1, r);
    light *= 0.5;

    vec3 color = mix(checker, LIGHTCOLOR, light);
    
    // Increase contrast
    color = pow(color, vec3(1./0.8));
    color = clamp(color, 0.0, 1.0);
    
    gl_FragColor = vec4(color, 1.0);
}