// Colorful Spinning Rectangle Tunnel + Audio-Reactive Spiky Sphere
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)

precision mediump float;

uniform vec3      iResolution;
uniform float     iTime;
uniform sampler2D iChannel0;   // waveform
uniform sampler2D iChannel1;   // spectrum

#define PI 3.14159265

// --- SPHERE HELPERS ---
vec3 hsv2rgb(vec3 c) {
    vec3 rgb = clamp(abs(mod(c.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return c.z * mix(vec3(1.0), rgb, c.y);
}

float map(vec3 p) {
    // Spin the sphere
    float mx = iTime * 0.8;
    float my = iTime * 0.6;
    float mz = iTime * 0.4;
    mat2 rotX = mat2(cos(mx), -sin(mx), sin(mx), cos(mx));
    mat2 rotY = mat2(cos(my), -sin(my), sin(my), cos(my));
    mat2 rotZ = mat2(cos(mz), -sin(mz), sin(mz), cos(mz));

    p.yz *= rotX;
    p.xz *= rotY;
    p.xy *= rotZ;

    float r = length(p);
    vec3 n = p / r;
    float a = acos(n.y); 
    float b = atan(n.z, n.x); 

    // Audio reactivity (Spectrum bass)
    float bass = texture2D(iChannel1, vec2(0.05, 0.5)).x;
    
    // Spikes
    float spike = pow(cos(a * 4.0), 4.0);
    if (a > PI / 8.0 && a < PI * 7.0 / 8.0) {
        spike *= pow(sin(b * 4.0), 4.0);
    }

    // INCREASED BASE RADIUS: 0.5 instead of 0.35
    float rad = 0.5 + (spike * 0.5) + (bass * 0.5 * spike);
    return r - rad;
}

vec3 calcNormal(vec3 p) {
    const vec2 k = vec2(1.0, -1.0);
    const float h = 0.005;
    return normalize(
        k.xyy * map(p + k.xyy * h) +
        k.yyx * map(p + k.yyx * h) +
        k.yxy * map(p + k.yxy * h) +
        k.xxx * map(p + k.xxx * h)
    );
}

// --- MAIN ---
void main() {
    vec2 uv = gl_FragCoord.xy;
    vec2 uv01 = uv / iResolution.xy;
    vec2 p = (uv01 * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;

    // --- LAYER 1: The Colorful Tunnel ---
    float r = length(p);
    float angle = atan(p.y, p.x);
    
    float spinSpeed = 0.1;
    float corkscrew = angle + iTime * spinSpeed + r * 2.0;
    
    float segments = 20.0;
    float segment = floor((1.0 / (r + 0.1)) * segments + iTime * 2.0);
    float segmentPos = fract((1.0 / (r + 0.1)) * segments + iTime * 2.0);
    
    float sides = 8.0;
    float sideAngle = 6.28318 / sides;
    float sidePos = mod(corkscrew + sideAngle * 0.5, sideAngle) - sideAngle * 0.5;
    
    float rect = 1.0 - smoothstep(0.2, 0.1, abs(sidePos)) * 
                 1.0 - smoothstep(1.0, 0.8, abs(segmentPos - 0.5));
    
    vec3 tunnelColor;
    float hue = (segment * 3.0) / 20.0;
    tunnelColor = 0.5 + 0.5 * cos(6.28318 * (hue + vec3(0.0, 0.33, 0.67)));
    
    float brightness = (1.0 - r) * (1.0 - smoothstep(0.0, 0.3, segmentPos));
    tunnelColor *= brightness * rect;
    
    // Black center hole
    float centerFade = smoothstep(0.0, 0.3, r);
    tunnelColor *= centerFade;
    
    vec3 glowColor = tunnelColor * 0.3 * (0.5 + 0.5 * sin(iTime * 3.0 + segment * 2.0));
    tunnelColor += glowColor * centerFade;

    // --- LAYER 2: The Spiky Sphere in the center ---
    vec3 col = tunnelColor;

    // WIDENED CHECK: 1.8 instead of 1.2 (sphere is bigger now)
    if (r < 1.8) {
        // MOVED CAMERA CLOSER: -3.0 instead of -4.5
        vec3 ro = vec3(0.0, 0.0, -3.0);
        vec3 rd = normalize(vec3(p, 1.5));

        float t = 0.0;
        float d = 0.0;
        float hit = 0.0;
        
        for (int i = 0; i < 60; i++) {
            vec3 pos = ro + rd * t;
            d = map(pos);
            
            if (d < 0.001) {
                hit = 1.0;
                break;
            }
            t += d;
            if (t > 10.0) break;
        }

        if (hit > 0.5) {
            vec3 pos = ro + rd * t;
            vec3 n = calcNormal(pos);

            // Reverse rotation for color mapping
            float mx = iTime * 0.8;
            float my = iTime * 0.6;
            float mz = iTime * 0.4;
            mat2 rotX = mat2(cos(mx), -sin(mx), sin(mx), cos(mx));
            mat2 rotY = mat2(cos(my), -sin(my), sin(my), cos(my));
            mat2 rotZ = mat2(cos(mz), -sin(mz), sin(mz), cos(mz));
            
            pos.xy *= rotZ;
            pos.xz *= rotY;
            pos.yz *= rotX;

            vec3 norm = normalize(pos);
            float a = acos(norm.y);
            float b = atan(norm.z, norm.x);
            
            float h = fract(b / (2.0 * PI) + iTime * 0.1);
            float s = 0.8 + 0.2 * sin(length(pos) * 5.0);
            float v = 0.8 - length(pos) * 0.2;

            vec3 sphereColor = hsv2rgb(vec3(h, clamp(s, 0.0, 1.0), clamp(v, 0.0, 1.0)));
            
            float diff = max(dot(n, -rd), 0.0);
            sphereColor *= (0.4 + diff * 0.6);
            
            // Draw the sphere ON TOP of the tunnel
            col = sphereColor;
        }
    }

    gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}