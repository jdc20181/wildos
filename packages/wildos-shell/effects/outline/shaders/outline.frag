// The effect prepends the #version line and the IN and FRAG_COLOR macros, so
// one source serves GLSL 1.10, 1.40 and GLSL ES.

#include "colormanagement.glsl"

// In logical pixels with the frame's top left at 0,0.
uniform vec2 frameSize;
uniform float radius;
// 1 when the ring sits inside the frame (maximised and snapped windows).
uniform float inset;
uniform float mainWidth;
uniform float edgeWidth;
// Straight alpha, already multiplied by the window's opacity.
uniform vec4 mainColour;
uniform vec4 edgeColour;
uniform float devicePixel;

IN vec2 texcoord0;

// Signed distance to the frame's rounded rectangle, negative inside.
float frameDistance(vec2 p)
{
    vec2 halfSize = 0.5 * frameSize;
    vec2 q = abs(p - halfSize) - (halfSize - vec2(radius));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

// Coverage of the band [lo, hi) by the device pixel centred on s.
float band(float s, float lo, float hi)
{
    float h = 0.5 * devicePixel;
    return clamp((min(s + h, hi) - max(s - h, lo)) / devicePixel, 0.0, 1.0);
}

void main()
{
    float d = frameDistance(texcoord0);
    // Distance away from the window's content: outwards for a ring round
    // the frame, inwards for one inside it, so the contrasting edge is
    // always on the far side of the coloured line.
    float s = inset > 0.5 ? -d : d;
    float m = band(s, 0.0, mainWidth);
    float e = band(s, mainWidth, mainWidth + edgeWidth);
    vec4 colour = vec4(mainColour.rgb * mainColour.a, mainColour.a) * m
        + vec4(edgeColour.rgb * edgeColour.a, edgeColour.a) * e;
    colour = sourceEncodingToNitsInDestinationColorspace(colour);
    FRAG_COLOR = nitsToDestinationEncoding(colour);
}
