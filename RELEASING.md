# Releasing FRASIL

A release is a GitHub release with the package `frasil-X.Y.fwsc` attached. The FM-1 Firmware Hub
([thefirmwarehub.com](https://thefirmwarehub.com/)) checks its listed repositories several times a day and offers
the newest release by itself. Once FRASIL is listed, publishing a release is all it takes.

## What the hub asks, and where FRASIL has it

| The hub's requirement | FRASIL |
| --- | --- |
| It makes music | a drone and texture instrument |
| Open source, the source public in the repository, a licence that allows passing it on | MPL-2.0 (`LICENSE`, `LICENSING.md`), this repository |
| A `.fwsc` on a GitHub release, one per release | `frasil-X.Y.fwsc`, made by `tools/release.sh` |
| An update loader in the package | felucca-bsp's, put in every package by fm1-base's build |
| An identity starting with `FM-1_`, its own | `FM-1_47XXYYZZ` (`id_prefix` in `app/fm1.toml`): 1.0 will be `FM-1_47010000`. No other firmware on the hub uses `FM-1_47` (checked October 2026) |
| A picture of its screen helps | `docs/img/screen-play.png` (and the others in `docs/img/`) |

The hub marks a card **beta** while the version is below 1.0, or when its name says beta, alpha, rc or test.

## Steps

**The version** is in `VERSION`, and every build carries it (`./build.sh` passes it as `--release`): its number on the
logo's screen, its identity, its package `app/build/frasil-VERSION.fwsc`. Until the first release, each build installed
on the FM-1 takes the next 0.9.x (0.9.1, 0.9.2 ...); the first release is 1.0, then 1.1, 1.2 ... (1.0.1 for a fix).

1. Set `VERSION` to the release's number, and update `CHANGELOG.md`: a `## X.Y` section with what changed.
2. Refresh the screenshots if the screen changed: `./build.sh --host && tools/screens.sh`.
3. Commit, then build and check: `tools/release.sh`. It runs the host checks, builds `app/build/frasil-X.Y.fwsc`,
   and confirms its identity.
4. Install that package on the FM-1 (`python3 base/tools/fm1_install.py app/build/frasil-X.Y.fwsc`) and play it.
   The hub also suggests booting it in [fm1-emulator](https://github.com/simonjohansson/fm1-emulator), which runs the
   real package.
5. Publish: `tools/release.sh --publish`. It tags `vX.Y`, creates the GitHub release with the package and the
   changelog section as notes, and publishes the site to GitHub Pages: front page, web installer, browser emulator,
   manual (fm1-base's `tools/publish_site.sh`; it needs Emscripten for the emulator).
   The release must be a full release, not a pre-release: the hub and GitHub's "latest" take full releases.
6. The first time only: in the repository's settings, set Pages to the `gh-pages` branch. Then ask for the
   listing (below).

## Getting listed (once)

Send an email to zven@thefirmwarehub.com once the first release is out:

> **Subject:** FM-1 Firmware Hub: my firmware
>
> Name: FRASIL
>
> Repository: https://github.com/styvesgoupil/frasil
>
> What it does: An ambient drone and texture instrument. Tap a key to latch a drone (a note, or a chord in the key
> you chose), press to make it swell. Three layers per voice (detuned saws, drifting partials, tuned noise) flow
> into a large reverb with shimmer and freeze. GLINT plays small icy FM bells on top, by hand, through an
> arpeggiator or a 32-step sequencer that follow MIDI clock; BASS holds one deep note under them. The drones are the
> notes you light, and they never change.
>
> Identity: FM-1_47XXYYZZ (FM-1_47 is FRASIL's own; 1.0 is FM-1_47010000).
>
> Licence: MPL-2.0. Built on fm1-base and felucca-bsp.
>
> Screen: https://github.com/styvesgoupil/frasil/blob/main/docs/img/screen-play.png
>
> Try it in the browser: https://styvesgoupil.github.io/frasil/emu/
