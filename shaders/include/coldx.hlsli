// Declarations that put resources where SDL_GPU expects them on every backend.
//
// SDL_GPU fixes which register space each kind of resource lives in, per shader stage, and
// Vulkan additionally wants a texture and its sampler combined into one binding. These
// macros hide both, so a shader declares a texture or a uniform block by slot number only.
//
//     CX_FRAGMENT_TEXTURE(myTexture, mySampler, 0);
//     CX_FRAGMENT_UNIFORMS(MyParams, 0) { float4 tint; float time; };
//
// Slots of each kind start at 0 and must not skip numbers.
//
// Vertex inputs use TEXCOORD0, TEXCOORD1, ... in the order of the vertex layout, because
// SDL's Direct3D 12 backend matches them by that semantic name.
//
// A fragment shader's input must list the same members as its vertex shader's output, in
// the same order, SV_Position included even when unused. Direct3D 12 matches the two by
// position and rejects the pipeline otherwise; Vulkan does not mind, so test on both.

#ifndef COLDX_HLSLI
#define COLDX_HLSLI

#ifdef __spirv__
#define CX_VK_COMBINED(slot, set) [[vk::combinedImageSampler]] [[vk::binding(slot, set)]]
#else
#define CX_VK_COMBINED(slot, set)
#endif

#define CX_VERTEX_TEXTURE(texture, sampler, slot) \
    CX_VK_COMBINED(slot, 0) Texture2D<float4> texture : register(t##slot, space0); \
    CX_VK_COMBINED(slot, 0) SamplerState sampler : register(s##slot, space0)

#define CX_FRAGMENT_TEXTURE(texture, sampler, slot) \
    CX_VK_COMBINED(slot, 2) Texture2D<float4> texture : register(t##slot, space2); \
    CX_VK_COMBINED(slot, 2) SamplerState sampler : register(s##slot, space2)

#define CX_VERTEX_UNIFORMS(name, slot) cbuffer name : register(b##slot, space1)
#define CX_FRAGMENT_UNIFORMS(name, slot) cbuffer name : register(b##slot, space3)

// A SpriteBatch fragment shader: SpriteBatch's own, and every Effect. Declares the sprite's
// texture at slot 0 and CxSpriteInput, what SpriteBatch's vertex shader hands on:
//
//     CX_SPRITE_EFFECT();
//     CX_FRAGMENT_UNIFORMS(Params, 0) { float Amount; };
//     float4 main(CxSpriteInput input) : SV_Target0 { return CX_SAMPLE(input) * input.color; }
//
// input.color is the Draw call's tint, premultiplied; input.position.xy the pixel being
// drawn, in the current target. Extra textures start at slot 1.
//
// CX_SAMPLE reads the sprite's texture at its own UV; CX_SAMPLE_AT at any UV. Both hold the
// UV inside the sprite's source rectangle, so a sprite from an atlas never shows its
// neighbours, through filtering or through an effect moving the UV (the wrap samplers get
// limits that never bind).
#define CX_SPRITE_EFFECT() \
    CX_FRAGMENT_TEXTURE(cxSpriteTexture, cxSpriteSampler, 0); \
    struct CxSpriteInput { \
        float4 position : SV_Position; \
        float2 uv : TEXCOORD0; \
        float4 color : TEXCOORD1; \
        nointerpolation float4 bounds : TEXCOORD2; \
    }
#define CX_SAMPLE_AT(input, at) cxSpriteTexture.Sample(cxSpriteSampler, clamp((at), (input).bounds.xy, (input).bounds.zw))
#define CX_SAMPLE(input) CX_SAMPLE_AT(input, (input).uv)

#endif
