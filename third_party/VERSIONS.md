# Third-party versions

Everything here is committed source, built by the top-level CMakeLists.txt. Nothing is
fetched at build time. To update a library, replace its folder with the new release and
update its row here in the same commit.

| Library | Version | Folder | Source | License |
|---|---|---|---|---|
| SDL | 3.4.18 | `SDL/` | `SDL3-3.4.18.tar.gz` from the libsdl-org/SDL GitHub release `release-3.4.18` (commit `829a65d769d935c4852f8159e964312c0957260a`) | zlib (`SDL/LICENSE.txt`) |
| stb_truetype | 1.26 | `stb/stb/` | `stb_truetype.h` from nothings/stb at commit `a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d` | MIT or public domain (`stb/LICENSE`) |
| Liberation Sans Regular | 2.1.5 | `fonts/liberation/` | `liberation-fonts-ttf-2.1.5.tar.gz` from the liberationfonts/liberation-fonts GitHub release `2.1.5` | SIL OFL 1.1 (`fonts/liberation/LICENSE`) |

SDL tarball SHA-256: `9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3`
`stb_truetype.h` SHA-256: `ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab`
`LiberationSans-Regular.ttf` SHA-256: `76d04c18ea243f426b7de1f3ad208e927008f961dc5945e5aad352d0dfde8ee8`

Liberation Sans is used by the samples and tests only; coldx itself ships no font. A game
that ships it must ship `LICENSE` beside it, as `samples/hello_text` does.

## Tools

Not vendored and not needed to build. These generated the committed headers in
`shaders/compiled/`; regenerating with other versions may change their bytes.

| Tool | Version | From |
|---|---|---|
| dxc | 1.9.0.5512 (01b62ad4) | Vulkan SDK 1.4.363.0 |
| spirv-cross | vulkan-sdk-1.4.357.0-39-gf11ba9f0 | Vulkan SDK 1.4.363.0 |
