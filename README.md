# ROTOR RAIDER

**An Amiga helicopter strike game by Paul Ross Seibert.**

**© 2026 Desert Palms Software.**

<p align="center"><img src="art/dps_logo_amiga.png" alt="Desert Palms Software" width="288"></p>

<p align="center"><img src="demo/rotor-raider.gif" alt="Rotor Raider: off the carrier, across the sea, bombing a war factory" width="480"></p>

<p align="center"><b><a href="demo/rotor-raider.mp4">▶ Watch the gameplay video (with sound)</a></b></p>

The Iron Combine has turned three island chains into war-factory theatres.
Fly the gunship *Kestrel* off the carrier DPS *Mirage*, which steams a slow
patrol across each theatre, and bomb the six factories before the Combine
overwhelms you. Its AA turrets throw flak, and homing missiles once half the
factories are down. Radar stations send interceptors after you. Bombers go
for your carrier. Supply boats rebuild damaged factories. In the shipyard a
cruiser is being built, and when it sails it hunts the carrier.

Three theatres, each harder than the last: **Coral Reach**, **Ember Flats**
and **Frostline**.

## Download

| File | What it is |
|---|---|
| [**rotor-raider.adf**](https://github.com/PaulSeibert/rotor-raider/releases/latest/download/rotor-raider.adf) | The game disk: a bootable Amiga floppy image. This is all you need to play. |
| [rotor-raider-1.0.zip](https://github.com/PaulSeibert/rotor-raider/releases/latest/download/rotor-raider-1.0.zip) | The full release: the disk, the program for hard-disk installs, a README and the demo GIF |
| [rotor-raider-demo.mp4](https://github.com/PaulSeibert/rotor-raider/releases/latest/download/rotor-raider-demo.mp4) | The gameplay video |

All versions are on the [Releases page](https://github.com/PaulSeibert/rotor-raider/releases).

**Requirements:** an Amiga 500 with **1 MB of RAM** (512K chip + 512K
expansion), Kickstart 1.3 or later, PAL. It runs at 50 frames a second on a
stock 7 MHz 68000 with the original OCS chips. It's tested on Kickstart 1.3,
Kickstart 3.1 and the free AROS Kickstart replacement.

## Playing it

The ADF boots by itself, just like putting the floppy in a real Amiga.

- **WinUAE** (Windows) or **FS-UAE** (Windows, macOS, Linux): an Amiga 500
  configuration with 512K chip + 512K slow RAM and PAL, `rotor-raider.adf`
  in drive DF0:. Without a Kickstart ROM, both emulators can use their
  built-in AROS ROM, which the game runs on. Kickstart ROMs are sold by
  Cloanto in Amiga Forever.
- **THEA500 Mini:** copy `rotor-raider.adf` to a FAT32 USB stick and pick it
  from the USB menu. Choose the A500 setup and PAL if it asks.
- **Amiberry** (Raspberry Pi and others): an A500 configuration, with the
  ADF in DF0:.
- **A real Amiga:** write the ADF to a floppy (with a Greaseweazle, or
  ADF-Copy), or copy it to a Gotek drive.

In an emulator, map your joystick or gamepad to port 2. Or play on the
keyboard: arrow keys, Space and B.

## Controls

Choose the steering on the title screen with left/right.

| Input | Classic steering | Arcade steering |
|---|---|---|
| Stick | Left/right turn the gunship, up flies forward, down brakes | Push where you want to go: it turns that way and flies |
| Fire | The chin gun (it strikes the ground ahead, and hits planes) | same |
| Hold fire (0.3 s) | Drop a bomb | same |
| Second button (or B) | Drop a bomb at once; then holding fire fires the gun continuously | same |

Keys: arrows, Space = fire, B = bomb, P = pause, M = music on/off, Esc = quit.

**Bombs** fall ahead of you in an arc: the blinking marker on the ground
shows where. You carry 8. **Land** on the carrier's deck (or the friendly
airstrip's helipad) by hovering slow and still over it: the gunship sets
down, rides with the ship, repairs and reloads. Any push takes off again.

## Difficulty

Press **Ctrl+L**, then a number key within two seconds. The setting shows on
the score bar (SK3) and stays until you change it.

| Key | Setting |
|---|---|
| 1 | Easiest: slower, less accurate flak, fewer and later planes |
| 2 | Easy |
| 3 | **Normal** (the default) |
| 4 | Hard |
| 5 | Hardest |
| 0 | **GODMODE**: nothing can destroy the gunship (hits still show). For testing; it doesn't set the hi-score. In GODMODE, **Ctrl+L then 9** jumps to the next standing factory. |

## How to play

- A factory takes three bombs. The gun doesn't hurt it, but it does destroy
  AA turrets and radar stations, and sinks supply boats.
- A damaged factory slowly repairs itself, and every supply boat that reaches
  its pier repairs it more. Sink the boats, finish what you start.
- Destroy the radar stations and the interceptors stop finding you.
- Bomb the shipyard to set the cruiser back. If it sails, sink it (it takes
  five bombs) before it sinks the carrier.
- If the carrier sinks, the airstrip is your only place to land.
- Damage shows on the HUD bar; at 100% the gunship goes down. You have four.
- The HUD shows the score, gunships left, damage, bombs, the carrier's
  strength, the shipyard's progress, factories left and a map of the whole
  theatre (red: factories, yellow: the carrier, white: you, orange: planes).

## What the Amiga is doing

- **Two playfields.** The terrain is an 8-way scrolling playfield of 16×16
  tiles: a ring of bitplane memory where moving the camera only moves the
  start pointer (rows wrap into each other horizontally, and the copper
  points the planes back at the ring's start where the picture reaches its
  end). New tiles are blitted into the margin before they come into view.
  Everything else (the carrier, planes, ships, flak, bombs, explosions, the
  shadow) is drawn by the blitter on the second playfield, so nothing has to
  restore the terrain.
- **The copper.** Each band of islands has its own eight colours, loaded
  where the band starts on screen, always in open sea, so the changes never
  show: about three times the colours on screen. The wave crests glitter,
  dusk falls over the theatre as the battle goes on, and the HUD is a
  separate screen under the play area.
- **Sprites.** The gunship is a 32×32, 15-colour attached hardware sprite in
  16 headings with a turning rotor, its shadow on the ground showing its
  height.
- **The Desert Palms Software logo** at boot: 16 colours with 4 reloaded on
  every line, about 320 colours in one picture, faded in and out by the
  copper.
- **Paula.** A looped rotor chop on its own channel, pitched by your speed;
  sound effects on two more; a sampled march on the title.
- **The floppy.** The three theatres, the logo and all the blitter objects
  are packed on the disk and loaded when needed, so the program stays small
  enough for a 1 MB A500.

## Building

The game is written in C, with the Amiga Game Kit (`agk`) and the
[ACE](https://github.com/AmigaPorts/ACE) engine. `src/logic.c` holds the game
rules and `src/enemies.c` the enemy's (portable C, unit-tested on a PC),
`src/theatre.c` reads a theatre, `src/scroll.c` is the scroller, `src/main.c`
the rest of the Amiga side, and `src/splash.c` the logo screen.

```sh
agk unit      # game rules on the host (tests/unit)
agk build     # build/rotorraider.adf, a bootable floppy
agk test      # emulator tests on A500 (KS 1.3, KS 3.1, AROS)
agk release && python3 tools/finish_release.py   # the zip for Aminet / itch.io (build/release/)
agk record demo/showcase.agk -o build/demo.mp4 --readme --gif demo/rotor-raider.gif --gif-start 8.5 --gif-seconds 12
cp build/demo.mp4 demo/rotor-raider.mp4
```

Everything is made by scripts, so every asset can be regenerated from source:
`art/tools/tiles.py` (the terrain tiles), `art/tools/theatres.py` (the three
theatres: islands from a seed, structures placed by rules), `art/tools/sprites.py`
(the gunship and everything else that moves, rendered from simple models at
every heading), `art/tools/make_dps_logo.py` (the logo) and
`sound/tools/make_samples.py` (the instruments).

## Credits

**Rotor Raider** © 2026 Desert Palms Software. All rights reserved.

Design, code, graphics and music: **Paul Ross Seibert**.
An original game, inspired by the helicopter strike games of the 1980s.

Built with:
- the [ACE](https://github.com/AmigaPorts/ACE) game engine, including its
  ProTracker player (Mozilla Public License 2.0)
- the Amiga Game Kit runtime (MIT License)
