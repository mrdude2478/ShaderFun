precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// IQ's noise function from the flame shader
vec2 hash(vec2 p) {
    p = vec2(dot(p,vec2(127.1,311.7)),
             dot(p,vec2(269.5,183.3)));
    return -1.0 + 2.0*fract(sin(p)*43758.5453123);
}

float noise(in vec2 p) {
    const float K1 = 0.366025404;
    const float K2 = 0.211324865;
    
    vec2 i = floor(p + (p.x+p.y)*K1);
    vec2 a = p - i + (i.x+i.y)*K2;
    vec2 o = (a.x>a.y) ? vec2(1.0,0.0) : vec2(0.0,1.0);
    vec2 b = a - o + K2;
    vec2 c = a - 1.0 + 2.0*K2;
    
    vec3 h = max(0.5-vec3(dot(a,a), dot(b,b), dot(c,c)), 0.0);
    vec3 n = h*h*h*h*vec3(dot(a,hash(i+0.0)), dot(b,hash(i+o)), dot(c,hash(i+1.0)));
    
    return dot(n, vec3(70.0));
}

float fbm(vec2 uv) {
    float f;
    mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
    f  = 0.5000*noise(uv); uv = m*uv;
    f += 0.2500*noise(uv); uv = m*uv;
    f += 0.1250*noise(uv); uv = m*uv;
    f += 0.0625*noise(uv); uv = m*uv;
    f = 0.5 + 0.5*f;
    return f;
}

// Classic flame function based on the reference
vec3 createFlame(vec2 flameUV, float intensity, float time) {
    // Remove the discrete strength steps that cause pulsing
    vec2 q = flameUV;
    q.x *= 4.0; // Reduced from 5.0 for smoother effect
    q.y *= 2.0;
    
    // Use continuous time instead of discrete strength steps
    float T3 = time * 3.0;
    q.x = fract(q.x) - 0.5; // Continuous wrapping
    q.y -= 0.25;
    
    // Smoother flame calculation
    float n = fbm(q * 3.0 - vec2(0.0, T3));
    
    // Simplified flame shape without discrete artifacts
    float flameShape = length(q * vec2(1.5 + q.y * 1.0, 0.8));
    float c = 1.0 - 12.0 * pow(max(0.0, flameShape - n * max(0.0, q.y + 0.25)), 1.5);
    
    float c1 = n * c * (1.5 - pow(2.0 * flameUV.y, 3.0));
    c1 = clamp(c1, 0.0, 1.0) * intensity;

    // Fire colors
    vec3 col = vec3(1.5 * c1, 1.5 * c1 * c1 * c1, c1 * c1 * c1 * c1 * c1 * c1);
    
    // Smooth alpha fade
    float a = (1.0 - pow(flameUV.y, 2.0)) * c;
    return col * a * a; // Square the alpha for smoother fade
}

void main() {
    vec2 uv = vUV;
    vec3 color = vec3(0.0);
    float time = iTime;
    
    // Create 3 fire balls
    for (int i = 0; i < 3; i++) {
        float idx = float(i);
        
        // Animated ball positions
        vec2 ballPos = vec2(
            0.3 + 0.4 * sin(time * 0.8 + idx * 2.094),
            0.4 + 0.3 * cos(time * 0.6 + idx * 1.047)
        );
        
        // Ball properties
        float ballRadius = 0.08;
        float ballPulse = 0.02 * sin(time * 4.0 + idx * 3.0);
        float currentRadius = ballRadius + ballPulse;
        
        // Distance to ball center
        float distToBall = length(uv - ballPos);
        
        // === GLOWING BALL CORE ===
        if (distToBall < currentRadius) {
            // White-hot core
            float core = 1.0 - smoothstep(0.0, currentRadius, distToBall);
            color += vec3(1.0, 0.9, 0.7) * core * 2.0;
            
            // Yellow middle
            float middle = 1.0 - smoothstep(0.0, currentRadius * 1.5, distToBall);
            color += vec3(1.0, 0.8, 0.3) * middle;
        }
        
        // === FLAMES EMANATING FROM THE BALL ===
        if (distToBall < currentRadius * 4.0) {
            // Create flame coordinate system relative to ball
            vec2 flameBase = ballPos;
            vec2 flameDir = uv - flameBase;
            
            // Normalize and create flame UVs
            float flameDist = length(flameDir);
            vec2 flameUV = vec2(0.0, 0.0);
            
            if (flameDist > 0.0) {
                // Create multiple flame streams around the ball
                
                float angle = atan(flameDir.y, flameDir.x);
                
                // Flame UV coordinates
                flameUV.x = (angle / 3.14159) * 2.0; // Wrap around the ball
                flameUV.y = (flameDist - currentRadius) * 3.0; // Distance from ball surface
                
                // Only create flames above the ball (rising flames)
                if (flameUV.y > 0.0 && flameUV.y < 1.5) {
                    // Vary flame intensity around the ball
                    float flameIntensity = 0.8 + 0.4 * sin(angle * 5.0 + time * 8.0);
                    
                    // Animate each flame stream differently
                    float flameTime = time * 2.0 + idx * 2.0 + angle * 2.0;
                    
                    // Create the flame effect
                    vec3 flame = createFlame(flameUV, flameIntensity, flameTime);
                    
                    // Fade flame with distance from ball
                    float flameFade = 1.0 - smoothstep(0.0, 0.6, flameUV.y);
                    flameFade *= 1.0 - smoothstep(currentRadius * 1.0, currentRadius * 3.5, distToBall);
                    
                    color += flame * flameFade;
                }
            }
        }
        
        // === FIREBALL GLOW ===
        float glow = 0.1 / (distToBall * distToBall + 0.01);
        color += vec3(1.0, 0.5, 0.1) * glow * 0.3;
    }
    
    // === FLOATING EMBERS ===
    for (int i = 0; i < 8; i++) {
        float idx = float(i);
        vec2 emberPos = vec2(
            fract(time * 0.2 + idx * 0.3) * 1.2 - 0.1,
            fract(time * 0.3 + idx * 0.4) * 0.8 + 0.1
        );
        float emberDist = length(uv - emberPos);
        float emberSize = 0.005 + 0.003 * sin(time * 8.0 + idx);
        
        if (emberDist < emberSize) {
            float intensity = 1.0 - emberDist / emberSize;
            float brightness = 0.6 + 0.4 * sin(time * 6.0 + idx);
            color += vec3(1.0, 0.6, 0.2) * intensity * brightness;
        }
    }
    
    // === HEAT HAZE ===
    vec2 heatUV = uv + vec2(noise(uv * 10.0 + time) * 0.005, 0.0);
    float backgroundGlow = 0.0;
    for (int i = 0; i < 3; i++) {
        float idx = float(i);
        vec2 ballPos = vec2(
            0.3 + 0.4 * sin(time * 0.8 + idx * 2.094),
            0.4 + 0.3 * cos(time * 0.6 + idx * 1.047)
        );
        float dist = length(heatUV - ballPos);
        backgroundGlow += 0.08 / (dist * dist + 0.02);
    }
    color += vec3(0.8, 0.3, 0.1) * backgroundGlow * 0.2;
    
    // Vignette
    vec2 centeredUV = (uv - 0.5) * 2.0;
    centeredUV.x *= iResolution.x / iResolution.y;
    float vignette = 1.0 - length(centeredUV) * 0.3;
    color *= vignette;
    
    gl_FragColor = vec4(color, 1.0);
}