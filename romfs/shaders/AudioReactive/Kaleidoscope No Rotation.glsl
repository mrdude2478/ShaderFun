precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
uniform float iAudioLevel;
uniform float iAudioBass;
uniform float iAudioMid;
uniform float iAudioHigh;
varying vec2 vUV;

/* Audio Reactive Kaleidoscope by MrDude */

mat2 rotate(float a) {
    return mat2(cos(a), -sin(a), sin(a), cos(a));
}

void main() {
    vec2 fragCoord = vUV * iResolution.xy;
    vec3 V = iResolution;
    vec4 O = vec4(0.0);
    
    float r = iTime;
    float t = 0.1;
    
    // Audio-reactive zoom - bass makes it zoom in/out
    float zoomOut = 1.0 + sin(iTime * 2.0 + iAudioBass * 5.0) * 0.5;
    
    // Audio-reactive rotation speed
    float rotationSpeed = 0.15 + iAudioMid * 0.3;
    
    // Audio-reactive iterations - more detail when music is loud
    int maxIterations = 30 + int(iAudioLevel * 30.0);
    
    for (int i = 0; i < 50; i++) {
        if (i >= maxIterations) break;
        
        // Apply zoom out by scaling the UV coordinates
        vec2 zoomedUV = (fragCoord + fragCoord - V.xy) / V.y / zoomOut;
        
        // Audio-reactive mirroring intensity
        float mirrorIntensity = 0.5 + iAudioHigh * 0.5;
        zoomedUV.xy = abs(zoomedUV.xy) * (1.0 + mirrorIntensity);
        
        // Create ray direction with zoom applied
        vec3 o = t * normalize(vec3(zoomedUV * rotate(r * rotationSpeed), 1.0));
        
        // Audio-reactive transformations
        o.z = mod(o.z + r, 0.2 + iAudioBass * 0.3) - 0.1;
        o.y = mod(o.y + t, 0.3 + iAudioMid * 0.2) - 0.2;
        
        // Audio-reactive pattern complexity
        float x = t * (0.06 + iAudioHigh * 0.04) - r * (0.2 + iAudioLevel * 0.1);
        float angle = atan(o.y, o.x);
        float rounded = round((angle - x) / (0.314 + iAudioBass * 0.1)) + x;
        
        // Apply rotation with audio influence
        o.xy *= rotate(rounded * (1.0 + iAudioMid * 0.5));
        
        // Audio-reactive fractal detail
        float fractalScale = 0.1 + iAudioHigh * 0.2;
        o.x = fract(o.x) - fractalScale;
        
        // Audio-reactive distance field
        float dist = length(o) * (0.5 + iAudioBass * 0.3) - 0.014;
        t += dist;
        
        // Audio-reactive colors
        vec4 color = (1.0 + cos(t * (0.5 + iAudioLevel * 0.3) + r + vec4(0.0, 1.0, 2.0, 0.0)));
        color *= (0.3 + sin(3.0 * t + r * (5.0 + iAudioBass * 10.0)) / 4.0);
        color /= (8.0 + dist * (400.0 - iAudioLevel * 100.0));
        
        // Boost colors with audio
        color.rgb *= (1.0 + iAudioLevel * 0.5);
        
        O += color;
        
        // Early exit if colors are saturated, but less likely with audio
        if (O.r > (1.5 - iAudioLevel * 0.5) && O.g > (1.5 - iAudioLevel * 0.5) && O.b > (1.5 - iAudioLevel * 0.5)) break;
    }
    
    // Final audio boost
    O.rgb *= (1.0 + iAudioLevel * 0.3);
    
    gl_FragColor = O;
}