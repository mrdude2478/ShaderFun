precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

//Scene Start

vec2 sim2d(in vec2 p, in float s)
{
   vec2 ret=p;
   ret=p+s/2.0;
   ret=fract(ret/s)*s-s/2.0;
   return ret;
}

vec3 stepspace(in vec3 p, in float s)
{
  return p-mod(p-s/2.0,s);
}

// Checkerboard pattern function with anti-aliasing
float checkerboard(vec2 p, float size) {
    vec2 pos = p / size;
    // Use smoothstep for anti-aliasing to remove white lines
    vec2 f = fract(pos);
    vec2 checker = smoothstep(0.45, 0.55, f) - smoothstep(0.95, 1.05, f + 0.5);
    return mod(floor(pos.x) + floor(pos.y), 2.0) < 0.5 ? 1.0 : 0.0;
}

// Simple plane object with waves
float obj(in vec3 p)
{ 
  // Apply the wave effect to the plane
  vec3 fp = stepspace(p, 4.0); // Larger spacing for larger squares
  float wave = sin(fp.x * 0.2 + iTime * 3.0) + cos(fp.z * 0.2 + iTime * 1.5);
  
  // Create a flat plane at y = 0 with waves
  return p.y - wave * 0.3; // Reduced wave amplitude for cleaner look
}

// Checkerboard Color - Black and White only
vec3 obj_c(vec3 p)
{
  // Create checkerboard pattern with larger squares
  float checker = checkerboard(p.xz, 4.0); // Larger squares (4.0 instead of 2.0)
  
  // Pure black and white colors
  vec3 color;
  if (checker > 0.5) {
    color = vec3(1.0, 1.0, 1.0); // White squares
  } else {
    color = vec3(0.0, 0.0, 0.0); // Black squares
  }
  
  return color;
}

//Raymarching Framework Start

float PI=3.14159265;

vec3 phong(
  in vec3 pt,
  in vec3 prp,
  in vec3 normal,
  in vec3 light,
  in vec3 color,
  in float spec,
  in vec3 ambLight)
{
   vec3 lightv=normalize(light-pt);
   float diffuse=dot(normal,lightv);
   vec3 refl=-reflect(lightv,normal);
   vec3 viewv=normalize(prp-pt);
   float specular=pow(max(dot(refl,viewv),0.0),spec);
   
   // Simple lighting for clean black/white look
   vec3 finalColor = color * (max(diffuse,0.0) + ambLight) + specular * 0.3;
   return finalColor;
}

float raymarching(
  in vec3 prp,
  in vec3 scp,
  in int maxite,
  in float precis,
  in float startf,
  in float maxd,
  out int objfound)
{ 
  const vec3 e=vec3(0.1,0.0,0.0);
  float s=startf;
  vec3 c,p,n;
  float f=startf;
  objfound=1;
  for(int i=0;i<96;i++){
    if (abs(s)<precis||f>maxd||i>maxite) break;
    f+=s;
    p=prp+scp*f;
    s=obj(p);
  }
  if (f>maxd) objfound=-1;
  return f;
}

vec3 camera(in vec3 prp, in vec3 vrp, in vec3 vuv, in float vpd)
{
  vec2 vPos = (vUV - 0.5) * 2.0;
  vPos.x *= iResolution.x / iResolution.y;
  
  vec3 vpn=normalize(vrp-prp);
  vec3 u=normalize(cross(vuv,vpn));
  vec3 v=cross(vpn,u);
  vec3 scrCoord=prp+vpn*vpd+vPos.x*u+vPos.y*v;
  return normalize(scrCoord-prp);
}

vec3 normal(in vec3 p)
{
  // Calculate normal for the waved plane with larger epsilon to reduce artifacts
  const float n_er=0.02; // Larger epsilon to reduce precision artifacts
  float h1 = obj(vec3(p.x-n_er, p.y, p.z));
  float h2 = obj(vec3(p.x+n_er, p.y, p.z));
  float h3 = obj(vec3(p.x, p.y, p.z-n_er));
  float h4 = obj(vec3(p.x, p.y, p.z+n_er));
  
  return normalize(vec3(h2-h1, 2.0*n_er, h4-h3));
}

vec3 render(
  in vec3 prp,
  in vec3 scp,
  in int maxite,
  in float precis,
  in float startf,
  in float maxd,
  in vec3 background,
  in vec3 light,
  in float spec,
  in vec3 ambLight,
  out vec3 n,
  out vec3 p,
  out float f,
  out int objfound)
{ 
  objfound=-1;
  f=raymarching(prp,scp,maxite,precis,startf,maxd,objfound);
  if (objfound>0){
    p=prp+scp*f;
    vec3 c=obj_c(p);
    n=normal(p);
    vec3 cf=phong(p,prp,n,light,c,spec,ambLight);
    return vec3(cf);
  }
  f=maxd;
  return vec3(background);
}

void main()
{
  //Camera animation
  vec3 vuv=vec3(0.0,1.0,0.0);
  
  // Target position
  vec3 vrp=vec3(iTime*1.0, 1.0, 0.0);
  
  // Camera angles for clear top-down view
  float mx = iTime * 0.2; // Slower rotation
  float my = 0.7; // Good angle to see checkerboard
  
  // Camera position
  vec3 prp=vrp+vec3(cos(my)*cos(mx),sin(my),cos(my)*sin(mx))*10.0;
  
  // Ensure camera stays high enough
  prp.y = max(prp.y, 6.0);
  vrp.y = max(vrp.y, 0.5);
  
  float vpd=1.5;
  vec3 light=prp+vec3(5.0, 12.0, 5.0); // High light source
  
  vec3 scp=camera(prp,vrp,vuv,vpd);
  vec3 n,p;
  float f;
  int o;
  const float maxe=0.02; // Larger precision tolerance
  const float startf=0.1;
  const vec3 backc=vec3(0.1, 0.1, 0.15); // Slightly brighter background
  const float spec=8.0; // Reduced specular
  const vec3 ambi=vec3(0.3, 0.3, 0.3); // Brighter ambient for clean look
  
  vec3 c1=render(prp,scp,64,maxe,startf,40.0,backc,light,spec,ambi,n,p,f,o);
  
  // Remove distance-based fading to avoid white lines
  // c1=c1*max(1.0-f*.015,0.0);
  
  vec3 c2=backc;
  if (o>0){
    scp=reflect(scp,n);
    c2=render(p+scp*0.05,scp,32,maxe,startf,8.0,backc,light,spec,ambi,n,p,f,o);
  }
  // Remove distance-based fading for reflections too
  // c2=c2*max(1.0-f*.1,0.0);
  
  gl_FragColor=vec4(c1,1.0); // No reflection mixing for cleaner look
}