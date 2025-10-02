//https://www.shadertoy.com/view/WsXGDH

precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define SCALE 3.0
#define SCENE_ROTATION_SPEED -0.1
#define PLANE_ROTATION_SPEED 0.3
#define EDGE_SHARPNESS 20000.0
#define AIR_PERSPECTIVE 2.0
#define SPIRAL_SPEED 5.0

mat2 rotate2d(float angle){
    return mat2(
        cos(angle), -sin(angle),
        sin(angle), cos(angle)
    );
}

void main() {
    // Convert from vUV to pixel coordinates (similar to fragCoord)
    vec2 fragCoord = vUV * iResolution.xy;
    
    vec2 pixel = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    pixel *= rotate2d(iTime * SCENE_ROTATION_SPEED);
    vec2 trans = vec2(pixel.x / pixel.y, 1.0 / pixel.y);
    trans *= rotate2d(iTime * PLANE_ROTATION_SPEED);    
    trans *= SCALE;
    
    vec2 inner = mod(trans, 2.0) - vec2(1.0);
    float angle = atan(inner.x, inner.y);
    float dist = length(inner);
    float luminance = sin(dist * 16.0 + angle - (iTime * SPIRAL_SPEED));

    // apply air perspective
    luminance *= pow(abs(pixel.y * 2.0), AIR_PERSPECTIVE);
    gl_FragColor = vec4(vec3(luminance), 1.0);
}

/*
Key changes made:

Removed Shadertoy-specific parameters: Replaced mainImage(out vec4 fragColor, in vec2 fragCoord) with standard GLSL main() function.

Used available uniforms: The program provides iResolution, iTime, and vUV uniforms.

Converted coordinates:

fragCoord is calculated from vUV * iResolution.xy to match Shadertoy's coordinate system

Used gl_FragColor instead of fragColor output

Fixed syntax: Changed vec2(1) to vec2(1.0) for proper type consistency.
*/