precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// https://iquilezles.org/articles/palettes/
vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263, 0.416, 0.557);
    return a + b * cos(6.28318 * (c * t + d));
}

float sdHexagram(in vec2 p, in float r) {
    const vec4 k = vec4(-0.5, 0.8660254038, 0.5773502692, 1.7320508076);
    p = abs(p);
    p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
    p -= 2.0 * min(dot(k.yx, p), 0.0) * k.yx;
    p -= vec2(clamp(p.x, r * k.z, r * k.w), r);
    return length(p) * sign(p.y);
}

float sid(float d) {
    float f = 8.0;
    d = sin(d * f + iTime) / f;    
    d = abs(d);
    d = pow(0.01 / d, 2.0);
    return d;
}

void main() {
    // Convert from texture coordinates to pixel coordinates
    vec2 fragCoord = vUV * iResolution.xy;
    
    vec2 uv = (fragCoord * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv;
    vec3 finalColor = vec3(0.0);
    float d0 = length(uv0);
    
    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;
        float d = sdHexagram(uv, d0) * exp(-d0);
        vec3 col = palette(d0 + (i + iTime) * 0.4);
        d = sid(-d);
        finalColor += col * d;
    }
    
    gl_FragColor = vec4(finalColor, 1.0);
}

/*
Key changes made:
Replaced mainImage with main - Standard GLSL entry point

Replaced fragColor with gl_FragColor - Standard GLSL output

Added precision mediump float; - Required for OpenGL ES

Added proper uniforms - iResolution, iTime, vUV

Converted texture coords to pixel coords - vec2 fragCoord = vUV * iResolution.xy;
*/