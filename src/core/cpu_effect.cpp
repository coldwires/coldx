// The registry of effects' CPU versions, and matching one to a compiled effect.

#include <coldx/core/cpu_effect.h>
#include <coldx/core/effect_format.h>

#include <map>

#include <SDL3/SDL.h>

#include "cpu_backend.h"

namespace coldx {

namespace detail {

namespace {

// A function-local static, so registering from another file's static initializer works.
std::map<std::string, CpuPort>& Registry() {
    static std::map<std::string, CpuPort> ports;
    return ports;
}

} // namespace

std::string CpuEffectKey(const std::string& name) {
    const size_t slash = name.find_last_of("/\\");
    std::string base = slash == std::string::npos ? name : name.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot > 0) base.resize(dot);
    return base;
}

const CpuPort* CpuEffect::Port() {
    if (resolved_) return port_;
    resolved_ = true;
    const auto found = Registry().find(CpuEffectKey(name_));
    if (found == Registry().end()) {
        SDL_Log("coldx.core: effect %s has no CPU version, drawn with the built-in shader", name_.c_str());
        return nullptr;
    }
    const CpuPort& port = found->second;
    const auto refuse = [this](const char* why, const std::string& what) {
        SDL_Log("coldx.core: effect %s: its CPU version %s%s, drawn with the built-in shader", name_.c_str(), why, what.c_str());
        return nullptr;
    };
    if (port.parameters.size() != compiled_.parameters.size())
        return refuse("lists a different number of parameters than the shader has", "");
    for (const effect_format::Parameter& p : compiled_.parameters) {
        const auto listed = port.parameters.find(p.name);
        if (listed == port.parameters.end()) return refuse("does not list the parameter ", p.name);
        if (listed->second != p.offset) return refuse("has the parameter at another offset: ", p.name);
        if (p.offset + effect_format::SizeOf(p.type) > port.size) return refuse("has a struct too small for ", p.name);
    }
    if (port.size > (compiled_.uniformSize + 15) / 16 * 16) return refuse("has a struct larger than the shader's block", "");
    port_ = &port;
    return port_;
}

} // namespace detail

namespace CpuEffects {

void Register(const std::string& name, std::size_t parametersSize, std::initializer_list<CpuParameter> parameters,
              Function function) {
    detail::CpuPort port;
    port.size = parametersSize;
    for (const CpuParameter& p : parameters) port.parameters[p.name] = static_cast<std::uint32_t>(p.offset);
    port.function = std::move(function);
    detail::Registry()[detail::CpuEffectKey(name)] = std::move(port);
}

void Register(const std::string& name, Float4 (*function)(const CpuPixel&)) {
    Register(name, 0, {}, [function](const CpuPixel& pixel, const void*) { return function(pixel); });
}

bool Has(const std::string& name) {
    return detail::Registry().count(detail::CpuEffectKey(name)) != 0;
}

} // namespace CpuEffects
} // namespace coldx
