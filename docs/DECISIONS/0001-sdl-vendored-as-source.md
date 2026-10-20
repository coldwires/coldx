# 0001: SDL is vendored as source

**Context.** coldx must build unchanged years from now, offline. SDL could be a git
submodule or committed source.

**Decision.** SDL is committed under `third_party/SDL`, taken from the release tarball
rather than a clone, because the tarball carries `REVISION.txt` and a filled-in
`SDL_revision.h`. The tarball was diffed against the release tag; only those release files
differ. Version, commit and tarball hash are in `third_party/VERSIONS.md`. SDL builds as a
static library with its tests and examples off.

**Consequences.** A clone needs no network and no submodule step. The repo carries about
50 MB of SDL source. Updating SDL means replacing the folder and the `VERSIONS.md` row in
one commit. SDL's own nested `.gitignore` files hide some files the release ships; those
are added with `git add -f` so the vendored copy matches the release.
