#if defined(VERTEX)

#if __VERSION__ >= 130
#define IN  in
#define OUT out
#else
#define IN attribute 
#define OUT varying 
#endif

uniform vec2 TextureSize;
uniform vec2 OutputSize;
uniform mat4 MVPMatrix;

IN vec2 VertexCoord;
IN vec2 TexCoord;

OUT vec2 v_tex_coord;
OUT float v_grid_pos;

void main() { 
    gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);
    v_tex_coord = TexCoord;
    // ensure varying grid pos has only an integer part
    if (TexCoord.x < 0.01) {
        v_grid_pos = 0.0;
    } else {
        v_grid_pos = OutputSize.x;
    }
}


#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define IN in
#define tex2D texture
out vec4 FragColor;
#else
#define IN varying
#define FragColor gl_FragColor
#define tex2D texture2D
#endif

#define PI 3.1415926535897932384626433832795

#define INTENSITY_CUTOFF 2.4
#define GRID_MIN 0.3
#define GRID_AMPLITUDE 0.4

#define INTENSITY_BASE 0.1
#define INTENSITY_SCALE 0.7

#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D Texture;
uniform vec2 TextureSize;

IN vec2 v_tex_coord;
IN float v_grid_pos;

float intensity_for_scanline(float intensity) {
    float div_intensity=INTENSITY_BASE+(intensity*INTENSITY_SCALE);
    return div_intensity;
}

void main() {
    vec4 col_intensity = tex2D(Texture, v_tex_coord);
    vec3 col = col_intensity.rgb;
    float intensity=min(1.0,col_intensity.a*INTENSITY_CUTOFF);

    // dot mask: 3 pixels-wide gradient pattern, variating between
    // [ MIN_GRID_FACTOR..(MIN_GRID_FACTOR+GRADIENT_AMPLITUDE) ]
    float cosgrid=GRID_MIN+(fract(floor(v_grid_pos)/3.0)*GRID_AMPLITUDE);
    float minuscos=1.0-cosgrid;
    // lower dot mask effect when intensity increases (factor --> 1.0)
    float factor=cosgrid+(minuscos*intensity);

    // translate intensity scale for scanline shader
    float scaled_intensity=intensity_for_scanline(col_intensity.a);
    FragColor = vec4(col*factor, scaled_intensity);
}

#endif
