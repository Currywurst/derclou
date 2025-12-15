derclou
=======

Der Clou! - SDL Port

A burglary simulation game originally created by neo Software Produktions GmbH. See:

http://www.mobygames.com/game/clue


Recent Work

- Modernized SDL input handling: window-close events now trigger a clean shutdown path (tcDone() + exit(0)), and F1–F10 remain the only shortcuts that jump directly into the save/load menu (F11+ ignored).
- Restored masked blitting for planning graphics so building floors/walls render correctly after refactors.
- Added WAV-based CD audio playback (intro speech and dialogue) alongside improved fade control across CDAudioFromCD/FromWav modes, plus related dialog/talk fixes.
- Refined planning systems: handler lifecycle cleanup helpers, guard/tool loading messaging, memset-based state resets, safer menu handler selection, and better plan file handling (graceful missing files, area-name bounds checking).
- Landscape subsystem cleanup: converted the global ls struct into gLandscapeState, tightened floor/object refresh logic, and improved scroll-state bookkeeping.
- Timing/graphics refresh: introduced a high-precision frame clock (configurable FPS, delta reporting) replacing the legacy 40 ms delay; updated intro animation pacing and SDL file modes accordingly.
- Housekeeping: warning fixes, comment tidy-ups, and generalized setup/options handling (e.g., better CLI parsing, audio fallbacks).
