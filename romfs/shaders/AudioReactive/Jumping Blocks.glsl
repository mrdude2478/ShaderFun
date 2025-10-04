// Frequency-Specific Jumping Blocks - Fixed Upward Jumping
precision mediump float;
uniform vec3 iResolution;
uniform float iTime;
uniform sampler2D iChannel0; // Waveform
uniform sampler2D iChannel1; // Spectrum
varying vec2 vUV;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 lerpColor(vec3 a, vec3 b, float t) {
    return a + (b - a) * t;
}

void main() {
    vec2 uv = vUV;
    
    // Sample audio data
    float waveform = texture2D(iChannel0, vec2(0.5, 0.0)).r;
    float spectrum = texture2D(iChannel1, vec2(0.5, 0.0)).r;
    
    float subBass = texture2D(iChannel1, vec2(0.02, 0.0)).r * 3.0;
    float bass = texture2D(iChannel1, vec2(0.1, 0.0)).r * 2.5;
    float lowMids = texture2D(iChannel1, vec2(0.25, 0.0)).r * 2.0;
    float mids = texture2D(iChannel1, vec2(0.4, 0.0)).r * 1.8;
    float highMids = texture2D(iChannel1, vec2(0.55, 0.0)).r * 1.6;
    float presence = texture2D(iChannel1, vec2(0.7, 0.0)).r * 1.4;
    float brilliance = texture2D(iChannel1, vec2(0.9, 0.0)).r * 1.2;
    
    vec3 color = vec3(0.0);
    
    float blockWidth = 80.0 / iResolution.x;
    float blockHeight = 120.0 / iResolution.y;
    float blockSpacing = 20.0 / iResolution.x;
    
    float maxJumpHeight = 0.4; // Can jump up to 60% of screen height
    float jumpThreshold = 0.01;
    
    // Start blocks lower on screen so they jump UP toward the top
    float restY = 0.3; // 30% from bottom (instead of 0.5/center)
    float blockCount = 8.0;
    
    for (float i = 0.0; i < blockCount; i++) {
        float totalWidth = blockCount * blockWidth + (blockCount - 1.0) * blockSpacing;
        float startX = (1.0 - totalWidth) * 0.5;
        float blockX = startX + i * (blockWidth + blockSpacing);
        
        float blockAudio = 0.0;
        float jumpSpeed = 0.0;
        float audioBoost = 1.0;
        
        if (i == 0.0) {
            blockAudio = subBass * 0.5;
            jumpSpeed = 2.5; // Slower speed for heavier blocks
            audioBoost = 0.5;
        } else if (i == 1.0) {
            blockAudio = bass * 1.0;
            jumpSpeed = 2.5;
            audioBoost = 1.0;
        } else if (i == 2.0) {
            blockAudio = lowMids * 1.5;
            jumpSpeed = 3.0;
            audioBoost = 2.0;
        } else if (i == 3.0) {
            blockAudio = mids * 2.5;
            jumpSpeed = 3.0;
            audioBoost = 2.5;
        } else if (i == 4.0) {
            blockAudio = mids * 2.5;
            jumpSpeed = 3.0;
            audioBoost = 3.0;
        } else if (i == 5.0) {
            blockAudio = highMids * 2.5;
            jumpSpeed = 3.0;
            audioBoost = 3.5;
        } else if (i == 6.0) {
            blockAudio = presence * 2.5;
            jumpSpeed = 3.0;
            audioBoost = 4.0;
        } else if (i == 7.0) {
            blockAudio = presence * 3.5;
            jumpSpeed = 3.5;
            audioBoost = 4.5;
        } else {
            blockAudio = brilliance * 0.2;
            jumpSpeed = 3.0;
            audioBoost = 2.2;
        }
        
        blockAudio = min(blockAudio * audioBoost, 1.0);
        
        float jump = 0.0;
        if (blockAudio > jumpThreshold) {
            float jumpPower = pow(blockAudio, 0.5);
            float jumpPhase = iTime * jumpSpeed;
            float jumpWave = sin(jumpPhase);
            
            if (jumpWave > 0.0) {
                jump = jumpPower * maxJumpHeight * jumpWave;
            }
            
            
            
            if (blockAudio > 0.1) {
                jump += blockAudio * maxJumpHeight * 0.3;
            }
        }
        
        // FIXED: Jump UPWARD toward top of screen
        float blockY = restY + jump; // ADD jump to move upward
        
        vec2 blockCenter = vec2(blockX + blockWidth * 0.5, blockY);
        vec2 pixelPos = vec2(uv.x, uv.y);
        vec2 dist = abs(pixelPos - blockCenter);
        
        if (dist.x < blockWidth * 0.5 && dist.y < blockHeight * 0.5) {
            vec3 baseColors[8];
            baseColors[0] = vec3(1.0, 0.1, 0.1);
            baseColors[1] = vec3(1.0, 0.4, 0.1);
            baseColors[2] = vec3(1.0, 0.8, 0.1);
            baseColors[3] = vec3(0.6, 1.0, 0.1);
            baseColors[4] = vec3(0.1, 1.0, 0.4);
            baseColors[5] = vec3(0.1, 0.8, 1.0);
            baseColors[6] = vec3(0.3, 0.3, 1.0);
            baseColors[7] = vec3(0.8, 0.3, 1.0);
            
            vec3 baseColor = baseColors[int(i)];
            float colorIntensity = 0.8 + blockAudio * 1.5;
            vec3 blockColor = baseColor * colorIntensity;
            
            if (jump > 0.0) {
                float glowIntensity = jump / maxJumpHeight;
                blockColor += baseColor * glowIntensity * 1.0;
                
                
                /*
                if (glowIntensity > 0.7) {
                    float hotSpot = smoothstep(0.7, 1.0, glowIntensity);
                    blockColor += vec3(1.0, 1.0, 0.8) * hotSpot * 0.5;
                }
                */
            }
            
            //float pulse = 1.0 + 0.3 * sin(iTime * jumpSpeed * 2.0);
            float pulse = 1.0 + 0.2 * sin(iTime * jumpSpeed * 1.5);
            blockColor *= pulse;
            
            color = blockColor;
            break;
        }
    }
    
    if (spectrum > 0.02) {
        float borderPulse = sin(iTime * 8.0) * 0.5 + 0.5;
        float border = smoothstep(0.45, 0.5, max(abs(uv.x - 0.5), abs(uv.y - 0.5)));
        color += vec3(spectrum * 0.1) * border * borderPulse;
    }
    
    gl_FragColor = vec4(color, 1.0);
}