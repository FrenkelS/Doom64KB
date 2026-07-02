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

OUT vec2 l2_coord;
OUT vec2 l1_coord;
OUT vec2 cu_coord;
OUT vec2 r1_coord;
OUT vec2 r2_coord;

void main() { 
    gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);
    float texel_width = 1.0/TextureSize.x;
    
    l2_coord = TexCoord - vec2(2.0*texel_width, 0.0);
    l1_coord = TexCoord - vec2(texel_width, 0.0);
    cu_coord = TexCoord;
    r1_coord = TexCoord + vec2(texel_width, 0.0);
    r2_coord = TexCoord + vec2(2.0*texel_width, 0.0);
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

IN vec2 l2_coord;
IN vec2 l1_coord;
IN vec2 cu_coord;
IN vec2 r1_coord;
IN vec2 r2_coord;

#define SQRT_3 1.732050808
#define GAMMA 0.9

void main() {
    vec3 l2_col = tex2D(Texture, l2_coord).rgb;
    vec3 l1_col = tex2D(Texture, l1_coord).rgb;
    vec3 cu_col = tex2D(Texture, cu_coord).rgb;
    vec3 r1_col = tex2D(Texture, r1_coord).rgb;
    vec3 r2_col = tex2D(Texture, r2_coord).rgb;

    // sharpen and ghosting kernel
    vec3 col = (-0.03 * l2_col)
             + ( 0.16 * l1_col)
             + ( 1.0  * cu_col)
             + (-0.16 * r1_col)
             + ( 0.03 * r2_col);

    // "luminous intensity" of the pixel, normalized
    float intensity = sqrt(dot(col, col)) / SQRT_3;
    vec3 gamma_col = pow(col, vec3(GAMMA));
    FragColor = vec4(gamma_col, intensity);
}

#endif
