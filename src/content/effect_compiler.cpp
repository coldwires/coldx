#include "effect_compiler.h"

#include <coldx/core/effect_format.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <random>

#include <SDL3/SDL.h>

#include "coldx_hlsli.h"

namespace coldx::content::detail {

namespace {

namespace fs = std::filesystem;
using effect_format::ParameterType;

// Just enough JSON for spirv-cross --reflect: objects, arrays, strings, numbers, booleans.
struct Json {
    enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
    double number = 0;
    std::string text;
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> members;

    const Json* Get(const std::string& key) const {
        for (const auto& [k, v] : members)
            if (k == key) return &v;
        return nullptr;
    }
    std::string Text(const std::string& key) const {
        const Json* v = Get(key);
        return v && v->kind == String ? v->text : std::string();
    }
    long long Int(const std::string& key, long long otherwise = -1) const {
        const Json* v = Get(key);
        return v && v->kind == Number ? static_cast<long long>(v->number) : otherwise;
    }
    bool Flag(const std::string& key) const {
        const Json* v = Get(key);
        return v && v->kind == Bool && v->number != 0;
    }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& s) : s_(s) {}

    bool Parse(Json& out) {
        if (!Value(out, 0)) return false;
        Skip();
        return at_ == s_.size();
    }

private:
    void Skip() {
        while (at_ < s_.size() && (s_[at_] == ' ' || s_[at_] == '\n' || s_[at_] == '\r' || s_[at_] == '\t')) ++at_;
    }
    bool Literal(const char* word) {
        const std::string w(word);
        if (s_.compare(at_, w.size(), w) != 0) return false;
        at_ += w.size();
        return true;
    }
    bool Str(std::string& out) {
        if (at_ >= s_.size() || s_[at_] != '"') return false;
        ++at_;
        while (at_ < s_.size() && s_[at_] != '"') {
            char c = s_[at_++];
            if (c == '\\' && at_ < s_.size()) {
                c = s_[at_++];
                if (c == 'n') c = '\n';
                else if (c == 't') c = '\t';
                else if (c == 'u') {  // names are ASCII; keep the escape as text
                    out += "\\u";
                    continue;
                }
            }
            out += c;
        }
        if (at_ >= s_.size()) return false;
        ++at_;
        return true;
    }
    bool Value(Json& out, int depth) {
        if (depth > 32) return false;
        Skip();
        if (at_ >= s_.size()) return false;
        const char c = s_[at_];
        if (c == '{' || c == '[') {
            const bool object = c == '{';
            out.kind = object ? Json::Object : Json::Array;
            ++at_;
            Skip();
            if (at_ < s_.size() && s_[at_] == (object ? '}' : ']')) {
                ++at_;
                return true;
            }
            for (;;) {
                Json item;
                std::string key;
                if (object) {
                    Skip();
                    if (!Str(key)) return false;
                    Skip();
                    if (at_ >= s_.size() || s_[at_++] != ':') return false;
                }
                if (!Value(item, depth + 1)) return false;
                if (object) out.members.emplace_back(std::move(key), std::move(item));
                else out.items.push_back(std::move(item));
                Skip();
                if (at_ >= s_.size()) return false;
                if (s_[at_] == ',') {
                    ++at_;
                    continue;
                }
                if (s_[at_++] != (object ? '}' : ']')) return false;
                return true;
            }
        }
        if (c == '"') {
            out.kind = Json::String;
            return Str(out.text);
        }
        if (Literal("true") || Literal("false")) {
            out.kind = Json::Bool;
            out.number = s_[at_ - 4] == 't' ? 1 : 0;  // "true" ends where "fals" + "e" does
            return true;
        }
        if (Literal("null")) return true;
        const size_t start = at_;
        while (at_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[at_])) || s_[at_] == '-' || s_[at_] == '+' ||
                                   s_[at_] == '.' || s_[at_] == 'e' || s_[at_] == 'E'))
            ++at_;
        if (start == at_) return false;
        out.kind = Json::Number;
        out.number = std::strtod(s_.substr(start, at_ - start).c_str(), nullptr);
        return true;
    }

    const std::string& s_;
    size_t at_ = 0;
};

