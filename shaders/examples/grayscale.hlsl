#include "coldx.hlsli"

// Drains the colour out of sprites: Amount 0 leaves them as they are, 1 makes them grey.
CX_SPRITE_EFFECT();
CX_FRAGMENT_UNIFORMS(Params, 0) {
    float Amount;
};

float4 main(CxSpriteInput input) : SV_Target0 {
    const float4 c = CX_SAMPLE(input) * input.color;
    // Rec. 601 luma. Grey is a weighted sum of premultiplied channels, so it stays premultiplied.
    const float grey = dot(c.rgb, float3(0.299, 0.587, 0.114));
    return float4(lerp(c.rgb, grey.xxx, Amount), c.a);
}
