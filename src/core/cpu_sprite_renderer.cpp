// SpriteBatch's quads drawn on the CPU, following the rules the GPU path's hardware follows:
// positions snapped to 1/256 pixel, the top-left fill rule, attributes interpolated at pixel
// centres, texture coordinates held to 1/256 texel, the sampler, float blending, and one
// conversion to 8 bits per channel at the end.

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <SDL3/SDL.h>

#include <coldx/core/effect.h>

#include "cpu_backend.h"
#include "effect_access.h"

namespace coldx::detail {

namespace {

constexpr std::int64_t SubpixelScale = 1 << SubpixelBits;
constexpr std::int64_t HalfPixel = SubpixelScale / 2;

// A position in texels, held to 1/256 texel as the GPU's sampler does before it picks texels,
// halves rounding up. Without this, a coordinate that should land exactly on a texel edge can
// fall just short of it in float and pick the texel before.
float Subtexel(float t) {
    return std::floor(t * 256.0f + 0.5f) / 256.0f;
}

// The weight of the second texel in a linear filter, from the fraction between them, kept
// to 8 bits.
float FilterWeight(float fraction) {
    return std::floor(fraction * 256.0f + 0.5f) / 256.0f;
}

// v / 255 for every byte, worked out once: the same floats, without a division per channel.
struct UnormTable {
    float values[256];
    UnormTable() {
        for (int i = 0; i < 256; ++i) values[i] = static_cast<float>(i) / 255.0f;
    }
};
const UnormTable unormTable;

float Unorm(std::uint8_t v) {
    return unormTable.values[v];
}

// v is clamped to 0..1 first, so truncating is the floor.
std::uint8_t ToUnorm(float v) {
    v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    return static_cast<std::uint8_t>(v * 255.0f + 0.5f);
}

// Whether v converts to 8 bits within NearTie of halfway between two steps, where a GPU
// blending at less than float precision may round the other way.
bool NearTie(float v) {
    v = (v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v)) * 255.0f;
    return std::fabs(v - std::floor(v) - 0.5f) < CpuNearTie;
}

int Address(int i, int size, bool wrap) {
    if (wrap) {
        i %= size;
        return i < 0 ? i + size : i;
    }
    return i < 0 ? 0 : (i >= size ? size - 1 : i);
}

Float4 Texel(const CpuTexture& t, int x, int y, bool* tie) {
    const size_t i = static_cast<size_t>(y) * t.Width() + x;
    if (tie && t.Ties() && t.Ties()[i]) *tie = true;
    const Color c = t.Pixels()[i];
    return {Unorm(c.r), Unorm(c.g), Unorm(c.b), Unorm(c.a)};
}

struct Vertex {
    std::int64_t x, y;  // snapped, in 1/256 pixel
    float u, v;
    float color[4];
};

// Which side of a->b p is on, times twice the triangle's area, in 1/256 pixel squared.
std::int64_t Edge(const Vertex& a, const Vertex& b, std::int64_t px, std::int64_t py) {
    return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
}

// Whether a pixel centre exactly on edge a->b belongs to this triangle, with the inside on
// the positive side: the top-left rule. A top edge is horizontal with the inside below it.
// A left edge has the inside to its right.
bool OwnsEdge(const Vertex& a, const Vertex& b) {
    const std::int64_t nx = -(b.y - a.y);
    const std::int64_t ny = b.x - a.x;
    return nx > 0 || (nx == 0 && ny > 0);
}

// How one quad's pixels get their colour: the built-in shader, or an effect's CPU version.
struct Shading {
    const CpuTexture* texture = nullptr;
    const float* bounds = nullptr;
    SamplerState sampler = SamplerState::PointClamp;
    const CpuPort* port = nullptr;     // null: the built-in shader
    const void* parameters = nullptr;  // the effect's uniform block, for port
    CpuPixelSource source;             // what port samples
};

class CpuSpriteRenderer final : public SpriteRenderer {
public:
    explicit CpuSpriteRenderer(CpuBackend& backend) : backend_(backend) {}
    bool Draw(const SpriteQuad* quads, std::uint32_t count, BlendState blend, SamplerState sampler, const Effect& effect,
              const Matrix& transform) override;

private:
    void Triangle(CpuTexture& target, Color* pixels, const Shading& shading, Vertex a, Vertex b, Vertex c, BlendState blend);

