// Colorful Checkerboard Tunnel with Seamless Colors
//Using colours - https://thebookofshaders.com/06/

precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define PI  3.14159265359
#define PI2 6.28318530718

// Color definitions - different colors for the checkerboard
#define COLOR1 vec3(0.149,0.141,0.912)   // Red
#define COLOR2 vec3(1.000,0.833,0.224)   // Green
#define COLOR3 vec3(1.0, 0.2, 1.0)   // Blue
#define COLOR4 vec3(1.0, 1.0, 0.2)   // Yellow

#define ARMS 6.
#define DENSITY 3.
#define ANGLE_SPEED 3.
#define ANGLE_TEMPO 2.
#define SPEED 1.0
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
    
    // Fix the seam by using a continuous angle calculation
    float a = atan(uv.y, uv.x); // This goes from -PI to PI
    
    // Make the angle continuous by adding PI (0 to 2PI)
    a += PI;
    
    // Apply the twist effect to the continuous angle
    float w = a - sin(1. / r) * r * ANGLE_SPEED * sin((T - 1./r) / ANGLE_TEMPO);

    vec2 polar = vec2(
        w * ARMS / PI2,
        1. / r * DENSITY + T * SPEED 
    );

    // Get checkerboard pattern (0 to 1)
    float pattern = smoothstepCheckerboard(polar);
    
    // Create colorful checkerboard with seamless color selection
    vec3 checker;
    
    // Use continuous position for color selection to avoid seams
    float colorSelector = mod(floor(polar.y * 2.0), 4.0);
    
    // Ensure color selection is continuous by using the twisted angle
    //float seamlessColorIndex = mod(w * ARMS / PI2 + polar.y * 0.5, 4.0);
    //colorSelector = floor(seamlessColorIndex);
    
    if (colorSelector < 1.0) {
        checker = mix(COLOR1, COLOR2, pattern);
    } else if (colorSelector < 2.0) {
        checker = mix(COLOR3, COLOR4, pattern);
    } else if (colorSelector < 3.0) {
        checker = mix(COLOR2, COLOR3, pattern);
    } else {
        checker = mix(COLOR4, COLOR1, pattern);
    }
    
    // Remove white pulsing center and make center black
    float centerMask = smoothstep(0.05, 0.15, r);
    
    vec3 color = checker * centerMask;
    
    // Increase contrast
    color = pow(color, vec3(1./0.8));
    color = clamp(color, 0.0, 1.0);
    
    gl_FragColor = vec4(color, 1.0);
}