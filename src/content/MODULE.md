# content

**Owns:** building `.rime` packages: content project files, processors (built-in and a
game's own), the processed-source cache, and the `coldxc` command line.
**Layer:** 1
**Depends on:** core
**Third-party:** SDL3
**Public headers:** `include/coldx/content/`
**Library target:** `coldx::content`
**Size:** ~1.4k lines

The format and reading packages are core's (`coldx/core/rime.h`, `ContentManager::Mount`),
so a game that only loads packages does not link this module. Tools link it.
`docs/DECISIONS/0012-rime-packages.md` has the reasons.

## Public surface

- `Processor`: `Name` (its project section), `Version` (bump to invalidate the cache),
  `SettingKeys`, `Process(source, settings, outputs, error)`. `Source`, `Output`,
  `Settings`. `BuiltInProcessors()`: texture, sound, music, font, data.
- `Build(BuildOptions, processors)` gives a `BuildResult`: counts, whether the package
  was rewritten, errors naming file and line.
- `RunTool(argc, argv, extra processors)`: the `coldxc` command line; a game's own tool
  calls it with its processors.
- CMake: `coldx_add_content(target PROJECT content.coldx [OUTPUT name.rime] [TOOL tool]
  [KEY_FILE file] [BUILD_ID id])` in `cmake/coldx_content.cmake`. `BuildOptions` has
  `keyFile`, `buildId`, `gameSource`.

## Files

- `project.*`: the project file (`seed`, `[processor] patterns`, `key = value`), glob
  matching, listing a folder.
- `processors.cpp`: the built-in processors. Each checks its source (a PNG is decoded) and
  stores it unchanged, except `[texture] format = raw`, which stores decoded pixels.
  Compression is the writer's (`rime::PackageWriter`), per entry, when it helps.
- `build.cpp`: assigning files to sections (`from` moves a section's base folder), the
  cache, the package, and writing it only when it changed.
- `protect_build.*`: key files, fingerprints (hash, mask, marker), the source generated
  into a game (`--game-source`) and `--identify`.
- `tool.cpp`: `RunTool`. `tools/coldxc/main.cpp` is `RunTool` with nothing added.

## Invariants

- A package depends only on the project and its sources: the same inputs give the same
  bytes, cached or not, on any machine.
- An unchanged package is not rewritten, so build systems see no change.
- A failed build never touches the existing package.
- A file matched by two sections, an unknown processor or setting, and two outputs with
  one name are errors, not silent choices.
- Patterns never leave their base folder; only a section's `from` setting names another.

## Gotchas

- The cache is keyed by processor name and version, settings, the source's name and its
  bytes. A processor whose output changes without a version bump keeps serving old
  results from the cache.
- The cache is never pruned; delete the folder to reclaim it.
