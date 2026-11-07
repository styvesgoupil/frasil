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

1. Set `VERSION` to the release's number, and `FRS_VERSION` in `engine/frasil_engine.h` the same (`engine/tests/run.sh`
   checks them), and update `CHANGELOG.md`: a `## X.Y` section with what changed.
2. Refresh the screenshots if the screen changed: `./build.sh --host && tools/screens.sh`.
   Keep FRASIL STREAM's engine for this version: `tools/stream_wasm.sh --keep` (`website/stream/engines/X.Y.wasm`, for ever:
   a stream plays on the engine it was made on). `--publish` stops without it.
3. Commit, then build and check: `tools/release.sh`. It runs the host checks, builds `app/build/frasil-X.Y.fwsc`,
   and confirms its identity.
4. Install that package on the FM-1 (`python3 base/tools/fm1_install.py app/build/frasil-X.Y.fwsc`) and play it.
   The hub also suggests booting it in [fm1-emulator](https://github.com/simonjohansson/fm1-emulator), which runs the
   real package.
5. Publish, from the private repository, outside office hours. This repository's history is private: the public one
   (origin, styvesgoupil/frasil) holds the firmware only, one snapshot commit per release.
   ```
   SNAP=$(tools/public_snapshot.sh dev --commit "FRASIL X.Y" | awk '/^snapshot/{print $2}')   # local, pushes nothing
   git push origin "${SNAP}:refs/heads/main"
   git tag -a vX.Y "$SNAP" -m "FRASIL X.Y" && git push origin vX.Y
   gh release create vX.Y app/build/frasil-X.Y.fwsc --title "FRASIL X.Y" --notes-file <the CHANGELOG's X.Y section>
   ```
   `tools/public_snapshot.sh` keeps what `tools/public_allowlist.txt` names (the firmware, its tests, docs and
   release tools) and nothing of the websites, the River's service or the apps. The release must be a full release,
   not a pre-release: the hub and GitHub's "latest" take full releases. The site is not published from here.
   A release DELIVERed from the admin's RELEASES (`tools/runner/README.md`, RELEASES) already has its section in
   `CHANGELOG.md` (its RELEASE NOTES): once this GitHub release exists, the runner sets its notes to that text (`gh
   release edit`, outside office hours; it never makes the release, the tag or the snapshot) and then does step 6
   itself. The release's sheet shows each one (site, GitHub, Discord).
6. Announce it on Discord once the GitHub release is live, from the private repository: by hand only for a release not
   DELIVERed from the admin (the runner announces those itself, once). First the dry run, which posts nothing: `python3 tools/discord_announce.py X.Y`. Read the message it
   builds from the `## X.Y` section of `CHANGELOG.md` (the bold leads of New and of Fixes and changes, the links)
   and the length checks. Then `python3 tools/discord_announce.py X.Y --post` sends it to #announcements, a copy
   to the product's news channel (`--product`, default fm-1: #fm1-news; `none`: #announcements alone), and publishes both to the servers following them. `--summary FILE` replaces the description with a Markdown
   file you wrote; `--channel ID` skips the lookup; `--state FILE` keeps each message so a second run never posts
   again. The bot's token is read from `~/.config/frasil/discord-bot.token`.
7. The first time only: ask for the listing (below).

## Getting listed (once)

Write to the FM-1 Firmware Hub's maintainer (the address is on the hub) once the first release is out:

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
