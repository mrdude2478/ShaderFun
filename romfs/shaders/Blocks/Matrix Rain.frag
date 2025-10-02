precision mediump float;

uniform vec3 iResolution;
uniform float iTime;
varying vec2 vUV;

#define NUM_COLS 60
#define NUM_ROWS 50

// Random number generator
float rand(float x){
    return fract(sin(x)*2758.50);
}

// Compute brightness of a character at a given row in a column
float charBrightness(float colSeed, float row, float speed){
    float offset = iTime * speed + colSeed * 10.0;
    float pos = mod(row + offset, float(NUM_ROWS));
    // Bright head
    if(pos < 1.0) return 1.0;
    // Trailing fade
    return exp(-pos*0.5);
}

void main(){
    vec2 uv = vUV;
    uv.x *= iResolution.x/iResolution.y;

    float colF = floor(uv.x * float(NUM_COLS));
    float rowF = floor(uv.y * float(NUM_ROWS));

    float colSeed = rand(colF);

    float speed = 2.0 + colSeed*2.0; // different speed per column
    float brightness = charBrightness(colSeed,rowF,speed);

    vec3 color = vec3(0.0, brightness, 0.0);

    gl_FragColor = vec4(color,1.0);
}
