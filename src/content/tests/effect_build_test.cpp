// The [effect] processor and coldxc --compile-effect: both give the bytes committed in the
// example headers, the cache skips unchanged effects, a game loads what was packed, and
// shaders SpriteBatch cannot feed fail the build with a reason. Needs dxc and spirv-cross
// (COLDX_DXC and COLDX_SPIRV_CROSS). Without them the test is skipped.

#include <coldx/coldx.h>
#include <coldx/content/build.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>

#include <SDL3/SDL.h>

#include "check.h"
#include "compiled/grayscale.h"

using namespace coldx;
using namespace coldx::content;
using coldx_test::CheckResult;
namespace fs = std::filesystem;

namespace {

void Write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

std::vector<std::uint8_t> ReadBytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}

const std::vector<std::uint8_t> Committed(std::begin(effects::grayscale), std::end(effects::grayscale));

class Driver : public Game {
public:
    std::function<void()> load;
    Driver() { Settings().console = ConsoleMode::Never; }

protected:
    void LoadContent() override { load(); }
    void Update(const GameTime&) override { Exit(); }
};

struct Built {
    std::vector<std::unique_ptr<Processor>> owned = BuiltInProcessors();
    std::vector<const Processor*> list;
    Built() {
        for (const auto& p : owned) list.push_back(p.get());
    }
};

void Packed(const fs::path& dir) {
    const fs::path project = dir / "content" / "content.coldx";
    Write(project, "seed = effect test\n[effect] fx/*.hlsl\n");
    fs::create_directories(dir / "content" / "fx");
    fs::copy_file(COLDX_TEST_EFFECTS "/grayscale.hlsl", dir / "content" / "fx" / "grayscale.hlsl");

    Built tools;
    BuildOptions options;
    options.project = project.string();
    options.output = (dir / "out" / "content.rime").string();
    options.cacheDirectory = (dir / "cache").string();
    BuildResult r = Build(options, tools.list);
    for (const std::string& e : r.errors) std::printf("error: %s\n", e.c_str());
    CHECK(r.ok && r.entries == 1 && r.processed == 1);
    r = Build(options, tools.list);
    CHECK(r.ok && !r.written && r.cached == 1 && r.processed == 0);

    Driver game;
    game.load = [&] {
        ContentManager& c = game.Content();
        c.SetLooseFiles(LooseFiles::Never);
        CHECK(c.Mount(options.output));
        // The packed effect is the one coldx_compile_effects committed, byte for byte.
        const auto bytes = c.ReadBytes("fx/grayscale.fx");
        CHECK(bytes && *bytes == Committed);
        const Effect gray = c.Load<Effect>("fx/grayscale");
        CHECK(!gray.IsEmpty() && gray.Has("Amount"));
    };
    CHECK(game.Run() == 0);

    // coldxc --compile-effect, to a .fx file: the same bytes again.
    const std::string source = (dir / "content" / "fx" / "grayscale.hlsl").string();
    const std::string fx = (dir / "out" / "grayscale.fx").string();
    std::vector<std::string> args = {"coldxc", "--compile-effect", source, "--output", fx};
    std::vector<char*> argv;
    for (std::string& a : args) argv.push_back(a.data());
    CHECK(RunTool(static_cast<int>(argv.size()), argv.data()) == 0);
    CHECK(ReadBytes(fx) == Committed);
}

