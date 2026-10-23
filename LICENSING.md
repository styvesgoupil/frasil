# Licensing

**FRASIL is MPL-2.0** (full text in [`LICENSE`](LICENSE)), file by file: each source file carries an
`SPDX-License-Identifier: MPL-2.0` header. You may use, change and redistribute it, including the firmware package. If
you distribute a changed file, publish your change to that file under MPL-2.0.

| What | Licence | Where |
| --- | --- | --- |
| FRASIL's code: the app, its DSP, its tools | MPL-2.0, Copyright (C) 2026 Styves Goupil | `app/`, `tools/`, `build.sh` |
| Parts of `app/app.c` and `app/app.h` from fm1-base's template | MPL-2.0, Copyright (C) 2026 Charles Vestal | marked in each file's header |
| The FRASIL name and logo | (C) 2026 Styves Goupil: redistribute them unchanged, as part of FRASIL's source and packages | `app/assets/frasil-logo.png` |
| [fm1-base](https://github.com/charlesvestal/fm1-base), with [felucca-bsp](https://github.com/hugelton/felucca-bsp) | MPL-2.0 (see its `LICENSING.md`) | `base/`, a git submodule |
| Barlow Semi Condensed, Terminus (the screen's fonts) | SIL OFL 1.1 | in fm1-base: `base/assets/fonts/` |
| JieLi AC79 SDK files a package needs (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`) | Apache-2.0 | read from your SDK checkout at build time; never in this repository |

FRASIL is built without Bluetooth or WiFi (`ble = false`), so it links none of JieLi's binary libraries.

**Forks are welcome.** If you publish a changed firmware, give it a name, a logo (`gen` in `app/fm1.toml`) and a
package identity (`id_prefix`) of its own. Players and installers can then tell it apart from FRASIL.
