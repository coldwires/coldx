#pragma once

namespace coldx {

// How a drawn color combines with what is already there. Colors are premultiplied
// everywhere in coldx except where NonPremultiplied says otherwise.
enum class BlendState {
    AlphaBlend,        // src + dst * (1 - src.a). The default.
    Additive,          // src * src.a + dst
    NonPremultiplied,  // src * src.a + dst * (1 - src.a), for straight-alpha art
    Opaque,            // src, ignoring alpha
};

// How a texture is read between texel centers and past its edges. Point keeps pixel art
// sharp when scaled; Linear smooths it. Wrap repeats the texture outside 0..1.
enum class SamplerState {
    // SpriteBatch::Begin's default: PointClamp in a game with GameSettings::pixelArt,
    // LinearClamp otherwise (as in XNA).
    Default,
    PointClamp,
    LinearClamp,
    PointWrap,
    LinearWrap,
};

} // namespace coldx
