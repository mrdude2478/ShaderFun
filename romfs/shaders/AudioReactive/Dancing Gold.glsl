// Audio Reactive Gold and Blue Ribbons
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel0; // Waveform
uniform sampler2D iChannel1; // Spectrum
varying vec2 vUV;

const float overallSpeed = 0.2;
const float gridSmoothWidth = 0.015;
const float axisWidth = 0.05;
const float majorLineWidth = 0.025;
const float minorLineWidth = 0.0125;
const float majorLineFrequency = 5.0;
const float minorLineFrequency = 1.0;
const vec4 gridColor = vec4(0.5);
const float scale = 5.0;
const vec4 blueColor = vec4(0.25, 0.5, 1.0, 1.0);
const vec4 goldColor = vec4(1.0, 0.84, 0.0, 1.0);
const float minLineWidth = 0.02;
const float maxLineWidth = 0.5;
const float lineSpeed = 1.0 * overallSpeed;
const float lineAmplitude = 1.0;
const float lineFrequency = 0.2;
const float warpSpeed = 0.2 * overallSpeed;
const float warpFrequency = 0.5;
const float warpAmplitude = 1.0;
const float offsetFrequency = 0.5;
const float offsetSpeed = 1.33 * overallSpeed;
const float minOffsetSpread = 0.6;
const float maxOffsetSpread = 2.0;
const int linesPerGroup = 8;

#define drawCircle(pos, radius, coord) smoothstep(radius + gridSmoothWidth, radius, length(coord - (pos)))
#define drawSmoothLine(pos, halfWidth, t) smoothstep(halfWidth, 0.0, abs(pos - (t)))
#define drawCrispLine(pos, halfWidth, t) smoothstep(halfWidth + gridSmoothWidth, halfWidth, abs(pos - (t)))
#define drawPeriodicLine(freq, width, t) drawCrispLine(freq / 2.0, width, abs(mod(t, freq) - (freq) / 2.0))

float drawGridLines(float axis) {
    return drawCrispLine(0.0, axisWidth, axis)
         + drawPeriodicLine(majorLineFrequency, majorLineWidth, axis)
         + drawPeriodicLine(minorLineFrequency, minorLineWidth, axis);
}

float drawGrid(vec2 space) {
    return min(1.0, drawGridLines(space.x) + drawGridLines(space.y));
}

float random(float t) {
    return (cos(t) + cos(t * 1.3 + 1.3) + cos(t * 1.4 + 1.4)) / 3.0;   
}

// Audio reactive functions
float getBass() {
    // Low frequencies (bass) - first 10% of spectrum
    float bass = 0.0;
    for(int i = 0; i < 5; i++) {
        bass += texture2D(iChannel1, vec2(float(i) / 50.0, 0.0)).r;
    }
    return bass / 5.0;
}

float getMid() {
    // Mid frequencies - 10% to 50% of spectrum
    float mid = 0.0;
    for(int i = 5; i < 25; i++) {
        mid += texture2D(iChannel1, vec2(float(i) / 50.0, 0.0)).r;
    }
    return mid / 20.0;
}

float getTreble() {
    // High frequencies - 50% to 100% of spectrum
    float treble = 0.0;
    for(int i = 25; i < 50; i++) {
        treble += texture2D(iChannel1, vec2(float(i) / 50.0, 0.0)).r;
    }
    return treble / 25.0;
}

float getWaveform() {
    // Current waveform amplitude
    return texture2D(iChannel0, vec2(0.5, 0.0)).r;
}

float getPlasmaY(float x, float horizontalFade, float offset, float audioInfluence) {
    float base = random(x * lineFrequency + iTime * lineSpeed) * horizontalFade * lineAmplitude;
    // Add audio-driven movement
    float audioMove = sin(x * 3.0 + iTime * 4.0) * audioInfluence * 2.0;
    return base + offset + audioMove;
}

