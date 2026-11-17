//!iChannel2 "scroller.jpg"
//!scroller on
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
uniform vec4 iMouse;
uniform sampler2D iChannel1; // Spectrum
uniform sampler2D iChannel2; // background.png
varying vec2 vUV;

// --- Line configuration ---
const float LINE_HEIGHT_PX = 11.0;
const float LINE1_Y      = 0.38;    // upper line position (0=bottom, 1=top of UV)
const float LINE2_Y      = 0.62;    // lower line position
const float LINE1_SPEED  = 0.12;    // positive = highlight moves one way
const float LINE2_SPEED  = -0.10;   // negative = opposite direction, slightly slower

// Pixel offset from the Y positions above (negative Y moves down)
const float LINE1_OFFSET_PX = -33.0; //neg value to move bottom line down
const float LINE2_OFFSET_PX = 32.0; //increase value to move top line up

// Returns (color.rgb, mask.a). rgb is alpha-unpremultiplied.
vec4 electricLine(vec2 uv, float centerY, float heightPx, float scrollSpeed) {
    float dy_px = abs(uv.y - centerY) * iResolution.y;
    float halfH = heightPx * 0.5;
    
    if (dy_px > halfH) return vec4(0.0);
    
    float edgeFade = 1.0 - smoothstep(halfH - 1.0, halfH, dy_px);
    float vGlow = 1.0 - pow(dy_px / halfH, 2.0) * 0.35;
    
    vec3 baseColor = vec3(0.0, 0.2666, 0.3682);
    
    // Negative scrollSpeed makes fract() wrap the other way
    float hPos = fract(iTime * scrollSpeed);
    float dx = abs(uv.x - hPos);
    dx = min(dx, 1.0 - dx);
    float highlight = pow(smoothstep(0.10, 0.0, dx), 1.5);
    
    vec3 color = mix(baseColor, vec3(0.85, 0.95, 1.0), highlight);
    
    float halo = smoothstep(0.20, 0.0, dx) * 0.2;
    color += halo * vec3(0.3, 0.6, 1.0);
    
    return vec4(color * vGlow, edgeFade);
}

void main() {
    vec3 img = texture2D(iChannel2, vUV).rgb;

    // Audio-reactive blue glow in the middle band
    float band = smoothstep(0.30, 0.40, vUV.y) * (1.0 - smoothstep(0.60, 0.70, vUV.y));
    float s = texture2D(iChannel1, vec2(abs(vUV.x - 0.5) * 0.6, 0.0)).r;
    float centre = 1.0 - abs(vUV.y - 0.5) * 3.0;
    float pulse = 0.5 + 0.5 * sin(iTime * 1.5 + vUV.x * 6.0);
    vec3 glow = vec3(0.0, 0.35, 0.8) * (0.15 * pulse + s * 4.0) * max(centre, 0.0) * band;

    // Touch interaction
    if (iMouse.x > 0.0) {
        float d = distance(vUV * iResolution.xy, iMouse.xy);
        glow += vec3(0.0, 0.3, 0.7) * exp(-d * 0.008) * 0.5;
    }
    
    vec3 col = img + glow;
    
    // Line 1 — upper, scrolling one direction
    float y1 = LINE1_Y + LINE1_OFFSET_PX / iResolution.y;
    vec4 l1 = electricLine(vUV, y1, LINE_HEIGHT_PX, LINE1_SPEED);
    col = mix(col, l1.rgb, l1.a);
    
    // Line 2 — lower, scrolling opposite direction
    float y2 = LINE2_Y + LINE2_OFFSET_PX / iResolution.y;
    vec4 l2 = electricLine(vUV, y2, LINE_HEIGHT_PX, LINE2_SPEED);
    col = mix(col, l2.rgb, l2.a);
    
    gl_FragColor = vec4(col, 1.0);
}