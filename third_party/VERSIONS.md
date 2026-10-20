# Third-party versions

Everything here is committed source, built by the top-level CMakeLists.txt. Nothing is
fetched at build time. To update a library, replace its folder with the new release and
update its row here in the same commit.

| Library | Version | Folder | Source | License |
|---|---|---|---|---|
| SDL | 3.4.18 | `SDL/` | `SDL3-3.4.18.tar.gz` from the libsdl-org/SDL GitHub release `release-3.4.18` (commit `829a65d769d935c4852f8159e964312c0957260a`) | zlib (`SDL/LICENSE.txt`) |

SDL tarball SHA-256: `9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3`

## Tools

Not vendored and not needed to build. These generated the committed headers in
`shaders/compiled/`; regenerating with other versions may change their bytes.

| Tool | Version | From |
|---|---|---|
| dxc | 1.9.0.5512 (01b62ad4) | Vulkan SDK 1.4.363.0 |
| spirv-cross | vulkan-sdk-1.4.357.0-39-gf11ba9f0 | Vulkan SDK 1.4.363.0 |
