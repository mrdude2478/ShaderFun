//https://www.shadertoy.com/view/WtVGDw

precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define PI  3.14159265359
#define PI2 6.28318530718

#define COLOR1 vec3(1.0, 1.0, 0.95)  // Brighter white
#define COLOR2 vec3(0.0, 0.0, 0.0)   // Pure black
#define LIGHTCOLOR  vec3(0.8, 0.8, 0.8)  // Brighter light

#define ARMS 10.
#define DENSITY 3.
#define ANGLE_SPEED 3.
#define ANGLE_TEMPO 2.
#define SPEED 2.
#define PULSE_SPEED 4.
#define SMOOTH 0.04

float smoothstepCheckerboard(in vec2 uv) {
    // Shift, as fract gives 0..1 and we will smooth
    // around 0.25 and 0.75.
    uv = fract(uv + 0.25);
    float sm2 = SMOOTH * 0.5;
    // p01 oscillates between 0 and 1.
    vec2 p01 =
        smoothstep(0.25 - sm2, 0.25 + sm2, uv) -
        smoothstep(0.75 - sm2, 0.75 + sm2, uv);
    // pn11 oscillates between -1 and 1.
    vec2 pn11 = (p01 - 0.5) * 2.;
    // This results in smoothed 0..1 transitions.
    // We take advantage of the fact that multiplication
    // of -1/1 behaves like XOR. Then we rescale
    // -1..1 result of the multiplication to 0..1.
    return 0.5 - 0.5 * pn11.x * pn11.y;
}

void main() {
    vec2
        R = iResolution.xy,
        // Cartesian [-1, 1] along shorter axis.
        uv = (2. * vUV * R - R) / min(R.x, R.y);
    
    float
        T = iTime,
        // Angle [-PI, PI] and radius.
        a = atan(uv.x, uv.y),
        r = length(uv),
        // Twisted angle
        w = a - sin(1. / r) * r * ANGLE_SPEED * sin((T - 1./r) / ANGLE_TEMPO);

    vec2 polar = vec2(
        w * ARMS / PI2,
        1. / r * DENSITY + T * SPEED 
    );

    vec3 checker = mix(COLOR1, COLOR2, smoothstepCheckerboard(polar));
    
    // Light in the center - made stronger
    float light = smoothstep(0.7 + 0.3 * sin(T * PULSE_SPEED), 0.1, r);
    light *= 1.0; // Boost the light intensity

    vec3 color = mix(checker, LIGHTCOLOR, light);
    
    // Increase contrast with a stronger gamma correction
    color = pow(color, vec3(1./0.8)); // Changed from 2.2 to 0.8 for more contrast
    
    // Clamp to ensure we don't get oversaturated colors
    color = clamp(color, 0.0, 1.0);
    
    gl_FragColor = vec4(color, 1.0);
}

/*
Key changes made:
Function signature: Changed from mainImage(out vec4 fragColor, in vec2 fragCoord) to standard main()

Coordinate conversion:

fragCoord is now calculated as vUV * R (where R = iResolution.xy)

The formula (2. * fragCoord - R) / min(R.x, R.y) is preserved but uses the converted coordinates

Output: Changed fragColor to gl_FragColor

Uniform usage: Using the provided iResolution, iTime, and vUV variables

This shader creates a beautiful spiral galaxy effect with:

ARMS: Number of spiral arms (10)

DENSITY: Controls the density of the spiral pattern

PULSE_SPEED: Speed of the central light pulsation

Smooth transitions between colors using the custom smoothstepCheckerboard function

The shader will create a rotating spiral pattern with a pulsating center light, giving it a dynamic galactic appearance that evolves over time. Save it as a .frag file and place it in your shaders directory to use it!
*/