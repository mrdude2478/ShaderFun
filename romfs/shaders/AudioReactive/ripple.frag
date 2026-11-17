//!iChannel2 "ripple.jpg"
//!scroller off
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
uniform vec4 iMouse;
uniform sampler2D iChannel1; // Spectrum
uniform sampler2D iChannel2; // Your image
varying vec2 vUV;

// Offset in pixels from screen center (negative x = left, positive y = up)
const vec2 RIPPLE_OFFSET_PX = vec2(-25.0, 10.0);

void main() {
    vec2 uv = vUV;
    float bass = texture2D(iChannel1, vec2(0.0, 0.0)).r;

    // Ripple centred on the touch point (or screen centre if untouched)
    vec2 defaultCenter = 0.5 + RIPPLE_OFFSET_PX / iResolution.xy;
    vec2 c = (iMouse.x > 0.04) ? iMouse.xy / iResolution.xy : defaultCenter;
    float d = distance(uv, c);
    uv += normalize(uv - c + 1e-4) * sin(d * 40.0 - iTime * 4.0) * (0.01 + bass * 0.3) * exp(-d * 3.0);

    gl_FragColor = vec4(texture2D(iChannel2, uv).rgb, 1.0);
}
