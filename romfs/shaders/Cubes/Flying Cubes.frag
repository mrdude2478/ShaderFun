precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

// Cheap vec3 to vec3 hash. Works well enough, but there are other ways.
vec3 hash33(vec3 p){ 
    float n = sin(dot(p, vec3(7.0, 157.0, 113.0)));    
    return fract(vec3(2097152.0, 262144.0, 32768.0)*n); 
}

float map(vec3 p){
    
    // Creating the repeat cubes, with slightly convex faces. Standard,
    // flat faced cubes don't capture the light quite as well.
   
    // Cube center offset, to create a bit of disorder, which breaks the
    // space up a little.
    vec3 o = hash33(floor(p))*.2; 
    
    // 3D space repetition.
    p = fract(p + o) - .5; 
    
    // A bit of roundness. Used to give the cube faces a touch of convexity.
    float r = dot(p, p) - 0.21;
    
    // Max of abs(x), abs(y) and abs(z) minus a constant gives a cube.
    // Adding a little bit of "r," above, rounds off the surfaces a bit.
    p = abs(p); 
    return max(max(p.x, p.y), p.z)*.95 + r*.05 - .21;
}

void main() {

    // Screen coordinates.
    vec2 uv = (vUV * iResolution.xy - iResolution.xy*.5)/iResolution.y;
    
    // Unit direction ray.
    vec3 rd = normalize(vec3(uv, (1.0 - dot(uv, uv)*.5)*.5));
    
    // Ray origin, scene color, and surface postion vector.
    vec3 ro = vec3(0.0, 0.0, iTime*3.0), col = vec3(0.0), sp;
    
    // Swivel the unit ray to look around the scene.
    float cs = cos(iTime*.375), si = sin(iTime*.375);    
    rd.xz = mat2(cs, si,-si, cs)*rd.xz;
    rd.xy = mat2(cs, si,-si, cs)*rd.xy;
    
    // Unit ray jitter
    rd *= 0.985 + hash33(rd)*.03;
    
    // Ray distance, bail out layer number, surface distance and normalized accumulated distance.
    float t=0.0, layers=0.0, d, aD;
    
    // Surface distance threshold.
    float thD = .035;
    
    // Reduced iterations for Switch performance
    for(int i=0; i<48; i++) {
        
        // Break conditions.
        if(layers>15.0 || col.x>1.0 || t>10.0) break;
        
        // Current ray postion.
        sp = ro + rd*t;
        
        d = map(sp); // Distance to nearest point in the cube field.
        
        // Normalized distance from the surface threshold value to our current isosurface value.
        aD = (thD-abs(d)*15.0/16.0)/thD;
        
        // If we're within the surface threshold, accumulate some color.
        if(aD>0.0) { 
            // Smoothly interpolate the accumulated surface distance value
            col += aD*aD*(3.0 - 2.0*aD)/(1.0 + t*t*.25)*.2; 
            layers++; 
        }

        // Ray marching step
        t += max(abs(d)*.7, thD*1.5); 
    }
    
    // Clamp color
    col = max(col, 0.0);
    
    // Color mixing - first pass
    col = mix(col, pow(col.x*vec3(1.5, 1.0, 1.0), vec3(1.0, 2.5, 12.0)), 
              dot(sin(rd.yzx*8.0 + sin(rd.zxy*8.0)), vec3(.1666)) + .4);
    
    // Color mixing - second pass
    col = mix(col, vec3(col.x*col.x*.85, col.x, col.x*col.x*.3), 
             dot(sin(rd.yzx*4.0 + sin(rd.zxy*4.0)), vec3(.1666)) + .25);
    
    // Final output
    gl_FragColor = vec4(max(col, 0.0), 1.0);
}