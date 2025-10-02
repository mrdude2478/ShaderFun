precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

const float TUNNEL_SIZE  = 0.25;
const float TUNNEL_SPEED = 0.5;
const float SPIN_SPEED = 1.0;
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
    // Checkerboard pattern
    float checker = mod(floor(uv.x * 20.0) + floor(uv.y * 10.0), 2.0);
    float stripes = checker * 0.7 + 0.3;
    float stripes2 = 1.0;
    
    // Animated colors
    float hue = iTime * 0.3;
    vec3 color1 = 0.5 + 0.5 * cos(hue + vec3(0.0, 2.0, 4.0));
    vec3 color2 = 0.5 + 0.5 * cos(hue + 1.57 + vec3(0.0, 2.0, 4.0));
    
    // Radial gradient for depth
    float radial = 1.0 - smoothstep(0.0, 1.0, uv.y);
    
    return mix(color1, color2, stripes * stripes2) * radial;
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
    
    // Add some scanlines for retro effect
    float scanline = sin(tunnelUV.y * 800.0) * 0.1 + 0.9;
    color *= scanline;
    
    // Center glow
    vec2 center = (vUV - 0.5) * 2.0;
    center.x *= iResolution.x / iResolution.y;
    float centerGlow = 1.0 - length(center) * 0.5;
    color += vec3(0.3, 0.2, 0.5) * centerGlow * 0.3;
    
    gl_FragColor = vec4(color, 1.0);
}