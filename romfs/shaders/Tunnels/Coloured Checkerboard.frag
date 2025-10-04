precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

const float TUNNEL_SIZE  = 0.25;
const float TUNNEL_SPEED = 0.5;
const float PI = 3.141592;

vec2 tunnel(vec2 uv, float size, float time)
{
    vec2 p = (uv - 0.5) * 2.0;
    p.x *= iResolution.x / iResolution.y;
    
    float a = atan(p.y, p.x);
    float r = length(p);
    return vec2(a / (2.0 * PI), time + (size / r));
}

// Multiple texture options - uncomment one:
vec3 tunnelTexture(vec2 uv)
{
    // Option 1: Colorful stripes (default)
    //float stripes = sin(uv.x * 30.0 + iTime * 3.0) * 0.5 + 0.5;
    //float stripes2 = cos(uv.x * 15.0 - iTime * 2.0) * 0.3 + 0.7;
    
    // Option 2: Checkerboard pattern
     float checker = mod(floor(uv.x * 20.0) + floor(uv.y * 10.0), 2.0);
     float stripes = checker * 0.7 + 0.3;
     float stripes2 = 1.0;
    
    // Option 3: Organic noise-like pattern
    // float n1 = sin(uv.x * 50.0 + uv.y * 30.0) * sin(uv.x * 70.0 - uv.y * 20.0);
    // float stripes = n1 * 0.5 + 0.5;
    // float stripes2 = 1.0;
    
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
    vec2 tunnelUV = tunnel(uv, TUNNEL_SIZE, iTime * TUNNEL_SPEED);
    
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