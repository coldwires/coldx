# 0004: Debug options come from environment variables

**Context.** Running a game unattended (a test, a script, a machine with no one watching)
needs a log file, a hidden window, a frame limit and a frame capture. coldx does not own
`main` and never sees the command line.

**Decision.** `Game::Run` reads `COLDX_CONSOLE`, `COLDX_LOG`, `COLDX_HEADLESS`,
`COLDX_FRAMES` and `COLDX_CAPTURE` from the environment. The README documents them.

**Consequences.** They work on any game without the game passing anything through. They
are compiled into release builds too; a shipped game that should not honour them will need
a switch to turn them off.
