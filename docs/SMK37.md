# FRASIL on the M-VAVE SMK-37

FRASIL runs on the SMK-37: the same instrument as on the FM-1, played from its 37 keys, knobs, faders and wheels.
Install it at your own risk.

The SMK-37 Elite, the SMK-37 Pro and the MKE-P37 are the same keyboard under three names. Each takes its own package,
and M-VAVE's updater refuses another model's:

| Your keyboard | Its package | Its backup's `--model` | Tested |
| --- | --- | --- | --- |
| SMK-37 Elite | `frasil-smk37-elite-X.Y.Z.fwsc` | `elite` | yes |
| SMK-37 Pro | `frasil-smk37-pro-X.Y.Z.fwsc` | `pro` | not yet: tell us if it works |
| MKE-P37 | `frasil-smk37-mke-X.Y.Z.fwsc` | `mke` | not yet: tell us if it works |

The Pro's and the MKE-P37's packages are the Elite's firmware under their own names, not tested on those keyboards
yet. If you try one, tell us how it went on [the issues](https://github.com/styvesgoupil/frasil/issues).

**Back the keyboard up first, once, whichever model it is.** Part of its flash belongs to your unit alone: the
factory calibration of its keys, pads and pedal, its Bluetooth pairings, its key and MAC address. No download can
replace them. FRASIL never writes them, but M-VAVE's own firmware has been seen to erase the calibration when you go
back to it. The backup is what puts it back.

This was tested on macOS. It needs Python 3 and a terminal. Below, `MODEL` is your keyboard's: `elite`, `pro` or
`mke`.

## 1. Get FRASIL and its tools

```sh
git clone --recursive https://github.com/styvesgoupil/frasil
cd frasil
python3 -m venv .venv && .venv/bin/pip install mido python-rtmidi pyusb libusb-package
```

Download your model's package, `frasil-smk37-MODEL-X.Y.Z.fwsc`, from the
[releases](https://github.com/styvesgoupil/frasil/releases) into this folder.

## 2. Back up the keyboard

The keyboard is on M-VAVE's firmware, connected by USB. Close every app that uses its MIDI port (M-Upgrade,
MidiSuite, a DAW, a browser page).

1. **Terminal A, first** (it waits two minutes for the keyboard):
   `sudo .venv/bin/python tools/smk37_backup.py read --model MODEL`
2. **Terminal B:** `.venv/bin/python tools/smk37_backup.py uboot frasil-smk37-MODEL-X.Y.Z.fwsc --port SMK-37`
   (on the MKE-P37: `--port MKE`), then answer `y`. M-VAVE's firmware checks FRASIL's package (nothing is written
   yet) and restarts into the chip's update mode. The keyboard shows up as a USB disk, `WL82 UBOOT1.00`. If your
   computer offers to initialise or format it: **Ignore**.
3. Terminal A reads the whole flash twice, checks the two reads are the same and that the calibration is there, and
   keeps `~/frasil-backups/smk37-MODEL/smk37-MODEL-DATE.bin` with its `.sha256`. Two reads that differ: nothing is
   kept. Switch the keyboard off and on, and start again at 1.
4. Copy the `.bin` and its `.sha256` somewhere else too: another computer, a drive. Never share it: it holds your
   unit's own key.

The reads use felucca-bsp's rescue tool (`base/bsp/tools/fm1_rescue.py`). It loads JieLi's flash loader into the
chip's RAM, downloaded and checked by its SHA-256, so the read needs the internet. Nothing in `tools/smk37_backup.py`
writes the keyboard's flash.

## 3. Install FRASIL

Switch the keyboard off and on. It starts in FRASIL's update loader and waits. Then:

```sh
.venv/bin/python base/tools/fm1_install.py frasil-smk37-MODEL-X.Y.Z.fwsc
```

Don't unplug the cable while it writes. The keyboard restarts on FRASIL: the logo screen shows its version.

FRASIL's first start on the SMK-37 clears what M-VAVE's firmware kept: its settings, its presets and its key mapping.
The backup has them.

**An update later** (FRASIL already on it): download the new `.fwsc` of the same model and run the same command.
Your scenes and presets stay.

## Back to M-VAVE's firmware

Install M-VAVE's V016 for your model with M-Upgrade, M-VAVE's updater (m-vave.com, Downloads). Then check at once
that every key plays its own note, with velocity, and that the pads light and answer. If not, the calibration was
erased: it is in your backup, at `0xC0000..0xC1FFF`. No tool in this repository writes it back yet; ask on
[the issues](https://github.com/styvesgoupil/frasil/issues) before you try anything by hand.

## Versions

Each package is named after its model, its version in it: 3.3.1 is `SMK-37 Elite_4030301`, `SMK-37 Pro_4030301`
and `MKE-P37_4030301` (the FM-1's is `FM-1_47030301`).
