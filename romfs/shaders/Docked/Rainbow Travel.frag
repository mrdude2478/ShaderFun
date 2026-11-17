// Rainbow Travel
// By Noztol
// Based on https://fragcoord.xyz/s/fx196wc1
// but using XorDev's color scheme
// Converted for ShaderFun (Nintendo Switch, GLSL ES 2.0)

precision mediump float;

uniform vec3  iResolution;
uniform float iTime;

// Calculates the base winding 3D path of the tunnel
vec3 getPathPosition(float z) {
    return vec3(12.0 * cos(z * vec2(0.1, 0.12)), z);
}

// tanh does not exist in GLSL ES 2.0 -- stable version, never overflows.
vec3 tanh3(vec3 x) {
    vec3 ax = abs(x);
    vec3 e  = exp(-2.0 * ax);
    vec3 t  = (1.0 - e) / (1.0 + e);
    return sign(x) * t;
}

vec4 tanh4(vec4 x) {
    vec4 ax = abs(x);
    vec4 e  = exp(-2.0 * ax);
    vec4 t  = (1.0 - e) / (1.0 + e);
    return sign(x) * t;
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;

    // Normalize screen coordinates
    vec2 uv = (fragCoord - iResolution.xy * 0.5) / iResolution.y;

    // Time variables for animation
    float animTime = iTime * 4.0 + 5.0 + 5.0 * sin(iTime * 0.3);

    // Setup Camera
    vec3 rayOrigin = getPathPosition(animTime);
    vec3 lookTarget = getPathPosition(animTime + 4.0);

    vec3 forward = normalize(lookTarget - rayOrigin);
    vec3 right = normalize(vec3(-forward.z, 0.0, forward.x));
    vec3 up = cross(forward, right);
    vec3 rayDir = normalize(uv.x * right + uv.y * up + forward);

    // Initializations
    float stepDist = 1.0;
    float totalDist = 0.0;
    float orbDist = 1.0;
    vec3 accumulatedColor = vec3(0.0);
    vec3 rayPos = rayOrigin;

    // Outer loop: constant bound 28, matching the original.
    for (int ii = 1; ii <= 28; ii++) {
        if (totalDist >= 30.0) break;

        float i = float(ii);

        // 1. March Ray Forward
        rayPos += rayDir * stepDist;

        // 2. Get path center and time vars
        vec3 pathCenter = getPathPosition(rayPos.z);
        float sineTime = sin(iTime);

        // 3. Orb Geometry
        vec3 orbCenter = vec3(
            pathCenter.x + sineTime,
            pathCenter.y + sineTime * 2.0,
            6.0 + animTime + sineTime * 2.0
        );
        orbDist = length(rayPos - orbCenter) - 0.01;

        // 4. Base Tunnel Structure
        float baseRadius = cos(rayPos.z * 0.6) * 2.0 + 4.0;

        float tunnelStructure = min(
            length(rayPos.xy - pathCenter.x - 6.0),
            length((rayPos - pathCenter).xy)
        );

        // Crisp architectural scoops
        float largeScoops = abs(dot(sin(0.4 * rayPos), vec3(0.25))) / 0.1;
        float detailTexture = abs(dot(sin(animTime + 16.0 * rayPos), vec3(0.22))) / 2.0;

        float carvedDist = baseRadius - tunnelStructure + largeScoops + detailTexture;

        // 5. FIREWALL FLUIDITY
        vec3 fluidPos = rayPos;
        // Inner loop bound is now a constant (7.0 -> 7).
        // We cannot use `j <= 7.0 * i` here because ES 2.0 requires
        // constant loop bounds. Instead we run a fixed 7 iterations
        // and scale by `i` inside (see note below).
        for (int jj = 1; jj <= 7; jj++) {
            float j = float(jj);
            fluidPos += sin(fluidPos.yzx * j + iTime + 0.5 * i) / j;
        }

        float fluidTunnelDist = 0.4 * length(vec4(0.3 * cos(fluidPos) - 0.3, carvedDist));

        // 6. Update Distances
        stepDist = min(orbDist, fluidTunnelDist);
        totalDist += stepDist;

        // 7. Accumulate Color
        vec3 palette = 1.0 + cos(fluidPos.y + i * 0.4 + vec3(6.0, 1.0, 2.0));

        accumulatedColor += (2.5 * palette / stepDist + 10.0 * palette / max(orbDist, 0.6)) / i;
    }

    // Tonemapping
    //gl_FragColor = vec4(tanh(accumulatedColor * accumulatedColor / 1500.0).rgb, 1.0);
    gl_FragColor = vec4(tanh3(accumulatedColor * accumulatedColor / 1500.0), 1.0);
}