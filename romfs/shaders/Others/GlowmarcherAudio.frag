// CC0: Another day, another glowmarcher
// By mrange
// Twigl: https://twigl.app?ol=true&ss=-OmZSyy0GfTTuMjy66sJ
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// Audio-lite: audio only modulates the orange glow brightness.

//!scroller off
//!audio off

precision mediump float;

uniform vec3      iResolution;
uniform float     iTime;
uniform sampler2D iChannel0;   // waveform (time-domain)
uniform sampler2D iChannel1;   // spectrum (frequency-domain)

#define N normalize
#define V vec3

// tanh does not exist in GLSL ES 2.0 -- provide our own.
vec4 tanh4(vec4 x) {
    vec4 e = exp(2.0 * x);
    return (e - 1.0) / (e + 1.0);
}

// Cheap loudness probe: RMS of the waveform over a few taps.
// Constant loop bound required by GLSL ES 2.0.
#define AUDIO_TAPS 8

float sampleRMS(sampler2D ch) {
    float acc = 0.0;
    for (int i = 0; i < AUDIO_TAPS; i++) {
        float x = (float(i) + 0.5) / float(AUDIO_TAPS);
        float v = texture2D(ch, vec2(x, 0.5)).x;
        acc += v * v;
    }
    return sqrt(acc / float(AUDIO_TAPS));
}

void main() {
    // ---- Audio: single scalar "loudness" ----
    // Smoothed + clamped so it gently rises/falls instead of strobing.
    float rms  = sampleRMS(iChannel0);
    float loud = clamp(rms * 2.0, 0.0, 1.0);

    // ---- Original setup (unchanged) ----
    vec2 A = vec2(0.4, 0.3);
    vec2 s;

    vec3 I = iResolution;

    vec2 s0 = sin(A * iTime);
    s = s0;
    vec3 S = vec3(s0, iTime);

    vec3 Z = N(vec3(A * cos(A * S.z), 1.0));

    vec3 p = N(cross(vec3(0.0, 1.0, 0.0) + vec3(A * A * s, 0.0), Z));

    vec2 C = gl_FragCoord.xy;
    C -= 0.5 * I.xy;
    I = N(C.y * cross(p, Z) + I.y * Z - C.x * p);

    vec4 O = vec4(0.0);

    for (float i = 0.0, d = 0.0, z = 0.0; ++i < 77.0; ) {
        p = z * I + S;
        p.xy -= s = sin(A * p.z);

        Z = N(vec3(A * cos(A * p.z), -1.0));
        p -= dot(p.xy, Z.xy) * 0.5 * Z;

        p.xy *= mat2(cos(vec4(0.0, 11.0, 33.0, 0.0) - 0.16 * s.x));

        vec4 o = 0.9 + sin(2.0 * abs(p.x) + vec4(8.0, 3.0, 4.0, 2.0));

        p -= floor(p + 0.5);   // round(p) equivalent for ES 2.0

        d = 1e-3 + 0.7 * abs(0.53 - sqrt(length(p * p)));
        z += d;

        O += o.w / d * o;
    }

    O = tanh4(O / 2e4);

    // ---- Orange glow boost, driven by audio ----
    // The final image is roughly cyan/teal because the accumulation tends
    // toward cool tones. To make it "orange", bias toward red and green
    // while suppressing blue, scaled by `loud`.
    //   - red  : full boost
    //   - green: ~0.55 of red (orange = red + some green)
    //   - blue : suppressed
    // Amount is small (0.35 max) so it glows rather than strobes.
    float glow = loud * 0.1;
    O.rgb += glow * vec3(1.0, 0.55, -0.25);

    // Clamp so we don't introduce negative blue on very bright pixels.
    O.rgb = max(O.rgb, 0.0);

    gl_FragColor = O;
}