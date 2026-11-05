//!iChannel2 "background.png"
//!scroller on
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
uniform vec4 iMouse;
uniform sampler2D iChannel1; // Spectrum
uniform sampler2D iChannel2; // background.png
varying vec2 vUV;

void main() {
    vec3 img = texture2D(iChannel2, vUV).rgb;

    // The dark middle band of the picture (where the scroller runs) gets a soft audio-reactive blue glow
    float band = smoothstep(0.30, 0.36, vUV.y) * (1.0 - smoothstep(0.64, 0.70, vUV.y));
    float s = texture2D(iChannel1, vec2(abs(vUV.x - 0.5) * 0.6, 0.0)).r;
    float centre = 1.0 - abs(vUV.y - 0.5) * 3.0;
    float pulse = 0.5 + 0.5 * sin(iTime * 1.5 + vUV.x * 6.0);
    vec3 glow = vec3(0.0, 0.35, 0.8) * (0.15 * pulse + s * 4.0) * max(centre, 0.0) * band;

    // Touching the screen lights up the area under your finger
    if (iMouse.x > 0.0) {
        float d = distance(vUV * iResolution.xy, iMouse.xy);
        glow += vec3(0.0, 0.3, 0.7) * exp(-d * 0.008) * 0.5;
    }
    gl_FragColor = vec4(img + glow, 1.0);
}
