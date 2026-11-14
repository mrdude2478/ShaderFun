// created by florian berger (flockaroo) - 2025
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// Custom palette: 60s Psychedelic (Yellows, Pinks, Teals)

precision highp float; // CRITICAL FIX: highp prevents the noisy dithering artifacts

uniform vec3  iResolution;
uniform float iTime;

#define ANIM
#define PI 3.1415926536

mat2 ROTM(float ang) {
    return mat2(cos(ang), sin(ang), -sin(ang), cos(ang));
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 c = fragCoord - iResolution.xy * 0.5;
    c *= ROTM(iTime * 0.1);
    
    float MaxLevel = 60.0;
    float level = MaxLevel * (1.0 - exp(-iTime / 10.0));

    for(int i = 0; i <= 60; i++) {
        float fi = float(i);
        if (fi > level) break;

        float R = exp(-fi / 20.0);
        if (i == int(level)) {
            R += 1.0 / (fi + 1.0) * (1.0 - fract(level));
        }
        
        float animScale = 1.0;
        #ifdef ANIM
            animScale = (1.1 - 0.1 * (cos(max(0.0, iTime - 37.0) * 0.2) * 0.5 + 0.5));
        #endif
        
        vec2 p = vec2(cos(fi * animScale), cos(fi * animScale - PI * 0.5)) * iResolution.y * R * 0.3;
        
        vec2 n = normalize(-p);
        c -= p; 
        
        float cn = dot(c, n);
        c = c - cn * n + abs(cn) * n;
        
        c += p;
    }
    
    // --- The Original Math ---
    float fade = 1.0 - 0.5 * exp(-iTime / 10.0);
    float flashing = (abs(level - MaxLevel) < 1.5) ? 2.0 : 0.0;
    vec3 animPhase = (iTime - 37.0) * flashing * vec3(1.0, 1.03, 1.05);
    vec3 base = vec3(length(c * 0.5) * 2.3) + animPhase;
    
    float wave = sin(base.r * fade);
    
    // --- 60s Psychedelic Palette ---
    // We map the color based on the distance from center (radius) and time.
    // This bypasses the noisy `wave` value for the hue calculation.
    
    float radius = length(c) / (iResolution.y * 0.5); 
    
    // Create a smooth radial rainbow that shifts over time
    float hue = radius * 0.5 - iTime * 0.1;
    
    // The "Yellow Submarine" palette:
    // Red and Green are high, Blue is suppressed, creating Yellows and Pinks.
    // As it shifts, it sweeps into Teals.
    vec3 pColor = 0.5 + 0.5 * cos(6.28318 * (hue + vec3(0.0, 0.15, 0.35)));
    
    // Add the structural folding details back in as shading
    float structure = 0.5 + 0.5 * wave;
    pColor *= (0.6 + 0.4 * structure);
    
    // Add a subtle white sparkle on the strongest edges
    float edge = abs(wave);
    pColor += vec3(1.0) * pow(edge, 12.0) * 0.5;
    
    // Vignette
    vec2 uv = fragCoord / iResolution.xy;
    float vignette = uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y);
    pColor *= pow(16.0 * vignette, 0.2);
    
    gl_FragColor = vec4(clamp(pColor, 0.0, 1.0), 1.0);
}