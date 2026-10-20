#include "coldx.hlsli"

CX_FRAGMENT_TEXTURE(spriteTexture, spriteSampler, 0);

// Same members, same order as the vertex shader's Output.
struct Input {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
};

// Texture and tint are both premultiplied, so a plain multiply is the whole blend input.
float4 main(Input input) : SV_Target0 {
    return spriteTexture.Sample(spriteSampler, input.uv) * input.color;
}