    CpuBackend& backend_;
};

bool CpuSpriteRenderer::Draw(const SpriteQuad* quads, std::uint32_t count, BlendState blend, SamplerState sampler,
                             const Effect& effect, const Matrix& transform) {
    if (count == 0) return true;
    CpuTexture* target = backend_.CurrentTarget();
    if (!target) {
        SDL_Log("coldx.core: SpriteBatch drawn outside Draw, nothing drawn");
        return false;
    }
    if (sampler == SamplerState::Default) sampler = SamplerState::LinearClamp;
    Shading shading;
    shading.sampler = sampler;
    shading.source.sampler = sampler;
    if (effect) {
        // The effect's parameters as they are now, as the GPU path pushes them at each draw.
        shading.port = static_cast<CpuEffect&>(EffectAccess::Backend(effect)).Port();
        if (shading.port) {
            shading.parameters = EffectAccess::Uniforms(effect).data();
            shading.source.count = EffectAccess::TextureCount(effect);
        }
    }

    // The game's transform, then pixels of the target to clip space, then the viewport.
    const float w = static_cast<float>(target->Width());
    const float h = static_cast<float>(target->Height());
    const Matrix m = transform * Matrix::CreateOrthographicOffCenter(0.0f, w, h, 0.0f, 0.0f, 1.0f);
    Color* pixels = target->Write(backend_.Frame());

    for (std::uint32_t q = 0; q < count; ++q) {
        const SpriteQuad& quad = quads[q];
        Vertex v[4];
        for (int i = 0; i < 4; ++i) {
            const SpriteVertex& s = quad.vertices[i];
            const float cx = s.x * m.m[0][0] + s.y * m.m[1][0] + m.m[3][0];
            const float cy = s.x * m.m[0][1] + s.y * m.m[1][1] + m.m[3][1];
            const float cw = s.x * m.m[0][3] + s.y * m.m[1][3] + m.m[3][3];
            const float sx = (cx / cw + 1.0f) * 0.5f * w;
            const float sy = (1.0f - cy / cw) * 0.5f * h;
            v[i].x = static_cast<std::int64_t>(std::nearbyint(sx * SubpixelScale));
            v[i].y = static_cast<std::int64_t>(std::nearbyint(sy * SubpixelScale));
            v[i].u = s.u;
            v[i].v = s.v;
            v[i].color[0] = Unorm(s.color.r);
            v[i].color[1] = Unorm(s.color.g);
            v[i].color[2] = Unorm(s.color.b);
            v[i].color[3] = Unorm(s.color.a);
        }
        shading.texture = static_cast<const CpuTexture*>(quad.texture);
        shading.bounds = quad.vertices[0].bounds;
        shading.source.bounds = shading.bounds;
        shading.source.textures[0] = shading.texture;
        if (shading.port)
            for (std::size_t slot = 1; slot < shading.source.count; ++slot)
                shading.source.textures[slot] = static_cast<const CpuTexture*>(EffectAccess::Texture(effect, slot, quad.texture));
        // The index buffer's two triangles: 0 1 2 and 1 3 2.
        Triangle(*target, pixels, shading, v[0], v[1], v[2], blend);
        Triangle(*target, pixels, shading, v[1], v[3], v[2], blend);
    }
    return true;
}

// Narrows first..last to the steps k where e + k * step >= 0.
void Span(std::int64_t e, std::int64_t step, std::int64_t& first, std::int64_t& last) {
    if (step == 0) {
        if (e < 0) first = last + 1;
        return;
    }
    if (step > 0) {
        if (e < 0) first = std::max(first, (-e + step - 1) / step);  // ceil(-e / step)
        return;
    }
    if (e < 0) {
        first = last + 1;
        return;
    }
    last = std::min(last, e / -step);  // floor(e / -step)
}

template <BlendState Blend>
void Raster(CpuTexture& target, Color* pixels, const Shading& sh, const Vertex& a, const Vertex& b, const Vertex& c,
            std::int64_t area) {
    const int tw = target.Width();
    const int th = target.Height();
    const auto lo = [](std::int64_t p) { return static_cast<int>((p - HalfPixel + SubpixelScale - 1) >> SubpixelBits); };
    const auto hi = [](std::int64_t p) { return static_cast<int>((p - HalfPixel) >> SubpixelBits); };
    const int x0 = std::max(0, lo(std::min({a.x, b.x, c.x})));
    const int x1 = std::min(tw - 1, hi(std::max({a.x, b.x, c.x})));
    const int y0 = std::max(0, lo(std::min({a.y, b.y, c.y})));
    const int y1 = std::min(th - 1, hi(std::max({a.y, b.y, c.y})));
    if (x0 > x1 || y0 > y1) return;

    // An edge that does not own its centres needs a strictly positive value: bias it by one.
    const std::int64_t biasA = OwnsEdge(b, c) ? 0 : 1;  // edge opposite a
    const std::int64_t biasB = OwnsEdge(c, a) ? 0 : 1;
    const std::int64_t biasC = OwnsEdge(a, b) ? 0 : 1;
    // Each edge value changes by a fixed amount per pixel to the right. Integers, so stepping
    // gives exactly what evaluating at each centre would.
    const std::int64_t stepA = -(c.y - b.y) * SubpixelScale;
    const std::int64_t stepB = -(a.y - c.y) * SubpixelScale;
    const std::int64_t stepC = -(b.y - a.y) * SubpixelScale;
    const float invArea = 1.0f / static_cast<float>(area);
    std::uint8_t* ties = target.Ties();
    bool tie = false;
    CpuPixelSource source = sh.source;
    source.tie = ties ? &tie : nullptr;
    CpuPixel pixel;
    source.Attach(pixel);

    for (int y = y0; y <= y1; ++y) {
        const std::int64_t py = (static_cast<std::int64_t>(y) << SubpixelBits) + HalfPixel;
        const std::int64_t px0 = (static_cast<std::int64_t>(x0) << SubpixelBits) + HalfPixel;
        std::int64_t ea = Edge(b, c, px0, py);
        std::int64_t eb = Edge(c, a, px0, py);
        std::int64_t ec = Edge(a, b, px0, py);
        // The pixels k steps right of x0 with every edge at least its bias, worked out exactly.
        std::int64_t first = 0;
        std::int64_t last = x1 - x0;
        Span(ea - biasA, stepA, first, last);
        Span(eb - biasB, stepB, first, last);
        Span(ec - biasC, stepC, first, last);
        if (first > last) continue;
        ea += stepA * first;
        eb += stepB * first;
        ec += stepC * first;
        Color* row = pixels + static_cast<size_t>(y) * tw;
        for (int x = x0 + static_cast<int>(first); x <= x0 + last; ++x, ea += stepA, eb += stepB, ec += stepC) {

            // Weights of b and c. Each attribute is a's value plus the change toward each.
            const float wb = static_cast<float>(eb) * invArea;
            const float wc = static_cast<float>(ec) * invArea;
            float u = a.u + wb * (b.u - a.u) + wc * (c.u - a.u);
            float v = a.v + wb * (b.v - a.v) + wc * (c.v - a.v);
            float tint[4];
            for (int i = 0; i < 4; ++i) tint[i] = a.color[i] + wb * (b.color[i] - a.color[i]) + wc * (c.color[i] - a.color[i]);

            tie = false;
            float s[4];
            if (sh.port) {
                pixel.position = Vector2(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
                pixel.uv = Vector2(u, v);
                pixel.color = Float4(tint[0], tint[1], tint[2], tint[3]);
                const Float4 o = sh.port->function(pixel, sh.parameters);
                s[0] = o.r;
                s[1] = o.g;
                s[2] = o.b;
                s[3] = o.a;
            } else {
                // The built-in fragment shader: clamp to the source rectangle, sample, tint.
                u = std::clamp(u, sh.bounds[0], sh.bounds[2]);
                v = std::clamp(v, sh.bounds[1], sh.bounds[3]);
                const Float4 t = Sample(*sh.texture, sh.sampler, u, v, source.tie);
                s[0] = t.r * tint[0];
                s[1] = t.g * tint[1];
                s[2] = t.b * tint[2];
                s[3] = t.a * tint[3];
            }

            Color& out = row[x];
            const float d[4] = {Unorm(out.r), Unorm(out.g), Unorm(out.b), Unorm(out.a)};
            float r[4];
            const float sa = s[3];
            for (int i = 0; i < 4; ++i) {
                if constexpr (Blend == BlendState::AlphaBlend) r[i] = s[i] + d[i] * (1.0f - sa);
                else if constexpr (Blend == BlendState::Additive) r[i] = s[i] * sa + d[i];
                else if constexpr (Blend == BlendState::NonPremultiplied) r[i] = s[i] * sa + d[i] * (1.0f - sa);
                else r[i] = s[i];
            }
            if (ties) {
                // A pixel is a tie if its own value is, or if it was made from one.
                const size_t i = static_cast<size_t>(y) * tw + x;
                tie = tie || (Blend != BlendState::Opaque && ties[i]);
                for (float value : r) tie = tie || NearTie(value);
                ties[i] = tie ? 1 : 0;
            }
            out = Color(ToUnorm(r[0]), ToUnorm(r[1]), ToUnorm(r[2]), ToUnorm(r[3]));
        }
    }
}

void CpuSpriteRenderer::Triangle(CpuTexture& target, Color* pixels, const Shading& shading, Vertex a, Vertex b, Vertex c,
                                 BlendState blend) {
    std::int64_t area = Edge(a, b, c.x, c.y);
    if (area == 0) return;
    // No culling: either winding draws. Turn it so the inside is positive.
    if (area < 0) {
        std::swap(b, c);
        area = -area;
    }
    switch (blend) {
    case BlendState::AlphaBlend: Raster<BlendState::AlphaBlend>(target, pixels, shading, a, b, c, area); break;
    case BlendState::Additive: Raster<BlendState::Additive>(target, pixels, shading, a, b, c, area); break;
    case BlendState::NonPremultiplied: Raster<BlendState::NonPremultiplied>(target, pixels, shading, a, b, c, area); break;
    case BlendState::Opaque: Raster<BlendState::Opaque>(target, pixels, shading, a, b, c, area); break;
    }
}

} // namespace

Float4 Sample(const CpuTexture& t, SamplerState sampler, float u, float v, bool* tie) {
    const bool wrap = sampler == SamplerState::PointWrap || sampler == SamplerState::LinearWrap;
    const int w = t.Width();
    const int h = t.Height();
    if (sampler == SamplerState::PointClamp || sampler == SamplerState::PointWrap) {
        const int x = Address(static_cast<int>(std::floor(Subtexel(u * w))), w, wrap);
        const int y = Address(static_cast<int>(std::floor(Subtexel(v * h))), h, wrap);
        return Texel(t, x, y, tie);
    }
    const float ax = Subtexel(u * w) - 0.5f;
    const float ay = Subtexel(v * h) - 0.5f;
    const float fx0 = std::floor(ax);
    const float fy0 = std::floor(ay);
    const float fx = FilterWeight(ax - fx0);
    const float fy = FilterWeight(ay - fy0);
    const int x0 = Address(static_cast<int>(fx0), w, wrap);
    const int x1 = Address(static_cast<int>(fx0) + 1, w, wrap);
    const int y0 = Address(static_cast<int>(fy0), h, wrap);
    const int y1 = Address(static_cast<int>(fy0) + 1, h, wrap);
    const Float4 p00 = Texel(t, x0, y0, tie), p10 = Texel(t, x1, y0, tie);
    const Float4 p01 = Texel(t, x0, y1, tie), p11 = Texel(t, x1, y1, tie);
    auto mix = [&](float c00, float c10, float c01, float c11) {
        const float top = c00 + (c10 - c00) * fx;
        const float bottom = c01 + (c11 - c01) * fx;
        return top + (bottom - top) * fy;
    };
    return {mix(p00.r, p10.r, p01.r, p11.r), mix(p00.g, p10.g, p01.g, p11.g), mix(p00.b, p10.b, p01.b, p11.b),
            mix(p00.a, p10.a, p01.a, p11.a)};
}

} // namespace coldx::detail

