# FRASIL: the firmware

The app that fm1-base builds into FRASIL's firmware (`../build.sh`, from the repository's root).

| File | What |
| --- | --- |
| `app.h` | the app's settings (name, block size, the store's objects) and its half of the contract with the platform |
| `app.c` | the app: keys, buttons, encoders, MIDI, the arpeggiator, scenes, the lights |
| `params.c` | every parameter: its range, how the screen writes it, its page; the factory scenes and GLINT's presets |
| `harmony.c` | the key (ROOT, SCALE) and what a played note becomes (CHORD, VOICING) |
| `ui.c` | the screen: RIVER (the play page), the edit pages, the logo screen, the help |
| `dsp/` | the sound, compiled apart at -O2: `frasil.c` (voices and layers), `glint.c` (GLINT and its delay), `bass.c` (BASS), `nuage.c` (CLOUD, the reverb) |
| `fm1.toml` | build and site settings: name, package identity (`FM-1_47`), USB names, tagline, source, licence |
| `assets/`, `tools/gen_logo.py` | the logo, made into a 4-bit image for the screen at build time |
| `words.tsv` | the screen reader's words (fm1-reader) |
| `tests/basic.txt` | host simulator checks: keys, chords, pressure, GLINT, the arpeggiator, BASS, FREEZE, scenes (and those saved before BASS), the lights, the help |
| `tests/screens.txt` | the screenshots (`../tools/screens.sh`) |
| `tests/demo.txt` | a short performance, rendered to WAV |
| `docs/MANUAL.md`, `site/landing.md` | the manual and the site's front page (fm1-base's `tools/make_site.py`) |
