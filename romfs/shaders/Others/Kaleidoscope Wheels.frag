// Kaleidoscope Wheels
// By Noztol
// Assisted with Google Gemini 3.1 Pro
// inspied by Reindernij Hoff Truchet tiles
// https://en.wikipedia.org/wiki/Truchet_tiling
// (The Art of Code) https://www.youtube.com/watch?v=2R7h76GoIJM

precision mediump float;

uniform vec3  iResolution;
uniform float iTime;
uniform float iTimeDelta;   // optional, declared to match Shadertoy
uniform int   iFrame;       // optional

#define PI          3.141592654
#define TAU         (2.0 * PI)

// ==========================================
// HELPER FUNCTIONS
// ==========================================

mat2 rotate2D(float angle) {
    float c = cos(angle), s = sin(angle);
    return mat2(c, s, -s, c);
}

// 1D and 2D pseudo-random hashes
float hash11(float p) { return fract(sin(p * 12.9898) * 13758.5453); }
float hash21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

// Psychedelic cosine color palette
vec3 colorPalette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.00, 0.33, 0.67); // Shifting RGB phases
    return a + b * cos(TAU * (c * t + d));
}

// Polynomial smooth maximum (used for smooth absolute value)
float smoothMax(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (a - b) / k, 0.0, 1.0);
    return mix(b, a, h) + k * h * (1.0 - h);
}

float smoothAbs(float a, float k) {
    return smoothMax(a, -a, k);
}

// Alpha blending helpers
vec4 blendColors(vec4 front, vec4 back) {
    float weight = front.w + back.w * (1.0 - front.w);
    vec3 rgb = (front.xyz * front.w + back.xyz * back.w * (1.0 - front.w)) / weight;
    return weight > 0.0 ? vec4(rgb, weight) : vec4(0.0);
}

// ==========================================
// GEOMETRY & SPACE FUNCTIONS
// ==========================================

// Smooth kaleidoscopic folding
float applyKaleidoscope(inout vec2 uv, float smoothFactor, float segments) {
    float angle = atan(uv.y, uv.x);
    float radius = length(uv);
    float segmentAngle = TAU / segments;

    // Modulo mirror folding
    float halfSeg = segmentAngle * 0.5;
    float cellId = floor((angle + halfSeg) / segmentAngle);
    angle = mod(angle + halfSeg, segmentAngle) - halfSeg;
    angle *= mod(cellId, 2.0) * 2.0 - 1.0;

    // Smooth the sharp corners
    float smoothAngle = PI / segments - smoothAbs(PI / segments - abs(angle), smoothFactor);
    angle = sign(angle) * smoothAngle;

    uv = radius * vec2(cos(angle), sin(angle));
    return cellId;
}

// twisting camera path
vec3 getCameraPath(float z) {
    float shift = z * 0.8; 
    vec2 xy = -0.075 * (vec2(cos(shift), sin(shift * 1.414)) + 
                        vec2(cos(shift * 0.75), sin(shift * 0.707 + iTime * 0.2)));
    return vec3(xy, z);
}

vec3 getCameraDerivative(float z) {
    return 0.5 * (getCameraPath(z + 0.1) - getCameraPath(z - 0.1)) / 0.1;
}

// Truchet distance field
// https://reindernijhoff.net/2019/10/truchet-tiles-simple-rules-infinite-patterns/
vec2 calculateTruchet(float radius, vec2 uv) {
    vec2 cellId = floor(uv + 0.5);
    vec2 localUv = fract(uv + 0.5) - 0.5;
    
    float randVal = hash21(cellId);
    vec2 absUv = abs(localUv) - 0.5;
    float circleDist = length(absUv);
    float lineDist = abs(circleDist - radius);
    
    // Diagonal line distance fields
    float diag1 = abs(dot(normalize(vec2(1.0, 1.0)), localUv));
    float diag2 = abs(dot(normalize(vec2(1.0, -1.0)), localUv));
    
    float dist = min(circleDist, lineDist);
    if (randVal > 0.85) dist = min(dist, min(diag1, diag2));
    else if (randVal > 0.5) dist = min(dist, diag1);
    else if (randVal > 0.15) dist = min(dist, diag2);
    
    return vec2(dist, circleDist - radius);
}

// ==========================================
// RENDERING
// ==========================================