// ---- CpuPixel, for effects' CPU versions

namespace coldx {

using detail::CpuPixelSource;

Float4 CpuPixel::Sample() const {
    return SampleAt(uv);
}

Float4 CpuPixel::SampleAt(Vector2 at) const {
    const CpuPixelSource& s = CpuPixelSource::Of(*this);
    const float u = std::clamp(at.x, s.bounds[0], s.bounds[2]);
    const float v = std::clamp(at.y, s.bounds[1], s.bounds[3]);
    return detail::Sample(*s.textures[0], s.sampler, u, v, s.tie);
}

Float4 CpuPixel::SampleTexture(int slot, Vector2 at) const {
    const CpuPixelSource& s = CpuPixelSource::Of(*this);
    if (slot < 0 || static_cast<std::size_t>(slot) >= s.count) return {};
    return detail::Sample(*s.textures[slot], s.sampler, at.x, at.y, s.tie);
}

int CpuPixel::TextureWidth(int slot) const {
    const CpuPixelSource& s = CpuPixelSource::Of(*this);
    return slot >= 0 && static_cast<std::size_t>(slot) < s.count ? s.textures[slot]->Width() : 0;
}

int CpuPixel::TextureHeight(int slot) const {
    const CpuPixelSource& s = CpuPixelSource::Of(*this);
    return slot >= 0 && static_cast<std::size_t>(slot) < s.count ? s.textures[slot]->Height() : 0;
}

} // namespace coldx

