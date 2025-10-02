precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define PI 3.141592653

vec4 hsv_to_rgb(float h, float s, float v, float a) {
    float c = v * s;
    h = mod((h * 6.0), 6.0);
    float x = c * (1.0 - abs(mod(h, 2.0) - 1.0));
    vec4 color;

    if (0.0 <= h && h < 1.0) color = vec4(c, x, 0.0, a);
    else if (1.0 <= h && h < 2.0) color = vec4(x, c, 0.0, a);
    else if (2.0 <= h && h < 3.0) color = vec4(0.0, c, x, a);
    else if (3.0 <= h && h < 4.0) color = vec4(0.0, x, c, a);
    else if (4.0 <= h && h < 5.0) color = vec4(x, 0.0, c, a);
    else if (5.0 <= h && h < 6.0) color = vec4(c, 0.0, x, a);
    else color = vec4(0.0, 0.0, 0.0, a);

    color.rgb += v - c;
    return color;
}

float lumfunc(vec2 v) {
    return 0.5 + 0.5*sin(10.0*v.x) + 0.5*sin(3.1*v.y);
}

void main() {
    float t = iTime;
    vec2 uv = vUV - 0.5;
    uv.x *= iResolution.x / iResolution.y;

    vec4 col = vec4(0.0);
    int layers = 3; // reduced for performance

    for (int i=0; i<layers; i++) {
        // polar coordinates
        float radius = max(length(uv), 0.01); // avoid division by zero
        float angle = atan(uv.y, uv.x) + 0.2*float(i-1);

        float mY = 1.0 / radius;
        float mX = angle;

        vec4 hue = hsv_to_rgb(6.0*fract(0.5+0.5*sin(float(i)*0.13 + 0.18*t)), 1.0, 1.0, 1.0);

        float ang = mX + 0.2*sin(0.5*mY) + 0.3*sin(1.2*mY + 0.5*t);

        float lum = floor(8.0*sin((float(i)*0.0 + 7.0)*ang + t) + 0.05*sin(5.0*mY)) / 8.0;
        lum += 0.05*mY - 0.2*lumfunc(vec2(2.0*mX, 1.0*mY + 1.2*t));

        col += clamp(lum, 0.0, 1.0) * hue;
    }

    gl_FragColor = col;
}
