precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define PI 3.1415926535897932384626433832795

// Enhanced HSV to RGB with brighter colors
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
    
    // Boost saturation and brightness for more vibrant colors
    color = pow(color, vec3(0.8));
    return color * 1.2;
}

mat2 rotate(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv;
    
    // Rotation
    float rotationAngle = iTime * 0.5;
    uv = uv * rotate(rotationAngle);
    uv0 = uv0 * rotate(rotationAngle);
    
    // Create Spinning Lollipop-style color patterns
    float spiralAngle = atan(uv0.x, uv0.y) - sin(iTime) * length(uv0) / 2.0 + iTime;
    float spiralIntensity = 0.5 + 0.25 * sin(15.0 * spiralAngle);
    
    vec3 finalColor = vec3(0.0);
    
    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;

        float d = length(uv) * exp(-length(uv0));

        // Combine fractal pattern with Spinning Lollipop colors
        float hue = (spiralAngle / PI + i * 0.25 + iTime * 0.1);
        float saturation = 0.8 + 0.2 * sin(d * 10.0 + iTime);
        float brightness = 0.9 + 0.1 * cos(i * 2.0 + iTime);
        
        vec3 col = hsv_to_rgb(fract(hue), saturation, brightness);

        d = sin(d * 8.0 + iTime) / 5.0;
        d = abs(d);
        d = pow(0.01 / d, 1.2);

        finalColor += col * d;
    }
    
    // Add some of the original Spinning Lollipop effect on top
    vec2 fragCoord = vUV * iResolution.xy;
    float x = fragCoord.x - iResolution.x/2.0;
    float y = fragCoord.y - iResolution.y/2.0;
    float r = length(vec2(x,y));
    float angle = atan(x,y) - sin(iTime)*r / 200.0 + iTime;
    float intensity = 0.5 + 0.25*sin(15.0*angle);
    vec3 lollipopColor = hsv_to_rgb(angle/PI, intensity, 1.0);
    
    // Blend with fractal
    finalColor = mix(finalColor, lollipopColor, 0.3);
        
    gl_FragColor = vec4(finalColor, 1.0);
}