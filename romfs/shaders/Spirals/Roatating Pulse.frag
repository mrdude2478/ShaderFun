precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

const float TUNNEL_SIZE  = 0.25;
const float TUNNEL_SPEED = 0.5;
const float SPIN_SPEED = 1.0;  // Controls how fast the tunnel spins
const float PI = 3.141592;

vec2 tunnel(vec2 uv, float size, float time, float spin)
{
    vec2 p = (uv - 0.5) * 2.0;
    p.x *= iResolution.x / iResolution.y;
    
    // Apply rotation for spinning effect
    float cosSpin = cos(spin);
    float sinSpin = sin(spin);
    p = mat2(cosSpin, -sinSpin, sinSpin, cosSpin) * p;
    
    float a = atan(p.y, p.x);
    float r = length(p);
    return vec2(a / (2.0 * PI), time + (size / r));
}

vec3 tunnelTexture(vec2 uv)
{
    
    // Colorful animated stripes
    float stripes = sin(uv.x * 30.0 + iTime * 5.0) * 0.5 + 0.5;
    float stripes2 = cos(uv.x * 15.0 - iTime * 3.0) * 0.3 + 0.7;
    float pulse = sin(iTime * 2.0) * 0.2 + 0.8;
    
    // Animated colors with phase shifts
    float hue = iTime * 0.4;
    vec3 color1 = 0.5 + 0.5 * cos(hue + vec3(0.0, 2.0, 4.0));
    vec3 color2 = 0.5 + 0.5 * cos(hue + 1.57 + vec3(0.0, 2.0, 4.0));
    vec3 color3 = 0.5 + 0.5 * cos(hue + 3.14 + vec3(0.0, 2.0, 4.0));
    
    // Radial gradient for depth with spiral effect
    float spiral = sin(uv.x * 8.0 + uv.y * 20.0 + iTime * 4.0) * 0.3 + 0.7;
    float radial = 1.0 - smoothstep(0.0, 1.0, uv.y) * spiral;
    
    return mix(mix(color1, color2, stripes), color3, stripes2) * radial * pulse;
}

void main()
{
    vec2 uv = vUV;
    
    // Calculate spin angle based on time
    float spinAngle = iTime * SPIN_SPEED;
    
    // Apply tunnel distortion with spinning
    vec2 tunnelUV = tunnel(uv, TUNNEL_SIZE, iTime * TUNNEL_SPEED, spinAngle);
    
    // Repeat the texture vertically for infinite tunnel
    tunnelUV.y = fract(tunnelUV.y);
    
    vec3 color = tunnelTexture(tunnelUV);
    
    // Add rotating scanlines that match the spin
    float scanline = sin(tunnelUV.y * 600.0 + spinAngle * 10.0) * 0.15 + 0.85;
    color *= scanline;
    
    // Spinning center vortex effect
    vec2 center = (vUV - 0.5) * 2.0;
    center.x *= iResolution.x / iResolution.y;
    
    // Rotate the center coordinates for vortex effect
    float vortexSpin = iTime * 3.0;
    float cosVortex = cos(vortexSpin);
    float sinVortex = sin(vortexSpin);
    vec2 vortexUV = mat2(cosVortex, -sinVortex, sinVortex, cosVortex) * center;
    
    float vortex = sin(length(vortexUV) * 50.0 - iTime * 10.0) * 0.3 + 0.7;
    float centerGlow = 1.0 - smoothstep(0.0, 1.5, length(center));
    color += vec3(0.4, 0.3, 0.8) * centerGlow * vortex * 0.4;
    
    // Add some spinning particles/dust
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
    
    // Dynamic vignette that pulses with the spin
    float vignette = 1.0 - length(center) * (0.4 + 0.1 * sin(iTime * 2.0));
    color *= vignette;
    
    gl_FragColor = vec4(color, 1.0);
}