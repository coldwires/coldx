#pragma once

// Maths for effects' CPU versions (coldx/core/cpu_effect.h), written in coldx so the result
// is the same on every compiler, standard library and CPU, and on systems with no maths
// library. std::sin and friends differ between libraries in the last bits, and a last bit
// can move a sample onto another texel. GPUs approximate these functions too, each its own
// way, so an effect that uses them matches a GPU closely but not exactly.
namespace coldx::CpuMath {

// Accurate to about 1e-7 for |x| up to a few thousand radians. Larger arguments lose
// precision in the range reduction, the same way everywhere.
float Sin(float x);
float Cos(float x);

} // namespace coldx::CpuMath
