precision mediump float;

uniform vec3  iResolution;
uniform float iTime;
uniform float iTimeDelta;   // optional, declared to match Shadertoy
uniform int   iFrame;       // optional

// Helper function to convert Hue to RGB (H=0.0 to 1.0)
vec3 hueToRGB(float h) {
    float r = abs(h * 6.0 - 3.0) - 1.0;
    float g = 2.0 - abs(h * 6.0 - 2.0);
    float b = 2.0 - abs(h * 6.0 - 4.0);
    return clamp(vec3(r, g, b), 0.0, 1.0);
}

// Path function (The "Swimming" logic)
vec2 getPos(float t, float id, vec2 resolution) {
    float aspect = resolution.x / resolution.y;
    
    // Create unique offsets for each tadpole ID
    return vec2(
        (0.5 + (0.35 + 0.05 * sin(id * 2.0)) * sin(t * (1.1 + id * 0.1) + id * 1.5)) * aspect, 
        0.5 + (0.25 + 0.05 * cos(id * 3.0)) * cos(t * (1.4 - id * 0.1) + id * 2.5)
    );
}

void main( )
{
    vec2 fragCoord = gl_FragCoord.xy;
    // Standard Shadertoy UV normalization
    vec2 uv = fragCoord / iResolution.xy;
    float aspect = iResolution.x / iResolution.y;
    vec2 p = uv;
    p.x *= aspect;

    vec3 finalCol = vec3(0.0);
    
    // Define how many tadpoles to render
    const float NUM_TADPOLES = 4.0;
    
    for(float tadID = 0.0; tadID < NUM_TADPOLES; tadID++) {
        
        // Calculate a unique, stable Hue for this specific tadpole
        float hueOffset = tadID / NUM_TADPOLES; 
        float hue = fract((iTime * 0.1) + hueOffset);
        vec3 tadpoleBaseColor = hueToRGB(hue);

        // Inner Loop: 20 segments per tadpole
        for(float i = 0.0; i < 20.0; i++) {
            float delay = i * 0.04; 
            float t = iTime - delay;
            
            // Get position using current resolution
            vec2 pos = getPos(t, tadID, iResolution.xy);
            float d = distance(p, pos);
            
            // Tail tapers off (glow logic)
            float size = 18.0 + (i * 10.0); 
            float glow = exp(-size * d);
            
            // Brightness tapers off along the tail
            float brightness = 1.0 - (i / 20.0);
            
            finalCol += tadpoleBaseColor * glow * brightness;
        }
    }

    // Output final color
    gl_FragColor = vec4(clamp(finalCol, 0.0, 1.0), 1.0);
}