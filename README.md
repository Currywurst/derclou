derclou
=======

Der Clou! - SDL Port

A burglary simulation game originally created by neo Software Produktions GmbH. See:

http://www.mobygames.com/game/clue


Recent Work

- Expanded the top-level CMake project to enumerate every source file, removed the obsolete `replace.sh`, and moved the shared `theclou.h` into `src/` so all targets include the same header tree.
- Introduced cross-platform packaging assets (Linux desktop file, Windows/macOS icons) and relocated every module into `src/…`, updating include paths and keeping CMake in sync.
- Dropped the handcrafted root `Makefile`, leaving CMake as the single build system.
- Completed the SDL3 migration: updated initialization/teardown in `base`/`inphdl`, rewrote large portions of `gfx` and `sound` to use the new audio streams, mutexes, and render APIs, and refreshed related headers.
- Repaired object-mask rendering in planning scenes by teaching `gfxnch4` and the landscape hardware code to keep the overlay bits intact when a container is opened.
- Documented the recent technical work in this README so new contributors see the current focus areas.
- Simplified keyboard handling: SDL quit events now trigger the global shutdown path and only F1–F10 are treated as instant menu shortcuts.
- Corrected a regression in `landscap/hardware.c` so palette indices stay valid when drawing scene tiles.
- Normalized a broad swath of comments and inline docs (anim, data, gameplay, gfx, landscapes, story, etc.) to clarify intent after the latest refactors.
- Cleared dozens of compiler warnings by tightening prototypes, headers, and enum usage across gameplay, landscape, living, planing, and audio modules.
- Added WAV-based CD audio emulation, speech playback, and fade control; touched `base`, `cdrom`, dialogs, `gfx`, and the full sound stack to support seamless CDAudioFromCD/FromWav switching.
- Performance pass: rebalanced data/gameplay structures, optimized landscape refresh paths, streamlined planing/present code, and pruned redundant math in the audio/graphics layers.
- Implemented a deterministic timestep by introducing a high-precision frame clock in `gfx`, updating `inphdl` and animation code to consume delta times instead of fixed sleeps.
- Brought in the initial CMake build script, describing targets, include paths, and compiler options for the full project.
