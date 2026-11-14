/*
    Mandelbrot Pattern Decoration
    Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)
*/

precision mediump float;

uniform vec3  iResolution;
uniform float iTime;

void main() {
    vec2 fragCoord = gl_FragCoord.xy;

    // Base color.
    vec3 col = vec3(0.0);
    
    // Antialiasing: 2x2 sample.
    #define AA 2
    for(int j=0; j<AA; j++){
        for(int i=0; i<AA; i++){

            // Offset centered coordinate
            vec2 p = (fragCoord + vec2(float(i), float(j))/float(AA) - iResolution.xy*.5)/iResolution.y;
            
            // Time, rotating back and forth.
            float ttm = cos(sin(iTime/8.))*6.2831;
            
            // --- Flying Cubes-style camera swivel ---
            // Fast continuous rotation (like rd.xy in Flying Cubes)
            float swivel = iTime * 0.2;
            float cs = cos(swivel), si = sin(swivel);
            p = mat2(cs, si, -si, cs) * ((p * 0.8)/0.455)/2.0;
            
            // Subtle zoom wobble (simulates "looking around")
            p *= 1.0 + sin(iTime * 0.5) * 0.015 * 2.0;
            
            // Existing slow oscillation (kept for variety)
            p *= mat2(cos(ttm), sin(ttm), -sin(ttm), cos(ttm));
            p -= vec2(cos(iTime/2.)/2., sin(iTime/3.)/5.);
             
            // Jump off point and zoom
                        // Zoom phase: -1 (zoomed out) to +1 (zoomed in)
            float zoomPhase = sin(iTime * 0.3);
            float zm = 500.0 * exp(zoomPhase * log(3.0));

            // Tilt synchronized with zoom — fractal banks as it dives in/pulls out
            float tilt = zoomPhase * 0.4;
            p = mat2(cos(tilt), sin(tilt), -sin(tilt), cos(tilt)) * p;
            vec2 cc = vec2(-.57735 + .004, .57735) + p/zm;
 
            // Position and derivative. Initialized to zero.
            vec2 z = vec2(0.0);
            vec2 dz = vec2(0.0);

            // Iterations
            const int iter = 48;
            int ik = 128; // Bail out value.
            vec3 fog = vec3(0.0);
             
            for(int k=0; k<iter; k++){
                // Derivative: z' = z*z'*2. + 1.
                dz = mat2(z, -z.y, z.x)*dz*2.0 + vec2(1.0, 0.0);
                
                // Position: z = z*z + c.
                z = mat2(z, -z.y, z.x)*z + cc;
                
                // Bailout
                if(dot(z, z) > 1.0/.005){
                    ik = k;
                    break;
                }
            }
            
            // Lines and shading.
            float ln = step(0.0, length(z)/15.5  - 1.0);
            
            // Distance and smooth coloring
            float d = sqrt(1.0/max(length(dz), .0001))*log(dot(z, z));
            d = clamp(d*50.0, 0.0, 1.0); 
            
            // Flagging successive layers.
            float dir = mod(float(ik), 2.0) < .5 ? -1.0 : 1.0;
            
            // Layer coloring and shading.
            float sh = (float(iter - ik))/float(iter);
            vec2 tuv = z/320.0;
            
            // Rotating the coordinates
            float tm = (-ttm*sh*sh*16.0);
            tuv *= mat2(cos(tm), sin(tm), -sin(tm), cos(tm));
            tuv = abs(mod(tuv, 1.0/8.0) - 1.0/16.0); 
          
            // Rendering a grid of circles
            float pat = smoothstep(0.0, 1.0/length(dz), length(tuv) - 1.0/32.0);
            pat = min(pat, smoothstep(0.0, 1.0/length(dz), abs(max(tuv.x, tuv.y) - 1.0/16.0) - .04/16.0));
            
            // Coloring the layer.
            vec3 lCol = pow(min(vec3(1.5, 1.0, 1.0)*min(d*.85, .96), 1.0), vec3(1.0, 3.0, 16.0))*1.15;
            
            // FIX: Replaced the ternary operator (? :) with a standard if/else.
            // The Switch ES 2.0 compiler fails when ? : returns a vec3.
            if (dir < 0.0) {
                lCol = lCol * min(pat, ln);
            } else {
                lCol = (sqrt(lCol)*.5 + .7) * max(1.0 - pat, 1.0 - ln);
            }
            
            // Fake reflection
            vec3 rd = normalize(vec3(p, 1.0));
            rd = reflect(rd, vec3(0.0, 0.0, -1.0));
            float diff = clamp(dot(z*.5 + .5, rd.xy), 0.0, 1.0)*d;
            
            // Fake reflective pattern
            tuv = z/200.0;
            tm = -tm/1.5 + .5;
            tuv *= mat2(cos(tm), sin(tm), -sin(tm), cos(tm));
            tuv = abs(mod(tuv, 1.0/8.0) - 1.0/16.0); 
            pat = smoothstep(0.0, 1.0/length(dz), length(tuv) - 1.0/32.0);
            pat = min(pat, smoothstep(0.0, 1.0/length(dz), abs(max(tuv.x, tuv.y) - 1.0/16.0) - .04/16.0));

            // Adding the fake gloss.
            lCol += mix(lCol, vec3(1.0)*ln, .5)*diff*diff*.5*(pat*.6 + .6);
            
            // Swizzling the color on every sixth layer
            if (mod(float(ik), 6.0) < .5) lCol = lCol.yxz;
            lCol = mix(lCol.xzy, lCol, d/1.2);
            
            // Deep black lined fringes
            lCol = mix(lCol, vec3(0.0), (1.0 - step(0.0, -(length(z)*.05*float(ik)/float(iter)  - 1.0)))*.95);
           
            // Applying the fog.
            lCol = mix(fog, lCol, sh*d);
            
            // Applying the color sample.
            col += min(lCol, 1.0);
        }
    }
    
    // Divide by the sample number.
    col /= float(AA*AA);
    
    // Subtle vignette.
    vec2 uv = fragCoord/iResolution.xy;
    col *= pow(16.0*(1.0 - uv.x)*(1.0 - uv.y)*uv.x*uv.y, 1.0/8.0)*1.15;
    
    gl_FragColor = vec4(sqrt(max(col, 0.0)), 1.0);
}