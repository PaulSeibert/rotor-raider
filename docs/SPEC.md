# Rotor Raider: game spec (retroactive)

Written after the fact, on 2026-11-11, from the code at `main` (2804254, "Rotor Raider 1.4:
version, README"), the repo's tests and commit messages, and the README's pacing measurements. It
describes the game as it is: where a Desert Palms playbook target isn't met, the section says
**Gap vs playbook** in plain words. It does not change the game. From now on it is the contract
everyone works from: change the design, change this file in the same commit.

Sources, used throughout: `src/logic.h` / `src/logic.c` (tuning, flight, weapons, flow),
`src/enemies.c` (the Iron Combine), `src/main.c` (display, sound, input, text, HUD),
`sound/sound.toml`, `art/art.toml`, `art/palette.txt`, `art/maps/theatre*.tmj`, `agk.toml`,
`tests/*.agk`, `tests/unit/test_logic.c`, `README.md` (the pacing table, from `tools/pacing.agk`
and `tools/pace.py`) and the art pilot's `~/art-pilot/rotorraider/STYLE.md` (approved 2026-11-11).
Line references like `enemies.c:236` are at 2804254. "Derived" means worked out from the code's
tables, not measured in a run. "Landing" means in flight on another branch when this was written
(section 12 lists them).

## 1. Pitch

You fly the gunship *Kestrel* off the carrier DPS *Mirage*, which steams a slow patrol across an
island theatre, and bomb the Iron Combine's six war factories before it overwhelms you. Sandbagged
AA guns turn to track you and throw flak (homing missiles once enough factories are down); radar
stations send interceptor jets after you; bombers go for your carrier; supply boats rebuild the
factories you damaged; and in the shipyard a cruiser is being built, which hunts the carrier when it
sails. Land slow and still on the deck to repair and reload. Three theatres, each harder
(Coral Reach, Ember Flats, Frostline). A top-down, open-map helicopter strike game in the 1980s
tradition, built from scratch: our own code, art, title march and sound; two credited modules
(Songerson, CC BY) on machines with 1 MB of chip RAM.

**Pillars**
1. **You pick the fight:** an open map. Flights between targets are quiet by design; at a factory
   the turrets put 2-4 threats up.
2. **Momentum is the feel:** classic steering glides and arcs (turning swings the nose and gun
   without changing where you go); arcade steering is snappy. The player chooses on the title.
3. **Every gun warns:** each AA round has a tell (16 frames on normal) and the deck is always safe
   from guns.

**Not:** a scrolling shooter with waves; a game with a boss per level (the cruiser is one set
piece a theatre); a game with an attract loop or a save (neither exists).

## 2. Target machine

| machine | what it gets |
|---|---|
| **A500 OCS, PAL 50 Hz, Kickstart 1.3** | the whole game. README: **1 MB (512K chip + 512K slow)**; the program is ~340 KB, the theatres load from the floppy as needed. 50 fps; the A500 fight averages 45-55% load. Title music: our own march (`theme.mml`). |
| 1 MB **chip** (A500+, A600, A1200) | Songerson's *dog68* (title) and *dog70* (in the air) from the floppy into chip RAM at boot, **only if 64 KB of chip stays free afterwards**; a title credits page; the rotor then borrows channel 0 (section 9). |
| A1200 AGA | nothing extra: the OCS picture. Tested as profile `a1200` only. **Gap vs playbook** (one visible A1200 upgrade per release). |
| Kickstart 3.1, AROS ROM | tested profiles `a500-ks31`, `a500-aros`: the same game. |

**Gap vs playbook:** the playbook's master is a 512K A500. The game needs the 1 MB expansion
(README); whether it boots on 512K total is not tested (open question 7). 512K-**chip** A500s
play the whole game with the original title march and no music in the air.

## 3. Core loop and feel

**Controls** (title: left/right chooses the steering; fire, released first, starts)

| input | classic steering | arcade steering |
|---|---|---|
| stick left / right | turn the nose one of 16 headings every 4 frames | push where you want to go: it turns that way and flies |
| stick up | thrust along the nose | (with a direction) fly |
| stick down | brake, then back off tail first (let go to hover) | (a direction) |
| fire (one button) | tap: the chin gun; hold 15 frames: a bomb | the same |
| second button, or B | a bomb at once; then holding fire autofires the gun every 7 frames (the game remembers a two-button pad) | the same |
| keys | arrows, Space fire, B bomb, P pause, M music, Esc quit | |
| Ctrl+L, then 0-5 | difficulty (section 7); Ctrl+L, 9 in GODMODE: jump to the next standing factory | |

**Feel in numbers** (`logic.h:285-304`, `logic.c:591-678`)
- `V_MAX` 40 = 2.5 px/frame in every direction (west is as fast as east).
- Classic: thrust closes 1/16 of the gap to full speed along the nose each frame (about 45 frames
  to 95%), so turning under thrust carves a wide arc; with the stick off the gunship glides on,
  losing 1/32 of its speed a frame (1 px/16 every 4 frames when slow): about 2.5 s to rest; brake
  1/8 a frame; reverse up to 0.8 px/frame, reached like thrust.
- Arcade: accel 2/16 px a frame squared (20 frames to full speed), drag 1 (40 frames to rest).
- Turn: 4 frames per heading step, 16 headings (a full circle in 64 frames). Facing (the nose, and the
  gun) is separate from moving in classic steering.
- Altitude `ALT_CRUISE` 24: the shadow's offset shows height. Cannon: 5 px/frame plus the gunship's
  velocity, its altitude dropping 2 a frame: it strikes the ground 60 px ahead after 12 frames, hits
  planes in the first 8 frames (altitude >= 8) and ground targets in the last 4 (altitude <= 6); 4
  shots live. Bombs: 8 carried, 2 falling at once, they leave at the gunship's speed plus 1 px/frame
  and fall for the altitude in frames (24 from cruise); the blinking reticle on the ground shows
  where. Blast radius 14 px. A bomb needs altitude >= 8 and the gunship in the air.
- Landing: slower than 4/16 px a frame, over the carrier's deck (+-36 px across, +-9 high) or the
  airstrip's helipad (+-10), for 25 frames; then it descends 1 a frame and rides with the ship. Any
  stick push takes off. On deck: 1 point of damage repaired every 10 frames (a full 100 in 1000 frames,
  20 s), 1 bomb loaded every 40 frames (8 in 320 frames).
- Camera: follows the gunship's position, closing 1/8 of the gap a frame (at least 1 px, at most
  8, `CAM_MAX_STEP`: the scroller draws a tile row a frame); no look-ahead (`CAM_LEAD` 0). The title pans slowly east and
  back.

