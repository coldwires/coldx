#include "coldx.hlsli"

CX_VERTEX_UNIFORMS(Transform, 0) {
    // Row-vector convention, stored row-major, as on the C++ side. HLSL reads it as the
    // transpose, so mul(transform, v) here is v * transform there.
    float4x4 transform;
};

struct Input {
    float2 position : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
    float4 bounds : TEXCOORD3;  // the source rectangle's UV limits: min xy, max zw
};

struct Output {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    nointerpolation float4 bounds : TEXCOORD2;
};

Output main(Input input) {
    Output output;
    output.position = mul(transform, float4(input.position, 0.0, 1.0));
    output.uv = input.uv;
    output.color = input.color;
    output.bounds = input.bounds;
    return output;
}
