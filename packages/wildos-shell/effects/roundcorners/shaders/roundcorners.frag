// The effect prepends the #version line and the IN, TEXTURE and FRAG_COLOR
// macros, so one source serves GLSL 1.10, 1.40 and GLSL ES.

#include "colormanagement.glsl"
#include "saturation.glsl"

uniform sampler2D sampler;
uniform vec4 modulation;

// Everything below is in logical pixels with the frame's top left at 0,0.
uniform vec2 expandedOrigin;
uniform vec2 expandedSize;
uniform vec2 frameSize;
uniform float radius;
uniform float edgeWidth;
uniform float devicePixel;
uniform float roundBottom;
uniform float squareTop;

uniform vec4 shadowColor;
uniform float shadowSigma;
uniform float shadowOffsetY;

IN vec2 texcoord0;

// KWin renders the window into its offscreen texture the GL way up, so the
// texture's y runs bottom to top while the window's runs top to bottom.
vec2 toTexture(vec2 p)
{
    vec2 t = (p - expandedOrigin) / expandedSize;
    return vec2(t.x, 1.0 - t.y);
}

vec2 fromTexture(vec2 t)
{
    return expandedOrigin + vec2(t.x, 1.0 - t.y) * expandedSize;
}

vec4 sampleAt(vec2 p)
{
    return TEXTURE(sampler, toTexture(p));
}

// Abramowitz and Stegun 7.1.26, good to 1.5e-7, which is far below one
// 8 bit step of the shadow.
float erfApprox(float x)
{
    float s = sign(x);
    float a = abs(x);
    float t = 1.0 / (1.0 + 0.3275911 * a);
    float y = 1.0 - (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t * exp(-a * a);
    return s * y;
}

// The decoration's shadow is a Gaussian of the frame moved down by
// shadowOffsetY. KWin only draws it outside the frame, so the corners we cut
// away would show the wallpaper undarkened. This continues the same Gaussian
// into them. Near a bottom corner only the near side of each axis matters.
vec4 shadowAt(vec2 p, float nearX)
{
    float k = shadowSigma * 1.41421356;
    float gx = 0.5 * (1.0 + erfApprox(nearX / k));
    float gy = 0.5 * (1.0 + erfApprox((frameSize.y + shadowOffsetY - p.y) / k));
    float a = shadowColor.a * gx * gy;
    return vec4(shadowColor.rgb * a, a);
}

// Coverage of the band [lo, hi) by the device pixel centred on x.
float band(float x, float lo, float hi)
{
    float h = 0.5 * devicePixel;
    return clamp((min(x + h, hi) - max(x - h, lo)) / devicePixel, 0.0, 1.0);
}

vec4 roundedBottomCorner(vec2 p, vec2 centre, vec2 edgeSample, float nearX, vec4 tex)
{
    float d = length(p - centre) - radius;
    float outer = clamp(0.5 - d / devicePixel, 0.0, 1.0);
    float inner = clamp(0.5 - (d + edgeWidth) / devicePixel, 0.0, 1.0);
    // The decoration draws the bottom edge as a straight line; reading it back
    // keeps the curved part the same tone in every state the theme draws.
    vec4 edge = sampleAt(edgeSample);
    return tex * inner + edge * (outer - inner) + shadowAt(p, nearX) * (1.0 - outer);
}

vec4 squaredTopCorner(vec2 p, float sampleX, float distX)
{
    vec4 edge = sampleAt(vec2(sampleX, 0.5 * edgeWidth));
    vec4 title = sampleAt(vec2(sampleX, edgeWidth + 1.5));
    float e = max(band(p.y, -1.0, edgeWidth), band(distX, -1.0, edgeWidth));
    return mix(title, edge, e);
}

void main()
{
    vec4 tex = TEXTURE(sampler, texcoord0);
    vec2 p = fromTexture(texcoord0);
    float r = radius;

    if (roundBottom > 0.5 && p.y > frameSize.y - r && p.y < frameSize.y) {
        if (p.x < r && p.x > 0.0) {
            tex = roundedBottomCorner(p, vec2(r, frameSize.y - r), vec2(r + 2.0, frameSize.y - 0.5 * edgeWidth), p.x, tex);
        } else if (p.x > frameSize.x - r && p.x < frameSize.x) {
            tex = roundedBottomCorner(p, vec2(frameSize.x - r, frameSize.y - r), vec2(frameSize.x - r - 2.0, frameSize.y - 0.5 * edgeWidth), frameSize.x - p.x, tex);
        }
    } else if (squareTop > 0.5 && p.y >= 0.0 && p.y < r) {
        if (p.x >= 0.0 && p.x < r) {
            tex = squaredTopCorner(p, r + 2.0, p.x);
        } else if (p.x > frameSize.x - r && p.x <= frameSize.x) {
            tex = squaredTopCorner(p, frameSize.x - r - 2.0, frameSize.x - p.x);
        }
    }

    tex = sourceEncodingToNitsInDestinationColorspace(tex);
    tex = adjustSaturation(tex);
    tex *= modulation;
    FRAG_COLOR = nitsToDestinationEncoding(tex);
}
