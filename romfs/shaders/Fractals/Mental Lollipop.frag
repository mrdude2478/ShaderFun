precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define PI 3.1415926535897932384626433832795

// Enhanced HSV to RGB with NEON colors
vec3 hsv_to_rgb(float h, float s, float v) {
    float c = v * s;
    h = mod((h * 6.0), 6.0);
    float x = c * (1.0 - abs(mod(h, 2.0) - 1.0));
    vec3 color;

    if (0.0 <= h && h < 1.0) color = vec3(c, x, 0.0);
    else if (1.0 <= h && h < 2.0) color = vec3(x, c, 0.0);
    else if (2.0 <= h && h < 3.0) color = vec3(0.0, c, x);
    else if (3.0 <= h && h < 4.0) color = vec3(0.0, x, c);
    else if (4.0 <= h && h < 5.0) color = vec3(x, 0.0, c);
    else color = vec3(c, 0.0, x);

    color.rgb += v - c;
    
    // NEON boost - extreme saturation and brightness
    color = pow(color, vec3(0.6)); // More contrast for neon effect
    return color * 2.0; // Super bright for neon
}

mat2 rotate(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv;
    
    // Alternate rotation direction every 5 seconds
    float cycleTime = 5.0;
    float cycle = floor(iTime / cycleTime);
    float cycleProgress = fract(iTime / cycleTime);
    
    // Smooth transition between rotation directions
    float rotationSpeed = 0.8; // Base speed
    float currentDir = mod(cycle, 2.0) == 0.0 ? 1.0 : -1.0; // Alternate direction
    float nextDir = -currentDir;
    
    // Smooth blend between rotation directions
    float dirBlend = smoothstep(0.0, 1.0, cycleProgress);
    float rotationDir = mix(currentDir, nextDir, dirBlend);
    
    // Apply rotation with alternating direction
    float rotationAngle = iTime * rotationSpeed * rotationDir;
    uv = uv * rotate(rotationAngle);
    uv0 = uv0 * rotate(rotationAngle);
    
    // Create Spinning Lollipop-style color patterns with NEON intensity
    float spiralAngle = atan(uv0.x, uv0.y) - sin(iTime * 2.0) * length(uv0) + iTime * 3.0;
    float spiralIntensity = 0.7 + 0.3 * sin(20.0 * spiralAngle + iTime);
    
    vec3 finalColor = vec3(0.0);
    
    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.8) - 0.5; // More contrast for neon

        float d = length(uv) * exp(-length(uv0));

        // NEON color parameters - extreme saturation and brightness
        float hue = (spiralAngle / PI + i * 0.3 + iTime * 0.5);
        float saturation = 0.9 + 0.1 * sin(d * 15.0 + iTime * 4.0); // Near max saturation
        float brightness = 1.0 + 0.3 * sin(i * 3.0 + iTime * 2.0); // Super bright with pulsing
        
        vec3 col = hsv_to_rgb(fract(hue), saturation, brightness);

        // Enhanced neon glow effect
        d = sin(d * 12.0 + iTime * 3.0) / 4.0;
        d = abs(d);
        d = pow(0.015 / d, 1.5); // Sharper, brighter glow

        finalColor += col * d;
    }
    
    // Add intense Spinning Lollipop NEON effect on top
    vec2 fragCoord = vUV * iResolution.xy;
    float x = fragCoord.x - iResolution.x/2.0;
    float y = fragCoord.y - iResolution.y/2.0;
    float r = length(vec2(x,y));
    float angle = atan(x,y) - sin(iTime * 3.0) * r / 100.0 + iTime * 2.0;
    float intensity = 0.8 + 0.2 * sin(25.0 * angle);
    vec3 lollipopColor = hsv_to_rgb(fract(angle/PI * 2.0), 1.0, 1.2); // Max saturation, super bright
    
    // Stronger blend for intense neon effect
    finalColor = mix(finalColor, lollipopColor, 0.4);
    
    // Add overall neon glow
    finalColor += finalColor * 0.3 * sin(iTime * 5.0); // Pulsing neon glow
    
    // Clamp to prevent oversaturation while maintaining neon intensity
    finalColor = clamp(finalColor, 0.0, 2.0);
        
    gl_FragColor = vec4(finalColor, 1.0);
}