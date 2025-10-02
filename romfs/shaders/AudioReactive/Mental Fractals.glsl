precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
uniform float iAudioLevel;
uniform float iAudioBass;
uniform float iAudioMid;
uniform float iAudioHigh;
varying vec2 vUV;

/* Audio Reactive Kaleidoscope 2 by MrDude - Enhanced */

mat2 rotate(float a) {
    return mat2(cos(a), -sin(a), sin(a), cos(a));
}

void main() {
    vec2 fragCoord = vUV * iResolution.xy;
    vec3 V = iResolution;
    vec4 O = vec4(0.0);
    
    float r = iTime;
    float t = 0.1;
    
    // Rotation controls - ONLY ADDING THIS
    float rotationSpeed = 0.0; // Adjust rotation speed here
    float overallRotation = iTime * rotationSpeed;
    
    // Audio-reactive zoom
    float zoomOut = 1.0 + sin(iTime * 2.0 + iAudioBass * 5.0) * 0.5;
    
    // Audio-reactive rotation speed
    float audioRotationSpeed = 0.15 + iAudioMid * 0.3;
    
    // Audio-reactive iterations
    int maxIterations = 30 + int(iAudioLevel * 30.0);
    
    for (int i = 0; i < 200; i++) {
        if (i >= maxIterations) break;
        
        // Apply zoom out by scaling the UV coordinates
        //vec2 zoomedUV = (fragCoord + fragCoord - V.xy) / V.y / zoomOut;
        vec2 zoomedUV = (fragCoord + fragCoord - V.xy) / V.yx;
        
        // ADDED: Apply overall rotation to entire pattern
        zoomedUV = rotate(overallRotation) * zoomedUV;
        
        // YOUR ORIGINAL MIRRORING - unchanged
        zoomedUV.xy = abs(zoomedUV.xy) / 0.314;
        
        // Create ray direction with zoom applied
        vec3 o = t * normalize(vec3(zoomedUV * rotate(r * audioRotationSpeed), 1.0));
        
        // YOUR ORIGINAL transformations
        o.z = mod(o.z + r, 0.2 + iAudioBass * 0.3) - 0.1;
        o.y = mod(o.y + t, 0.3 + iAudioMid * 0.2) - 0.2;
        
        float x = t * (0.06 + iAudioHigh * 0.04) - r * (0.2 + iAudioLevel * 0.1);
        float angle = atan(o.y, o.x);
        float rounded = round((angle - x) / 0.314) + x;
        //float rounded = round((angle - x) * 0.314);
        
        // Apply rotation with audio influence
        o.xy *= rotate(rounded * (1.0 + iAudioMid * 0.5));
        
        // YOUR ORIGINAL fractal detail
        float fractalScale = 0.1 + iAudioHigh * 0.2;
        o.x = fract(o.x) - fractalScale;
        
        // Distance field and stepping
        float dist = length(o) * (0.5 + iAudioBass * 0.3) - 0.014;
        t += dist;
        
        // YOUR ORIGINAL color calculation
        vec4 color = (1.0 + cos(t * 0.5 + r + vec4(0.0, 1.0, 2.0, 0.0)));
        color *= (0.3 + sin(3.0 * t + r * (5.0 + iAudioBass * 10.0)) / 4.0);
        color /= (8.0 + dist * (400.0 - iAudioLevel * 100.0));
        
        // ADDED: Beat pulse effect
        float beat = step(0.5, iAudioBass);
        color.rgb *= (1.0 + iAudioLevel * 0.5 + beat * 2.0);
        
        O += color;
        
        // Early exit if colors are saturated
        if (O.r > (1.5 - iAudioLevel * 0.5) && O.g > (1.5 - iAudioLevel * 0.5) && O.b > (1.5 - iAudioLevel * 0.5)) break;
    }
    
    // Final audio boost
    O.rgb *= (1.0 + iAudioLevel * 0.3);
    
    gl_FragColor = O;
}