**Hitbox (hero syndrome)**
- The gunship sprite is 32 x 32. Hits are by distance from its centre, not boxes: a flak burst hurts
  inside 14 px (`dist` is the longer side plus half the shorter: an octagon about 51% of the sprite's
  area), a missile inside 9 px (about 21%), a jet's ram inside 14 px and above altitude 12. **Gap vs
  playbook** (25-50%): flak is at the top edge of the range; missiles and the ram are generous.
- Flak hits only where it **bursts**, at the aimed point after its flight; passing through a shell
  does nothing. Damage is a bar: flak 8, a missile 18, a jet ram 30 (of 100) on normal; at 100 the
  gunship goes down: about 13 flak hits or 6 missiles. No invulnerability frames after a hit.
- A landed gunship is never a target for guns, flak or missiles. A sunk carrier with the gunship on it
  destroys the gunship.

**Fun at three scales**
- 10 s: weave through the flak after a tell, line the reticle on a factory, drop a bomb.
- 1 min: a factory: approach, silence the turrets with the gun, three bombs, away before the boats
  arrive.
- 10 min: a theatre (six factories, two to three minutes of play when it goes well), then a harder
  one.

**Scoring** (`logic.c`, `enemies.c`)
| event | points |
|---|---|
| factory destroyed | 1000 |
| radar station | 250 |
| AA turret | 100 |
| interceptor jet / bomber / supply boat | 200 / 300 / 150 |
| bomb on the shipyard (while the cruiser has not sailed) | 50 |
| cruiser sunk | 3000, and the carrier is repaired to 100 |
| theatre cleared | 2000 + 500 per life left |

Ranks by score: CADET, 5000 PILOT OFFICER, 12000 FLIGHT LIEUTENANT, 25000 SQUADRON LEADER, 40000
WING COMMANDER, 60000 SKY MARSHAL. Kept on death: score, factories destroyed, damaged structures
(they keep repairing). Lost on death: nothing else; the gunship is back on deck with 0 damage and 8
bombs. Lives: 4 for the whole game, not refilled between theatres. The hi-score starts at 10,000 and
is **not saved** (**Gap**, section 10).

## 4. Structure and content budget

| theatre | map | factory | cruiser built in | missiles from alert | turret cooldown | jets / bombers / boats |
|---|---|---|---|---|---|---|
| 1 Coral Reach | 160 x 128 tiles (2560 x 2048 px) | 24 hp: 3 bombs | 9000 frames (3 min) | 3 | base | base |
| 2 Ember Flats | the same size | 32 hp: 4 bombs | 7500 frames (2.5 min) | 2 | -35 frames | 300 / 300 / 100 frames sooner |
| 3 Frostline | the same size | 40 hp: 5 bombs | 6000 frames (2 min) | 1 | -60 frames | 500 / 600 / 200 frames sooner |

Each theatre (`art/maps/theatre*.tmj`, `art/tools/theatres.py`): 3 island bands (tile rows 0, 42, 85)
with open sea between, 6 factories (3x3 tiles, intact / damaged / rubble), 18 AA turrets, 3 radar
stations, 1 shipyard (4x2), 1 airstrip (5x1, a helipad first), 6 piers (one road and one boat route
per factory), 5 plane spawn points at the map edge, and the carrier's patrol lane (it moves 0.25
px/frame east and west, turning at the ends). `STRUCT_MAX` 48 (35 used). The carrier starts at tile
(80, 76) / (80, 75) / (80, 46); the nearest turret is 708-776 px away (about 4.7-5.2 s of flight at top
speed to its 120 px range, derived), the nearest factory 769-858 px.
Theatre generation is a seed plus rules; the structures are placed by rules, not by hand.

- **Loading:** the three theatres, the BOB sheet and the logos are packed on the floppy: theatre
  8.8-8.9 KB packed each (20,792 bytes raw), BOBs 19.3 KB, DPS logo 22.2 KB, the two modules 65.0 and 48.1
  KB packed (79,424 and 61,684 raw). The executable is ~340 KB on the build on
  `art-pilot-integration` (2026-11-11; re-measure after the merge). About 520 KB of the 880 KB ADF
  (901,120 bytes) is used, derived. A theatre loads while the previous one's CLEARED screen shows
  (400 frames). **Fire-to-play and theatre load times on a real floppy are not measured** (open
  question 5); the playbook's target is <= 20 s.
