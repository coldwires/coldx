# 0012: Packed content is a hashed archive read by core, built by the content module

**Context.** Spec 7.1 and 7.2 ask for `.rime` packages built by a `coldxc` tool from a
content project, read by `ContentManager`, with nothing in a file naming what it holds.
`ContentManager` is in core, layer 0; the pipeline is the `content` module, layer 1, which
core may not depend on. The spec also has processors turn sources into GPU-ready data
(raw RGBA textures, PCM sound, baked fonts), but compression is not in yet (owner,
2026-10-20), and raw RGBA is five to ten times the size of the PNG it came from.

**Decision.**
- The format and its reader and writer are core's: `include/coldx/core/rime.h` gives the
  layout, the name hash and `rime::PackageWriter`; `ContentManager::Mount` reads packages.
  A game that loads packed content links only core.
- A package is a virtual file system. Each entry is a content path (`sprites/player.png`,
  lower case, forward slashes) stored only as a 64-bit hash, seeded per package, with a
  type number, flags, a CRC-32 and its bytes. `ContentManager` looks in mounted packages
  before loose files, by the same names and extensions, so code does not change between
  loose and packed content.
- The built-in processors check their sources and store them as they are: PNG, WAV and
  OGG, TTF. Decoding stays where it already is, at load. A font in a package can therefore
  be baked at any size, as a loose one can.
- `content` holds the content project parser, the `Processor` interface for game-defined
  processors, the build cache and `RunTool`. `tools/coldxc` is `RunTool` with the built-in
  processors; a game with its own processors builds its own tool by calling `RunTool` with
  them, so nobody edits coldx.
- Loaders that read bytes (`RegisterDataLoader`) see packed and loose content alike;
  loaders that take a path (`RegisterLoader`) see loose files only.
- Reserved for protection: header flags, a key id and a fingerprint field; per-entry
  compressed and encrypted flags. A v1 reader refuses entries with flags it does not know,
  and versions it does not know.

**Later (2026-10-21).** Compression came in with LZ4 (owner's go-ahead): the writer
stores an entry as one LZ4 block, compressed at the highest level, when that saves at
least a twentieth, so WAV, TTF and data shrink and PNG and OGG stay as they are. The
reader caps the claimed size of a compressed entry at what LZ4 can expand to before
allocating. `[texture]` takes `format = raw` to store decoded, premultiplied pixels behind
a binary tag: a 1280x720 frame was 74 KB as PNG and 84 KB raw and compressed, with no PNG
decode at load. PNG stays the default.

**Consequences.** Packages are about the size of their sources. Load time is what loose
files cost now, minus the file system. When compression is added, a processor can store
decoded, compressed data under the same entry flags without a format change. The hash
hides names but not contents: a PNG entry is still a PNG until encryption (milestone 12).
In release builds loose files are read only when no package is mounted, so a game that
ships a package cannot be modded by dropping files beside it unless it asks.
