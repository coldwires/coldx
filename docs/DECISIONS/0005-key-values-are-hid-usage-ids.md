# 0005: Key values are USB HID usage IDs

**Context.** `Key` needs a numbering that does not expose SDL types but maps to SDL
without a table.

**Decision.** `Key` values are USB HID keyboard usage IDs, the same numbers SDL scancodes
use. `keyboard.cpp` pins the correspondence with `static_assert`s.

**Consequences.** Mapping is a cast. Keys name physical positions, so `Key::W` is the same
key on QWERTY and AZERTY. Typed text, which depends on layout, will need a separate API.
