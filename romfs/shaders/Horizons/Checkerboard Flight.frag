//https://www.shadertoy.com/view/XtKXWt

precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// Textures
vec3 checkerboard(vec2 p,float size){
  p*=size;
  vec2 f=fract(p.xy)-0.5;
  return vec3(f.x*f.y>0.0?1.0:0.0);
}

// Object Color
vec3 objcolor(vec3 p){
  return checkerboard(p.xz,0.4);
}

void main(){
    
  vec2 uv = vUV - 0.5;
  uv.x *= iResolution.x/iResolution.y;

  //Camera
  vec3 lookat=vec3(0.0,-2.0,-iTime*16.0);
  vec3 cam=vec3(sin(iTime*2.0)*4.0,0.0,10.0)+vec3(0.0,0.0,lookat.z);
  vec3 up=vec3(sin(iTime*2.0+3.14)*0.5,1.0,0.0);

  float camdist=2.0;
  float camsize=2.0;
  float maxdist=50.0;
  float preci=0.001;

  vec3 v=cam-lookat;
  vec3 camx=normalize(cross(up,v))*camsize;
  vec3 camy=normalize(cross(v,camx))*camsize;

  vec3 campoint=cam-normalize(v)*camdist+
      camx*uv.x+
      camy*uv.y;

  vec3 ray=normalize(campoint-cam);

  //Ray tracing plane y=-2.0 and y=1.0
  float s = (-2.0-campoint.y)/ray.y;
  if( s<0.0 ) s = (1.0-campoint.y)/ray.y;
  vec3 p=campoint + ray*s;
    
  float fadeout=max(maxdist-s,0.0)/maxdist;
  gl_FragColor = vec4(objcolor(p)*fadeout, 1.0);
}