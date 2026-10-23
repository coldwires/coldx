#include "coldx.hlsli"

// Washes sprites towards Color, keeping their shape: a hit flash is Color white, Amount
// from 1 down to 0 over a few frames.
CX_SPRITE_EFFECT();
CX_FRAGMENT_UNIFORMS(Params, 0) {
    float4 Color;
    float Amount;
};

float4 main(CxSpriteInput input) : SV_Target0 {
    const float4 c = CX_SAMPLE(input) * input.color;
    // Color laid on with the sprite's own coverage, so edges stay premultiplied.
    return float4(lerp(c.rgb, Color.rgb * c.a, Amount), c.a);
}
