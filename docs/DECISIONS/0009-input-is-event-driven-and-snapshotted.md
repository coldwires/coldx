# 0009: Input is event-driven and snapshotted per Update

**Context.** XNA games call `GetState` during `Update` and compare it with last frame's
state to find presses. Reading SDL's live key array gives a different answer partway
through an Update if events arrive, cannot be driven by injected events, and leaves every
game to keep its own previous state.

**Decision.** coldx keeps its own input state, updated only from SDL events (keyboard,
mouse, pad connect and disconnect); pads are read from SDL at the same point. Before each
Update the live state is copied to current, and current to previous. `GetState` returns
current; `WasPressed` and `WasReleased` compare current with previous. Scripted input
(`COLDX_INPUT`) is applied to the live state just before that copy. Losing window focus
releases every key and mouse button.

**Consequences.** An Update sees one consistent state. Edges work without game code, and
with a fixed timestep running several Updates in one frame, only the first sees a press.
Pushed SDL events and scripted input behave like real input, which is what makes input
testable headless. Pad button presses are subject to SDL's background-input rule.
