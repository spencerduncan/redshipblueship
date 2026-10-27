# Debug seed: the Sonata and the Deku Mask in Mido's House

A paired seed for reproducing the Woodfall Temple reports of
[#1070](https://github.com/spencerduncan/redshipblueship/issues/1070) without playing a seed through. Mido's House
holds Majora's Mask's **Sonata of Awakening** and **Deku Mask**, Closed Forest is Off, and everything else is the
shipped defaults. A new file can open the chests within a minute, walk to the Happy Mask Shop and cross: both items
are waiting at the Clock Tower.

## The files

| File | Where it is | What it is |
| --- | --- | --- |
| `midos-house-sonata-deku.rsbs-seed.zip` | Not in the repository (a binary); kept beside this README in the hand-off folder, or made again with the command below | The seed package: the whole paired world, both games' settings and the spoiler |
| `20-36-63-29-61.json` | This folder | The seed's spoiler (the same document as the package's `oot-spoiler.json`), for reading only |

- Seed string `RSBSMIDOSDEBUG`, seed name (hash icons) `20-36-63-29-61`.
- Generated at `21fabe5b` (branch `claude/debug-seed-forced-placements`). Its world code is that of `b4dd0299`, so
  the package's build line reads `v0.3.1-prealpha+b4dd029-dirty`.
- Non-default settings: Randomizer > Logic/Access > **Closed Forest = Off**. Everything else is the default,
  including Majora's Mask's Dungeon Access (Transformation & Song).

## Which build loads it

A seed package is not tied to one commit. Randomizer > General refuses a package only when its generator version
(`RSBS_GENERATOR_VERSION`, 1 here) differs from the build's, or when Majora's Mask's half (47928 bytes) or the combo
record (1712 bytes) has another size than the build's (`games/oot/soh/Enhancements/randomizer/SeedPackageSingleExe.cpp`).
So any build of `main` from `db963ca9` on loads it until one of those changes. If a build refuses it ("Refused: Seed
package damaged or incomplete" or "Refused: Seed package from another generator"), make it again on that build with the
command below. The game's own Generate cannot pin a placement: for a paired world nothing in the menus can, which
is why this seed is made headless.

## Load it and create the file

1. Copy `midos-house-sonata-deku.rsbs-seed.zip` into the **`Randomizer`** folder of the game install (the folder
   beside `redship.exe` that holds the spoilers; create it if it is missing).
2. Launch the game. On the file select, with no file loaded, open the menu (Esc): **Randomizer > General**. Under
   Seed Package, pick `midos-house-sonata-deku.rsbs-seed.zip` in the list and press **Load seed package**. A toast
   reads "Seed package loaded: midos-house-sonata-deku.rsbs-seed.zip" and the line under the buttons names the
   staged package.
3. Pick an empty slot, choose the **Randomizer** quest, then **Start Randomizer**, enter a name and confirm. Creation is
   instant: the file uses the package's world.
4. Optional check, in the spoiler: `combo.crossingStore.mmItemsInOoT`
   lists "KF Mido Top Left Chest" holding the Sonata of Awakening and "KF Mido Top Right Chest" holding the Deku Mask.
   (Hyrule's own `locations` list shows "Blue Rupee" on those chests, as it does on every chest that holds a Majora's
   Mask item: that is the cover the Hyrule side carries.)

## The walk

1. Leave Link's house and go into **Mido's House** (Kokiri Forest, beside the shop). Open the chests: one gives the
   **Sonata of Awakening**, one the **Deku Mask** (both are Majora's Mask items and are kept for Termina). The other
   two hold the Postman's Hat and the Bunny Hood, also Majora's Mask items.
2. Leave the forest by the bridge to Hyrule Field (Closed Forest is Off, so nobody stops you), cross the field to
   the Market by day, and go into the **Happy Mask Shop**. Its door takes you to the **Clock Tower** in Termina.
3. At the Clock Tower the crossing redemption hands over what you found in Hyrule: the Sonata of Awakening and the
   Deku Mask (and the two masks, if you opened all four chests). The paired start has already given you the Ocarina
   of Time and the Song of Time.

## The Woodfall test

1. Go to Woodfall: leave Clock Town to the south, take the road to the Southern Swamp, cross the swamp as Deku Link
   (the lily pads), and climb to the Woodfall platform on the Deku flowers.
2. As **Deku Link**, stand on the **Deku Scrub symbol on the floor between the pink flower and the owl statue**
   (x within 40, z from 1000 to 1078). The moment Deku Link takes out the pipes there, a **short chime** plays: that
   says you are on the spot. No chime means you are off it; move before you play.
3. Play the **Sonata of Awakening**.
   - **Pass:** the temple rises (its cutscene plays).
   - **Fail:** the chime played, you played the Sonata as Deku Link on the symbol, and the temple does not rise. Keep
     the save, the newest `.log` in `logs/`, `stderr.txt` (launch with `cmd /c "redship.exe 2> stderr.txt"`) and the
     spoiler above, and attach them to [#1070](https://github.com/spencerduncan/redshipblueship/issues/1070).
   - Played on the pink flower, or where the Song of Soaring sets you down, the Sonata gives its full announcement and
     nothing happens. That is vanilla, not a finding.
4. Do it twice: once with **Settings > Controls > Ocarina Controls > Dpad Ocarina Playback** on and the Sonata played
   on the D-pad, once with it off and the Sonata played on the C buttons. Note which way each attempt was played; a
   difference between the two is the finding.

## Make it again

From a build tree of a build that has `--test debug-seed-forced-placements` (this branch, or `main` once it merges),
with no Majora's Mask or Ocarina of Time ROM needed:

```bat
set SDL_AUDIODRIVER=dummy
set RSBS_DISABLE_OTR_INIT=1
set RSBS_DIAG_CVARS=gRandoSettings.ClosedForest=2
set RSBS_FORCE_PLACEMENTS=oot:KF Mido Top Left Chest=mm:Sonata of Awakening,oot:KF Mido Top Right Chest=mm:Deku Mask
set RSBS_DEBUG_SEED=RSBSMIDOSDEBUG
set RSBS_SEED_PACKAGE_PATH=%CD%\midos-house-sonata-deku.rsbs-seed.zip
redship.exe --test debug-seed-forced-placements
```

It prints `[TEST] PASS: debug seed 'RSBSMIDOSDEBUG' -> <package>` once the fill has proved the seed beatable and both
items sit in the crossing store on their chests, and writes the spoiler to `Randomizer/20-36-63-29-61.json` in the
working folder. Other placements use the same form, comma-separated: `<game>:<check>=<game>:<item>`, where a game is
`oot` or `mm` and a check or item is its spoiler name or `#<id>`. A name that names nothing lists the checks whose
names contain it. Up to eight rows; each must be a progression item of the bag on an empty check it may cross to.
