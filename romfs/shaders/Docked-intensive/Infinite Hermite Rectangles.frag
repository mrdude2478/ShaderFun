precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define SHOW_GRID 1
#define FUN_REFLECT 0

const float c_scale = 0.3;  // Reduced scale for less extreme waves
const float c_rate = 1.5;   // Slower rate for smoother animation

#define FLT_MAX 3.402823466e+38

//=======================================================================================
float CubicHermite (float A, float B, float C, float D, float t)
{
    float t2 = t*t;
    float t3 = t*t*t;
    float a = -A/2.0 + (3.0*B)/2.0 - (3.0*C)/2.0 + D/2.0;
    float b = A - (5.0*B)/2.0 + 2.0*C - D / 2.0;
    float c = -A/2.0 + C/2.0;
    float d = B;
    
    return a*t3 + b*t2 + c*t + d;
}

//=======================================================================================
float hash(float n) {
    return fract(sin(n) * 43758.5453123);
}

//=======================================================================================
float GetHeightAtTile(vec2 T)
{
    float rate = hash(hash(T.x) * hash(T.y)) * 0.5 + 0.5;
    
    // Smoother animation with time scaling
    float smoothTime = iTime * 0.5; // Slow down overall animation
    return (sin(smoothTime * rate * c_rate) * 0.5 + 0.5) * c_scale;
}

//=======================================================================================
float HeightAtPos(vec2 P)
{
    vec2 tile = floor(P);
    P = fract(P);
    
    float CP0X = CubicHermite(
        GetHeightAtTile(tile + vec2(-1.0,-1.0)),
        GetHeightAtTile(tile + vec2(-1.0, 0.0)),
        GetHeightAtTile(tile + vec2(-1.0, 1.0)),
        GetHeightAtTile(tile + vec2(-1.0, 2.0)),
        P.y
    );
    
    float CP1X = CubicHermite(
        GetHeightAtTile(tile + vec2( 0.0,-1.0)),
        GetHeightAtTile(tile + vec2( 0.0, 0.0)),
        GetHeightAtTile(tile + vec2( 0.0, 1.0)),
        GetHeightAtTile(tile + vec2( 0.0, 2.0)),
        P.y
    );    
    
    float CP2X = CubicHermite(
        GetHeightAtTile(tile + vec2( 1.0,-1.0)),
        GetHeightAtTile(tile + vec2( 1.0, 0.0)),
        GetHeightAtTile(tile + vec2( 1.0, 1.0)),
        GetHeightAtTile(tile + vec2( 1.0, 2.0)),
        P.y
    );        
    
    float CP3X = CubicHermite(
        GetHeightAtTile(tile + vec2( 2.0,-1.0)),
        GetHeightAtTile(tile + vec2( 2.0, 0.0)),
        GetHeightAtTile(tile + vec2( 2.0, 1.0)),
        GetHeightAtTile(tile + vec2( 2.0, 2.0)),
        P.y
    );
    
    return CubicHermite(CP0X, CP1X, CP2X, CP3X, P.x);
}

//=======================================================================================
vec3 NormalAtPos( vec2 p )
{
    float eps = 0.01;
    vec3 n = vec3( HeightAtPos(vec2(p.x-eps,p.y)) - HeightAtPos(vec2(p.x+eps,p.y)),
                         2.0*eps,
                         HeightAtPos(vec2(p.x,p.y-eps)) - HeightAtPos(vec2(p.x,p.y+eps)));
    return normalize( n );
}

//=======================================================================================
vec3 DiffuseColor (in vec3 pos)
{
    #if SHOW_GRID
    pos = mod(floor(pos),2.0);
    return vec3(mod(pos.x + pos.z, 2.0) < 1.0 ? 1.0 : 0.4);
    #else
    return vec3(0.1, 0.8, 0.9);
    #endif
}

