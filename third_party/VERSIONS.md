# Third-party versions

Everything here is committed source, built by the top-level CMakeLists.txt. Nothing is
fetched at build time. To update a library, replace its folder with the new release and
update its row here in the same commit.

| Library | Version | Folder | Source | License |
|---|---|---|---|---|
| SDL | 3.4.18 | `SDL/` | `SDL3-3.4.18.tar.gz` from the libsdl-org/SDL GitHub release `release-3.4.18` (commit `829a65d769d935c4852f8159e964312c0957260a`) | zlib (`SDL/LICENSE.txt`) |
| stb_truetype | 1.26 | `stb/stb/` | `stb_truetype.h` from nothings/stb at commit `a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d` | MIT or public domain (`stb/LICENSE`) |
| dr_wav | 0.14.6 | `dr_libs/dr_libs/` | `dr_wav.h` from mackron/dr_libs at commit `dfe8377631000664666519fdb83da193fd8037f4` | MIT-0 or public domain (`dr_libs/LICENSE`) |
| stb_vorbis | 1.22 | `stb/stb/` | `stb_vorbis.c` from nothings/stb at commit `a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d` | MIT or public domain (`stb/LICENSE`) |
| LZ4 | 1.10.0 | `lz4/lz4/` | `lz4.c`, `lz4.h`, `lz4hc.c`, `lz4hc.h` from `lib/` of `v1.10.0.tar.gz`, the lz4/lz4 GitHub tag `v1.10.0` | BSD 2-Clause (`lz4/LICENSE`) |
| Liberation Sans Regular | 2.1.5 | `fonts/liberation/` | `liberation-fonts-ttf-2.1.5.tar.gz` from the liberationfonts/liberation-fonts GitHub release `2.1.5` | SIL OFL 1.1 (`fonts/liberation/LICENSE`) |

SDL tarball SHA-256: `9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3`
`stb_truetype.h` SHA-256: `ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab`
`dr_wav.h` SHA-256: `03e70c1a2d9787cd7ed3e966c075bea7bac6373f759db9cd7ca9ccdfc4ec4493`
`stb_vorbis.c` SHA-256: `4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636`
LZ4 tarball SHA-256: `537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b`
`lz4.c` SHA-256: `9396f7de527bc8435de9c7569fb7998e56545a84b4f3c2d808c0235c01774539`
`lz4hc.c` SHA-256: `126cafafdb91767e6e55238298a910903851b35b2cee27ce80ae2280469ee232`
`LiberationSans-Regular.ttf` SHA-256: `76d04c18ea243f426b7de1f3ad208e927008f961dc5945e5aad352d0dfde8ee8`

Liberation Sans is used by the samples and tests only; coldx itself ships no font. A game
that ships it must ship `LICENSE` beside it, as `samples/hello_text` does.

## Generated audio

Made for coldx with ffmpeg (libvorbis), so they are covered by coldx's own license:

- `src/core/tests/data/tone.ogg`: a 441 Hz sine, 0.5 s, mono, 44.1 kHz.
  `ffmpeg -f lavfi -i "sine=frequency=441:sample_rate=44100:duration=0.5" -ac 1 -c:a libvorbis -q:a 4 tone.ogg`
- `src/core/tests/data/stereo.ogg`: 441 Hz left, 882 Hz right, 0.5 s, 44.1 kHz.
  `ffmpeg -f lavfi -i "sine=frequency=441:sample_rate=44100:duration=0.5" -f lavfi -i "sine=frequency=882:sample_rate=44100:duration=0.5" -filter_complex "[0][1]amerge=inputs=2" -c:a libvorbis -q:a 2 -map_metadata -1 -fflags +bitexact stereo.ogg`
- `samples/hello_audio/content/loop.ogg`: an 8 s looping arpeggio over a drone, stereo.
  An `aevalsrc` expression: notes from A3 up a minor pentatonic (0, 3, 5, 7, 10, 12, 10, 7
  semitones) every quarter second, each decaying as `exp(-14 t)`, over 110 Hz and
  164.81 Hz sines; libvorbis `-q:a 3`.

Also made for coldx: `samples/demo/content/sheet.png` (drawn with Pillow; a grid of
32 px tiles, each outlined in magenta with its name under it, for editing by hand),
`pickup.wav` (a 0.15 s rising chirp) and `hit.wav` (0.3 s of filtered brown noise), both
from ffmpeg `aevalsrc`/`anoisesrc`.

## Tools

Not vendored and not needed to build. These generated the committed headers in
`shaders/compiled/`; regenerating with other versions may change their bytes.

| Tool | Version | From |
|---|---|---|
| dxc | 1.9.0.5512 (01b62ad4) | Vulkan SDK 1.4.363.0 |
| spirv-cross | vulkan-sdk-1.4.357.0-39-gf11ba9f0 | Vulkan SDK 1.4.363.0 |
