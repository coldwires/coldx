# Architecture

coldx is a set of static libraries. A game links the ones it uses and includes
`<coldx/...>` headers only. `core` is required; every other module is opt-in, and a game
that does not link one contains none of its code.

## Modules and layers

A module is a directory under `src/` with a `MODULE.md`. Its public headers are in
`include/coldx/<module>/`, its tests in `src/<module>/tests/`, and its CMake target is
`coldx::<module>`. `MODULE.md` gives the module's layer, what it depends on, its public
surface and its invariants; read it before changing the module.

| Layer | Modules | May depend on |
|---|---|---|
| 0 | `core` | nothing in coldx |
| 1 | `content`; `protect` (planned) | `core` |
| 2 | `tools/coldxc` content tool | `content`, `core` |

Networking and physics are not planned as coldx modules: games build them above coldx,
as FNA games do.

A module depends only on lower layers. Two modules in the same layer never depend on each
other. When a lower layer needs to tell a higher one something, it does so through a
callback or an event it defines itself, never by including the higher one.

Samples and games see public headers only.

## Rules the code follows

- **Public headers are the contract.** No third-party type appears in one: no `SDL_*`, no
  Box2D type. Swapping a dependency changes `src/` only.
- **No exceptions cross the public API.** Failures are logged and returned as an empty
  object or a false/nonzero result.
- **GPU and other resources are move-only RAII types.**
- **Log lines start with the module name**, `coldx.core: ...`, so a log points at the code.
- **Files stay under 800 lines.** Split by concept rather than append.
- **ASCII only** in source and docs.
- **Games never edit coldx.** A game that needs something coldx cannot do gets a new
  extension point: a virtual on `Game`, a component, a loader, a callback.

## Enforcement

The compiler enforces linking: a module's CMake target links only what it may depend on.
`tools/check.py` enforces the rest, and runs as the `check` test under CTest:

- every module has a `MODULE.md` with layer, dependencies, third-party prefixes and size;
- dependencies point to lower layers only;
- public headers include no third-party header and only coldx modules their owner depends on;
- source reaches another module only through its public headers, never `../` into its `src/`;
- samples include public headers only;
- the line cap, and ASCII;
- a doc that embeds a sample (`<!-- sample: path -->`) matches that file exactly.

## Where to read next

- `docs/QUICKSTART.md`: writing a game, the API on a page.
- `docs/STATUS.md`: what exists, what is stubbed, what is not started.
- `docs/FLOWS.md`: how data moves through a frame, a texture load, a shader build.
- `docs/DECISIONS/`: why things are the way they are. Read the relevant one before
  changing what it covers.
