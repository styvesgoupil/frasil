# Releasing FRASIL

A release is a GitHub release with the packages attached: `frasil-X.Y.fwsc`, the FM-1's, and from 3.3.1 the M-VAVE
SMK-37's, one for each of its models (the same keyboard): `frasil-smk37-elite-X.Y.fwsc` (tested),
`frasil-smk37-pro-X.Y.fwsc` and `frasil-smk37-mke-X.Y.fwsc` (the Pro's, the MKE-P37's: not tested yet); and
`frasil-X.Y.rfm1`, the FM-1's simulator in WebAssembly for the hosts' apps (RemoteFM1). The FM-1 Firmware Hub
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
| Its card's text, in `fmhub.json` at the repository's root | `fmhub.json`: `what`, `kinds`, `play`, `editor`, `image` |
| The SMK-37 page: a card per firmware, installed from the maker's own page, tested on a real SMK-37 | `fmhub.json`'s `smk37` block (its text, its picture `app/docs/img/smk37/screen-play.png`, its install page `frasilsynth.com/guide/install/?device=smk37`, its guide); a package for each model, `frasil-smk37-elite-X.Y.fwsc`, `frasil-smk37-pro-X.Y.fwsc`, `frasil-smk37-mke-X.Y.fwsc`, identities `SMK-37 Elite_4XXYYZZ`, `SMK-37 Pro_4XXYYZZ`, `MKE-P37_4XXYYZZ` (`id_prefix_smk37` in `app/fm1.toml`: the Elite's name leaves 7 of the 20 characters; M-VAVE's updater takes its own model's alone); the backup first, `tools/smk37_backup.py` (`docs/SMK37.md`) |

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
   The SMK-37 Elite's screens too: `FM1B_BOARD=smk37 FM1B_MODEL=elite ./build.sh --host && tools/screens.sh smk37`.
3. Commit, then build and check: `tools/release.sh`. It runs the host checks, builds `app/build/frasil-X.Y.fwsc`,
   its `.rfm1` (`./build.sh --rfm1`, driven by fm1-base's `tests/rfm1_test.mjs`), then the SMK-37's simulator
   scripts and its three models' packages (`FM1B_MODEL=elite`, `pro`, `mke`; fm1-base names the Pro's
   `frasil-smk37-X.Y.fwsc`, release.sh `frasil-smk37-pro-X.Y.fwsc`), and confirms every identity.
4. Install that package on the FM-1 (`python3 base/tools/fm1_install.py app/build/frasil-X.Y.fwsc`) and play it; the
   SMK-37 Elite's on an SMK-37 Elite backed up (`docs/SMK37.md`).
   The hub also suggests booting it in [fm1-emulator](https://github.com/simonjohansson/fm1-emulator), which runs the
   real package.
5. Publish, from the private repository, outside office hours. This repository's history is private: the public one
   (origin, styvesgoupil/frasil) holds the firmware only, one snapshot commit per release.
   A release DELIVERed from the admin's RELEASES (`tools/runner/README.md`, RELEASES) does this step and the next
   itself, for a firmware release (area FM-1, or none), outside office hours: the snapshot of the merged commit, `main`
   pushed (never forced: refused, it stops and says so), the annotated tag `vX.Y` on it (by FRASIL runner), the
   GitHub release with PREPARE's packages (`frasil-X.Y.fwsc`, the three `frasil-smk37-*-X.Y.fwsc` and
   `frasil-X.Y.rfm1`, each sha256 checked) and the release notes, then the
   site (its status, `X.Y · out now`, is in the same commit as the CHANGELOG's section: the site goes once the GitHub
   release is live), then Discord. Each step is skipped when already done; a GitHub release made by hand gets the notes
   only. The release's sheet shows each one (GitHub, site, Discord). By hand, for a release made without the admin:
   ```
   SNAP=$(tools/public_snapshot.sh dev --commit "FRASIL X.Y" | awk '/^snapshot/{print $2}')   # local, pushes nothing
   git push origin "${SNAP}:refs/heads/main"
   git tag -a vX.Y "$SNAP" -m "FRASIL X.Y" && git push origin vX.Y
   gh release create vX.Y app/build/frasil-X.Y.fwsc app/build/frasil-smk37-{elite,pro,mke}-X.Y.fwsc \
       app/build/frasil-X.Y.rfm1 \
       --title "FRASIL X.Y" \
       --notes-file <the CHANGELOG's X.Y section>
   ```
   `tools/public_snapshot.sh` keeps what `tools/public_allowlist.txt` names (the firmware, its tests, docs and
   release tools) and nothing of the websites, the River's service or the apps. The release must be a full release,
   not a pre-release: the hub and GitHub's "latest" take full releases. By hand, the site's status
   (`website/site.toml`, `status = "X.Y · out now"`) is then committed and the site deployed
   (`tools/site-deploy.sh frasil`).
   FRASIL STREAM's engine goes on air with it: DELIVER puts PREPARE's kept `website/stream/engines/X.Y.wasm` into
   nimbus's e/ through the runner's door (`place-engine`), before the service; the radio's cron publishes it once the
   service is up (card #134). It never fails the release: the door not reached, the minute loop tries again (6 hours,
   then Lukarn and the hand's way, `tools/stream_engine.sh X.Y --go` from the M4 at the release's commit). By hand,
   for a release made without the admin: that same command.
6. Announce it on Discord once the GitHub release is live and the site says so, from the private repository: by hand
   only for a release not DELIVERed from the admin (the runner announces those itself, once). First the dry run, which posts nothing: `python3 tools/discord_announce.py X.Y`. Read the message it
   builds from the `## X.Y` section of `CHANGELOG.md` (the bold leads of New and of Fixes and changes, the links)
   and the length checks. Then `python3 tools/discord_announce.py X.Y --post` sends it to #announcements, a copy
   to the product's news channel (`--product`, default fm-1: #fm1-news; `none`: #announcements alone), and publishes both to the servers following them. `--summary FILE` replaces the description with a Markdown
   file you wrote; `--channel ID` skips the lookup; `--state FILE` keeps each message so a second run never posts
   again. The bot's token is read from `~/.config/frasil/discord-bot.token`.
7. The first time only: ask for the listing (below), and the SMK-37 card once (after 3.3.1).

**Still by hand at a release** (seen at 3.2, 3.3, 3.3.1; not the runner's, each for a reason):
- base/ and base/bsp pushed to their forks (fm1-base, felucca-bsp) before PREPARE, outside office hours (public
  repositories, timestamped); PREPARE says so plainly when a submodule's commit is missing there.
- The guide, the cheat sheets and the books for the version's new features (words, not steps: a session's work).
- A hold's commit (`tools/live.json`): the held work's first commit, as DELIVER ships what changed since its parent.
- Styves's own FM-1 flashed with the released package (his OK on the M4, SEL), and recorded when the bridge's
  first-boot check says "mismatch" while `fm1_install.py --info` says the version runs.
- The iPad app's vendored FRASIL bumped to the release (another repository).
- The one email to the Hub's maintainer, when there is anything to say (a draft for Styves to send; never sent by a
  session).

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

## The SMK-37 page (once, from 3.3.1)

The hub's SMK-37 page (thefirmwarehub.com/smk37/) has a card per firmware, which links to the maker's own install
page: the hub does not install SMK-37 firmwares itself. Its text, picture, install page and guide come from
`fmhub.json`'s `smk37` block (`what`, `image`, `install`, `guide`, no other field), read every three hours. The card
itself is added by the hub's maintainer: the hub's submit form (thefirmwarehub.com/submit/, device SMK-37) or a mail,
once the release is out and it was tested on a real SMK-37 Elite: installed from M-VAVE's V016, and left again. The
Pro's and the MKE-P37's packages are on the release too, not tested yet: the hub's card says which models it has.
