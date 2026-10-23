#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// The compiled form of an Effect: what coldxc writes, as a ".fx" entry in a .rime or as a
// byte array in a C++ header, and what Effect::FromMemory reads. Little-endian:
//
//   0   "CXFX"
//   4   u16 version (1)
//   6   u16 texture count, the sprite texture at slot 0 included
//   8   u32 uniform block size in bytes, 0 for none
//   12  u32 parameter count
//   16  u32 SPIR-V size, u32 DXIL size, u32 MSL size
//   28  u32 zero
//   32  parameters: u8 type, u8 name length, u16 zero, u32 offset, the name
//       textures: u8 slot, u8 name length, the name
//       SPIR-V, DXIL, MSL
namespace coldx::effect_format {

inline constexpr char Magic[4] = {'C', 'X', 'F', 'X'};
inline constexpr std::uint16_t Version = 1;
// Texture slots an effect may use, the sprite texture's included.
inline constexpr std::size_t MaxTextures = 8;

// What a uniform block member is, as HLSL declares it. A bool is a UInt.
enum class ParameterType : std::uint8_t {
    Float = 1,
    Float2,
    Float3,
    Float4,
    Float4x4,
    Int,
    UInt,
};

// Bytes a value of type takes in the uniform block.
std::size_t SizeOf(ParameterType type);

struct Parameter {
    std::string name;
    ParameterType type = ParameterType::Float;
    std::uint32_t offset = 0;
};

struct Texture {
    std::string name;
    std::uint32_t slot = 0;
};

struct Compiled {
    std::uint32_t uniformSize = 0;
    std::vector<Parameter> parameters;
    std::vector<Texture> textures;  // slots 0 to size - 1, in slot order
    std::vector<std::uint8_t> spirv;
    std::vector<std::uint8_t> dxil;
    std::vector<std::uint8_t> msl;
};

std::vector<std::uint8_t> Encode(const Compiled& compiled);
// False, with error set, for anything that is not a well-formed compiled effect: a wrong
// magic or version, sizes past the end, parameters outside the block, slots out of order.
bool Decode(const void* data, std::size_t size, Compiled& out, std::string& error);

} // namespace coldx::effect_format