void main() {
    vec2 fragCoord = vUV * iResolution.xy;
    vec2 uv = vUV;
    vec2 space = (fragCoord - iResolution.xy / 2.0) / iResolution.x * 2.0 * scale;
    
    // Get audio data
    float bass = getBass();
    float mid = getMid();
    float treble = getTreble();
    float waveform = getWaveform();
    
    float horizontalFade = 1.0 - (cos(uv.x * 6.28) * 0.5 + 0.5);
    float verticalFade = 1.0 - (cos(uv.y * 6.28) * 0.5 + 0.5);

    // Audio-reactive warping
    float audioWarp = bass * 2.0;
    space.y += random(space.x * warpFrequency + iTime * warpSpeed) * (warpAmplitude + audioWarp) * (0.5 + horizontalFade);
    space.x += random(space.y * warpFrequency + iTime * warpSpeed + 2.0) * (warpAmplitude + mid) * horizontalFade;
    
    vec4 lines = vec4(0.0);
    
    for(int l = 0; l < linesPerGroup; l++) {
        float normalizedLineIndex = float(l) / float(linesPerGroup);
        float offsetTime = iTime * offsetSpeed;
        float offsetPosition = float(l) + space.x * offsetFrequency;
        float rand = random(offsetPosition + offsetTime) * 0.5 + 0.5;
        
        // Audio-reactive line width
        float widthMod = 1.0 + treble * 3.0;
        float halfWidth = mix(minLineWidth, maxLineWidth * widthMod, rand * horizontalFade) / 2.0;
        
        // Audio-reactive offset
        float audioOffset = bass * 1.5 * sin(float(l) * 0.5 + iTime * 2.0);
        float offset = random(offsetPosition + offsetTime * (1.0 + normalizedLineIndex)) * 
                      mix(minOffsetSpread, maxOffsetSpread * (1.0 + mid * 2.0), horizontalFade) + audioOffset;
        
        // Audio-reactive plasma Y position
        float linePosition = getPlasmaY(space.x, horizontalFade, offset, waveform);
        
        // Audio-enhanced line drawing
        float lineBrightness = 1.0 + treble * 2.0;
        float line = (drawSmoothLine(linePosition, halfWidth, space.y) / 2.0 + 
                     drawCrispLine(linePosition, halfWidth * 0.15, space.y)) * lineBrightness;
        
        // Audio-reactive circles
        float circleX = mod(float(l) + iTime * (lineSpeed + bass * 0.5), 25.0) - 12.0;
        vec2 circlePosition = vec2(circleX, getPlasmaY(circleX, horizontalFade, offset, waveform));
        float circleSize = 0.01 * (1.0 + mid * 0.5);
        float circle = drawCircle(circlePosition, circleSize, space) * 4.0 * (1.0 + waveform * 2.0);
        
        line = line + circle;
        
        // Audio-reactive color selection
        vec4 currentColor;
        if (mod(float(l) + bass * 10.0, 2.0) < 1.0) {
            currentColor = blueColor;
        } else {
            currentColor = goldColor;
        }
        
        // Add color pulsing with audio
        currentColor *= (1.0 + mid * 0.5);
        lines += line * currentColor * rand * (1.0 + waveform);
    }
    
    // Audio-reactive background
    float bgPulse = bass * 0.3;
    vec4 bgColor = mix(blueColor * (0.3 + bgPulse), goldColor * (0.2 + bgPulse), 
                      uv.x * 0.5 + 0.5 + sin(iTime * 2.0) * treble * 0.1);
    bgColor *= verticalFade;
    bgColor.a = 1.0;
    
    // Add overall audio brightness
    float audioBrightness = 1.0 + (bass + mid + treble) * 0.2;
    vec4 finalColor = (bgColor + lines) * audioBrightness;
    
    // Add occasional flash on strong beats
    if (bass > 0.3) {
        finalColor += vec4(1.0, 1.0, 1.0, 0.0) * bass * 0.3 * smoothstep(0.3, 0.5, bass);
    }
    
    gl_FragColor = finalColor;
}