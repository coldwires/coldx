#include "coldx.hlsli"

// Every parameter type and a second texture, for effect_test.cpp. Mode picks what comes
// out, so each draw checks one thing.
CX_SPRITE_EFFECT();
CX_FRAGMENT_TEXTURE(Mask, MaskSampler, 1);
CX_FRAGMENT_UNIFORMS(Params, 0) {
    float A;
    float2 B;
    float3 C;
    float D;
    float4 E;
    float4x4 M;
    int Mode;
    bool Invert;
};

float4 main(CxSpriteInput input) : SV_Target0 {
    const float4 texel = CX_SAMPLE(input);
    float4 c;
    if (Mode == 0) c = float4(A, B.y, C.z, D);
    else if (Mode == 1) c = E;
    else if (Mode == 2) c = mul(M, float4(0, 0, 0, 1));
    else if (Mode == 3) c = Mask.Sample(MaskSampler, input.uv);
    else c = float4(B.x, C.x, C.y, 1);
    if (Invert) c.rgb = 1 - c.rgb;
    return c * texel.a;
}
