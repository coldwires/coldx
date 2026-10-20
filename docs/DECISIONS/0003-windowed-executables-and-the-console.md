# 0003: Games are windowed executables; the console opens at runtime

**Context.** A console-subsystem program on Windows always opens a console window beside
the game. Release builds should not show one; debug builds want one for the log.

**Decision.** `coldx_game(target)` makes the executable a windowed program
(`WIN32_EXECUTABLE`), and on MSVC sets `/ENTRY:mainCRTStartup` so the entry point stays
`main`. `Game::Run` opens a console with `AllocConsole` when `GameSettings::console` asks
for one (by default, in debug builds), unless stdout and stderr are already redirected.

Rejected: SDL_main, which would put an SDL header in the game's own source; a `WinMain` in
coldx calling `main`, which is ill-formed C++.

**Consequences.** Games write a plain `main` on every platform. Output redirected by a
launcher or a test runner goes where it was pointed. Closing the console window ends the
process without the normal shutdown.
