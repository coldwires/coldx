# 0002: Drawing goes into an internal back buffer

**Context.** The swapchain texture cannot be read back, and does not exist without a
visible window. Capturing a frame and drawing headless both need something else to draw
into.

**Decision.** `GraphicsDevice` owns an R8G8B8A8 texture the size of the window's pixel
area (or of `GameSettings` when headless). All drawing targets it. At the end of the
frame it is blitted to the swapchain with nearest filtering at the same size.

**Consequences.** One full-screen blit per frame. Frames can be captured
(`COLDX_CAPTURE`) and games can run with no window shown (`COLDX_HEADLESS`), which is
what the render tests use. Pixel output is identical to drawing straight into the
swapchain; the render tests check that. A fixed virtual resolution scaled into the window
can be added at the blit later.