// Each shader is built alone. The build fails naming the file, with why.
void Refused(const fs::path& dir) {
    Built tools;
    const fs::path project = dir / "bad" / "content.coldx";
    auto build = [&](const std::string& shader) {
        Write(dir / "bad" / "fx.hlsl", shader);
        Write(project, "[effect] fx.hlsl\n");
        BuildOptions o;
        o.project = project.string();
        o.output = (dir / "bad" / "out.rime").string();
        BuildResult r = Build(o, tools.list);
        std::string all;
        for (const std::string& e : r.errors) all += e + "\n";
        CHECK(!r.ok);
        return all;
    };
    const std::string head = "#include \"coldx.hlsli\"\nCX_SPRITE_EFFECT();\n";
    const std::string body = "float4 main(CxSpriteInput input) : SV_Target0 { return CX_SAMPLE(input); }\n";

    // The compiler's own message, with the line.
    std::string e = build(head + "float4 main(CxSpriteInput input) : SV_Target0 { return nope; }\n");
    std::printf("%s", e.c_str());
    CHECK(e.find("fx.hlsl: [effect] dxc:") != std::string::npos && e.find(":3:") != std::string::npos &&
          e.find("nope") != std::string::npos);
    e = build(head + "CX_FRAGMENT_UNIFORMS(P, 0) { float4 Many[4]; };\n" +
              "float4 main(CxSpriteInput input) : SV_Target0 { return CX_SAMPLE(input) * Many[1]; }\n");
    CHECK(e.find("Many: arrays are not supported") != std::string::npos);
    e = build(head + "CX_FRAGMENT_UNIFORMS(P, 0) { int2 Cell; };\n" +
              "float4 main(CxSpriteInput input) : SV_Target0 { return CX_SAMPLE(input) * Cell.x; }\n");
    CHECK(e.find("Cell: ivec2 is not supported") != std::string::npos);
    e = build(head + "CX_FRAGMENT_UNIFORMS(P, 0) { row_major float4x4 M; };\n" +
              "float4 main(CxSpriteInput input) : SV_Target0 { return mul(M, CX_SAMPLE(input)); }\n");
    CHECK(e.find("without row_major") != std::string::npos);
    e = build("#include \"coldx.hlsli\"\nfloat4 main() : SV_Target0 { return 1; }\n");
    CHECK(e.find("must sample the sprite's texture") != std::string::npos);
    e = build("#include \"coldx.hlsli\"\nCX_FRAGMENT_TEXTURE(t, s, 0);\n"
              "float4 main(float4 p : SV_Position, float3 uv : TEXCOORD0) : SV_Target0 { return t.Sample(s, uv.xy) * uv.z; }\n");
    CHECK(e.find("CxSpriteInput") != std::string::npos);
    e = build(head + "CX_FRAGMENT_TEXTURE(Extra, ExtraSampler, 2);\n" +
              "float4 main(CxSpriteInput input) : SV_Target0 { return CX_SAMPLE(input) * Extra.Sample(ExtraSampler, input.uv); }\n");
    CHECK(e.find("slot 1 is missing") != std::string::npos);

    // A good one passes, so the failures above are the shaders' and not the setup's.
    Write(dir / "bad" / "fx.hlsl", head + body);
    BuildOptions o;
    o.project = project.string();
    o.output = (dir / "bad" / "out.rime").string();
    CHECK(Build(o, tools.list).ok);

    // No compiler: says what to install.
    const std::string dxc = SDL_getenv("COLDX_DXC");
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_DXC", (dir / "no-such-dxc.exe").string().c_str(), true);
    Write(dir / "bad" / "fx.hlsl", head + body + "// changed, so the cache cannot answer\n");
    BuildResult r = Build(o, tools.list);
    CHECK(!r.ok && !r.errors.empty() && r.errors[0].find("install the Vulkan SDK") != std::string::npos);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_DXC", dxc.c_str(), true);
}

} // namespace

int main() {
    const char* dxc = SDL_getenv("COLDX_DXC");
    const char* spirvCross = SDL_getenv("COLDX_SPIRV_CROSS");
    if (!dxc || !*dxc || !spirvCross || !*spirvCross) {
        std::printf("skipped: COLDX_DXC and COLDX_SPIRV_CROSS are not set (install the Vulkan SDK and reconfigure)\n");
        return 77;
    }
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "COLDX_HEADLESS", "1", true);
    const fs::path dir = fs::temp_directory_path() / "coldx_effect_build_test";
    fs::remove_all(dir);
    Packed(dir);
    Refused(dir);
    fs::remove_all(dir);
    return CheckResult();
}
