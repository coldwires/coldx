# protect

**Owns:** making a shipped game less legible: strings kept out of the executable as text.
Package encryption and build fingerprints are in core (`rime.h`, `ContentManager::SetKey`)
and the content module (`coldxc --key-file`, `--build-id`, `--identify`), where the
packages are read and written; release build flags are `cmake/coldx_harden.cmake`.
**Layer:** 1
**Depends on:** none
**Third-party:** none
**Public headers:** `include/coldx/protect/`
**Library target:** `coldx::protect` (header-only)
**Size:** ~0.1k lines

## Public surface

- `COLDX_HIDDEN("text")`: a `std::string` of the text, stored scrambled with a key from the
  file, line and a counter, revealed each time the expression runs.

## Invariants

- The plain text of a hidden literal is not in the executable, in Debug or hardened
  Release: `protect.hidden` searches its own executable for it.
- Revealing reads the scrambled bytes through `volatile`, so no optimizer can fold the text
  back in.

## Gotchas

- Not encryption: the key is in the code beside the data, and the revealed string is in
  memory. It removes a search shortcut, nothing more.
- Do not pass a hidden string's literal to anything that stringizes its argument (an
  assert or a `CHECK` macro): the macro puts the text back into the binary.
