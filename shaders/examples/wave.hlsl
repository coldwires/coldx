#include "coldx.hlsli"

// Ripples sprites sideways: each row of pixels moves by Amplitude texels times a sine of
// its height on screen. Frequency is radians per pixel (0.3 is a gentle wave); advance Time
// in seconds to animate it. The sprite never shows its atlas neighbours: CX_SAMPLE_AT holds
// the moved UV inside its source rectangle.
CX_SPRITE_EFFECT();
CX_FRAGMENT_UNIFORMS(Params, 0) {
    float Time;
    float Amplitude;
    float Frequency;
};

float4 main(CxSpriteInput input) : SV_Target0 {
    float width, height;
    cxSpriteTexture.GetDimensions(width, height);
    const float shift = Amplitude * sin(input.position.y * Frequency + Time * 6.2831853);
    return CX_SAMPLE_AT(input, input.uv + float2(shift / width, 0)) * input.color;
}
