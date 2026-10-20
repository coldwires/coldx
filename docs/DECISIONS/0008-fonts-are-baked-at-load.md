# 0008: Fonts are baked at load, one pixel size per SpriteFont

**Context.** XNA's SpriteFont is baked by its content pipeline; coldx's content pipeline
does not exist yet, and pixel-art games want text that is exactly the rasterized font,
not a scaled or filtered copy of it.

**Decision.** `SpriteFont::FromFile` and `FromMemory` bake a TTF with stb_truetype at load,
at one pixel height, into a single atlas holding printable ASCII and Latin-1. Layout rounds
the pen to a whole pixel for each glyph, and keeps the font data for kerning. A different
size is a different `SpriteFont`. Measuring and drawing go through one layout routine.

**Consequences.** Text at a whole-pixel position with `PointClamp` matches the font's own
rasterization pixel for pixel; `core.font` checks exactly that. Changing size costs a bake
(fast at UI sizes). Characters outside Latin-1 draw as the default character until a wider
range is baked. When the content pipeline exists, baked fonts will load from it with the
same layout code.