// Runs a tool, waits for it, and returns what it printed, standard error included.
bool RunProgram(const std::vector<std::string>& args, std::string& output, bool& started) {
    std::vector<const char*> argv;
    for (const std::string& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process* process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    started = process != nullptr;
    if (!process) {
        output = SDL_GetError();
        return false;
    }
    size_t size = 0;
    int code = -1;
    if (void* data = SDL_ReadProcess(process, &size, &code)) {
        output.assign(static_cast<const char*>(data), size);
        SDL_free(data);
    }
    SDL_DestroyProcess(process);
    return code == 0;
}

// The environment variable, then the Vulkan SDK's Bin folder, then the bare name for PATH.
std::string FindTool(const char* variable, const char* exe) {
    if (const char* set = SDL_getenv(variable); set && *set) return set;
    if (const char* sdk = SDL_getenv("VULKAN_SDK"); sdk && *sdk) {
        for (const char* bin : {"Bin", "bin"}) {
            fs::path candidate = fs::path(sdk) / bin / exe;
#ifdef _WIN32
            candidate += ".exe";
#endif
            std::error_code ec;
            if (fs::is_regular_file(candidate, ec)) return candidate.string();
        }
    }
    return exe;
}

bool ReadAll(const fs::path& path, std::vector<std::uint8_t>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    out.assign(std::istreambuf_iterator<char>(file), {});
    return true;
}

// A folder of its own in the temp folder, removed when this goes.
struct WorkFolder {
    fs::path path;
    WorkFolder() {
        std::random_device random;
        char name[48];
        std::snprintf(name, sizeof(name), "coldxc-effect-%08x%08x", random(), random());
        path = fs::temp_directory_path() / name;
        fs::create_directories(path);
    }
    ~WorkFolder() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

bool TypeOf(const std::string& reflected, ParameterType& type) {
    static const std::map<std::string, ParameterType> types = {
        {"float", ParameterType::Float},   {"vec2", ParameterType::Float2},    {"vec3", ParameterType::Float3},
        {"vec4", ParameterType::Float4},   {"mat4", ParameterType::Float4x4}, {"int", ParameterType::Int},
        {"uint", ParameterType::UInt},
    };
    const auto found = types.find(reflected);
    if (found == types.end()) return false;
    type = found->second;
    return true;
}

// Reads the parameter table and texture slots out of spirv-cross's reflection, refusing
// anything SpriteBatch cannot feed.
bool Reflect(const Json& r, effect_format::Compiled& out, std::string& error) {
    for (const char* unsupported : {"separate_images", "separate_samplers", "images", "ssbos", "push_constants"}) {
        if (const Json* v = r.Get(unsupported); v && !v->items.empty()) {
            error = std::string("an effect takes textures declared with CX_FRAGMENT_TEXTURE and one CX_FRAGMENT_UNIFORMS block; "
                                "this one also declares ") + unsupported;
            return false;
        }
    }

    // What SpriteBatch's vertex shader hands on, by location: uv, color, bounds.
    static const char* inputs[] = {"vec2", "vec4", "vec4"};
    if (const Json* list = r.Get("inputs")) {
        for (const Json& input : list->items) {
            const long long location = input.Int("location");
            if (location < 0 || location > 2 || input.Text("type") != inputs[location]) {
                error = "the input must be CxSpriteInput, from CX_SPRITE_EFFECT() in coldx.hlsli";
                return false;
            }
        }
    }

    const Json* textures = r.Get("textures");
    std::vector<effect_format::Texture> slots;
    if (textures) {
        for (const Json& t : textures->items) {
            if (t.Int("set") != 2) {
                error = "texture " + t.Text("name") + " is not a fragment texture (CX_FRAGMENT_TEXTURE)";
                return false;
            }
            slots.push_back({t.Text("name"), static_cast<std::uint32_t>(t.Int("binding"))});
        }
    }
    std::sort(slots.begin(), slots.end(), [](const auto& a, const auto& b) { return a.slot < b.slot; });
    if (slots.empty() || slots[0].slot != 0) {
        error = "an effect must sample the sprite's texture: declare CX_SPRITE_EFFECT() and use CX_SAMPLE";
        return false;
    }
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].slot != i) {
            error = "texture slots must run 0, 1, 2... without a gap, and each must be sampled; slot " + std::to_string(i) +
                    " is missing";
            return false;
        }
    }
    if (slots.size() > effect_format::MaxTextures) {
        error = "an effect takes at most " + std::to_string(effect_format::MaxTextures) + " textures";
        return false;
    }
    out.textures = slots;

    const Json* ubos = r.Get("ubos");
    if (!ubos || ubos->items.empty()) return true;
    if (ubos->items.size() > 1) {
        error = "an effect takes one CX_FRAGMENT_UNIFORMS block, at slot 0";
        return false;
    }
    const Json& block = ubos->items[0];
    if (block.Int("set") != 3 || block.Int("binding") != 0) {
        error = "the parameter block must be CX_FRAGMENT_UNIFORMS(name, 0)";
        return false;
    }
    out.uniformSize = static_cast<std::uint32_t>(block.Int("block_size", 0));
    const Json* types = r.Get("types");
    const Json* type = types ? types->Get(block.Text("type")) : nullptr;
    const Json* members = type ? type->Get("members") : nullptr;
    if (!members) {
        error = "spirv-cross's reflection has no members for the parameter block";
        return false;
    }
    for (const Json& m : members->items) {
        effect_format::Parameter p;
        p.name = m.Text("name");
        p.offset = static_cast<std::uint32_t>(m.Int("offset", 0));
        const std::string reflected = m.Text("type");
        if (m.Get("array")) {
            error = "parameter " + p.name + ": arrays are not supported yet";
            return false;
        }
        if (!TypeOf(reflected, p.type)) {
            error = "parameter " + p.name + ": " + (reflected.rfind('_', 0) == 0 ? std::string("a struct") : reflected) +
                    " is not supported; use float, float2, float3, float4, float4x4, int, uint or bool";
            return false;
        }
        // spirv-cross reports HLSL's default (column_major) as row_major: that is the one
        // Effect::Set(Matrix) writes for.
        if (p.type == ParameterType::Float4x4 && !m.Flag("row_major")) {
            error = "parameter " + p.name + ": declare float4x4 without row_major";
            return false;
        }
        out.parameters.push_back(std::move(p));
    }
    return true;
}

} // namespace

