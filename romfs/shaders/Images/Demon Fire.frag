// Rising fire / ember field driven by bass
precision mediump float;
//!scroller off
//!audio off
//!iChannel2 "demon.jpg"
precision mediump float;
uniform vec4 iMouse;

uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2; // background.png
varying vec2 vUV;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 3; i++) {   // 5 → 3 octaves
        v += a * noise(p);
        p *= 2.05;
        a *= 0.5;
    }
    return v;
}

void main() {
		vec3 img = texture2D(iChannel2, vUV).rgb;
    vec2 uv = vUV;
    vec2 p = (uv - 0.5) * 2.0;
    p.x *= iResolution.x / iResolution.y;

    float bass = 0.0;
    for (int i = 0; i < 6; i++)
        bass += texture2D(iChannel1, vec2(float(i) / 48.0, 0.0)).r;
    bass *= 0.16;

    float t = iTime;
    vec2 q = vec2(p.x * 1.2 + sin(p.y * 3.0 + t) * 0.04,
                  p.y * 0.9 - t * (0.35 + bass * 0.4));
    float n = fbm(q * 1.5 + vec2(0.0, t * 0.2));
    n += 0.35 * fbm(q * 5.0);

    float flame = n - (p.y + 0.7) * 0.55 + bass * 0.35;
    flame = clamp(flame, 0.0, 1.0);
    flame = pow(flame, 1.4);

    vec3 c_cool = vec3(0.05, 0.0, 0.02);
    vec3 c_mid  = vec3(0.9, 0.25, 0.02);
    vec3 c_hot  = vec3(1.0, 0.9, 0.4);
    vec3 col = mix(c_cool, c_mid, flame);
    col = mix(col, c_hot, pow(flame, 2.5));

    // Fire flicker (2 sines total)
    col *= 1.0 + 0.08 * sin(t * 17.0) + 0.04 * sin(t * 31.0);

    // === DANCING EMBERS ===
    float aspectRatio = iResolution.x / iResolution.y;

    for (int i = 0; i < 10; i++) {   // 14 → 10
        float fi = float(i);

        float h = hash(vec2(fi, 1.7));
        float seedX     = h;
        float seedLife  = fract(h * 7.31  + 0.123);
        float seedSpeed = 0.06 + fract(h * 13.7 + 0.456) * 0.10;
        float seedPhase = fract(h * 31.1 + 0.789) * 6.28318;
        float seedSway  = 0.05 + fract(h * 17.3 + 0.234) * 0.10;

        float life = fract(t * seedSpeed + seedLife);
        float emberY = life * 1.15 - 0.05;

        float swayScale = seedSway * (0.3 + life * 1.2);
        float sway = sin(t * 0.02 + seedPhase + life * 0.2) * swayScale
                   + sin(t * 0.7 + seedPhase * 1.7 + life * 3.0) * swayScale * 0.35;

        vec2 emberPos = vec2(seedX + sway, emberY);

        vec2 diff = uv - emberPos;
        diff.x *= aspectRatio;
        float emberDist = length(diff);

        float emberSize = 0.003 + 0.004 * life + 0.0015 * sin(t * 10.0 + fi * 2.1);

        float fadeIn  = smoothstep(0.0, 0.06, life);
        float fadeOut = 1.0 - smoothstep(0.70, 1.0, life);
        float lifetime = fadeIn * fadeOut;

        float heat = 1.0 - life * 0.8;
        vec3 emberColor = mix(vec3(1.0, 0.2, 0.0), vec3(1.0, 0.95, 0.55), heat);

        float boost = 1.0 + bass * 4.0;
        vec3 boostTint = mix(vec3(1.0), vec3(1.2, 0.8, 0.5), bass * 0.5);

        if (emberDist < emberSize) {
            float intensity = 0.7 - emberDist / emberSize;   // fixed
            intensity = pow(intensity, 1.0);
            col += emberColor * intensity * lifetime * 3.5 * boost * boostTint;
        }
    }

    // Ground glow
    col += vec3(1.0, 0.6, 0.05) * exp(-(p.y + 1.0) * 3.0) * 0.4;

    // Waveform heat shimmer at bottom
    float w = texture2D(iChannel0, vec2(uv.x, 0.6)).r;
    col += vec3(1.0, 0.4, 0.1) * exp(-(p.y + 1.0) * 3.0) * w * 0.1;

    gl_FragColor = vec4(img + col, 1.0);
}