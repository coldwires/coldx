#include "coldx.hlsli"

CX_FRAGMENT_TEXTURE(spriteTexture, spriteSampler, 0);

// Same members, same order as the vertex shader's Output.
struct Input {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    nointerpolation float4 bounds : TEXCOORD2;
};

// Texture and tint are both premultiplied, so a plain multiply is the whole blend input.
// The UV is first held inside the sprite's source rectangle, so a sprite from an atlas
// never shows its neighbours' texels: not through filtering, and not through rounding when
// it is shrunk far (SpriteBatch sets the limits; wrap modes get limits that never bind).
float4 main(Input input) : SV_Target0 {
    const float2 uv = clamp(input.uv, input.bounds.xy, input.bounds.zw);
    return spriteTexture.Sample(spriteSampler, uv) * input.color;
}
