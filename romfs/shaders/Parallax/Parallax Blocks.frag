// Enhanced abstract parallax effect for Switch
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

float hash(vec2 p) {
    return fract(sin(dot(p + 42.0, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    uv *= 15.0; // Scale for pattern density
    
    float layers = 5.0;
    float layer = floor(layers * hash(vec2(0.0, floor(uv.y))));
    float layerNorm = layer / layers;
    
    // Animate each layer at different speeds (parallax effect)
    uv.x += (3.0 * layer - 2.0) * iTime * 0.5;
    
    vec2 gridPos = fract(uv);
    vec2 cellPos = floor(uv);
    
    // Create rectangles with different sizes per layer
    float showRect = hash(cellPos) > 0.4 ? 1.0 : 0.0;
    vec2 rectSize = vec2(layerNorm * 0.8);
    vec2 inRect = step(abs(gridPos - 0.5), rectSize * 0.5);
    float rect = inRect.x * inRect.y;
    
    // Color based on layer and position
    vec3 color = vec3(0.0);
    if (rect > 0.0 && showRect > 0.5) {
        // Create colorful pattern based on layer and time
        float hue = fract(layer * 0.3 + iTime * 0.1 + uv.x * 0.05);
        color = 0.5 + 0.5 * cos(6.28318 * (hue + vec3(0.0, 0.33, 0.67)));
        color *= layerNorm * 1.5; // Brightness varies by layer
    }
    
    gl_FragColor = vec4(color, 1.0);
}