# FRASIL

*[English](README.md) · Français*

**Des textures de drone figées en mouvement.** Un firmware sur mesure qui transforme le M-VAVE FM-1 en instrument
pour drones et textures ambient.

<p align="center">
  <img src="docs/img/screen-logo.png" width="240" alt="L'écran-logo de FRASIL : le nom au-dessus de granules de glace à la dérive, par Styves Goupil">
  <img src="docs/img/screen-play.png" width="240" alt="La page de jeu : le nom de la scène dans l'en-tête, trois voix de drone en courants de glace dans une rivière la nuit, le mélange du drone en dessous, en quatre strates">
  <img src="docs/img/screen-glints.png" width="240" alt="GLINT : des étincelles FM glacées qui brillent au-dessus du drone">
</p>

*Le frasil : des cristaux de glace qui se forment dans une eau vive et libre (sur le fleuve Saint-Laurent, entre
autres). Ils restent en suspension dans le courant : granuleux, à la dérive, jamais immobiles.*

Les drones sont les notes que tu allumes, et elles ne changent jamais. L'arpégiateur de GLINT joue les touches que tu
tiens, son séquenceur les pas que tu écris; le hasard n'entre que là où tu le laisses entrer (CHANCE fait sauter des
pas, RANDOM et DRIFT choisissent l'ordre de l'arpège, SPARKLE fait tomber des glints sur les notes de tes drones). Un tap
latche un drone : une note, ou un accord construit dans la gamme que tu as choisie. Appuyer plus fort le fait
gonfler. Le timbre dérive et reflue de lui-même, tout doucement; les notes des drones, elles, ne changent jamais. Le tout se déverse dans une grande
réverb luxuriante, avec shimmer et freeze. Par-dessus les drones, tu peux jouer GLINT, de petites cloches FM glacées,
à la main, avec un arpégiateur ou un séquenceur de 32 pas qui suivent l'horloge MIDI. Sous eux, BASS tient une note
grave.

FRASIL est pensé comme une voix dans un setup plus grand. Il reçoit les notes MIDI, la pression, les CC et l'horloge
par USB et TRS.

> **FRASIL 1.0.** Installe-le à tes risques : l'installateur peut toujours remettre le firmware de M-VAVE. FRASIL
> n'est pas affilié à M-VAVE.

## Installer

