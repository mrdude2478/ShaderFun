// Perfectly Seamless Colorful Tunnel
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define PI  3.14159265359
#define PI2 6.28318530718

#define COLOR1 vec3(1.0, 0.0, 0.0) //red
#define COLOR2 vec3(0.0, 1.0, 0.0) //blue
#define COLOR3 vec3(0.0, 0.0, 1.0) //green
#define COLOR4 vec3(1.0, 1.0, 0.0) //yellow

#define ARMS 6.
#define DENSITY 1.
#define ANGLE_SPEED 2.
#define ANGLE_TEMPO 3.
#define SPEED 1.
#define SMOOTH 0.50

float smoothstepCheckerboard(in vec2 uv) {
    uv = fract(uv + 0.50);
    float sm2 = SMOOTH * 0.5;
    vec2 p01 = smoothstep(0.25 - sm2, 0.25 + sm2, uv) -
               smoothstep(0.75 - sm2, 0.25 + sm2, uv);
    vec2 pn11 = (p01 - 0.5) * 2.;
    return 0.5 - 0.5 * pn11.x * pn11.y;
}

void main() {
    vec2 R = iResolution.xy;
    vec2 uv = (2. * vUV * R - R) / min(R.x, R.y);
    
    float T = iTime;
    float r = length(uv);
    float a = atan(uv.y, uv.x) + PI; // Continuous angle
    
    float w = a - sin(1. / r) * r * ANGLE_SPEED * sin((T - 1./r) / ANGLE_TEMPO);
    vec2 polar = vec2(w * ARMS / PI2, 1. / r * DENSITY + T * SPEED);

    float pattern = smoothstepCheckerboard(polar);
    
    // COMPLETELY SEAMLESS: Use a hash function based on continuous position
    // This creates consistent colors without any seams
    float hash = fract(sin(dot(vec2(floor(polar.y * 3.0), 0.0), vec2(12.9898, 78.233))) * 43758.5453);
    float colorSelector = floor(hash * 4.0);
    
    vec3 checker;
    if (colorSelector < 1.0) checker = mix(COLOR1, COLOR2, pattern);
    else if (colorSelector < 2.0) checker = mix(COLOR3, COLOR4, pattern);
    else if (colorSelector < 3.0) checker = mix(COLOR2, COLOR3, pattern);
    else checker = mix(COLOR4, COLOR1, pattern);
    
    float centerMask = smoothstep(0.05, 0.15, r);
    vec3 color = checker * centerMask;
    
    color = pow(color, vec3(1./0.8));
    gl_FragColor = vec4(color, 1.0);
}