namespace coldx::detail {

void ScaleInto(const CpuTexture& source, Color* out, int outWidth, const Rectangle& placed, bool smooth) {
    if (!smooth) {
        // Nearest: each output column and row maps to one source texel, found once. Rows that
        // map to the same source row are copies of the one before.
        std::vector<int> columns(static_cast<size_t>(placed.width));
        for (int x = 0; x < placed.width; ++x)
            columns[static_cast<size_t>(x)] =
                Address(static_cast<int>(std::floor(Subtexel((x + 0.5f) / placed.width * source.Width()))), source.Width(), false);
        int lastRow = -1;
        for (int y = 0; y < placed.height; ++y) {
            const int sy = Address(static_cast<int>(std::floor(Subtexel((y + 0.5f) / placed.height * source.Height()))),
                                   source.Height(), false);
            Color* row = out + static_cast<size_t>(placed.y + y) * outWidth + placed.x;
            if (sy == lastRow) {
                std::copy(row - outWidth, row - outWidth + placed.width, row);
                continue;
            }
            const Color* from = source.Pixels() + static_cast<size_t>(sy) * source.Width();
            for (int x = 0; x < placed.width; ++x) {
                const Color c = from[columns[static_cast<size_t>(x)]];
                row[x] = Color(c.r, c.g, c.b, 255);
            }
            lastRow = sy;
        }
        return;
    }
    const SamplerState sampler = SamplerState::LinearClamp;
    for (int y = 0; y < placed.height; ++y) {
        const float v = (y + 0.5f) / placed.height;
        Color* row = out + static_cast<size_t>(placed.y + y) * outWidth + placed.x;
        for (int x = 0; x < placed.width; ++x) {
            const Float4 c = Sample(source, sampler, (x + 0.5f) / placed.width, v, nullptr);
            row[x] = Color(ToUnorm(c.r), ToUnorm(c.g), ToUnorm(c.b), 255);
        }
    }
}

std::unique_ptr<SpriteRenderer> CreateCpuSpriteRenderer(CpuBackend& backend) {
    return std::make_unique<CpuSpriteRenderer>(backend);
}

} // namespace coldx::detail
