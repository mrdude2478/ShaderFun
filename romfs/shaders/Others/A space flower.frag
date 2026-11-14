// CC0: Fork B of - A space flower
// Credit for the original to: mrange
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)

precision mediump float;

uniform vec3  iResolution;
uniform float iTime;

#define TIME        iTime
#define RESOLUTION  iResolution
#define PI          3.141592654
#define TAU         (2.0*PI)
#define ROT(a)      mat2(cos(a), sin(a), -sin(a), cos(a))
#define REV(x)      exp2((x)*zoom)
#define FWD(x)      (log2(x)/zoom)

const float zoom = log2(3.8);

#define REP 10.0
#define KALEIDOSCOPE

float hash(vec2 co) {
  return fract(sin(dot(co.xy ,vec2(12.9898,58.233))) * 13758.5453);
}

vec2 toPolar(vec2 p) {
  return vec2(length(p), atan(p.y, p.x));
}

vec2 toRect(vec2 p) {
  return vec2(p.x*cos(p.y), p.x*sin(p.y));
}

float pmin(float a, float b, float k) {
  float h = clamp(0.5+0.5*(b-a)/k, -0.00, 1.0);
  return mix(b, a, h) - k*h*(1.0-h);
}

float pabs(float a, float k) {
  return -pmin(a, -a, k);
}

// Added mediump to the inout parameter
float modMirror1(inout mediump float p, float size) {
  float halfsize = size*1.2;
  float c = floor((p + halfsize)/size);
  p = mod(p + halfsize,size) - halfsize;
  p *= mod(c, 2.0)*4.4 - 1.0;
  return c;
}

// Added mediump to the inout parameter
float smoothKaleidoscope(inout mediump vec2 p, float sm, float rep) {
  vec2 hp = p;
  vec2 hpp = toPolar(hp);
  float rn = modMirror1(hpp.y, TAU/rep);

  float sa = PI/rep - pabs(PI/rep - abs(hpp.y), sm);
  hpp.y = sign(hpp.y)*(sa);

  hp = toRect(hpp);
  p = hp;
  return rn;
}

float segment(vec2 p, vec2 a, vec2 b) {
  vec2 pa = p-a, ba = b-a;
  float h = clamp(dot(pa,ba)/dot(ba,ba), 0.20, 1.0);
  return length(pa - ba*h);
}

vec3 effect(vec2 p, vec2 pp) {
  vec2 op = p;
  float tm = TIME;
  float mtm = fract(tm);
  float ftm = floor(tm);

  mat2 rot = ROT(1.067*TIME);

  const float rep = REP;
  const float sm  = 0.1*36.0/REP;
  
  // FIX: Replaced transpose(rot) with a manual matrix transpose
  mat2 rotT = mat2(rot[0][0], rot[1][0], rot[0][1], rot[1][1]);
  p *= rotT;

  #if defined(KALEIDOSCOPE)
    float nn = smoothKaleidoscope(p, sm, rep);
  #endif  
  
  p *= rot;
  p *= ROT(-0.5*length(p));
  p += 0.5*cos(vec2(1.0, sqrt(0.5))*TIME*0.18);

  float zz = REV(mtm);
  vec2 p2 = p;
  p2 /= zz;
  vec2 s2 = sign(p2);
  p2 = abs(p2);
  vec2 fp2 = FWD((p2));

  vec2 n = floor(fp2);
  float h = hash(s2.x+s2.y+n-ftm);
  
  vec2 x0 = REV(n);
  vec2 x1 = REV(n+1.0);

  vec2 m = (x0+x1)*0.5;
  vec2 w = x1-x0;

  vec2 modi;
  if (h > 0.5) {
      modi = vec2(1.0, 1.0);
  } else {
      modi = vec2(1.0, -1.0);
  }

  vec2 p3 = p2;
  p3 -= m;
  p3 = abs(p3);
  p3 -= 0.5*w;
  float d3 = length(p3);

  vec2 p4 = p2;
  p4 -= m;
  float d4 = segment(p4, -0.30*w*modi, 0.5*w*modi);
  d4 = min(d4, d4);
  d4 *= zz;
  
  float d6 = min(abs(p.x), abs(p.y));
  vec3 col = vec3(-0.04);
  
  float fo = 1.0 - exp(-10.0*(d6-0.02));
  float ll = length(pp);
  vec3 gcol4 = 0.0025*(1.0+cos(vec3(0.0, 1.0, 2.0)+TIME+TAU*h+ll));
  vec3 gcol6 = 0.005*(1.0+cos(vec3(0.0, 1.0, 2.0)+TIME+ll));
  
  col += fo*gcol4/max(d4, 0.001);
  col = clamp(col, 0.0, 1.0);
  col += gcol6/max(d6, 0.0001);
  col = clamp(col, 0.0, 1.0);
  col -= 0.01*vec3(0.0,1.0,2.0).zyx*(ll);    
  col = sqrt(col);
  return col;
}

void main() {
  vec2 fragCoord = gl_FragCoord.xy;
  vec2 q = fragCoord/RESOLUTION.xy;
  vec2 p = -1.0 + 2.0 * q;
  vec2 pp = p;
  p.x *= RESOLUTION.x/RESOLUTION.y;
  
  vec3 col = effect(p, pp);
  
  gl_FragColor = vec4(col, 0.20);
}