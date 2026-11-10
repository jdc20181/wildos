// Puts a blurred window on screen. The effect prepends the #version line and
// the IN, TEXTURE and FRAG_COLOR macros.

#include "colormanagement.glsl"
#include "saturation.glsl"

// The window as drawn, and its blur at half size.
uniform sampler2D sampler;
uniform sampler2D sampler1;
uniform vec4 modulation;

uniform float offset;
uniform vec2 halfpixel;
// 0 sharp, 1 fully blurred and dimmed.
uniform float progress;
uniform float dim;

// In logical pixels with the frame's top left at 0,0.
uniform vec2 expandedOrigin;
uniform vec2 expandedSize;
uniform vec2 frameSize;
uniform float radius;

IN vec2 texcoord0;

// The last upsampling step of the dual Kawase blur, done here rather than
// into a full size texture of its own.
vec4 blurred(vec2 uv)
{
    vec4 sum = TEXTURE(sampler1, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);
    sum += TEXTURE(sampler1, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += TEXTURE(sampler1, uv + vec2(0.0, halfpixel.y * 2.0) * offset);
    sum += TEXTURE(sampler1, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += TEXTURE(sampler1, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);
    sum += TEXTURE(sampler1, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
    sum += TEXTURE(sampler1, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);
    sum += TEXTURE(sampler1, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;
    return sum / 12.0;
}

float frameDistance(vec2 p)
{
    vec2 halfSize = 0.5 * frameSize;
    vec2 q = abs(p - halfSize) - (halfSize - vec2(radius));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main()
{
    vec4 sharp = TEXTURE(sampler, texcoord0);
    vec4 soft = blurred(texcoord0);

    // Offscreen textures are the GL way up: y runs bottom to top.
    vec2 p = expandedOrigin + vec2(texcoord0.x, 1.0 - texcoord0.y) * expandedSize;
    float inside = clamp(0.5 - frameDistance(p), 0.0, 1.0);

    // The blur smears the window's edge into its shadow and the wallpaper
    // showing through its corners. Taking only the colour from the blur and
    // the coverage from the window as drawn keeps the window's shape, so
    // where it ends stays as clear as before.
    vec3 colour = soft.a > 0.004 ? soft.rgb / soft.a : vec3(0.0);
    vec4 shaped = vec4(colour * sharp.a, sharp.a);

    vec4 result = mix(sharp, shaped, progress * inside);
    result.rgb *= 1.0 - dim * progress;

    result = sourceEncodingToNitsInDestinationColorspace(result);
    result = adjustSaturation(result);
    result *= modulation;
    FRAG_COLOR = nitsToDestinationEncoding(result);
}
