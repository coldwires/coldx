# 0013: Protection is a set of cost-raisers, each where its data lives

**Context.** Spec milestone 12 asks for encrypted packages, per-build fingerprints,
obfuscation and a hardened release profile. A game's client holds everything it runs, keys
included, so none of this can stop a determined reader; it can only make reading slower
and copying traceable. Each piece touches a different layer: release flags touch every
target, encryption touches the package reader in core, fingerprints touch the tool and the
game executable, string hiding touches game code.

**Decision.**
- Release builds: `COLDX_HARDEN` and `coldx_harden` (`cmake/coldx_harden.cmake`) apply
  `/GR-`, whole-program optimization, `/OPT:REF /OPT:ICF`, `/Brepro`, no tool version
  record, trimmed source paths and symbols kept out of the executable. coldx itself uses no
  RTTI, so it builds that way.
- Encryption: ChaCha20 (RFC 8439), written here and checked against the RFC's vectors
  rather than adding a crypto dependency. Entries are compressed, then encrypted; the nonce
  is the entry's name hash and CRC-32, so builds stay deterministic and a key never meets
  one nonce with two contents. The header's key check tells a wrong key from damage. The
  key reaches the game through a source file coldxc generates (`--game-source`), which
  stores it masked and shuffled across arrays and installs it with
  `ContentManager::SetDefaultKey` before `main`.
- Fingerprints: a build id hashes to 64 bits, stored masked in the package header and in
  the generated source behind a marker derived from the package seed, so there is no byte
  pattern common to all coldx games to search for. `coldxc --identify` reads either.
- Watermarks (added the same day): `[texture] watermark = rgb | alpha` and
  `[sound] watermark = on` write the build's fingerprint into each file, in layers that
  each read back from the suspect file alone with the project's secret (the seed, and the
  key if there is one). PNG: a private chunk, the filter chosen for each row, the colour of
  fully transparent pixels, and the lowest bit of a channel (or alpha 254/255 on opaque
  pixels, which keeps a pixel-art palette exact) at 80 places in every 32x32 tile,
  shuffled by the secret. WAV: a private chunk and the lowest bit of non-silent samples at
  80 places in every 2048. Each layer repeats an 80-bit payload (fingerprint and a 16-bit
  check) and decides by majority. Tiling makes crops and trimmed clips readable: the
  reader tries every alignment, and to keep false accusations out accepts one only with
  two copies of every bit, 90% agreement and the check (40 random images and 20 wrong
  secrets in the tests: no claims). The first version spread marks over the whole file
  with a fixed stride, which needed the uncropped file and left a regular lattice. Marked PNGs are
  written by coldxc itself, as 8-bit RGBA, compressed with stb_image_write's zlib; stb_image
  supplies the decompressor that reads row filters back.
- Strings: `COLDX_HIDDEN` in the header-only `protect` module scrambles literals at
  compile time.

**Consequences.** Casual extraction (opening the package, searching the executable for
text or class names) finds nothing useful. A reader with a debugger finds the key at
`SetDefaultKey` and the revealed strings in memory; that is accepted, and written down
where each piece is documented. Fingerprints are removable by anyone who knows where they
are; they catch careless leaks, not careful ones. Watermark layers survive what the test
shows: a direct rip keeps them all, a lossless re-save or a crop of about 64x64 pixels or
more keeps the pixel bits and transparent pixels, a trimmed clip keeps the sample bits;
resizing, lossy re-encoding, screenshots and OGG files are not
covered. A robust, spread-spectrum mark would be the next step if that matters. A game using `coldx_harden` cannot use
`dynamic_cast` or `typeid` on its own types.
