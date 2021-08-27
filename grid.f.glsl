#version 430

layout(origin_upper_left) in vec4 gl_FragCoord;

layout(location = 0) out vec4 color;

layout(location = 1)  uniform sampler2D Glyphs;
layout(location = 2)  uniform usampler2D TerminalCell; // GlyphIndexX, GlyphIndexY, Foreground, Background

layout(location = 3)  uniform uvec2 CellSize;
layout(location = 4)  uniform uvec2 TermSize;
layout(location = 5)  uniform uvec2 TopLeftMargin;
layout(location = 6)  uniform uint BlinkModulate;
layout(location = 7)  uniform uint MarginColor;

layout(location = 8)  uniform uvec2 StrikeThrough; // Min, Max
layout(location = 9) uniform uvec2 Underline; // Min, Max

vec3 UnpackColor(uint Packed) {
    uint B = Packed & 0xffu;
    uint G = (Packed >> 8) & 0xffu;
    uint R = (Packed >> 16) & 0xffu;

    return vec3(R, G, B) / 255.0;
}

vec4 ComputeOutputColor(in uvec2 ScreenPos) {

    uvec2 CellIndex = uvec2(floor((ScreenPos - TopLeftMargin) / CellSize));
    uvec2 CellPos = (ScreenPos - TopLeftMargin) % CellSize;

    vec3 Result;

    if((ScreenPos.x >= TopLeftMargin.x) &&
            (ScreenPos.y >= TopLeftMargin.y) &&
            (CellIndex.x < TermSize.x) &&
            (CellIndex.y < TermSize.y))
    {
        uvec4 CellProperties = texelFetch(TerminalCell, ivec2(CellIndex), 0); // .y * TermSize.x + CellIndex.x);
        vec4 GlyphTexel = texelFetch(Glyphs, ivec2(CellProperties.xy*CellSize + CellPos), 0);

        vec3 Foreground = UnpackColor(CellProperties.z);
        vec3 Background = UnpackColor(CellProperties.w);
        vec3 Blink = UnpackColor(BlinkModulate);

        if(bool((CellProperties.z >> 28) & 1u)) Foreground *= Blink;
        if(bool((CellProperties.z >> 25) & 1u)) Foreground *= 0.5;

        /* // TODO: proper ClearType blending */
        Result = (1-GlyphTexel.a)*Background + GlyphTexel.rgb*Foreground;
        /* Result = GlyphTexel.rgb; */

        if (bool(CellProperties.z >> 27 & 1u) &&
                (CellPos.y >= Underline.x) &&
                (CellPos.y < Underline.y)) Result.rgb = Foreground.rgb;
        if (bool(CellProperties.z >> 31) &&
                (CellPos.y >= StrikeThrough.x) &&
                (CellPos.y < StrikeThrough.y)) Result.rgb = Foreground.rgb;
    } else {
        Result = UnpackColor(MarginColor);
    }

    // NOTE(casey): Uncomment this to view the cache texture
    /* Result = texelFetch(Glyphs, ivec2(ScreenPos), 0).rgb; */

    return vec4(Result, 1);
}

void main() {
    // uhhh
    uvec2 ScreenPos = uvec2(floor(gl_FragCoord.xy));
    color = ComputeOutputColor(ScreenPos);
}
