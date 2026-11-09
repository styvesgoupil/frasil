# FRASIL: the firmware

The app that fm1-base builds into FRASIL's firmware (`../build.sh`, from the repository's root): the FM-1's shell
around FRASIL's engine (`../engine/`: the sound and the brain, without the FM-1).

| File | What |
| --- | --- |
| `app.h` | the app's settings (name, block size, the store's objects) and its half of the contract with the platform |
| `app.c` | the shell: the store's records, the 16 slots, STORE and the names (32 characters; in OBJ_NAMES with each scene's LFO AMOUNTs and SWELL), the pop-ups, the panel (buttons, holds, encoders, the keys' scan and lights), MIDI learn; the engine's hooks |
| `pages.c` | the pages: each a button's, its four settings on K1 .. K4 |
| `ui.c` | the screen: RIVER (the play page), the edit pages, the logo screen, the help |
| `web.c` | the browser build's exports: the River plays and edits a shard (the engine's THE KEYS) |
| `fm1.toml` | build and site settings: name, package identity (`FM-1_47`), USB names, tagline, source, licence |
| `assets/`, `tools/gen_logo.py` | the logo, made into a 4-bit image for the screen at build time |
| `words.tsv` | the screen reader's words (fm1-reader) |
| `tests/basic.txt` | host simulator checks: keys, chords, pressure, GLINT, the arpeggiator, BASS, FREEZE, scenes (and those saved before BASS), the lights, the help |
| `tests/names.txt` | the names of 32 (`names.sh`), each scene's LFO AMOUNTs and SWELL: stored, a reboot, a backup, the earlier OBJ_NAMES (FRNM; FRNN, its AMOUNTs in percent, made multipliers) and settings (FRSQ; FRSR, the same) |
| `tests/screens.txt` | the screenshots (`../tools/screens.sh`); at its end the header's battery: its levels, colours and smoothing, and its shots (`battery-*.png`, not the manual's) |
| `tests/demo.txt` | a short performance, rendered to WAV |
| `tests/anchor.txt` | ANCHOR: set, BACK, MORPH (its frame erased as it goes), a hand, a scene, STORE and a take during it |
| `tests/live.txt` | PERFORM's LIVE: each engine input as an op (keys, pressure, settings, MIDI, PLAY, a scene in parts...), STATE's layout and its running state (card #131: its head, its bytes as taken at the ask), a STATE asked changing nothing (`pfstill`: the same renders with the asks as without), MIDI's 8-to-7, the queue held and full |
| `tests/engine.txt` | the engine end to end, each part's render hashed (renderhash): every render bit for bit |
| `tests/engine_alone.txt` | the engine with MIDI and settings alone: the same renders from the engine alone (`../engine/tests/run.sh`) |
| `docs/MANUAL.md`, `site/landing.md` | the manual and the site's front page (fm1-base's `tools/make_site.py`) |
