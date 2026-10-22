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
  Watermarks inside images and sound are not built.
- Strings: `COLDX_HIDDEN` in the header-only `protect` module scrambles literals at
  compile time.

**Consequences.** Casual extraction (opening the package, searching the executable for
text or class names) finds nothing useful. A reader with a debugger finds the key at
`SetDefaultKey` and the revealed strings in memory; that is accepted, and written down
where each piece is documented. Fingerprints are removable by anyone who knows where they
are; they catch careless leaks, not careful ones. A game using `coldx_harden` cannot use
`dynamic_cast` or `typeid` on its own types.
