// Colorful Checkerboard Tunnel - Seamless Fix
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define PI  3.14159265359
#define PI2 6.28318530718

// Color definitions
#define COLOR1 vec3(1.0, 0.2, 0.2)   // Red
#define COLOR2 vec3(0.2, 0.8, 0.2)   // Green
#define COLOR3 vec3(0.2, 0.2, 1.0)   // Blue
#define COLOR4 vec3(1.0, 0.8, 0.2)   // Yellow

#define ARMS 10.
#define DENSITY 3.
#define ANGLE_SPEED 3.
#define ANGLE_TEMPO 2.
#define SPEED 2.
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
        r = length(uv);
    
    // Continuous angle calculation
    float a = atan(uv.y, uv.x) + PI; // 0 to 2PI
    
    // Apply the twist effect
    float w = a - sin(1. / r) * r * ANGLE_SPEED * sin((T - 1./r) / ANGLE_TEMPO);

    vec2 polar = vec2(
        w * ARMS / PI2,
        1. / r * DENSITY + T * SPEED 
    );

    // Get checkerboard pattern
    float pattern = smoothstepCheckerboard(polar);
    
    // SEAMLESS COLOR FIX: Use only radial position for color selection
    // This ensures no angular discontinuities
    float colorSelector = mod(floor(polar.y * 2.0), 4.0);
    
    vec3 checker;
    if (colorSelector < 1.0) {
        checker = mix(COLOR1, COLOR2, pattern);
    } else if (colorSelector < 2.0) {
        checker = mix(COLOR3, COLOR4, pattern);
    } else if (colorSelector < 3.0) {
        checker = mix(COLOR2, COLOR3, pattern);
    } else {
        checker = mix(COLOR4, COLOR1, pattern);
    }
    
    // Black center
    float centerMask = smoothstep(0.05, 0.15, r);
    
    vec3 color = checker * centerMask;
    
    // Increase contrast
    color = pow(color, vec3(1./0.8));
    color = clamp(color, 0.0, 1.0);
    
    gl_FragColor = vec4(color, 1.0);
}