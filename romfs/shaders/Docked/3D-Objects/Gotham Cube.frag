// Example: explicitly hide 3D models for this shader
precision mediump float;
//!model on
//!scroller off
//!model "cube_atlas.obj" "batman.jpg"
//!material neon
//!model_ground off
//!model_audio off
//!model_zoom 3.5
//!model_rotate 0.6 
//!model_rotate_x 0.4    // X – vertical, shows top and bottom
//!model_rotate_z 0.2    // Z – roll
//!model_pos 0.0 0.2 0.0
//!model_yaw 0.0   
//!model_pitch 0.0   
//!model_auto on

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

void main() {
    vec2 uv = vUV * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;
    float t = iTime * 0.3;
    vec3 col = 0.5 + 0.5 * cos(t + uv.xyx + vec3(0,2,4));
    col *= 0.35 + 0.15 * sin(length(uv) * 6.0 - t * 2.0);
    gl_FragColor = vec4(col, 1.0);
}
