// Colorful Spinning Rectangle Tunnel with Classic Starfield
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// Starfield function - stars moving from center outward
vec3 starfield(vec2 uv, float time) {
    vec3 color = vec3(0.0);
    
    // Number of stars (increased for better coverage)
    int numStars = 20;
    
    for(int i = 0; i < numStars; i++) {
        // Create unique seed for each star
        float seed = float(i) * 123.456;
        
        // Random angle for star direction from center
        float angle = fract(sin(seed * 12.9898) * 43758.5453) * 6.28318;
        
        // Random starting distance from center (most start very close to center)
        float startDistance = fract(sin(seed * 45.543) * 43758.5453) * 0.1;
        
        // Random speed (slower overall)
        float speed = 0.1 + fract(cos(seed * 78.233) * 43758.5453) * 0.2;
        
        // Current distance from center (increases over time)
        float currentDistance = startDistance + mod(time * speed, 2.0); // Stars travel 2 units max
        
        // Calculate star position based on angle and distance
        vec2 starDirection = vec2(cos(angle), sin(angle));
        vec2 starPos = starDirection * currentDistance;
        
        // Calculate distance from current pixel to star
        float dist = length(uv - starPos);
        
        // Star size increases with distance (larger as they move outward)
        float starSize = 0.005 + currentDistance * 0.01; // Much larger stars
        
        // Brightness - stars fade in as they start, brighten, then fade out
        float brightness = 1.0;
        if(currentDistance < 0.3) {
            brightness = smoothstep(0.0, 0.3, currentDistance); // Fade in
        } else if(currentDistance > 1.5) {
            brightness = 1.0 - smoothstep(1.5, 2.0, currentDistance); // Fade out at edge
        }
        
        // Add star to scene with smooth falloff
        float star = smoothstep(starSize, 0.0, dist) * brightness;
        
        // Classic star colors (mostly white with some variation)
        vec3 starColor = vec3(1.0, 0.95, 0.9); // Warm white base
        // Add some color variation
        float colorVar = fract(sin(seed * 91.123) * 43758.5453);
        if(colorVar > 0.7) {
            starColor = vec3(0.9, 0.95, 1.0); // Cool blue-white
        } else if(colorVar > 0.4) {
            starColor = vec3(1.0, 0.9, 0.8); // Warm yellow-white
        }
        
        color += star * starColor;
    }
    
    return color;
}

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    
    // Tunnel coordinates
    float r = length(uv);
    
    // Generate starfield
    vec3 stars = starfield(uv, iTime);
    
    // Show stars in the center region and fading out as they reach tunnel
    float starRegion = 1.0 - smoothstep(0.0, 1.5, r); // Stars visible up to r = 1.5
    
    // Original tunnel effect
    float angle = atan(uv.y, uv.x);
    
    // Corkscrew spin effect
    float spinSpeed = 0.1;
    float corkscrew = angle + iTime * spinSpeed + r * 2.0;
    
    // Create rectangle grid in tunnel space
    float segments = 20.0;
    float segment = floor((1.0 / (r + 0.1)) * segments + iTime * 2.0);
    float segmentPos = fract((1.0 / (r + 0.1)) * segments + iTime * 2.0);
    
    // Rectangle pattern around the circle
    float sides = 8.0;
    float sideAngle = 6.28318 / sides;
    float side = floor((corkscrew + sideAngle * 0.5) / sideAngle);
    float sidePos = mod(corkscrew + sideAngle * 0.5, sideAngle) - sideAngle * 0.5;
    
    // Rectangle dimensions
    float rectWidth = sideAngle * 0.0;
    float rectHeight = 0.0;
    
    // Create rectangle
    float rect = 1.0 - smoothstep(0.2, 0.1, abs(sidePos)) * 
                 1.0 - smoothstep(1.0, 0.8, abs(segmentPos - 0.5));
    
    // Color each rectangle differently
    vec3 color;
    float hue = (segment * 3.0) / 20.0;
    color = 0.5 + 0.5 * cos(6.28318 * (hue + vec3(0.0, 0.33, 0.67)));
    
    // Brightness based on position in tunnel
    float brightness = (1.0 - r) * (1.0 - smoothstep(0.0, 0.3, segmentPos));
    color *= brightness * rect;
    
    // Black center with starfield
    float centerFade = smoothstep(0.0, 0.3, r);
    color *= centerFade;
    
    // Add starfield (stars are already positioned correctly)
    color += stars * starRegion;
    
    // Add some glow to the tunnel (but not the stars)
    vec3 glowColor = color * 0.3 * (0.5 + 0.5 * sin(iTime * 3.0 + segment * 2.0));
    color += glowColor * centerFade; // Only apply glow to tunnel areas
    
    gl_FragColor = vec4(color, 1.0);
}

/*
Key changes for the classic starfield effect:

Outward Movement: Stars now start at the center (startDistance near 0) and move outward along radial lines based on random angles.

Slower Speed: Reduced overall speed to 0.1-0.3 range for a more relaxed, classic feel.

Larger Stars: Increased base star size to 0.005 with additional growth as they move outward (currentDistance * 0.01).

Radial Distribution: Each star gets a random angle and moves straight out from the center along that direction.

Distance-Based Effects:

Stars grow larger as they move away from center

They fade in when starting and fade out near the edge

Brightness is maintained during most of their journey

Classic Star Colors: Used warm white with some blue and yellow variations for that authentic retro feel.

Extended Visibility: Stars are now visible across most of the screen (r < 1.5) rather than just the very center.

This creates the classic "warp speed" effect where stars appear to radiate from the center of the screen, growing as they move outward, just like in old space games and demoscene productions!
*/