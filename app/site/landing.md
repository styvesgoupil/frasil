# FRASIL

**Drone textures frozen in motion.** FRASIL turns the M-VAVE FM-1 into an instrument for ambient drones and
textures. It has no sequencer and composes nothing:
the drones are the notes you light, and the arpeggiator plays only the keys you hold. A tap latches a drone, in the key you
chose, and pressing makes it swell. The timbre drifts and ebbs on its own, slowly. Everything flows into a large,
lush reverb with shimmer and freeze. Above the drones, GLINT plays small icy FM bells, by hand or through an
arpeggiator that follows MIDI clock.

![The play page: three drone voices as currents of ice in a river at night, the drone's mix under the waterline](manual/img/screen-play.png "FRASIL on the FM-1's screen")

Version {{VERSION}}, beta. Installing is at your own risk; the installer can put M-VAVE's own firmware back.

- [Try it in the browser](emu/): the same code the FM-1 runs, with sound. No FM-1 needed.
- [Install](install/): from Chrome or Edge, with the FM-1 connected by USB.
- [Manual](manual/): the sound, the screen, the controls, scenes, MIDI.
- [Download {{PKG}}](firmware/{{PKG}}): for the command-line installer, `python3 base/tools/fm1_install.py {{PKG}}`.
- [Source](https://github.com/styvesgoupil/frasil): MPL-2.0. By Styves Goupil. Built on fm1-base and felucca-bsp,
  from Felucca by Leo Kuroshita, Hügelton Instruments.

M-VAVE and FM-1 are trademarks of their owners. FRASIL is not affiliated with M-VAVE.
