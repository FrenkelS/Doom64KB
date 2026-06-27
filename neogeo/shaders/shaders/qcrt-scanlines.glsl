#if defined(VERTEX)

#if __VERSION__ >= 130
#define IN  in
#define OUT out
#else
#define IN attribute 
#define OUT varying 
#endif

uniform vec2 TextureSize;
uniform vec2 InputSize;
uniform vec2 OutputSize;
uniform mat4 MVPMatrix;

IN vec2 VertexCoord;
IN vec2 TexCoord;

OUT vec2 cu_coord;
OUT float v_scanline_pos;

void main() { 
    // Screen and texture's initial position
    gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);
    cu_coord = TexCoord;
    if (TexCoord.y < 0.01) {
        v_scanline_pos = 0.0;
    } else {
        v_scanline_pos = InputSize.y;
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

#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D Texture;
uniform vec2 TextureSize;

IN vec2 cu_coord;
IN float v_scanline_pos;

#define GAUSS_BASE 0.60
#define SCANLINE_CENTER_OFFSET 0.6

void main() {
    vec4 col_intensity = tex2D(Texture, cu_coord);
    vec3 col = col_intensity.rgb;
    float div_intensity = col_intensity.a;

    // guassian scanline
    float center_y = SCANLINE_CENTER_OFFSET-fract(v_scanline_pos);
    float gauss = exp(-(center_y*center_y)/(GAUSS_BASE*div_intensity));

    FragColor = vec4(col*gauss, 1.0);
}

#endif
