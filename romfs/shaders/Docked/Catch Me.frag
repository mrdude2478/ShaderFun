// Catch Me
// Starfield + Volumetric Clouds + 2 Swimming Orbs.
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)

precision mediump float;

uniform vec3  iResolution;
uniform float iTime;

#define T iTime

// --- 1. PALETTE & HELPERS ---
vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.2, 0.6);
    vec3 b = vec3(0.5, 0.4, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.0, 0.25, 0.5);
    return a + b * cos(6.28318 * (c * t + d));
}

vec3 hueToRGB(float h) {
    float r = abs(h * 6.0 - 3.0) - 1.0;
    float g = 2.0 - abs(h * 6.0 - 2.0);
    float b = 2.0 - abs(h * 6.0 - 4.0);
    return clamp(vec3(r, g, b), 0.0, 1.0);
}

// --- 2. STARFIELD ---
vec3 starfield(vec2 uv, float time) {
    vec3 color = vec3(0.0);
    int numStars = 25;
    
    for(int i = 0; i < numStars; i++) {
        float seed = float(i) * 123.456;
        float angle = fract(sin(seed * 12.9898) * 43758.5453) * 6.28318;
        float startDistance = fract(sin(seed * 45.543) * 43758.5453) * 0.1;
        float speed = 0.1 + fract(cos(seed * 78.233) * 43758.5453) * 0.2;
        float currentDistance = startDistance + mod(time * speed, 2.0);
        
        vec2 starDirection = vec2(cos(angle), sin(angle));
        vec2 starPos = starDirection * currentDistance;
        
        float dist = length(uv - starPos);
        float starSize = 0.005 + currentDistance * 0.01; 
        
        float brightness = 1.0;
        if(currentDistance < 0.3) {
            brightness = smoothstep(0.0, 0.3, currentDistance);
        } else if(currentDistance > 1.5) {
            brightness = 1.0 - smoothstep(1.5, 2.0, currentDistance);
        }
        
        float star = smoothstep(starSize, 0.0, dist) * brightness;
        
        vec3 starColor = vec3(1.0, 0.95, 0.9);
        float colorVar = fract(sin(seed * 91.123) * 43758.5453);
        if(colorVar > 0.7) starColor = vec3(0.9, 0.95, 1.0);
        else if(colorVar > 0.4) starColor = vec3(1.0, 0.9, 0.8);
        
        color += star * starColor;
    }
    return color;
}

// --- 3. TADPOLE LOGIC ---
vec2 getPos(float t, float id, vec2 resolution) {
    float aspect = resolution.x / resolution.y;
    return vec2(
        (0.5 + (0.35 + 0.05 * sin(id * 2.0)) * sin(t * (1.1 + id * 0.1) + id * 1.5)) * aspect, 
        0.5 + (0.25 + 0.05 * cos(id * 2.0)) * cos(t * (1.2 - id * 0.1) + id * 2.5)
    );
}

void main() {
    vec2 u = gl_FragCoord.xy;
    vec2 uv = (u - 0.5 * iResolution.xy) / iResolution.y;
    vec2 uv01 = u / iResolution.xy;
    float aspect = iResolution.x / iResolution.y;
    
    // --- LAYER 1: Starfield ---
    vec2 starUv = (uv01 - 0.5);
    starUv.x *= aspect;
    vec3 finalColor = starfield(starUv, T);

    // --- LAYER 2: Volumetric Clouds ---
    // (From your voodoo.frag, slightly optimized)
    vec3 ro = vec3(sin(T * 0.2) * 2.0, cos(T * 0.3) * 1.0, -4.0);
    vec3 rd = normalize(vec3(uv, 1.5));
    
    float t = 0.0;
    vec4 cloudGlow = vec4(0.0);
    
    for (int i = 0; i < 25; i++) { // 40 steps is enough for the clouds
        vec3 p = ro + rd * t;
        
        vec3 q = p;
        q.x += sin(p.z * 0.8 + T * 1.5) * 1.5;
        q.y += cos(p.z * 0.6 + T * 1.2) * 1.5;
        
        float d = length(q) - 1.5;
        d += sin(p.x * 2.0 + T) * cos(p.y * 1.0 - T) * 0.4;
        
        float density = exp(-abs(d) * 0.95);
        vec3 col = palette(p.z * 0.5 + T * 0.3);
        
        cloudGlow += vec4(col, 1.0) * density * 0.15;
        
        t += 0.2;
        if (t > 8.0) break;
    }
    
    // Tone-map and blend clouds over stars
    vec4 ax = abs(cloudGlow);
    vec4 e = exp(-2.0 * ax);
    vec4 tonedClouds = sign(cloudGlow) * (1.0 - e) / (1.0 + e);
    
    // Add the clouds to the stars (using screen blend mode for a nice glow)
    finalColor = 1.0 - (1.0 - finalColor) * (1.0 - tonedClouds.rgb);

    // --- LAYER 3: The Orbs ---
    vec2 p = uv01;
    p.x *= aspect;

    for(int tp = 0; tp < 2; tp++) {
        float tadID = float(tp);
        float hue = (tadID == 0.9) ? 0.85 : 0.05;
        vec3 tadpoleBaseColor = hueToRGB(hue);

        for(int i = 0; i < 30; i++) {
            float fi = float(i);
            float delay = fi * 0.04; 
            float timeOffset = T - delay;
            
            vec2 pos = getPos(timeOffset, tadID, iResolution.xy);
            float d = distance(p, pos);
            
            float size = 8.0 + (fi * 10.0); 
            float glow = exp(-size * d * 2.0);
            float brightness = 1.2 - (fi / 30.0);
            
            finalColor += tadpoleBaseColor * glow * brightness;
        }
    }

    // Vignette
    float vignette = 1.0 - length(uv * 0.6);
    finalColor *= vignette;
    
    gl_FragColor = vec4(clamp(finalColor, 0.0, 1.0), 1.0);
}