- **Map and tiles** (measured on `art/maps/*.tmj`, 2026-11-11): 16 x 16 tiles, 77 in the kit (the
  playbook's 150-200 is a bigger platformer bank; a top-down island kit is smaller), **53 distinct tiles
  used per theatre, the top 10 cover 93.9-94.0%** (one sea tile alone is 55%), **13 used once** (target
  <= 40). Map data 20 KB raw, 8.8 KB packed (playbook: ~8 KB).
- **Disks:** one floppy; a theatre never spans disks. Files: executable, `bobs.bin`, `dpslogo.bin`,
  `theatre1-3.bin`, `m_dog68.mod`, `m_dog70.mod` (packed in the ADF).

## 5. Pacing

Measured at v1.4 on normal, A500, with `tools/pacing.agk` (all three theatres flown the same
scripted way in GODMODE: per factory, 12 s weaving and shooting round it, its bombs, 6 s flying off;
about 2.5 minutes a theatre) and `tools/pace.py`, which reads what the game prints every 16 frames:
planes and enemy shots (flak, missiles) on screen, and shots fired at the gunship. Threat = planes +
shots on screen (`logicThreat`; the cruiser's shells at the carrier don't count).

| theatre | threat mean | p90 / max | nothing to dodge | shots on screen mean / p90 | shots fired a minute | rest beats (3 s+ quiet) |
|---|---|---|---|---|---|---|
| Coral Reach | 0.64 | 2 / 4 | 65% | 0.56 / 2 | 30 | 12 in 2.3 min |
| Ember Flats | 1.02 | 3 / 6 | 48% | 0.92 / 3 | 46 | 10 in 2.4 min |
| Frostline | 1.35 | 4 / 8 | 45% | 1.21 / 4 | 67 | 9 in 2.6 min |
| *playbook target* | *2.5-3, +50% a level* | *6 / 10-12* | *8-12%* | *0.7-1.5 / 2.5-5* | *30-100* | *a 4-8 s rest every 60-80 s* |

**Gap vs playbook** (the README says the same and says "not tuned"):
- **Threat mean is 0.64-1.35 against 2.5-3,** and **nothing-to-dodge is 45-65% against 8-12%.**
  This is an open map where the player picks the fight; the flights between targets are quiet by
  design. The shots on screen (0.56-1.21 mean) and fired a minute (30-67) are inside the playbook's
  bands. Not tuned.
- **The step between theatres:** +59% and +32% threat, +53% and +46% shots fired (30 / 46 / 67 a
  minute), against +50% a level. Before 1.4 it was +23% and +21%.
- **The scripted run is quicker than a player:** it rarely reaches the two 2-minute alert steps or
  the cruiser (built after 2-3 minutes), which add to these numbers later. Not measured: a human
  run (open question 4).
- Deck rest beat: a gun never fires at a landed gunship, so landing is always a rest.
- No wave table, no spawn by map row: enemies come on **timers** (derived, normal skill, `enemies.c:612-641`):

| spawn | first | then every (frames) | notes |
|---|---|---|---|
| interceptor jet | 1200 (24 s) | `900 + rand(0..255) - 100 * alert - theatre(0/300/500)`, +900 more with no radar up; floor 200 | at most 4 planes live; each lives 1500 frames then leaves |
| bomber | 2400 (48 s) | `1800 + rand(0..511) - 150 * alert - theatre(0/300/600)`, floor 400 | only while the carrier floats |
| supply boat | 400 (8 s) | `700 + rand(0..255) - 50 * alert - theatre(0/100/200)` | only while a pier's factory stands; 3 boats max |
| cruiser | built over 9000 / 7500 / 6000 frames of play | the shipyard bomb sets it back 1500 | sails once |

Alert level (0-5) = factories destroyed + one step per 6000 frames of play (2 at most). It
shortens jet, bomber and boat timers (above) and the AA cooldown (8 frames a level), and turns on
missiles at level 3 / 2 / 1 by theatre.

- **First threat:** a turret about 4.7-5.2 s of flight away (derived), the first jet at 24 s.
- **Set piece:** the cruiser, built after 2-3 minutes: section 6. **Gap:** it comes at a time set
  by a clock and the shipyard, not at "about 50%" of progress.
- **Pace pressure:** the cruiser's build clock and the alert clock only bite slow players: the
  playbook's "a timer that only bites slow players" (met).
- **Checker:** `tools/pace.py` reports; it is **not** the kit's `wavescript` checker and a miss
  doesn't fail the build (**Gap**).

## 6. Enemies and bosses

| enemy | HP | movement | fire | tell | score |
|---|---|---|---|---|---|
| AA turret | 3 (the gun 1 a round, a bomb kills) | fixed; the gun turns toward you 1 of 8 aims every 4 frames from 200 px | flak at 1.5 px/frame, in range 120 px; from the alert level, 1 round in 4 is a homing missile (2.75 px/frame, life 110 frames, turns every 4 frames) | the barrels hold still and glow red 8-24 frames (16 normal), then flash for 4 frames | 100 |
| radar station | 4 (gun 1 a round, a bomb kills) | fixed | none; while one stands, jets home straight at you; with none they search | none | 250 |
| interceptor jet | 2 (gun) | from a map-edge spawn, straight at you if a radar stands, 3.5 px/frame, turns every 4 frames (8 without radar) | a missile within 110 px when its nose is within one heading step of you, then 90 frames' wait; first shot at least 60 frames after spawn; ram 30 damage | the nose pointing at you (no flash) | 200 |
| bomber | 4 (gun) | goes for the carrier, 1.5 px/frame | drops on it: 12 damage, then leaves | n/a | 300 |
| supply boat | 3 (gun, or one bomb within 18 px) | in from open sea along a pier's axis, 0.6 px/frame | none | n/a | 150 |
| **cruiser** | 40 (bomb 8, gun 1) | sails along the carrier's lane from the far side at 0.5 px/frame, stops 140 px from the carrier | shells the carrier (3 px/frame, 15 damage; 7 to sink it) every 120 frames | klaxon on sailing | 3000 |

- **Tells: Gap vs playbook.** The AA tell is 16 frames on normal (24 / 20 / 16 / 12 / 8 on skills
  1-5): at the hardest skill it is **8 frames, below the playbook's 12**. The interceptor jets' missiles
  have **no flash tell**; the warning is the jet's approach and its nose lining up. Unit tests:
  `testNoShotWithoutATell`, `testTellHoldsFireAndCantBeCut` (every flak and missile from a gun comes
  after a tell; a hit never shortens it).
- **Shot speeds. Gap vs playbook** (1-1.5 px/frame): flak 1.5 px/frame (met after 1.4: it was 3);
  **homing missiles 2.75 px/frame** and jets 3.5 px/frame (a ram). The cruiser's shells are 3 px/frame
  at the carrier, not at you.
- **Fire from off screen:** guns fire only inside 120 px of you (on screen). Jets fire inside 110 px.
  The cruiser fires from where it is; it can be off screen (its target is the carrier).
- **Sight <= weapon range:** guns track from 200 px but fire only inside 120 px. Radar sees the whole
  map.
- **Path scripts:** none. Movement is code (`enemies.c`); no `wavescript`. (**Gap**)
- **The cruiser, the theatre's set piece** (`enemies.c:468-538`, `testCruiserEscalatesAndRewards`):
  - parts: one hull (96 x 32), no separate core; the kill condition is its 40 hit points (5 bombs);
  - the HUD's YARD bar becomes ENEMY, its strength in red, while it sails;
  - phases: closing in (to 140 px of the carrier), then shelling;
  - **anti-stall:** ignored (no hit on it) for **45 s (2250 frames)** it closes at twice the speed
    and fires twice as often; at **90 s (4500 frames)** it launches a jet at the gunship every
    **20 s (1000 frames)**; each escalation sounds the klaxon again; any hit restarts the clock (the
    level stays). The weak point is open from the start: it is always reachable (it sails the carrier's
    lane);
  - death: two more big explosions either side, 3000 points, the carrier repaired, a message.
- **The carrier:** 100 hp; 12 per bomber, 15 per shell. Sunk, the airstrip's helipad is your only
  deck, the pointer and HUD arrow point to it, and the gunship, if landed on the carrier, is destroyed.

## 7. Difficulty

Ctrl+L, then a key within two seconds; shown on the score bar (SK3). Tables `enemies.c:11-27`.

| | 1 easiest | 2 easy | **3 normal** | 4 hard | 5 hardest |
|---|---|---|---|---|---|
| AA tell (frames) | 24 | 20 | 16 | 12 | 8 |
| flak damage (of 100) | 4 | 6 | 8 | 10 | 12 |
| aim error (px, +-) | 32 | 24 | 16 | 8 | 4 |
| gun cooldown | +60 | +30 | base 110 | -20 | -35 |
| jets come every | +700 | +350 | base | -250 | -400 frames |
| bombers come every | +900 | +450 | base | -350 | -600 frames |

(the gun's cooldown is `110 - tell + skill + theatre - 8 * alert + rand(0..31)`, floor 30 - tell, so a
gun never fires more than once in 30 frames, the tell included.) Skill 0 is GODMODE: it plays like
skill 3 and nothing destroys the gunship (hits still show, damage stays 0); it doesn't set the
hi-score.

- **Never changes:** the maps, structures and their hit points, every speed, hit radius, the
  carrier, the cruiser's anti-stall times, every tell existing, lives (4), bombs (8).
- **The ramp:** the alert level inside a theatre (section 5), the theatre table in section 4 across
  theatres (factory 3 / 4 / 5 bombs, cruiser 3 / 2.5 / 2 min, missiles from alert 3 / 2 / 1, gun
  cooldown -35 / -60, jets 300 / 500 sooner).
- **Hero devices:** a damage bar (not one-hit death), repair and reload on a deck (the deck is safe),
  4 lives, the carrier repaired by the cruiser kill, 2000 + 500 per life at the end. No smart bomb,
  no extra lives by score (**Gap vs playbook**, partly: score bonus per life exists, extra lives don't).
- **Respawn:** in place of the carrier's deck (the airstrip once it is sunk), damage 0, 8 bombs,
  after 100 frames of crash and 150 of LOST. Enemy shots and planes stay. Structures stay damaged or
  destroyed. No continue, no checkpoint: game over (350 frames) goes to the title.
- **Exploit rules (from the code and tests):**
  - **Jets camping the deck** (a jet held within 150 px of the deck for 26 s, two at a time, then
    ambushed the take-off): **landing** on branch `fix-jet-camping`, described there. Until it merges
    this is a known bug, not a rule.
  - Guns never fire at a landed gunship, so the deck is a free rest and repair: intended, but with
    jets circling it is a trap, hence the fix above.
  - Factories repair themselves (1 hp per 300 frames) and boats add 6 hp: finishing what you start
    is the rule, not stalling.
  - A boat is 150 points every 700+ frames while a pier's factory stands (a small farm; unclosed).
  - No parking off screen: jets home on you if a radar stands; the cruiser and alert clocks run.
- **Testing aids:** GODMODE (key 0), Ctrl+L then 9 (the next standing factory, GODMODE only), the
  serial heartbeat every 16 frames (planes, shots on screen, shots fired) for `tools/pace.py`, the
  checkpoint `fight`.

## 8. Art direction / style guide

The approved direction is `~/art-pilot/rotorraider/STYLE.md` (approved 2026-11-11). **Landing:** an
the integration is under way on branch `art-pilot-integration` (the terrain kit, the Frostline
palettes, the pilot gunship, jet, bomber, boat, carrier, cruiser, explosion, puff and AA guns with
their tell frames, goldens; and the HUD restyle with a framed minimap). Update this
section after the merge. What `main` has today is the 1.0-1.4 look, drawn by
`art/tools/tiles.py`, `sprites.py` and `theatres.py` (all our own code). A copy of the pilot's
STYLE.md could join the repo as `docs/STYLE.md` (open question 8).

**Look:** sunlit military islands seen straight down, saturated sea and land, hard light from the
top left, crisp 2-4 px clusters, 1-2 px cast shadows to the bottom right. Steel-grey enemy hardware
against warm land, one red Combine accent; the Kestrel is the only olive-and-glass object. One tile
kit, many theatres: each island band is a different copper palette over the same pixels.

**Palette (OCS 12-bit, dual playfield)**

| slots | use |
|---|---|
| COLOR00 | the deep sea (0x026 Coral Reach, 0x034 Ember Flats, 0x124 Frostline) |
| PF1 1-7 (objects, fixed all game) | 0x111 black outline, 0x444 shadow steel, 0x888 grey, 0xFEC white, 0xF60 orange, 0xFC2 yellow, 0xC22 red (the Combine's mark) |
| PF2 9-15 (terrain, 8 pens per island band) | pen 0 deep sea, pen 1 wave crest (copper-cycled glitter), 2 shallow water, 3 surf and concrete, 4 beach and roads, 5 land, 6 land dark, 7 bright |
| sprites 17-31 | the gunship (sprites 0-3) and AA guns (4-5) share all 15: 111 outline; 232 464 686 8A8 body ramp; 25A 7CF glass; 555 999 rotor; F80 FF4 stripe and lamp; CCC C22 333 FFF steel, Combine red, dark steel, specular (every register pinned in `chopper.txt`) |
| HUD (lines 216-255) | its own copper: panel 8BD, 469, 247, 236, 225, 224, 113, 112 down the lines; text pens 4 / 5 / 6 are numbers (white to steel), labels (hot metal), messages (ice to gold); bar fills red F64 to 912, gold FF8 to E72 |

The terrain pens per band and theatre are in STYLE.md section 2 (e.g. Coral Reach b1 / b2 / b3 land:
6A3 / 3A4 / AA4). **The Frostline fix:** the shipped band 1 and 3 land pens 3, 4, 5, 7 are all within a
step of white (EEF CCD EEF FFF), so factories, roads and yards vanish on the snow; the pilot's land is a
blue-grey snow (0xBCD) under concrete (0xEEF) and lights (0xFFF). No AGA extras. Dusk (a colour mix
toward 0x635) is applied to every band equally, so it can't open a step.

**Style rules:** light from the top left everywhere, never turning with a craft (a turned frame is
re-lit); a 1-px 0x111 outline inside the box on everything that matters (not rotor blades, wakes,
explosions or terrain); convex parts lit top-left, concave bottom-right; cast shadows 1-2 px down-right
in pen 6 (terrain) or pen 2 (decks); the gunship's shadow is its own BOB offset by altitude. Seamless
tiles: grass and forest wrap on a torus, coasts are **sampled** from the land and sea base tiles at the
same coordinates, so a coast tile next to grass continues its exact grain; 35% of open-sea cells get a
crest variant and 6% of land cells a forest or rock variant.

**Blended zone edges and band switches (the user's rule: no visible seam).** Each theatre has 3 island
bands (tile rows 0, 42, 85); where a band's first row comes on screen the copper reloads pens 2-7 (and
the glitter). Pens 0 and 1 are forced equal across a theatre's bands, so the switch line must fall in
open sea drawn only in pens 0 and 1 (the shared pens per boundary are tabulated in STYLE.md 7b).
**Gap vs the rule, on `main` today:** the shipped sea tiles each carry one foam crest in pen 3; pen 3
differs at Ember Flats band 2 to 3 (0xDDD to 0xCCC) and Frostline band 1 to 2 (0xEEF to 0xDDE), so
those foam dashes change colour exactly on the switch line (`bandcheck.py`: the shipped art fails 2 of
6 boundaries, the pilot passes all 6). The pilot's crests are pen 1 only: **landing**.
Coasts: deep sea, a 1-px dither drop-off, shallows, a broken foam line, surf, beach, a dithered beach
edge, land; roads from every factory run to a pier across the beach (the connector the eye follows).
There is **no parallax:** depth is the gunship's shadow.

**Animation** (frame counts from speed): gunship 32 frames (16 headings x 2 rotor phases; the pilot
draws 4 and turns the rest, re-lit); enemies 8 headings; boat 8; carrier 4 (east / west wake x 2); cruiser
2; explosion 6 frames of 4 game frames (0.5 s); puff 4; AA gun 24 (8 aims, 8 firing, 8 tell: about 16
frames of glow on normal, then 2 of muzzle flash); the rotor alternates every frame (every 8 on deck);
the water has no frames (the copper cycles the crest colour).

**Sizes:** gunship 32x32, shadow 32x32 x16, AA gun 16x16, jet and bomber 32x32, boat 16x16, carrier
and cruiser 96x32, explosion 32x32, puff 16x16, shot 16x4, bomb 16x8, flak 16x4, missile 16x16 x8,
reticle 16x16, pointer 16x16 x8, title logo 160x146, factory 48x48 (3x3 tiles).

**Sources (honest list)**
- 1.0-1.4: our own scripts (`art/tools/*.py`, rendered from simple models at every heading); the
  logos from the user's source pictures (`art/src/*.png`, `make_title_logo.py`, `make_dps_logo.py`).
- Approved pilot (**landing**): Retro Diffusion (`rd_pro__topdown` with our own approved frames as
  the reference for the gunship, jet and bomber; `rd_core__topdown`; `rd_animation__vfx` for the
  explosion), each request palette-locked with `input_palette`, then `lock`, `remove_orphans`,
  `despeckle`, an inside outline, RotSprite-style turns and re-lighting, all scripted. It **failed** for
  buildings (4 tries), ships (3), 16-px items, terrain textures and rotations: those are built in code
  (the factories and ships from models, the terrain by rules, a 6x-supersampled model for the gun
  dome). No other games' art as input.
- **Sign-off:** the user approved the pilot on 2026-11-11.

## 9. Audio

**Channel policy** (README, `sound/sound.toml`, `main.c:685-830`). Paula plays 0 and 3 left, 1 and 2
right.

| channel | side | in the air | on the title and in jingles |
|---|---|---|---|
| 0 | left | the rotor while it has the channel, otherwise music | music |
| 1 | right | music; effects on the right of the screen | music; menu effects |
| 2 | right | music | music |
| 3 | left | music; effects on the left of the screen | music; menu effects |

- **Effects** (`sfx_channels = [1, 3]`) play on the side of the screen where they happen
  (`logicSfxChannel`: flak, missiles, explosions and factories by their position, the gunship's own
  sounds by where it is); within 40 px of the middle they take whichever of the two is free; the
  cruiser's klaxon comes from anywhere. An effect takes its channel from the music for its length
  (0.08-0.5 s), the bass and drum voices, which return at their next note. **Cap:** at most one
  effect a side and two a picture, and a minor one only alone (each `ptplayerSfxPlay` costs ~4% of a
  frame and a later one would only cut an earlier one short, `dec3829`).
- **The rotor** (a 1200-byte loop; a longer 8000-sample take-off and landing burst, 0.68 s at period
  300, with dog70 only) borrows channel 0. In the air the music is dog70, whose voice 0 (string chords)
  is silent for its first 8 positions (61 s), then loops positions 8-13 on all four voices. So the rotor
  loops on channel 0 while there is no music in the air (a 512K-chip A500, or M off) and for dog70's
  first 59 s (`DOG70_VOICE0_FREE` 2950 VBLs), fades out over 64 VBLs, gives the channel back, and from
  then is a short fading chop at take-off and landing on the gunship's side's effect channel. Before 1.4
  the rotor kept channel 2 all game and dog70 lost its loudest voice (the analog string pad, sounding
  86% of the song, measured by `sound/tools/mod_voices.py`). The module itself is unchanged.
- **Music:** one tune per state. Title: *dog68* (credited, below) or our own march `theme.mml` on a
  512K-chip A500; in the air: *dog70* or nothing (no music on a 512K-chip A500, by policy); `fanfare`
  for a theatre cleared and victory; `gameover`. The modules are loaded into chip RAM at boot only if
  64 KB stays free afterwards (A500+, A600, A1200). The title's credits page alternates with the
  controls every 256 frames (5.1 s) while they play.
- **Credits (third-party):** `songerson_-_dog68.mod` (title) and `songerson_-_dog70.mod` (in the
  air), **Songerson**, The Mod Archive (modarchive.org), **CC BY**, used unchanged. Recorded in
  `sound.toml` (`artist`, `license`, `url`) and in the release zip's `CREDITS.txt`; README credits
  them. Everything else is ours: the effects (`sound.toml`) and the march, fanfare, game-over tune
  from synthesised instruments (`sound/tools/make_samples.py`).
- **Priorities** (higher wins; important effects short): crash 6; klaxon 5, factory 5; bigboom 4,
  hit 4; missile 3, start 3; bomb 2, boom 2; cannon 1, flak 1, land 1.
- **Gap vs playbook:** "menus leave a voice free": 1.4 deliberately gives the title music all four
  channels. A looped effect (the rotor) normally takes a channel from a 4-voice module; here it takes
  the one voice the song leaves silent for 61 s, then steps aside (a deliberate compromise).

## 10. Presentation

| item | state |
|---|---|
| boot | the Desert Palms Software logo (16 colours, 4 reloaded per line, ~320 colours), faded by the copper, fire skips |
| title | the ROTOR RAIDER logo (160x146, 3 planes, 7 pens reloaded on each of its 146 lines: 319 colours) over the slowly panning theatre; "BY PAUL ROSS SEIBERT", copyright, the steering choice (left classic, right arcade), PRESS FIRE; an alternating credits page for the music (on 1 MB-chip machines) |
| attract loop | **gap:** none (no hall of fame, no demo) |
| options | the steering on the title; difficulty by Ctrl+L |
| mission flow | on a theatre's start: its name for 220 frames over the play; a HUD message for each factory, the cruiser, the carrier sunk. **Gap:** no route map, preview card or brief |
| in play | HUD (40 lines under the 216-line play area): score, hi-score, gunships left (HELI), damage bar, bombs, SK(ill), the carrier's strength, the shipyard's progress (YARD, becomes ENEMY with the cruiser's strength), factories left, the compass arrow to home, and a 40 x 32 minimap of the whole theatre (a pixel per 4 x 4 tiles: red factories, yellow carrier, white you, orange planes). A yellow pointer at the edge of the play area points home when it is off screen (blinks when out of bombs or at damage 70+) |
| end of theatre | THEATRE CLEARED, ALL FACTORIES DESTROYED, NEXT: name; 400 frames; the next theatre loads |
| game over | GAME OVER, score, rank, 350 frames, then the title |
| high scores | **gap:** one hi-score (starts at 10,000), not saved to disk, no table, no initials |
| ending | ALL THEATRES CLEARED, THE IRON COMBINE IS BROKEN, score and rank, 350 frames. **Gap:** not animated |
| credits | the title's second page for the music; the release zip's `CREDITS.txt`. No art credits page (all art is ours; the Retro Diffusion pilot's tool is named here) |

**README drift (a doc bug, not a design):** the README's download table still links
`rotor-raider-1.3.zip` while `agk.toml` and "New in 1.4" say 1.4.

## 11. Tech plan

- **Display** (`main.c:1-30`): PAL lores 320x256, dual playfield, 6 planes. PF2 (planes 2/4/6): the
  terrain, an 8-way ring scroller (`scroll.c`): moving the camera moves only the start pointer; rows
  wrap into each other horizontally and the copper points the planes back at the ring's start; new
  tiles are blitted into the margin before they come into view (3 dirty tiles a frame, `DIRTY_PER_FRAME`;
  `CAM_MAX_STEP` 8 px). PF1 (planes 1/3/5): a double-buffered 3-plane object frame buffer (48 px margin
  left, 64 right, 224 rows) where the blitter draws everything else, so nothing restores the terrain. The
  play area is 216 lines; below it a plain 3-plane HUD screen (40 lines) with its own copper block.
  DDFSTRT `$30` (a fetch word early for fine scroll; costs sprite 7). One raw copper list per buffer,
  rebuilt each picture; the title's logo is a 146-line block of 1 WAIT + 7 MOVEs, written once.
  Per-band palettes (8 pens) load where each band's first row comes on screen; a glittering sea
  (crest colour cycled), dusk falling as the battle goes on (a colour mix toward 0x635).
- **Sprites:** 0-3 the gunship (32 x 32, 15 colours, attached pairs, 32 frames, two sets of bitmaps
  one per copper buffer, no per-frame copying: ~25 raster lines saved); 4-5 AA guns (two attached
  pairs of 15 colours, each reused down the screen; a gun with no room for its lines is drawn by the
  blitter, BOB fallback `gun.txt`); 6 and 7 not used by the guns (the fine-scroll fetch takes 7; `61adf62`).
- **BOBs:** 16 types on PF1 (`DRAW_MAX` 72), blitter queue (`agk/blitq`). Limits: 4 cannon shots, 2
  bombs falling, 8 enemy shots, 4 planes, 3 boats, 8 explosions, 48 structures, 24 dirty tiles.
- **Frame budget (A500):** `tests/perf.agk` (a fight by a factory in GODMODE: flak, jets, boats,
  bombs, explosions, the scroller) asserts `expect-no-dropped-frames` and `expect-max-load 95`. Its
  header records: average 45-55%, the worst game frame ~87% (a bomb landing on a factory), the
  title-to-game switch ~93% for a picture or two; none dropped. Before `dec3829` the worst frames were
  97-100% (a loop over every turret in `logicDraw`, fixed). **Gap vs playbook** (worst < 100%): met, with
  93% as the highest recorded. The average target (<= 70%): met (45-55%).
- **Memory:** `expect-min-chip-free 64` (KB) on every profile. Chip: the PF1 buffers x2, the HUD x2,
  the tileset, sprite frames, the loaded theatre, and on 1 MB-chip machines two modules (141,108
  bytes) only if 64 KB stays free. The executable is ~340 KB (see section 4).
- **Kit techniques used:** `blitq`, `copw` (raw copper), `text`, `pack` (`agkLoadFile` for theatres,
  BOBs, logos and modules), `perf`, ACE's ptplayer with the sound `mod =` entries (`artist`, `license`,
  `disk = true`; effectively `modcredits`), our own scroller. Not used: `wavescript`, `longmap`,
  `multiplex`, `agahud`.
- **A1200 extras:** none (the OCS picture). `a1200` is a test profile only.

## 12. Production

- **Order of work (as it happened):** 1.0 (2026-11-04) -> 1.1 momentum, the way home, harder theatres
  (11-04) -> 1.2 AA guns that aim, a styled HUD (11-04) -> the title logo, Songerson's music (11-09) ->
  1.3 (11-10) -> 1.4 (11-11): the AA tell, flak at half speed, the rotor on channel 0, stereo effects,
  pacing measured per theatre, the cruiser as a set piece, the frame-time fix -> this spec.
- **Milestones:** playable core done; three complete theatres done; polish in progress (the art
  pilot, the HUD, the jet-camping fix, all **landing**); release 1.4 version-bumped, a 1.4 release
  zip and README download links not yet done (see section 10).
- **Playtests:** Paul's reports are what drive fixes (the carrier duplicate, jets camping, a cut-off
  mountain in the other game); logged as tuning tickets.

**Landing branches** (checked 2026-11-11, `git log` on each; update this section
after the merge):
- `art-pilot-integration` (5 commits ahead of `main`): the pilot terrain kit (77 tiles, same numbers)
  and the Frostline palettes (`17d2f26`), the pilot gunship, jet, bomber, boat, carrier, cruiser,
  explosion, puff and AA guns with their tell frames (`2ec4ddf`), goldens for them (`4e2f74e`), the
  HUD restyle with recessed wells and a framed minimap (`6402e07`, `54540c8`). The **carrier
  duplicate-draw bug** Paul reported is **not in any commit on this branch yet**; spec section 8 and 10
  say "landing" only for what the commits show.
- `fix-jet-camping`: **no commits yet**; uncommitted work in its worktree (README note, `enemies.c`,
  `logic.h`, a unit test): while the gunship is on a deck or going down to one, and for 150 frames
  (`JET_DECK_GRACE`) after take-off, every jet breaks off and flies out, no new jet is called (the timer
  waits), the cruiser launches none; and within 250 px (`JET_DECK_ZONE`) of the carrier only one jet at
  a time commits to the gunship. README note: in 85 s on deck, jet-seconds within 150 px went from
  60 to 0. Open play unchanged. It may bump the version to 1.4.1.

**Acceptance criteria** (state as recorded at 2804254; **not re-run for this spec**)
- [x] `agk unit` passes (58 tests in `tests/unit/test_logic.c`, three files compiled for the host)
- [x] `agk test` passes on a500, a500-ks31, a500-aros, a1200 (10 scenario files; `f69b004` regenerated the goldens)
- [x] no dropped frames in the perf scenario, max load under 95%
- [ ] the pacing checker passes section 5's targets: **no** (threat mean and quiet share miss, by design, README "not tuned"); and the checker (`tools/pace.py`) isn't a build gate
- [x] tells on every AA round, unit-tested (`testNoShotWithoutATell`); **not** on interceptor missiles
- [x] cruiser anti-stall in place, unit-tested (`testCruiserEscalatesAndRewards`)
- [ ] the 512K master boots: **unverified**; README says 1 MB
- [ ] a release zip with README, CREDITS.txt and a demo video: the 1.3 release exists (`demo/rotor-raider.mp4`); the 1.4 zip is not built

**Prove It** (what each recent feature needs on WinUAE or real hardware, and who runs it. vAmiga is
the test bench, not the player: its recorded audio is mono, it ignores AGA BPLCON1's 35 ns bits, its
disk is writable and keys are held in real time.)

| feature | what proves it | who | state |
|---|---|---|---|
| **stereo effect panning by ear** | on speakers or headphones in WinUAE or a real Amiga: a flak burst on the left of the screen is heard on the left (Paula 0 and 3 left, 1 and 2 right), one on the right on the right, the middle on either; fly a circle round a gun and hear it move | Paul | **unproven** (vAmiga records mono; `testSfxPanning` checks only the channel choice; measured: 19 left, 13 right, 29 middle in a 44 s fight) |
| **dog70's strings returning at 61 s** | on a 1 MB-chip machine (WinUAE with 1 MB chip) in the air for 70 s: the rotor loops on channel 0 and the music has no string chords until ~61 s; the rotor fades out over ~1 s at ~59 s; from 61 s all four voices play and the take-off and landing chop is short; no click at the hand-over | Paul | **unproven** (the timing is derived from the module and `mod_voices.py`; nobody has listened across the boundary) |
| 1 MB-chip music load on real hardware | A500+, A600 or A1200 (or WinUAE): the modules load at boot with 64 KB left; the title shows the credits page; the music plays; on 512K chip, the march and no music in the air | Paul | unproven on real hardware |
| AA gun tell readable in play | played: the barrels' glow is noticed before the round on a real display, at each skill | Paul playtest | unproven by a person |
| AA guns on sprites 4-5 | on a real display: no grey blobs where two guns share lines; none lost at the fine-scroll edge | Paul | unproven on hardware (vAmiga only) |
| classic steering momentum | by hand: glide, arc, tail-first reverse feel right with a real stick | Paul | unproven by feel |
| the cruiser set piece | played to the cruiser (3 min in): the ENEMY bar, the 45 s and 90 s escalations, the klaxon, sinking it | Paul | unit-tested; **not played** (the scripted pacing run rarely reaches it) |
| theatre load between clears on a real floppy | stopwatch on a real A500 or WinUAE at floppy speed: the CLEARED screen (400 frames) covers the load | Paul | not measured |
| jets no longer camp the deck (**landing**) | sit on the carrier for 60+ s on skill 3 in WinUAE: no jet circles it; take off: no ambush for 3 s | Paul | pending the merge |
| the approved new art and HUD (**landing**) | at 1x on a real display: the Frostline snow reads (factories, roads, yards), no foam colour change on the band-switch lines, the framed minimap is legible | Paul after the merge | pending the merge |
| 319-colour title and 320-colour boot logo | by eye on a CRT and an LCD | Paul | unproven |

## Open questions for Paul

1. The AA tell is 8 frames at the hardest skill, below the playbook's 12. Raise it, or leave the
   hardest as designed?
2. Interceptor jets' missiles have no flash tell and fly at 2.75 px/frame; accept, or add a tell?
3. Threat mean (0.64-1.35) and nothing-to-dodge (45-65%) miss the playbook by a wide margin. Is this
   open-map design the intent, or should the factories get more to dodge? Add the pacing check as a
   build gate?
4. The pacing numbers come from a scripted GODMODE run that rarely reaches the cruiser or the
   2-minute alert steps. Is a human or longer run wanted?
5. Fire-to-play and theatre load times on a real floppy are unmeasured. Budget?
6. No hi-score table, no save, no attract loop, no animated ending. Which are wanted before the next
   release?
7. Does it boot on 512K chip + no expansion? README says 1 MB; the playbook's master is 512K.
8. Add `docs/STYLE.md` (a copy of the pilot's) after the art lands?
9. The README's download table says 1.3; the version is 1.4 (and may become 1.4.1 with the jet fix).
10. The carrier duplicate-draw bug is on no branch's commit list yet: which branch owns it?

## Version history

| version | date | change |
|---|---|---|
| v1.0 | 2026-11-11 | retroactive: the game as it is at `main` 2804254 (v1.4); the approved art, the HUD and the jet-camping fix are noted as landing |
