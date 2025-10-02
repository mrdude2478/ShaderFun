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

// Rainbow color palette function
vec3 rainbow(float t) {
    // t from 0 to 1, cycles through rainbow colors
    vec3 color = 0.5 + 0.5 * cos(6.28318 * (t + vec3(0.0, 0.333, 0.667)));
    return color;
}

// Hash function for random colors based on position
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

//Object
float obj(in vec3 p)
{ 
  vec3 fp=stepspace(p,2.0);
  float d=sin(fp.x*0.3+iTime*4.0)+cos(fp.z*0.3+iTime*2.0);
  p.y=p.y+d;
  p.xz=sim2d(p.xz,2.0);
  //c1 is IQ RoundBox
  float c1=length(max(abs(p)-vec3(0.6,0.6,0.6),0.0))-0.35;
  //c2 is a Sphere
  float c2=length(p)-1.0;
  float cf=sin(iTime)*0.5+0.5;
  return mix(c1,c2,cf);
}

// Get maximum wave height at a position
float getMaxWaveHeight(in vec3 p)
{
  vec3 fp=stepspace(p,2.0);
  // Maximum possible wave height is 2.0 (sin + cos both max at 1.0)
  return 2.0;
}

//Object Color - Now with rainbow colors!
vec3 obj_c(vec3 p)
{
  // Get the base grid position for consistent coloring
  vec3 gridPos = stepspace(p, 4.0);
  
  // Create a unique seed for each cube based on its grid position
  float seed = hash(gridPos.xz + gridPos.y * 37.0);
  
  // Option 1: Static rainbow colors based on position
  // vec3 color = rainbow(seed);
  
  // Option 2: Animated rainbow colors that cycle over time
  //vec3 color = rainbow(seed + iTime * 0.2);
  
  // Option 3: Different color patterns:
  
  // Vertical gradient (height-based)
  // vec3 color = rainbow(p.y * 0.3 + iTime * 0.1);
  
  // Horizontal gradient (x-based) 
  // vec3 color = rainbow(p.x * 0.2 + iTime * 0.15);
  
  // Wave-based colors
   vec3 fp = stepspace(p, 2.0);
   float wave = sin(fp.x * 0.3 + iTime * 4.0) + cos(fp.z * 0.3 + iTime * 2.0);
   vec3 color = rainbow(wave * 0.3 + iTime * 0.1);
  
  // Enhanced brightness and saturation
  color = color * 1.2;
  color = clamp(color, 0.0, 1.0);
  
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
   
   // Enhanced lighting to make colors pop
   vec3 finalColor = (max(diffuse,0.0) + ambLight) * color + specular * vec3(1.0, 1.0, 0.8);
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
  for(int i=0;i<96;i++){ // Reduced from 256 for performance
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
  //tetrahedron normal
  const float n_er=0.01;
  float v1=obj(vec3(p.x+n_er,p.y-n_er,p.z-n_er));
  float v2=obj(vec3(p.x-n_er,p.y-n_er,p.z+n_er));
  float v3=obj(vec3(p.x-n_er,p.y+n_er,p.z-n_er));
  float v4=obj(vec3(p.x+n_er,p.y+n_er,p.z+n_er));
  return normalize(vec3(v4+v1-v3-v2,v3+v4-v1-v2,v2+v4-v3-v1));
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
  return vec3(background); //background color
}

void main()
{
  //Camera animation - ensure camera stays above blocks
  vec3 vuv=vec3(0.0,1.0,0.0);
  
  // Calculate safe camera height (above maximum wave height)
  float maxWaveHeight = 2.0; // Maximum possible wave amplitude
  float blockHeight = 1.2; // Height of the blocks themselves
  float safeHeight = maxWaveHeight + blockHeight + 3.0; // Extra margin
  
  // Target position with safe height constraint
  vec3 vrp=vec3(iTime*2.0, max(sin(iTime)*2.0, safeHeight - 1.0), 0.0);
  
  // Fixed camera angles for automatic rotation with height safety
  float mx = iTime * 0.5;
  float my = sin(iTime * 0.3) * 0.3 + 0.7; // Keep camera angled downward
  
  // Camera position with minimum height constraint
  float minCameraHeight = safeHeight;
  vec3 prp=vrp+vec3(cos(my)*cos(mx),sin(my),cos(my)*sin(mx))*12.0; // Increased distance
  
  // Ensure camera never goes below minimum safe height
  prp.y = max(prp.y, minCameraHeight);
  
  // Also ensure look-at point stays at reasonable height
  vrp.y = max(vrp.y, safeHeight - 2.0);
  
  float vpd=1.5;
  vec3 light=prp+vec3(5.0,8.0,5.0); // Higher light position
  
  vec3 scp=camera(prp,vrp,vuv,vpd);
  vec3 n,p;
  float f;
  int o;
  const float maxe=0.01;
  const float startf=0.1;
  const vec3 backc=vec3(0.05, 0.05, 0.1); // Dark blue background instead of black
  const float spec=8.0;
  const vec3 ambi=vec3(0.15, 0.15, 0.15); // Brighter ambient for better colors
  
  vec3 c1=render(prp,scp,64,maxe,startf,40.0,backc,light,spec,ambi,n,p,f,o);
  c1=c1*max(1.0-f*.015,0.0);
  vec3 c2=backc;
  if (o>0){
    scp=reflect(scp,n);
    c2=render(p+scp*0.05,scp,32,maxe,startf,8.0,backc,light,spec,ambi,n,p,f,o);
  }
  c2=c2*max(1.0-f*.1,0.0);
  gl_FragColor=vec4(c1.xyz*0.75+c2.xyz*0.25,1.0);
}