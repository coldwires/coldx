#include <coldx/core/effect_format.h>

#include <cstring>

namespace coldx::effect_format {

namespace {

constexpr std::size_t HeaderSize = 32;

void Put16(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void Put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    Put16(out, v & 0xffff);
    Put16(out, v >> 16);
}

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    bool Has(std::size_t n) const { return n <= size_ - at_; }
    std::uint32_t U8() { return data_[at_++]; }
    std::uint32_t U16() {
        const std::uint32_t v = data_[at_] | (data_[at_ + 1] << 8);
        at_ += 2;
        return v;
    }
    std::uint32_t U32() {
        const std::uint32_t lo = U16();
        return lo | (U16() << 16);
    }
    void Bytes(std::size_t n, std::vector<std::uint8_t>& out) {
        out.assign(data_ + at_, data_ + at_ + n);
        at_ += n;
    }
    void Text(std::size_t n, std::string& out) {
        out.assign(reinterpret_cast<const char*>(data_ + at_), n);
        at_ += n;
    }
    std::size_t Left() const { return size_ - at_; }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t at_ = 0;
};

bool KnownType(std::uint32_t type) {
    return type >= static_cast<std::uint32_t>(ParameterType::Float) && type <= static_cast<std::uint32_t>(ParameterType::UInt);
}

} // namespace

std::size_t SizeOf(ParameterType type) {
    switch (type) {
    case ParameterType::Float: return 4;
    case ParameterType::Float2: return 8;
    case ParameterType::Float3: return 12;
    case ParameterType::Float4: return 16;
    case ParameterType::Float4x4: return 64;
    case ParameterType::Int: return 4;
    case ParameterType::UInt: return 4;
    }
    return 0;
}

std::vector<std::uint8_t> Encode(const Compiled& c) {
    std::vector<std::uint8_t> out(Magic, Magic + 4);
    Put16(out, Version);
    Put16(out, static_cast<std::uint32_t>(c.textures.size()));
    Put32(out, c.uniformSize);
    Put32(out, static_cast<std::uint32_t>(c.parameters.size()));
    Put32(out, static_cast<std::uint32_t>(c.spirv.size()));
    Put32(out, static_cast<std::uint32_t>(c.dxil.size()));
    Put32(out, static_cast<std::uint32_t>(c.msl.size()));
    Put32(out, 0);
    for (const Parameter& p : c.parameters) {
        out.push_back(static_cast<std::uint8_t>(p.type));
        out.push_back(static_cast<std::uint8_t>(p.name.size()));
        Put16(out, 0);
        Put32(out, p.offset);
        out.insert(out.end(), p.name.begin(), p.name.end());
    }
    for (const Texture& t : c.textures) {
        out.push_back(static_cast<std::uint8_t>(t.slot));
        out.push_back(static_cast<std::uint8_t>(t.name.size()));
        out.insert(out.end(), t.name.begin(), t.name.end());
    }
    out.insert(out.end(), c.spirv.begin(), c.spirv.end());
    out.insert(out.end(), c.dxil.begin(), c.dxil.end());
    out.insert(out.end(), c.msl.begin(), c.msl.end());
    return out;
}

bool Decode(const void* data, std::size_t size, Compiled& out, std::string& error) {
    out = Compiled{};
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (!bytes || size < HeaderSize || std::memcmp(bytes, Magic, 4) != 0) {
        error = "not a compiled effect";
        return false;
    }
    Reader r(bytes + 4, size - 4);
    const std::uint32_t version = r.U16();
    if (version != Version) {
        error = "compiled effect version " + std::to_string(version) + ", this coldx reads version " + std::to_string(Version);
        return false;
    }
    const std::uint32_t textures = r.U16();
    out.uniformSize = r.U32();
    const std::uint32_t parameters = r.U32();
    const std::uint32_t spirv = r.U32();
    const std::uint32_t dxil = r.U32();
    const std::uint32_t msl = r.U32();
    r.U32();
    if (textures == 0 || textures > MaxTextures) {
        error = "an effect has 1 to " + std::to_string(MaxTextures) + " textures, not " + std::to_string(textures);
        return false;
    }
    for (std::uint32_t i = 0; i < parameters; ++i) {
        if (!r.Has(8)) break;
        Parameter p;
        const std::uint32_t type = r.U8();
        const std::uint32_t length = r.U8();
        r.U16();
        p.offset = r.U32();
        if (!KnownType(type) || !r.Has(length)) {
            error = "damaged parameter table";
            return false;
        }
        p.type = static_cast<ParameterType>(type);
        r.Text(length, p.name);
        if (p.offset + SizeOf(p.type) > out.uniformSize) {
            error = "parameter " + p.name + " lies outside the uniform block";
            return false;
        }
        out.parameters.push_back(std::move(p));
    }
    if (out.parameters.size() != parameters) {
        error = "damaged parameter table";
        return false;
    }
    for (std::uint32_t i = 0; i < textures; ++i) {
        if (!r.Has(2)) break;
        Texture t;
        t.slot = r.U8();
        const std::uint32_t length = r.U8();
        if (t.slot != i || !r.Has(length)) {
            error = "damaged texture table";
            return false;
        }
        r.Text(length, t.name);
        out.textures.push_back(std::move(t));
    }
    if (out.textures.size() != textures) {
        error = "damaged texture table";
        return false;
    }
    if (static_cast<std::uint64_t>(spirv) + dxil + msl != r.Left()) {
        error = "shader sizes do not match the data";
        return false;
    }
    r.Bytes(spirv, out.spirv);
    r.Bytes(dxil, out.dxil);
    r.Bytes(msl, out.msl);
    return true;
}

} // namespace coldx::effect_format