vec4 renderDepthPlane(vec3 rayOrigin, vec3 rayDir, vec3 hitPos, vec3 pathOffset, float antiAlias, float planeId) {
    float h = hash11(planeId);
    float distToCam = length(hitPos - rayOrigin);

    vec2 uv = (hitPos - pathOffset * vec3(1.0, 1.0, 0.0)).xy;
    
    // Fluctuating rotations
    uv *= rotate2D(sin(iTime * 0.3) * 0.5);
    
    // Fluctuating kaleidoscope segments based on time and plane depth
    float segments = 2.0 * round(mix(3.0, 12.0, 0.5 + 0.5 * sin(iTime * 0.2 + h * TAU)));
    float smoothFactor = 0.05 * 20.0 / segments;
    
    applyKaleidoscope(uv, smoothFactor, segments);
    
    // Internal spinning
    uv *= rotate2D(TAU * fract(1777.0 * h) + iTime * 0.5);
    
    float zoom = mix(0.2, 0.4, fract(3167.0 * h));
    uv /= zoom;
    uv += 0.5 + floor(fract(2087.0 * h) * 1000.0);
    
    float radius = mix(0.30, 0.45, 0.5 + 0.5 * cos(0.1 * planeId));
    vec2 truchetDist = calculateTruchet(radius, uv);
    truchetDist *= zoom;
    
    float lineWidth = 0.025 * zoom; 
    float dist = truchetDist.x - lineWidth;
    
    // Coloring based on time, position, and depth
    vec3 psychColor = colorPalette(distToCam * 0.2 - iTime * 0.4 + planeId * 0.1);
    
    vec3 col = mix(psychColor, vec3(0.0), smoothstep(antiAlias, -antiAlias, dist));
    
    // Create pulsing wavy shadows/highlights
    float wave = sin(PI * 100.0 * dist + iTime * 5.0);
    col = mix(col, psychColor * 0.2, smoothstep(0.5, 1.0, wave));
    col = mix(col, vec3(0.0), step(truchetDist.y, 0.0));
    
    float alpha = smoothstep(antiAlias, -antiAlias, -truchetDist.y - 3.0 * lineWidth) * 
                  mix(0.5, 1.0, smoothstep(antiAlias, -antiAlias, -truchetDist.y - lineWidth));
                  
    return vec4(col, alpha);
}

// ==========================================
// MAIN SCENE
// ==========================================

vec3 renderScene(vec3 camForward, vec3 camRight, vec3 camUp, vec3 rayOrigin, vec2 uv) {
    // Lens distortion
    float fov = 2.0 + 1.0 * clamp(length(uv), -1.0, 1.0);
    vec3 rayDir = normalize(uv.x * camRight + uv.y * camUp + fov * camForward);
    
    // Offset ray for anti-aliasing estimation
    vec2 uvOffset = uv + 1.0 / iResolution.xy;
    vec3 rayDirAA = normalize(uvOffset.x * camRight + uvOffset.y * camUp + fov * camForward);

    const float planeSpacing = 0.75;
    const int maxPlanes = 6;
    const int fadeStart = 1;
    
    float startZ = floor(rayOrigin.z / planeSpacing);
    vec4 accumColor = vec4(0.0);
    
    // Pulsating color
    vec3 pulseColor = colorPalette(iTime * 0.1) * pow(max(dot(rayDir, vec3(0.0, 0.0, 1.0)), 0.0), 10.0);

    for (int i = 1; i <= maxPlanes; ++i) {
        float planeZ = planeSpacing * startZ + planeSpacing * float(i);
        float t = (planeZ - rayOrigin.z) / rayDir.z;

        if (t > 0.0 && accumColor.w < 0.95) {
            vec3 hitPos = rayOrigin + rayDir * t;
            vec3 hitPosAA = rayOrigin + rayDirAA * t;
            float antiAlias = 3.0 * length(hitPos - hitPosAA);
            vec3 pathOffset = getCameraPath(hitPos.z);

            vec4 planeCol = renderDepthPlane(rayOrigin, rayDir, hitPos, pathOffset, antiAlias, startZ + float(i));

            float distZ = hitPos.z - rayOrigin.z;
            float fadeIn = smoothstep(planeSpacing * float(maxPlanes), planeSpacing * float(fadeStart), distZ);
            float fadeOut = smoothstep(0.0, planeSpacing * 0.1, distZ);
            
            planeCol.xyz = mix(pulseColor, planeCol.xyz, fadeIn);
            planeCol.w *= fadeOut;
            planeCol = clamp(planeCol, 0.0, 1.0);

            accumColor = blendColors(planeCol, accumColor);
        } else {
            break;
        }
    }

    return mix(pulseColor, accumColor.xyz, accumColor.w);
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    
    float timeScaled = iTime * 0.25;
    vec3 rayOrigin = getCameraPath(timeScaled);
    vec3 camForward = normalize(getCameraDerivative(timeScaled));
    
    // Compute camera coordinate frame with tilting (roll)
    vec3 camDerivative2 = 0.125 * (getCameraDerivative(timeScaled + 0.1) - getCameraDerivative(timeScaled - 0.1)) / 0.1;
    vec3 camRight = normalize(cross(normalize(vec3(0.0, 1.0, 0.0) + camDerivative2), camForward));
    vec3 camUp = normalize(cross(camForward, camRight));

    vec3 col = renderScene(camForward, camRight, camUp, rayOrigin, uv);
    
    // Post-processing / Tonemapping
    col = clamp(col, 0.0, 1.0);
    col = pow(col, vec3(1.0 / 2.2)); // Gamma correction
    col = col * 0.6 + 0.4 * col * col * (3.0 - 2.0 * col); // Contrast boost
    
    // vignette
    vec2 screenUv = fragCoord / iResolution.xy;
    col *= 0.5 + 0.5 * pow(16.0 * screenUv.x * screenUv.y * (1.0 - screenUv.x) * (1.0 - screenUv.y), 0.3);
    
    gl_FragColor = vec4(col, 1.0);
}