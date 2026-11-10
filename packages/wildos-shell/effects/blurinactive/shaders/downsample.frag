// The downsampling pass of the dual Kawase blur, as in KWin's blur effect.
// The effect prepends the #version line and the IN, TEXTURE and FRAG_COLOR
// macros.

uniform sampler2D sampler;
uniform float offset;
uniform vec2 halfpixel;

IN vec2 texcoord0;

void main()
{
    vec2 uv = texcoord0;
    vec4 sum = TEXTURE(sampler, uv) * 4.0;
    sum += TEXTURE(sampler, uv - halfpixel.xy * offset);
    sum += TEXTURE(sampler, uv + halfpixel.xy * offset);
    sum += TEXTURE(sampler, uv + vec2(halfpixel.x, -halfpixel.y) * offset);
    sum += TEXTURE(sampler, uv - vec2(halfpixel.x, -halfpixel.y) * offset);
    FRAG_COLOR = sum / 8.0;
}
