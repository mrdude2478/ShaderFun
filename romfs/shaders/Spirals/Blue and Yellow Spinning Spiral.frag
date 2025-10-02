precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

void main() {
    vec2 uv = vUV - 0.5;
    uv.x *= iResolution.x / iResolution.y;

    float angle = atan(uv.y, uv.x);
    float radius = length(uv);

    float tunnel = 0.5 + 0.5 * sin(10.0*radius - iTime*5.0 + angle*5.0);

    gl_FragColor = vec4(vec3(tunnel, tunnel*0.5, 1.0-tunnel), 1.0);
}