//=======================================================================================
vec3 ShadePoint (in vec3 pos, in vec3 rayDir, bool fromUnderneath)
{
    vec3 diffuseColor = DiffuseColor(pos);
    vec3 reverseLightDir = normalize(vec3(1.0,1.0,-1.0));
    vec3 lightColor = vec3(0.95,0.95,0.95);    
    vec3 ambientColor = vec3(0.1,0.1,0.1); // Brighter ambient

    vec3 normal = NormalAtPos(pos.xz);
    normal *= fromUnderneath ? -1.0 : 1.0;

    // diffuse
    vec3 color = diffuseColor * ambientColor;
    float dp = dot(normal, reverseLightDir);
    if(dp > 0.0)
        color += (diffuseColor * dp * lightColor);
    
    // specular
    vec3 reflection = reflect(reverseLightDir, normal);
    dp = dot(rayDir, reflection);
    if (dp > 0.0)
        color += pow(abs(dp), 15.0) * vec3(0.5);        
    
    // Simple sky color with gradient
    vec3 skyColor = mix(vec3(0.1, 0.2, 0.4), vec3(0.3, 0.5, 0.8), rayDir.y * 0.5 + 0.5);
    reflection = reflect(rayDir, normal);
    color += skyColor * 0.15; // Reduced reflection intensity
    
    return color;
}

//=======================================================================================
void main()
{   
    // Convert to pixel coordinates
    vec2 fragCoord = vUV * iResolution.xy;
    
    // Better camera positioning - closer and lower angle to fill screen
    float angleX = 3.14 + iTime * 0.15; // Slower rotation
    float angleY = 0.3 + sin(iTime * 0.05) * 0.1; // Lower angle, slower movement
    
    vec3 cameraOffset = vec3(iTime * 0.3, 0.3, iTime * 0.3); // Slower scrolling
    vec3 cameraAt = vec3(0.5, 0.2, 0.5) + cameraOffset; // Lower target point
    
    // Closer camera distance
    vec3 cameraPos = (vec3(sin(angleX)*cos(angleY), sin(angleY), cos(angleX)*cos(angleY))) * 3.0;
    cameraPos += vec3(0.5, 0.5, 0.5) + cameraOffset;

    vec3 cameraFwd = normalize(cameraAt - cameraPos);
    vec3 cameraLeft = normalize(cross(cameraFwd, vec3(0.0, 1.0, 0.0)));
    vec3 cameraUp = normalize(cross(cameraLeft, cameraFwd));

    // Larger field of view to fill screen
    float cameraViewWidth = 8.0;
    float cameraViewHeight = cameraViewWidth * iResolution.y / iResolution.x;
    float cameraDistance = 4.0; // Closer for more coverage
    
    // Ray direction with better FOV calculation
    vec2 percent = (vUV - 0.5) * 2.0; // -1 to 1 range
    
    vec3 rayTarget = (cameraFwd * cameraDistance)
                   - (cameraLeft * percent.x * cameraViewWidth * 0.5)
                   + (cameraUp * percent.y * cameraViewHeight * 0.5);
    vec3 rayDir = normalize(rayTarget);
    
    // Improved ray marching with more steps and adaptive step size
    vec3 pixelColor = mix(vec3(0.1, 0.2, 0.4), vec3(0.3, 0.5, 0.8), vUV.y); // Vertical sky gradient
    
    const int steps = 80; // More steps for better quality
    bool hitFound = false;
    bool fromUnderneath = false;
    float minDist = FLT_MAX;
    vec3 hitPos = vec3(0.0);
    
    vec3 rayPos = cameraPos;
    float firstSign = sign(rayPos.y - HeightAtPos(rayPos.xz));
    
    for (int i = 0; i < steps; i++) {
        float height = HeightAtPos(rayPos.xz);
        float dist = abs(rayPos.y - height);
        
        if (dist < minDist) {
            minDist = dist;
            hitPos = rayPos;
        }
        
        if (sign(rayPos.y - height) * firstSign < 0.0) {
            fromUnderneath = firstSign < 0.0;
            hitFound = true;
            break;
        }
        
        // Adaptive step size - smaller steps when close to surface
        float stepSize = 0.1 + dist * 0.5;
        rayPos += rayDir * stepSize;
        
        // Early exit if too far
        if (length(rayPos - cameraPos) > 20.0) break;
    }
    
    if (hitFound) {
        pixelColor = ShadePoint(hitPos, rayDir, fromUnderneath);
    }
    
    gl_FragColor = vec4(clamp(pixelColor, 0.0, 1.0), 1.0);
}