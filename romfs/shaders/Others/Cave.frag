// Rio Subterraneo / Cave
// Original by the author on Shadertoy
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
// Optimized: removed globals, fixed loops, removed bump/AO for stability.

precision mediump float;

uniform vec3  iResolution;
uniform float iTime;

#define h21(p) fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123)
#define path(z) vec2(sin(z * .15) * 2.4, cos(z * .25) * 1.7)
#define ballMovez(t)  tanh1(cos(t * .5) * 6. + 4.) * 3. + 7.     
#define ballMoveXY(t) tanh1(sin(t) * 5. - 3.) * 4.
#define tri(x) abs(x - floor(x) - .5)
#define trap(x, pf) (tri(x - pf * .125) + tri(x + pf * .125)) * .5
#define t iTime
#define FAR 40.0

// tanh does not exist in GLSL ES 2.0
float tanh1(float x) {
    float e = exp(2.0 * x);
    return (e - 1.0) / (e + 1.0);
}

// We need our own tanh for the macros
float openCave(float t) {
    return (tanh1(cos(t * .05) * 12. + 10.) * .5 + .5);
}

vec3 lp;
bool G;

float waterSurf(vec3 p) {
    p += vec3(0, 0, t * 4.);
    return dot(sin(p + sin(p.yzx * 2.) * cos(p.y)), vec3(.2)) + 1.;
}

// map() now takes 'p' and returns the distance. 
// We use 'G' to skip the water calculation for the refraction pass.
float map(vec3 p, bool isRefractionPass) {
    float sf = dot(trap(p * .384 + trap(p.yzx * .192, .75), .75), vec3(.666));
    sf = (1. - sf) * (cos(p.z * .4) + 1.02);
    
    p.xy -= path(p.z);
    
    float tunel = 1.5 - length(p.xy * vec2(.6, 0.7071)) * openCave(t) + sf;
    float ball = length(p - lp) - 1.;
    
    tunel = min(tunel, ball);
    
    if (isRefractionPass) {
        return tunel;
    }
    
    p.y += waterSurf(p);
    return min(tunel, p.y);
}

vec3 getNormal(vec3 p, bool isRefractionPass) {
    vec2 e = vec2(.01, 0.0);
    return normalize(vec3(
        map(p + e.xyy, isRefractionPass) - map(p - e.xyy, isRefractionPass),
        map(p + e.yxy, isRefractionPass) - map(p - e.yxy, isRefractionPass),
        map(p + e.yyx, isRefractionPass) - map(p - e.yyx, isRefractionPass)
    ));
}

vec3 doColor(vec3 sp, vec3 rd, vec3 sn, vec3 lp, float t) {
    vec3 col = vec3(0.0);
    if (t < FAR) {
        vec3 ld = lp - sp;
        float lDist = max(length(ld), .001);
        ld /= lDist;
        
        float atten = 1.5 / (1. + lDist * .125 + lDist * lDist * .025);
        
        float diff = max(dot(sn, ld), 0.);
        diff = pow(diff, 4.) * .66  +  pow(diff, 8.) * .34;
        
        float spec = pow(max(dot(reflect(-ld, sn), -rd), 0.0), 32.);
        
        vec3 objCol = vec3(1.0, .8, 0.5);
        // Removed svObjID check to avoid global state, defaulting to cave color
        
        col = (objCol * (diff + .7 + vec3(1, .6, .2) * spec * 2.));
        col *= atten;
    }
    return col;
}

// Replaced while loop with fixed for loop, guaranteed to terminate.
float march(vec3 ro, vec3 rd, bool isRefractionPass) {
    float t = 0.0;
    for (int i = 0; i < 64; i++) {
        float d = map(ro + rd * t, isRefractionPass);
        if (abs(d) < .001 * (t * .5 + 1.) || t > FAR) break;
        t += d * 0.8;
    }
    return t;
}

void main() {
    vec2 u = gl_FragCoord.xy;
    vec2 r = iResolution.xy;
    u = (u - r / 2.) / r.y;
    
    vec3 ro = vec3(0, 0, t);
    vec3 lk = ro + vec3(0, 0, .25);
    lp = ro + vec3(0, .25, ballMovez(t));
    
    ro.xy += path(ro.z);
    lk.xy += path(lk.z);
    lp.xy += path(lp.z + ballMoveXY(t)) - vec2(0, 1);
    
    vec3 forward = normalize(lk - ro);
    vec3 right = normalize(vec3(forward.z, 0., -forward.x));
    vec3 up = cross(forward, right);
    vec3 rd = normalize(forward + (u.x * right + u.y * up));
    
    // ------------------------------------------------
    float tMarch = march(ro, rd, false);
    vec3 sp = ro + rd * tMarch;
    
    vec3 sn = getNormal(sp, false);
    vec3 col = doColor(sp, rd, sn, lp, tMarch);
    
    float fog = tMarch / FAR + .2;
    
    vec3 refr = refract(rd, sn, 1. / 1.33);
    float fr = clamp(1. + dot(rd, sn), 0., 1.);
    
    // Check if we hit the water surface (the original used a global ID for this).
    // Since we removed the global, we approximate it by seeing if refraction is possible.
    bool isWater = (length(refr) > 0.01 && tMarch < FAR);
    
    if (isWater) {
        float tRefr = march(sp, refr, true);
        vec3 refSp = sp + refr * tRefr;
        vec3 snRefr = getNormal(refSp, true);
        
        vec3 refrColor = doColor(refSp, refr, snRefr, lp, tRefr);
        col = col * .2 + mix(refrColor, col, pow(fr, 5.) * .4 + .2);
    } else {
        col = pow(col, vec3(.5)) * 0.5 - fog * .3;
    }
    
    gl_FragColor = vec4(pow(smoothstep(.05, .4, col * fog), vec3(.45)), 1.0);
}