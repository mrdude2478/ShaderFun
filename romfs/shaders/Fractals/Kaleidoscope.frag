precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

/* Kaleidoscope by MrDude*/

mat2 rotate(float a) {
    return mat2(cos(a), -sin(a), sin(a), cos(a));
}

void main() {
    vec2 fragCoord = vUV * iResolution.xy;
    vec3 V = iResolution;
    vec4 O = vec4(0.0);
    
    float r = iTime;
    float t = 0.1;
    
    // Zoom out factor - adjust this value to control zoom level
    float zoomOut = 1.0; // Increase to zoom out more
    
    for (int i = 0; i < 50; i++) {
        // Apply zoom out by scaling the UV coordinates
        vec2 zoomedUV = (fragCoord + fragCoord - V.xy) / V.y / zoomOut;
        
        //mirror effect
        zoomedUV.xy = abs(zoomedUV.xy);
        
        
        // Create ray direction with zoom applied
        vec3 o = t * normalize(vec3(zoomedUV * rotate(r * 0.15), 1.0));
        
        // Original vortex transformations
        //o.y += t * t * 0.09;
        o.z = mod(o.z + r, 0.2) - 0.1;
        o.y = mod(o.y + t, 0.3) - 0.2;
        
        float x = t * 0.06 - r * 0.2;
        float angle = atan(o.y, o.x);
        //float rounded = round((angle - x) / 0.314) * 0.314 + x;
        float rounded = round((angle - x) / 0.314) + x;
        
        // Apply rotation
        o.xy *= rotate(rounded);
        o.x = fract(o.x) - 0.1;
        
        // Distance field and stepping
        float dist = length(o) * 0.5 - 0.014;
        t += dist;
        
        // Original color calculation
        vec4 color = (1.0 + cos(t * 0.5 + r + vec4(0.0, 1.0, 2.0, 0.0)));
        color *= (0.3 + sin(3.0 * t + r * 5.0) / 4.0);
        color /= (8.0 + dist * 400.0);
        
        O += color;
        
        // Early exit if colors are saturated
        if (O.r > 1.0 && O.g > 1.0 && O.b > 1.0) break;
    }
    
    gl_FragColor = O;
}