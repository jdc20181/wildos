// The upsampling pass of the dual Kawase blur, as in KWin's blur effect.
// The effect prepends the #version line and the IN, TEXTURE and FRAG_COLOR
// macros.

uniform sampler2D sampler;
uniform float offset;
uniform vec2 halfpixel;

IN vec2 texcoord0;

void main()
{
    vec2 uv = texcoord0;
    vec4 sum = TEXTURE(sampler, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);
    sum += TEXTURE(sampler, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += TEXTURE(sampler, uv + vec2(0.0, halfpixel.y * 2.0) * offset);
    sum += TEXTURE(sampler, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += TEXTURE(sampler, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);
    sum += TEXTURE(sampler, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
    sum += TEXTURE(sampler, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);
    sum += TEXTURE(sampler, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;
    FRAG_COLOR = sum / 12.0;
}
