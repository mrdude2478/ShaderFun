precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// 2D rotation matrix
mat2 rot(float a){
    float c = cos(a);
    float s = sin(a);
    return mat2(c, -s, s, c);
}

void main() {
    // Normalized coordinates centered at (0,0), aspect corrected
    vec2 uv = vUV;
    vec2 p = uv * 2.0 - 1.0;
    p.x *= iResolution.x / iResolution.y;

    // --- Spinning: rotate the sampling space ---
    float spinSpeed = 0.6; // change this number for faster/slower spin
    p = rot(iTime * spinSpeed) * p;

    // Polar coords
    float angle = atan(p.y, p.x);
    float radius = length(p);

    // Tunnel pattern: rings that move inward over time
    float ringFreq = 28.0;
    float ringSpeed = 4.0;
    float rings = sin(radius * ringFreq - iTime * ringSpeed);

    // Make sharp ring lines
    float lineWidth = 0.08;
    float line = 1.0 - smoothstep(lineWidth * 0.5, lineWidth, abs(rings));

    // Add angular (rotating) streaks for more structure
    float streaks = 0.5 + 0.5 * sin(angle * 6.0 + iTime * 1.2);

    // Radial falloff so center is bright and edges fade
    float fade = 1.0 / (1.0 + radius * radius * 3.0);

    // Color: rainbow based on angle + time
    vec3 col = vec3(
        0.5 + 0.5 * cos(angle * 3.0 + iTime * 1.5),
        0.5 + 0.5 * cos(angle * 3.0 + iTime * 1.5 + 2.0),
        0.5 + 0.5 * cos(angle * 3.0 + iTime * 1.5 + 4.0)
    );

    // Combine elements
    vec3 color = col * (0.6 * line + 0.4 * streaks) * fade;

    // Add center glow pulse
    float pulse = 0.6 + 0.4 * sin(iTime * 2.0);
    color += vec3(0.25, 0.15, 0.35) * (1.0 - smoothstep(0.0, 1.2 * radius, 0.5)) * pulse * 0.6;

    // Slight vignette
    float vign = smoothstep(1.2, 0.6, radius);
    color *= vign;

    gl_FragColor = vec4(color, 1.0);
}
