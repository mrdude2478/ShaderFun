precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

vec3 palette(float d){
    return mix(vec3(0.2,0.7,0.9),vec3(1.,0.,1.),d);
}

vec2 rotate(vec2 p, float a){
    float c = cos(a);
    float s = sin(a);
    return vec2(c*p.x + s*p.y, -s*p.x + c*p.y);
}

float map(vec3 p){
    for(int i=0;i<4;i++){ // reduced iterations
        float t = iTime*0.2;
        p.xz = rotate(p.xz,t);
        p.xy = rotate(p.xy,t*1.89);
        p.xz = abs(p.xz);
        p.xz -= 0.5;
    }
    return dot(sign(p),p)/5.0;
}

vec4 rm(vec3 ro, vec3 rd){
    float t = 0.0;
    vec3 col = vec3(0.0);
    float d;
    for(int i=0;i<32;i++){ // reduced steps
        vec3 p = ro + rd*t;
        d = map(p)*0.5;
        if(d<0.05) break;
        if(d>20.0) break; // clamp distance
        col += palette(length(p)*0.1)/(400.0*d);
        t += d;
    }
    return vec4(col,1.0/(d*50.0));
}

void main() {
    vec2 uv = vUV - 0.5;
    uv.x *= iResolution.x / iResolution.y;

    vec3 ro = vec3(0.0,0.0,-20.0); // closer camera
    ro.xz = rotate(ro.xz,iTime*0.5);
    vec3 cf = normalize(-ro);
    vec3 cs = normalize(cross(cf,vec3(0.0,1.0,0.0)));
    vec3 cu = normalize(cross(cf,cs));

    vec3 uuv = ro + cf*3.0 + uv.x*cs + uv.y*cu;
    vec3 rd = normalize(uuv - ro);

    vec4 col = rm(ro,rd);
    gl_FragColor = col;
}