bool CompileEffect(const std::string& path, std::vector<std::uint8_t>& out, std::string& error) {
    const std::string dxc = FindTool("COLDX_DXC", "dxc");
    const std::string spirvCross = FindTool("COLDX_SPIRV_CROSS", "spirv-cross");
    WorkFolder work;
    {
        std::ofstream include(work.path / "coldx.hlsli", std::ios::binary);
        include << ColdxHlsli;
    }
    const std::string inc = work.path.string();
    const std::string spirv = (work.path / "effect.spv").string();
    const std::string dxil = (work.path / "effect.dxil").string();
    const std::string msl = (work.path / "effect.msl").string();
    const std::string json = (work.path / "effect.json").string();

    // HLSL's own packing on Vulkan too (-fvk-use-dx-layout), so one parameter table holds
    // for every backend.
    const std::vector<std::vector<std::string>> steps = {
        {dxc, "-T", "ps_6_0", "-E", "main", "-spirv", "-fspv-target-env=vulkan1.0", "-fvk-use-dx-layout", "-I", inc, path, "-Fo", spirv},
        {dxc, "-T", "ps_6_0", "-E", "main", "-I", inc, path, "-Fo", dxil},
        {spirvCross, spirv, "--msl", "--output", msl},
        {spirvCross, spirv, "--reflect", "--output", json},
    };
    for (const auto& step : steps) {
        std::string output;
        bool started = false;
        if (!RunProgram(step, output, started)) {
            if (!started)
                error = "cannot run " + step[0] + " (" + output + "); install the Vulkan SDK, or set " +
                        (step[0] == dxc ? "COLDX_DXC" : "COLDX_SPIRV_CROSS");
            else
                error = fs::path(step[0]).stem().string() + ": " + output;
            while (!error.empty() && (error.back() == '\n' || error.back() == '\r')) error.pop_back();
            return false;
        }
    }

    effect_format::Compiled compiled;
    std::vector<std::uint8_t> reflection;
    if (!ReadAll(spirv, compiled.spirv) || !ReadAll(dxil, compiled.dxil) || !ReadAll(msl, compiled.msl) || !ReadAll(json, reflection)) {
        error = "a compiler step wrote no output";
        return false;
    }
    Json r;
    const std::string text(reflection.begin(), reflection.end());
    if (!JsonParser(text).Parse(r)) {
        error = "cannot read spirv-cross's reflection";
        return false;
    }
    if (!Reflect(r, compiled, error)) return false;
    out = effect_format::Encode(compiled);
    return true;
}

std::string EffectEntryName(const std::string& sourceName) {
    std::string name = sourceName;
    for (const char* suffix : {".hlsl", ".frag"}) {
        const std::string s(suffix);
        if (name.size() > s.size() && name.compare(name.size() - s.size(), s.size(), s) == 0) name.resize(name.size() - s.size());
    }
    return name + ".fx";
}

std::string EffectHeader(const std::vector<std::uint8_t>& compiled, const std::string& sourcePath) {
    const std::string file = fs::path(sourcePath).filename().string();
    std::string identifier = file.substr(0, file.find('.'));
    for (char& c : identifier)
        if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
    if (identifier.empty() || std::isdigit(static_cast<unsigned char>(identifier[0]))) identifier = "_" + identifier;

    std::string h = "// Generated from " + file + " by coldxc --compile-effect. Do not edit.\n"
                    "#pragma once\n\nnamespace effects {\n\ninline constexpr unsigned char " + identifier + "[] = {";
    char byte[8];
    for (size_t i = 0; i < compiled.size(); ++i) {
        if (i % 16 == 0) h += "\n    ";
        std::snprintf(byte, sizeof(byte), "0x%02x,", compiled[i]);
        h += byte;
    }
    h += "\n};\n\n} // namespace effects\n";
    return h;
}

std::uint32_t EffectIncludeVersion() {
    std::uint32_t hash = 2166136261u;
    for (const char* c = ColdxHlsli; *c; ++c) hash = (hash ^ static_cast<unsigned char>(*c)) * 16777619u;
    return hash & 0x3fffffff;
}

} // namespace coldx::content::detail
