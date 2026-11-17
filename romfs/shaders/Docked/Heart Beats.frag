// Heart Beats
// By Noztol
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// Audio-reactive: iChannel0 = waveform, iChannel1 = spectrum

precision mediump float;

uniform vec3      iResolution;
uniform float     iTime;
uniform sampler2D iChannel0;   // waveform
uniform sampler2D iChannel1;   // spectrum (declared, unused here)

vec2 rotate2D(vec2 position, float angle) {
    float s = sin(angle), c = cos(angle);
    return mat2(c, -s, s, c) * position;
}

vec3 acesTonemap(vec3 colorInput) {
    mat3 m1 = mat3(0.59719, 0.07600, 0.02840,
                   0.35458, 0.90834, 0.13383,
                   0.04823, 0.01566, 0.83777);
    mat3 m2 = mat3(1.60475, -0.10208, -0.00327,
                   -0.53108, 1.10813, -0.07276,
                   -0.07367, -0.00605, 1.07602);
    vec3 v = m1 * colorInput;
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return m2 * (a / b);
}

// Xor's version
float dotNoise(vec3 position) {
    const float PHI = 1.618033988;
    const mat3 GOLD = mat3(
        -0.571464913, +0.814921382, +0.096597072,
        -0.278044873, -0.303026659, +0.911518454,
        +0.772087367, +0.494042493, +0.399753815
    );
    return dot(cos(GOLD * position), sin(PHI * position * GOLD));
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;

    vec3 finalColor = vec3(0.0);

    // ---- Audio sampling (ES 2.0: texture2D, not texture) ----
    float rawBass = texture2D(iChannel0, vec2(0.05, 0.25)).x;
    float pulse = smoothstep(0.4, 0.9, rawBass);
    float mid = texture2D(iChannel0, vec2(0.50, 0.25)).x;

    float state = sin(iTime * 0.5) * 0.5 + 0.5;
    state = smoothstep(0.1, 0.9, state);

    const int antiAliasingLevel = 2;

    for (int xOffset = 0; xOffset < antiAliasingLevel; xOffset++) {
        for (int yOffset = 0; yOffset < antiAliasingLevel; yOffset++) {
            vec2 subPixelOffset = (vec2(float(xOffset), float(yOffset)) + 0.5)
                                  / float(antiAliasingLevel) - 0.5;
            vec2 uv = fragCoord + subPixelOffset;

            float stepDistance = 0.0;
            float currentTime = iTime;

            vec3 rayPosition = vec3(0.0);          // fully initialised
            vec3 accumulatedLight = vec3(0.0);
            vec3 warpedPosition = vec3(0.0);
            vec3 rayDirection = vec3(0.0);

            rayPosition.z = currentTime * 1.5 + mix(0.0, pulse * 0.1, state);

            rayDirection = normalize(vec3(2.0 * uv - iResolution.xy, iResolution.y));

            for (int iter = 0; iter < 14; iter++) {
                float iteration = float(iter);

                warpedPosition = rayPosition;

                float twist = mix(0.0, pulse * 0.5, state);
                warpedPosition.xy = rotate2D(sin(warpedPosition.xy * 0.25),
                                             currentTime * 0.5 + warpedPosition.z * 2.0 + twist);

                float noiseIntensity = mix(0.7, 0.7 + mid * 0.3, state);
                stepDistance = 0.001 + abs(dotNoise(warpedPosition * 20.0) / 20.0
                                         - dotNoise(warpedPosition)) * noiseIntensity;

                float phaseShift = mix(0.0, pulse * 1.5, state);
                float waveAmp = mix(0.5, 0.4 + pulse * 0.4, state);
                float wave = abs(rayPosition.y * 0.2
                               + sin(rayPosition.z * 2.0
                                     + (abs(rayPosition.x) * 0.5)
                                     - phaseShift));
                stepDistance += wave * waveAmp;

                rayPosition += rayDirection * stepDistance;

                float blend = sin(iteration * 0.4 + length(rayPosition.xy * 0.5)
                                - currentTime * 1.5) * 0.5 + 0.5;
                blend = smoothstep(0.3, 0.7, blend);

                vec3 softPink = vec3(1.0, 0.5, 0.7);
                vec3 softBlue = vec3(0.4, 0.85, 1.0);

                vec3 hardPink = vec3(1.0, 0.1, 0.5);
                vec3 hardBlue = vec3(0.05, 0.6, 1.0);

                vec3 currentPink = mix(softPink, hardPink, state);
                vec3 currentBlue = mix(softBlue, hardBlue, state);
                vec3 palette = mix(currentPink, currentBlue, blend);

                vec3 softLight = palette * 1.4;
                vec3 hardLight = palette * (0.6 + pulse * 0.5);
                accumulatedLight += mix(softLight, hardLight, state) / stepDistance;
            }

            float exposure = mix(1500.0, 1200.0, state);
            finalColor += acesTonemap(accumulatedLight * accumulatedLight / exposure);
        }
    }

    gl_FragColor = vec4(finalColor / float(antiAliasingLevel * antiAliasingLevel), 1.0);
}