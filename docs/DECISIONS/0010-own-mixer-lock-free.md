# 0010: Audio is coldx's own mixer, lock-free at the thread boundary

**Context.** SDL3 can mix several audio streams itself, but then volume, pitch, pan,
looping and when a sound has finished are spread across SDL calls, and nothing can be
tested without a device. The audio thread must also never wait on the game thread: a
lock held by the game for a moment is a click in the output.

**Decision.** One SDL device stream whose callback runs `Mixer::Mix`. The mixer holds
up to 64 voices and one music stream and mixes them to stereo float at 48 kHz with linear
resampling. The game thread talks to it only through a single-producer single-consumer
queue of commands; the mixer reports back through per-slot atomic generation numbers and
a second queue that returns finished music streams. The audio thread never allocates or
frees: sound data stays owned by the game thread until its voice is reported ended, and
music decodes from memory into a buffer made when the stream was opened.

**Consequences.** The mixer is plain arithmetic, tested sample by sample with no device.
Commands take effect at the next mix, a few milliseconds later, so state the game asks
about right after a call (`IsPlaying`, `State`) is tracked on the game side by generation.
A full command queue drops commands, logged once; 1,024 per mix is far beyond normal use.
Linear resampling is simple and cheap; it softens high frequencies slightly when pitching
up, which suits game effects.
