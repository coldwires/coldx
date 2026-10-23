#include "coldx.hlsli"

// The texture, the input and the clamped sampling every Effect uses too.
CX_SPRITE_EFFECT();

// Texture and tint are both premultiplied, so a plain multiply is the whole blend input.
float4 main(CxSpriteInput input) : SV_Target0 {
    return CX_SAMPLE(input) * input.color;
}
