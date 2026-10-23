#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace coldx::content::detail {

// Compiles the HLSL effect at path into coldx/core/effect_format.h's form: SPIR-V and DXIL
// from dxc, MSL and the parameter table from spirv-cross. The tools are found through
// COLDX_DXC and COLDX_SPIRV_CROSS (coldx_add_content sets them), then the Vulkan SDK's
// VULKAN_SDK, then PATH. coldx.hlsli is built into coldxc, so it always matches. False with
// error set (the compiler's own message for a shader that does not compile).
bool CompileEffect(const std::string& path, std::vector<std::uint8_t>& out, std::string& error);

// The package name for an effect source: "fx/glow.hlsl" and "fx/glow.frag.hlsl" are
// "fx/glow.fx".
std::string EffectEntryName(const std::string& sourceName);

// A C++ header holding compiled as `inline constexpr unsigned char <identifier>[]` in
// namespace effects. identifier is made from the source's file name: "glow.hlsl" -> glow.
std::string EffectHeader(const std::vector<std::uint8_t>& compiled, const std::string& sourcePath);

// Changes whenever the coldx.hlsli built into coldxc does, so cached effects are rebuilt.
std::uint32_t EffectIncludeVersion();

} // namespace coldx::content::detail