- **Depuis le navigateur :** Chrome ou Edge, avec le FM-1 branché en USB. Utilise [The FM-1 Firmware Hub](https://thefirmwarehub.com/)
  quand FRASIL y sera listé.
- **Depuis un terminal :** télécharge `frasil-X.Y.fwsc` depuis les [versions](https://github.com/styvesgoupil/frasil/releases),
  puis lance `python3 base/tools/fm1_install.py frasil-X.Y.fwsc` depuis un clone de ce dépôt.
- **Retour au firmware officiel :** installe le V15 de M-VAVE depuis le hub. Si le FM-1 ne démarre plus, tiens OCT−
  enfoncé pendant que tu l'allumes : il attend alors une mise à jour par USB.

Identifiant du paquet : `FM-1_47XXYYZZ`, avec sa version : la 1.0 est `FM-1_47010000`. La version est dans `VERSION`.

### Où sont gardées tes scènes

FRASIL garde ce que tu sauvegardes dans la flash du FM-1, hors du firmware lui-même, dans la zone qu'utilise la
famille de firmwares de Felucca. Il n'a pas de zone à lui :

| Quoi | Où | Comment |
| --- | --- | --- |
| Les 16 scènes | `0xDA000` | deux secteurs de 4 Kio, écrits à tour de rôle, chaque enregistrement avec un CRC |
| Les réglages : le son au démarrage, les canaux MIDI, le tempo, MORPH et KEEP, le métronome, les CC appris | `0xFC000` | pareil, dans la paire de secteurs où Felucca garde ses réglages globaux |
| La calibration du panneau | `0xDC000` | dans le format de Felucca, pour qu'elle reste valide de l'un à l'autre |

Quand tu changes de firmware :

- **FRASIL ne lit que ses propres enregistrements.** Il ne lit pas les projets ni les samples d'un autre firmware.
  Tout le reste, à ces adresses, est ignoré : FRASIL repart alors avec ses scènes d'usine et ses réglages par défaut.
- **Sauvegarder peut écraser les données d'un autre firmware** : les réglages globaux de Felucca, et une partie de la
  zone où Felucca garde ses projets et ses samples. Fais une copie de sauvegarde du firmware d'où tu viens avant de
  sauvegarder dans FRASIL.
- **Un autre firmware peut écraser les scènes de FRASIL.** Mettre à jour ou installer FRASIL n'écrit que la zone du
  firmware (le chargeur de mise à jour de felucca-bsp) : tes scènes restent. Ce que fait l'installation d'un autre
  firmware dépend de son propre chargeur. Le V15 de M-VAVE écrit son image au complet : pars du principe que tes
  scènes n'y survivront pas.
- **Il n'y a pas encore de copie de sauvegarde des scènes de FRASIL.** Les sauvegarder dans un fichier et les
  restaurer, par USB depuis le navigateur, est prévu. D'ici là, note ce qui compte pour toi avant de changer.

## Le son

Chaque voix de drone (jusqu'à 8) a trois couches. Elles jouent toutes les mêmes notes; tu les mélanges et tu façonnes
chacune d'elles :

| Couche | Ce que c'est |
| --- | --- |
| **CHOIR** | trois dents de scie (ou impulsions) désaccordées passées dans un filtre qui suit la note : passe-bas, passe-bande ou passe-haut, résonant |
| **PARTIALS** | 8 harmoniques additives, chacune dérivant de son côté en niveau et en accord; du brillant au sombre, impaires, étirées vers la cloche, battantes, avec un formant qui peut isoler une seule harmonique |
| **BREATH** | du bruit ou des grains épars passés dans trois résonateurs accordés sur la note, de l'air (Q bas) au verre (Q élevé) |

Chaque couche a une deuxième page dans EDIT pour sa source et ce qui la façonne ensuite (CHOIR II, PARTIALS II,
BREATH II), et DRONE PARAMS façonne tout le drone (TIMBRE, SPACE, MOTION, FULLNESS). PRESETS fait défiler 24 presets
de drone (MIST, CATHEDRAL, GLACIER, ABYSS, AURORA, EMBER, TIDE, DUST, WIND, ORGAN, HOLLOW, VOWEL, SHIMMER, CHANTER,
ENSEMBLE, BELLS, NOCTURNE, HAZE, PULSE, RIME, DEEP, SPECTRAL, RUST, STILL) : touche un réglage et le drone devient
CUSTOM, qu'un autre preset ne fait pas perdre. ALGORITHM amincit le drone (FULLNESS), peu importe le preset. Trois
**LFO** (sur LFO, après DRONE MOTION) font bouger chacun jusqu'à quatre réglages, en vitesse libre ou sur l'horloge
MIDI ou TEMPO.

Ensuite, WARMTH (saturation douce) alimente **CLOUD**, la réverb : un réseau de délais à rétroaction modulé à 8 lignes,
avec de la diffusion dans sa boucle, un pré-délai, un **shimmer** transposé dans la boucle, et **FREEZE**. La queue va
de 0,4 à 60 secondes.

**GLINT** se pose par-dessus : 4 voix de FM propre à deux opérateurs, jamais latchées. Il a 22 presets (ICE, BELL, BLIP,
GLASS, CHIME, DROP, FROST, CELESTA, SINE, KALIMBA, MARIMBA, MUSIC BOX, VIBES, HARP, PIPE, BOWED, et les leads WARM
LEAD, BRASS, SQUARE, SUB LEAD, STRINGS, REED : SUSTAIN les tient tant que la touche est enfoncée). Son TYPE choisit
comment les touches le jouent : chaque touche son glint, son propre arpégiateur (UP, DOWN, UPDOWN, RANDOM, DRIFT, avec
latch et CHANCE), ou un séquenceur de 32 pas (écrit pas à pas ou en direct, REC y entre tout droit, avec accents et
liaisons, transposé dans la gamme par les touches, un par scène). OCTAVE déplace l'arpège et la séquence pendant
qu'ils jouent. Il a un délai synchronisé au tempo, mono ou ping-pong. La pression rend un glint plus fort et plus long,
et avec VIBRATO, un tremblement du doigt infléchit un peu sa hauteur (une pression stable reste juste).

**BASS** se glisse en dessous : une seule note grave à la fois, parmi huit MODELs (SINE, TRIANGLE, SQUARE, SAW, REESE,
SUB, ORGAN, FM), DRIVE dans un écrêtage doux, un passe-bas résonant (FILTER, RESONANCE), avec un glissando d'une note à
l'autre. Elle est tenue comme un drone et reste hors de la réverb.

**GENERATIVE FX** (la première page de FX) agit sur tout à la fois : SWELL fait gonfler toutes les voix et les y
tient, SPARKLE fait briller des glints tout seuls sur les notes des drones, BLUR fond le drone dans le nuage, SINK
plonge tout sous un passe-bas.

## Jouer

| Contrôle | Ce qu'il fait |
| --- | --- |
| **Touches** | tap : allume un drone (il monte lentement et reste). Tape-le de nouveau : il s'estompe. Tiens et appuie : il gonfle et s'ouvre |
| **SELECT / PRESETS / ALGORITHM** (A B C) | KEYBOARD, ce que jouent les touches (DRONE, GLINT, BASS) / le PRESET du drone (24, et ton CUSTOM) / FULLNESS, sur toutes les pages |
| **K1–K4** | les quatre réglages de la page à l'écran |
| **REC** | dans le séquenceur de GLINT, en enregistrant LIVE (les touches sur GLINT, la séquence lancée); encore une fois : RECORD OFF, les touches reviennent à ce qu'elles jouaient avant REC. REC clignote sur le temps tant qu'il enregistre, avec un métronome; REC tenu l'éteint ou le rallume |
| **ARP** | les pages de GLINT (tap de nouveau pour la suivante : GLINT PARAMS, II, III, DELAY, puis GLINT ARP ou celles du séquenceur selon TYPE). Tenu : LATCH de l'arpège |
| **SEQ** | les pages de BASS (tap de nouveau pour la suivante). Tenu : SWELL, toutes les voix gonflent ensemble |
| **SEL** | HARMONY : fondamentale, gamme, accord, voicing. Tiens SEL et tape une touche : une note seule par-dessus un accord |
| **EDIT, ENV, LFO, FX, GLO** | DRONE PARAMS et les LAYERS (×7 : chaque couche et sa deuxième page), DRONE SHAPE, DRONE MOTION et les trois LFO (×7), GENERATIVE FX, CLOUD (×3) et PUMP (×2), GLOBAL, MIDI et MASTERING. EDIT tenu : COMPARE, la scène telle que sauvée. FX tenu : FREEZE. LFO tenu en tournant K1–K4 : ce réglage sur un LFO. GLO tenu en bougeant un CC : MIDI learn |
| **HOME** | la page de jeu; encore une fois : le MIXER; encore une fois : l'écran du logo, vivant. Tenu : l'aide à l'écran (5 pages, tourne SELECT) |
| **SAVE** | SCENES (K1 l'emplacement, MORPH, KEEP, UNDO); encore une fois : retour. SAVE tenu + REC : STORE. SAVE tenu + une touche blanche : charger cette scène |
| **PLAY** | le transport de GLINT : la séquence s'arrête ou repart du début, l'arpège attend ou reprend; sur SCENES, charger. Tenu : RELEASE ALL; tenu de nouveau : RESTORE |
| **FREEZE** (FX tenu, ou K4 de CLOUD) | le nuage retient ce qu'il contient, plus rien n'y entre (FX vif entre-temps) |
| **OCT− / OCT+** | octave, de −3 à +2 (les deux : retour à −1). Ils clignotent sur le temps |

Un appui montre, un appui tenu agit : chaque bouton agit quand tu le relâches (OCT tout de suite); tenu, il fait autre
chose.

À l'écran, chaque réglage porte le badge de son contrôle : un disque bleu de 1 à 4 pour K1–K4, un disque orange A, B ou
C pour les trois encodeurs de gauche. Sur la page de jeu, chaque couche a son élément dans la rivière : le courant,
c'est CHOIR, les cristaux, c'est PARTIALS, la brume, c'est BREATH; la basse, c'est une lente houle sur toute la rivière,
l'eau plus sombre en dessous. Les quatre strates sous la ligne d'eau montrent le mélange exact, WARMTH avec lui.
L'en-tête nomme la scène, avec un point ambre dès que le son a changé, et des points sous le titre de chaque page
disent où elle est parmi les pages de son bouton. En haut à droite, un badge montre ce que jouent les touches
(SELECT) et l'octave : un petit clavier et DRONE en blanc, BASS en orange, ou en bleu ce que font les touches de GLINT
(GLINT, ARP, SEQ; REC en orange). Son contour est plein quand les notes sont latchées, pointillé quand elles ne
sonnent que tant que la touche est tenue.

**Harmonie.** 12 gammes (chromatique, les sept modes, mineure harmonique et mineure mélodique, les deux pentatoniques).
Accords construits dans la gamme (SINGLE, FIFTH, TRIAD, SEVENTH, SUS2, SUS4, ADD9, QUARTAL) ou de type fixe (MAJ, MIN,
MAJ7, MIN7, DOM7, AUG, DIM). Voicings : CLOSE, OPEN, WIDE. Une touche hors gamme joue la note de la gamme juste en
dessous; les touches de la gamme s'allument.

**Arpégiateur.** Avec TYPE sur ARP, il joue les touches tenues de GLINT une à une, à RATE, sur RANGE octaves. Il suit
l'horloge MIDI, ou TEMPO quand aucune horloge n'entre. Un nouvel accord prend effet au prochain pas de RATE, ce qui
garde le motif sur la grille. Un Start MIDI remet la grille à zéro. Avec LATCH activé (par défaut), le motif continue
après que tu as levé les mains. La touche dont la note sonne s'allume aussi longtemps que dure la note.

**Basse.** Avec les touches sur BASS, un tap tient une note : la note de la touche dans la gamme, une octave plus bas
(OCTAVE, sur BASS II : une ou deux). Tape une autre touche et la basse glisse vers elle; tape sa propre touche et elle
lâche. Appuyer dedans la rend plus forte et ouvre son filtre. Avec KEYS sur HOLD (GLOBAL), elle sonne tant que sa touche
est tenue.

**Mixer, pump, mastering.** Le deuxième écran de HOME est un MIXER : un fader et un vu-mètre en direct pour DRONE,
BASS, GLINT et CLOUD, le retour de la réverb. PUMP (FX) fait plonger chaque piste sur le temps, comme le ferait un
sidechain, de 4 mesures au 1/8, synchronisé à l'horloge MIDI ou à TEMPO; sa remontée, son attaque et son décalage
sont sur PUMP II. MASTERING (GLO) est un compresseur léger sur
la sortie, avec sa réduction de gain à l'écran.

**Scènes.** 16 emplacements, dont 8 scènes d'usine (MIST, GLASS, ABYSS, CHORALE, WIND, BELLS, TIDE, AURORA). Une scène
contient le son, les drones latchés, l'arpège et la séquence de GLINT, et la basse. SAVE tenu + REC sauvegarde; SAVE
tenu + une touche blanche charge l'une des 16, comme PLAY sur SCENES ou un Program Change MIDI. MORPH fait glisser le
son vers la scène (jusqu'à deux minutes) ou le change d'un coup (CUT); KEEP sur SCENE relâche tout et joue les notes
de la scène, sur PLAYING ne change que le son. Tiens EDIT pour entendre la scène telle que sauvée (COMPARE); SAVE
s'allume tant que le son a changé. Les scènes sauvegardées par un FRASIL antérieur se chargent telles quelles, les
réglages plus récents à leur valeur par défaut.

Le manuel complet est [app/docs/MANUAL.md](app/docs/MANUAL.md). Un aide-mémoire d'une page (en français) est
[docs/cheatsheet.html](docs/cheatsheet.html).

### MIDI (USB et TRS)

Chaque piste a son propre canal, peu importe ce que jouent les touches : les drones sur 1, GLINT sur 2, BASS sur 3 par
défaut (la deuxième page de GLO, MIDI : OFF, 1–16 ou OMNI pour chacune). Les notes des drones passent par l'harmonie,
donc la note unique d'un séquenceur joue un accord; le sustain (CC 64) les retient. Les notes de GLINT et de BASS
jouent telles quelles, sans gamme ni octave ajoutée (celles de GLINT vont dans son arpégiateur ou son séquenceur quand l'un d'eux est actif); la
basse est mono, la dernière note l'emporte. Les notes sonnent seulement tant qu'elles sont tenues. L'aftertouch
polyphonique fait gonfler une note; la pression de canal fait gonfler toute la piste. Les CC du son arrivent sur le canal
des drones; 120 et 123 relâchent les notes de la piste dont c'est le canal. Un Program Change de 0 à 15 sur le canal
des drones charge les scènes 1 à 16.

| CC | | CC | |
| --- | --- | --- | --- |
| 1 | MOTION | 74 | TIMBRE |
| 91 | SPACE | 11 | SWELL |
| 70 · 71 · 72 | CHOIR · PARTIALS · BREATH | 73 · 75 | ATTACK · RELEASE |
| 76 · 77 | SIZE · DECAY | 78 · 79 | SHIMMER · WARMTH |
| 12 · 13 | FULLNESS · le PRESET du drone | 14 · 15 · 16 | SPARKLE · BLUR · SINK |
| 17 · 18 · 19 | les faders DRONE · GLINT · BASS | 80 | FREEZE (≥ 64) |
| 120 · 123 | relâcher les notes de la piste de ce canal | | |

MIDI learn : tourne un réglage, tiens GLO et bouge un bouton du contrôleur (un CC sur le canal des drones) : ce CC le
tourne désormais. Jusqu'à 8, gardés avec les réglages; un CC appris passe avant ceux du tableau, sauf 64, 120 et 123.

L'horloge (`F8`) pilote l'arpégiateur, le séquenceur et le délai de GLINT, et Start (`FA`) remet la grille à zéro.

## Compiler à partir des sources

```sh
git clone --recursive https://github.com/styvesgoupil/frasil
cd frasil
python3 -m venv .venv && .venv/bin/pip install Pillow fonttools mido python-rtmidi numpy scipy matplotlib
brew install libraqm                     # les polices de l'écran (macOS)

./build.sh --host                        # le simulateur hôte : app/build/host/frasil_host
app/build/host/frasil_host app/tests/basic.txt app/build/host      # les vérifications
app/build/host/frasil_host app/tests/demo.txt renders              # une performance, en fichiers WAV
./build.sh --web                         # l'émulateur navigateur : app/build/emu/ (Emscripten)
./build.sh                               # le firmware : app/build/frasil-VERSION.fwsc (la version dans VERSION)
```

La compilation du firmware demande la chaîne d'outils Linux de JieLi et le SDK AC79, exécutés dans Docker (OrbStack sur
macOS). Consulte [le README de fm1-base](https://github.com/charlesvestal/fm1-base#readme) pour la mise en place.
`tools/screens.sh` génère les captures d'écran. `tools/bench/` mesure le DSP : rendus de test et nombre d'instructions
comparés au budget du FM-1.

| Chemin | Contenu |
| --- | --- |
| `app/` | le firmware : `app.c` (contrôles, MIDI, scènes), `ui.c` (l'écran), `params.c`, `harmony.c` |
| `app/dsp/` | le son : `frasil.c` (voix), `glint.c` (GLINT et son délai), `bass.c` (BASS), `nuage.c` (CLOUD, la réverb) |
| `app/docs/`, `app/site/` | le manuel et la page d'accueil du site (`base/tools/make_site.py`) |
| `base/` | [fm1-base](https://github.com/charlesvestal/fm1-base), la plateforme (sous-module git) |
| `tools/` | captures d'écran, banc d'essai du DSP, script de publication |

Publication des versions : voir [RELEASING.md](RELEASING.md).

## Licence et crédits

FRASIL est un logiciel libre sous la [Mozilla Public License 2.0](LICENSE) : voir [LICENSING.md](LICENSING.md).

- Par Styves Goupil.
- Construit sur [fm1-base](https://github.com/charlesvestal/fm1-base) de Charles Vestal et
  [felucca-bsp](https://github.com/hugelton/felucca-bsp), issu de [Felucca](https://github.com/hugelton/Felucca) de
  Leo Kuroshita (Hügelton Instruments).
- Polices : Barlow (The Barlow Project Authors) et Terminus (Dimitar Toshkov Zhekov), SIL OFL 1.1, via fm1-base.

M-VAVE et FM-1 sont des marques de commerce de leurs propriétaires.
