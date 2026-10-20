# 0007: The GPU device lives as long as the Game

**Context.** A game holds textures and batches as members of its `Game` subclass. Those
are destroyed after `Run` returns, but before the `Game` base. If `Run` destroyed the GPU
device, each of them would release against a dead device.

**Decision.** `Run` only releases the window from the device. `~Game` destroys the device
and then calls `SDL_Quit`. Every GPU resource holds a shared `DeviceToken`; the device
clears it when destroyed, and a resource whose token is clear skips its release.

**Consequences.** Members of the game class clean up correctly with no code from the game.
A resource kept somewhere that outlives the `Game` does not crash; its GPU memory went with
the device. Calling `Run` again on the same `Game` reuses the device.
