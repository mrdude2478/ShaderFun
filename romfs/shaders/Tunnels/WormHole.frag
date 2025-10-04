// Colorful Spinning Rectangle Tunnel
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    
    // Tunnel coordinates
    float r = length(uv);
    float angle = atan(uv.y, uv.x);
    
    // Corkscrew spin effect
    float spinSpeed = 0.1;
    float corkscrew = angle + iTime * spinSpeed + r * 2.0; // Corkscrew: angle changes with radius
    
    // Create rectangle grid in tunnel space
    float segments = 20.0; // Number of segments along the tunnel
    float segment = floor((1.0 / (r + 0.1)) * segments + iTime * 2.0);
    float segmentPos = fract((1.0 / (r + 0.1)) * segments + iTime * 2.0);
    
    // Rectangle pattern around the circle
    float sides = 8.0; // Number of sides (rectangles around)
    float sideAngle = 6.28318 / sides; // 2PI / sides
    float side = floor((corkscrew + sideAngle * 0.5) / sideAngle); // Which side are we on?
    float sidePos = mod(corkscrew + sideAngle * 0.5, sideAngle) - sideAngle * 0.5;
    
    // Rectangle dimensions
    float rectWidth = sideAngle * 3.8; // Width of each rectangle
    float rectHeight = 1.0; // Height along tunnel
    
    // Create rectangle
    float rect = 1.0 - smoothstep(0.0, 0.1, abs(sidePos)) * 
                 1.0 - smoothstep(0.0, 0.05, abs(segmentPos - 0.5));
    
    // Color each rectangle differently
    vec3 color;
    float hue = (segment * 3.0) / 20.0;
    color = 0.5 + 0.5 * cos(6.28318 * (hue + vec3(0.0, 0.33, 0.67)));
    
    // Brightness based on position in tunnel
    float brightness = (1.0 - r) * (1.0 - smoothstep(0.0, 0.3, segmentPos));
    color *= brightness * rect;
    
    // Black center
    float centerFade = smoothstep(0.0, 0.3, r);
    color *= centerFade;
    
    // Add some glow
    color += color * 0.3 * (0.5 + 0.5 * sin(iTime * 3.0 + segment * 2.0));
    
    gl_FragColor = vec4(color, 1.0);
}