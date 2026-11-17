// Rotating Pulse (fixed atan seam)
precision mediump float;

uniform vec3  iResolution;
uniform float iTime;
varying vec2 vUV;

#define TAU 6.28318530718
#define PI  3.14159265359

const float TUNNEL_SIZE  = 0.25;
const float TUNNEL_SPEED = 0.5;
const float SPIN_SPEED   = 1.0;

vec2 tunnel(vec2 uv, float size, float time, float spin)
{
    vec2 p = (uv - 0.5) * 2.0;
    p.x *= iResolution.x / iResolution.y;

    // Spin
    p = mat2(cos(spin), -sin(spin), sin(spin), cos(spin)) * p;

    float a = atan(p.y, p.x);
    float r = length(p);

    // Small epsilon on r avoids division-by-zero at the center
    return vec2(a / TAU, time + size / (r + 0.001));
}

vec3 tunnelTexture(vec2 uv)
{
    // FIXED: TAU-multiples ensure continuity across the atan seam
    float stripes  = sin(uv.x * TAU * 5.0 + iTime * 5.0) * 0.5 + 0.5;
    float stripes2 = cos(uv.x * TAU * 2.0 - iTime * 3.0) * 0.3 + 0.7;
    float pulse    = sin(iTime * 2.0) * 0.2 + 0.8;

    // Hue cycles
    float hue    = iTime * 0.4;
    vec3 color1  = 0.5 + 0.5 * cos(hue + vec3(0.0, 2.0, 4.0));
    vec3 color2  = 0.5 + 0.5 * cos(hue + 1.57 + vec3(0.0, 2.0, 4.0));
    vec3 color3  = 0.5 + 0.5 * cos(hue + 3.14 + vec3(0.0, 2.0, 4.0));

    // FIXED: spiral x-frequency is TAU * 1
    float spiral = sin(uv.x * TAU + uv.y * 20.0 + iTime * 4.0) * 0.3 + 0.7;
    float radial = 1.0 - smoothstep(0.0, 1.0, uv.y) * spiral;

    return mix(mix(color1, color2, stripes), color3, stripes2) * radial * pulse;
}

void main()
{
    vec2 uv = vUV;
    float spinAngle = iTime * SPIN_SPEED;

    // Tunnel distortion
    vec2 tunnelUV = tunnel(uv, TUNNEL_SIZE, iTime * TUNNEL_SPEED, spinAngle);
    tunnelUV.y = fract(tunnelUV.y);

    vec3 color = tunnelTexture(tunnelUV);

    // Scanlines (uses tunnelUV.y, which is fine)
    float scanline = sin(tunnelUV.y * 600.0 + spinAngle * 10.0) * 0.15 + 0.85;
    color *= scanline;

    // Vortex center
    vec2 center = (uv - 0.5) * 2.0;
    center.x *= iResolution.x / iResolution.y;

    float vs = iTime * 3.0;
    vec2 vortexUV = mat2(cos(vs), -sin(vs), sin(vs), cos(vs)) * center;

    float vortex      = sin(length(vortexUV) * 50.0 - iTime * 10.0) * 0.3 + 0.7;
    float centerGlow  = 1.0 - smoothstep(0.0, 1.5, length(center));
    color += vec3(0.4, 0.3, 0.8) * centerGlow * vortex * 0.4;

    // Particles
    for (int i = 0; i < 5; i++) {
        float idx = float(i);
        vec2 particlePos = vec2(
            sin(iTime * 2.0 + idx * 1.256) * 0.7,
            cos(iTime * 1.5 + idx * 0.942) * 0.5
        );
        float particleDist = length(center - particlePos);
        float particleSize = 0.01 + 0.005 * sin(iTime * 5.0 + idx);

        if (particleDist < particleSize) {
            float intensity = 1.0 - particleDist / particleSize;
            vec3 particleColor = 0.5 + 0.5 * cos(iTime * 0.5 + idx + vec3(0.0, 2.0, 4.0));
            color += particleColor * intensity * 0.8;
        }
    }

    // Vignette
    float vignette = 1.0 - length(center) * (0.4 + 0.1 * sin(iTime * 2.0));
    color *= vignette;

    gl_FragColor = vec4(color, 1.0);
}