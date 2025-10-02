// Neon parallax by nimitz (twitter: @stormoid)
// Converted for Nintendo Switch Shadertoy
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

float pulse(float cn, float wi, float x)
{
    return 1.0-smoothstep(0.0, wi, abs(x-cn));
}

float hash11(float n)
{
    return fract(sin(n)*43758.5453);
}

vec2 hash22(vec2 p)
{
    p = vec2( dot(p,vec2(127.1, 311.7)), dot(p,vec2(269.5, 183.3)));
    return fract(sin(p)*43758.5453);
}

vec2 field(in vec2 p)
{
    vec2 n = floor(p);
    vec2 f = fract(p);
    vec2 m = vec2(1.0);
    vec2 o = hash22(n)*0.17;
    vec2 r = f+o-0.5;
    float d = abs(r.x) + abs(r.y);
    if(d<m.x)
    {
        m.x = d;
        m.y = hash11(dot(n,vec2(1.0, 2.0)));
    }
    return vec2(m.x,m.y);
}

void main()
{
    vec2 uv = (vUV * iResolution.xy * 2.0 - iResolution.xy) / iResolution.y;
    uv *= 0.9;
    uv *= 4.0;
    
    vec2 p = uv*0.01;
    p *= 1.0/(p-1.0);
    
    //global movement
    uv.y += iTime*1.2;
    uv.x += sin(iTime*0.3)*0.8;
    vec2 buv = uv;
    
    float rz = 0.0;
    vec3 col = vec3(0.0);
    for(float i=1.0; i<=26.0; i++)
    {
        vec2 rn = field(uv);
        uv -= p*(i-25.0)*0.2;
        rn.x = pulse(0.35,0.02, rn.x+rn.y*0.15);
        col += rn.x*vec3(sin(rn.y*10.0), cos(rn.y)*0.2,sin(rn.y)*0.5);
    }
    
    //animated grid
    buv*= mat2(0.707,-0.707,0.707,0.707);
    float rz2 = 0.4*(sin(buv*10.0+1.0).x*40.0-39.5)*(sin(uv.x*10.0)*0.5+0.5);
    vec3 col2 = vec3(0.2,0.4,2.0)*rz2*(sin(2.0+iTime*2.1+(uv.y*2.0+uv.x*10.0))*0.5+0.5);
    float rz3 = 0.3*(sin(buv*10.0+4.0).y*40.0-39.5)*(sin(uv.x*10.0)*0.5+0.5);
    vec3 col3 = vec3(1.9,0.4,2.0)*rz3*(sin(iTime*4.0-(uv.y*10.0+uv.x*2.0))*0.5+0.5);
    
    col = max(max(col,col2),col3);
    
    gl_FragColor = vec4(col,1.0);
}