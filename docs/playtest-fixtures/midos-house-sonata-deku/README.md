# Debug seed: the Sonata, the Deku Mask and a bottle in Mido's House

A paired seed for reproducing the Woodfall Temple reports of
[#1070](https://github.com/spencerduncan/redshipblueship/issues/1070) without playing a seed through. Mido's House
holds Majora's Mask's **Sonata of Awakening**, **Deku Mask** and an **Empty Bottle**, Closed Forest is Off, Shared
Bottles is On, and everything else is the shipped defaults. A new file can open the chests within a minute, walk to
the Happy Mask Shop and cross: the Sonata and the mask are waiting at the Clock Tower, and the bottle is in both
games.

## What changed (second version)

The first version (made by #1234) had the Sonata and the Deku Mask only; the bottom chests held the Postman's Hat and
the Bunny Hood by chance. This version keeps the seed string and both of those rows and adds a third:
**KF Mido Bottom Left Chest = Majora's Mask's Empty Bottle**.

- **Why that bottle.** A Majora's Mask bottle is the one that tests the pair: found in Hyrule, it is the far game's
  item, so it exercises the cross-game path, and an empty one leaves the contents rule easy to read. Majora's Mask's
  "Empty Bottle" (id 19, `games/mm/2s2h/Rando/StaticData/Items.cpp`) is a progression row of this seed's bag, so the
  forced-placement knob takes it as it is.
- **Why Shared Bottles is On.** With the rule off (its default), a bottle is each game's own: this one would cross
  to Termina and do nothing in Hyrule. On, the bottle COUNT is one quantity for the pair (#970), so the bottle is
  usable in both games. The rule is frozen into the file at creation like every other combo setting. The rule also
  trims the pair's bottle items to four (OoT's four slots, Ruto's Letter counted): this seed keeps Ruto's Letter
  (Woodfall, near the owl), the Empty Bottle in Mido's House, Majora's Mask's Bottle with Red Potion (Water Temple
  Longshot Chest) and OoT's Bottle with Bugs (Stone Tower Temple, Wind Room ledge chest).
- **The fourth chest moved.** KF Mido Bottom Right Chest now holds a Huge Rupee (Hyrule's own item); the Postman's
  Hat and the Bunny Hood are elsewhere.

## The files

| File | Where it is | What it is |
| --- | --- | --- |
| `midos-house-sonata-deku.rsbs-seed.zip` | Not in the repository (a binary); kept beside this README in the hand-off folder, or made again with the command below | The seed package: the whole paired world, both games' settings and the spoiler |
| `20-36-63-29-61.json` | This folder | The seed's spoiler (the same document as the package's `oot-spoiler.json`), for reading only |

- Seed string `RSBSMIDOSDEBUG`, seed name (hash icons) `20-36-63-29-61`. The icons did not change from the first
  version: they come from the seed string and Hyrule's settings, and the new rule is a combo setting. The world
  did change, and so did its combo fingerprint (`comboSettingsHash` `39443F88`, was `06DDDEBF`).
- Generated at `191a19a1` (`main`, no code change), so the package's build line reads `v0.3.1-prealpha+191a19a`.
- Non-default settings: Randomizer > Logic/Access > **Closed Forest = Off**; Cross-Game Rules > **Shared Bottles =
  On** (`crossItemFlags` `0x00100000`). Everything else is the default, including Majora's Mask's Dungeon Access
  (Transformation & Song).

## Which build loads it

A seed package is not tied to one commit. Randomizer > General refuses a package only when its generator version
(`RSBS_GENERATOR_VERSION`, 1 here) differs from the build's, or when Majora's Mask's half (47928 bytes) or the combo
record (1712 bytes) has another size than the build's (`games/oot/soh/Enhancements/randomizer/SeedPackageSingleExe.cpp`).
So any build of `main` from `191a19a1` on loads it until one of those changes. If a build refuses it ("Refused: Seed
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
   instant: the file uses the package's world, Shared Bottles included (the menu's own Shared Bottles setting does
   not matter; the package's frozen rule wins).
4. Optional check, in the spoiler: `combo.crossingStore.mmItemsInOoT` lists "KF Mido Top Left Chest" holding the
   Sonata of Awakening, "KF Mido Top Right Chest" holding the Deku Mask and "KF Mido Bottom Left Chest" holding the
   Empty Bottle, and `combo.comboSettings.crossItemFlags` is 1048576 (Shared Bottles). (Hyrule's own `locations`
   list shows "Blue Rupee" on those three chests, as it does on every chest that holds a Majora's Mask item: that is
   the cover the Hyrule side carries.)

## The walk

1. Leave Link's house and go into **Mido's House** (Kokiri Forest, beside the shop). Open the chests: one gives the
   **Sonata of Awakening**, one the **Deku Mask** (both are Majora's Mask items and are kept for Termina), the
   bottom left one the **Empty Bottle**, and the bottom right one a Huge Rupee.
2. The bottle is in Hyrule at once: with Shared Bottles on, a far game's bottle steps the shared bottle count when it
   is picked up, and Hyrule re-applies the count on the spot, so Link has an **empty bottle in OoT's first bottle
   slot** before he leaves the house (`OoT_CreditSharedPickup`, #1120). It does not wait for a crossing, unlike the
   Sonata and the mask; a bottle that only appears after a round trip is a finding for the bottle test below.
3. Leave the forest by the bridge to Hyrule Field (Closed Forest is Off, so nobody stops you), cross the field to
   the Market by day, and go into the **Happy Mask Shop**. Its door takes you to the **Clock Tower** in Termina.
4. At the Clock Tower the crossing redemption hands over what you found in Hyrule: the Sonata of Awakening and the
   Deku Mask. Termina also applies the shared bottle count on arrival, so Link has an **empty bottle in Majora's
   Mask's first bottle slot**. The paired start has already given you the Ocarina of Time and the Song of Time.

## The bottle test

The rules, from `src/common/context.h` (`RSBS_SHARED_RES_BOTTLE_COUNT`), `src/common/shared_items.h` (the
`BOTTLE_COUNT` row of the shared-quantity policy) and the two games' apply paths
(`games/oot/soh/GameExports_SingleExe.cpp`, `games/mm/2s2h/GameExports_SingleExe.cpp`):

- What is shared is the **count**: how many bottle slots hold a bottle, whatever is in it (a bottle with a bug, a
  fairy, milk or Ruto's Letter counts the same as an empty one). The shared count runs 0 to 4, OoT's four slots.
- The **contents never cross**. When a game's count is raised on arrival, the new bottles are **empty**, in that
  game's next empty bottle slots; a filled slot is never touched.
- The count only goes **up**. Drinking a potion or releasing a bug empties the bottle but keeps the slot, so the
  count does not move; nothing takes a bottle away.
- Majora's Mask has six slots: its fifth and sixth bottles stay Majora's Mask's own and never raise Hyrule's count.

Steps:

1. **Hyrule to Termina.** In Hyrule, fill the bottle (a bug from under a rock, a fish, a fairy). Cross to the Clock
   Tower. Expected: Termina's bottle is still **empty** (it never held anything; the bug does not come along), and
   Termina has **one** bottle, not two.
2. **Termina to Hyrule.** In Termina, fill Termina's bottle (anything a bottle can hold) and keep the Hyrule one
   full. Go back through the Clock Tower door to the Happy Mask Shop. Expected: Hyrule's bottle still holds what you
   put in it in step 1, Hyrule has **one** bottle, and Termina's bottle, on the next crossing back, still holds what
   you put in it.
3. **Empty it.** Use up or release the contents in one game and cross. Expected: both games still have exactly one
   bottle.
4. A second bottle (the Red Potion in the Water Temple, the Bugs in Stone Tower) raises both games to two the same
   way. Any other result (a missing bottle, a second empty bottle appearing, contents copied across, a filled bottle
   emptied by a crossing) is a finding for
   [#970](https://github.com/spencerduncan/redshipblueship/issues/970): keep the save, the newest `.log` in `logs/`
   and `stderr.txt` (launch with `cmd /c "redship.exe 2> stderr.txt"`).

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

From a build tree of a build that has `--test debug-seed-forced-placements` (`main` from `c9fd36de` on), with no
Majora's Mask or Ocarina of Time ROM needed:

```bat
set SDL_AUDIODRIVER=dummy
set RSBS_DISABLE_OTR_INIT=1
set RSBS_DIAG_CVARS=gRandoSettings.ClosedForest=2,gCombo.Rando.SharedBottles=1
set RSBS_FORCE_PLACEMENTS=oot:KF Mido Top Left Chest=mm:Sonata of Awakening,oot:KF Mido Top Right Chest=mm:Deku Mask,oot:KF Mido Bottom Left Chest=mm:Empty Bottle
set RSBS_DEBUG_SEED=RSBSMIDOSDEBUG
set RSBS_SEED_PACKAGE_PATH=%CD%\midos-house-sonata-deku.rsbs-seed.zip
redship.exe --test debug-seed-forced-placements
```

It prints `[TEST] PASS: debug seed 'RSBSMIDOSDEBUG' -> <package> (3 forced row(s), 0 failure(s))` once the fill has
proved the seed beatable and all three items sit in the crossing store on their chests, and writes the spoiler to
`Randomizer/20-36-63-29-61.json` in the working folder. To check the package loads, run the same build with
`RSBS_SEED_PACKAGE_PATH` naming it and `RSBS_SEED_PACKAGE_REPORT` naming a scratch file:
`redship.exe --test seed-package-load` prints `[TEST] PASS: seed package load rc=0`.

Other placements use the same form, comma-separated: `<game>:<check>=<game>:<item>`, where a game is `oot` or `mm`
and a check or item is its spoiler name or `#<id>`. A name that names nothing lists the checks whose names contain
it. Up to eight rows; each must be a progression item of the bag on an empty check it may cross to. Without
`gCombo.Rando.SharedBottles=1` the same rows should make a seed whose bottle is Termina's alone (it crosses like the
Sonata); that variant was not generated.
