# Open issues against upstream fixes (2026-10-26)

**Status:** read-only cross-reference for the operator. Nothing here is ported, filed or closed.
**Our tree:** `main` at `038ae33d` (every `path:line` below is as of that commit).
**Upstream read:** Ship of Harkinian `origin/develop` `2827fc2a0` (2026-10-20), 2Ship2Harkinian `origin/develop`
`b30a286c5` (2026-10-24), libultraship `port-maintenance` `da6daf1a` (unchanged since 2026-10-12). Nothing is reported
upstream; the clones are reference material only.

## Method

The 52 open issues (all operator-filed) were matched against the 2026-10-18 backport audit's record of every upstream
first-parent commit since our import bases (Ship of Harkinian 681 commits since `e149e09e0`, 2Ship2Harkinian 274 since
`d05565623`, libultraship 110 since `8c55f607`), plus the commits that landed upstream after that snapshot. One Opus
matcher per upstream repository proposed matches and searched upstream history by the mechanisms the issues name. One
Opus verifier per matched issue then read the issue, the upstream diff and our tree, and answered three questions: does
`main` already resolve the issue (issue bodies lag main), is the upstream change already in our tree, and would porting
it close the issue, part of it, or only a sibling of it. Where the verifier found more upstream commits in the same
table, hook or subsystem, they are listed as the issue's fix family.

| | Count |
|---|---|
| Open issues | 52 |
| Issues with a proposed upstream match | 19 |
| Verified in code (medium or high confidence lead) | 14 |
| Low-confidence leads only (not verified) | 5 |
| No upstream counterpart (epics, decisions, RSBS-only mechanisms) | 33 |

## Summary

| Issue | Upstream fix | Relation | In our tree | Porting it closes the issue | Effort |
|---|---|---|---|---|---|
| #1162 gossip stones say "No Hint" | SoH `2f262f492` (#6824) junk stone hints not generated | fixes | absent | yes | S |
| #1168 checks without the `RG_CAN_SPEAK` clause | SoH `c906c71f7` (#6357), `1413ceb33` (#7121), `0702b11b9` (#6285) | fixes, together | absent | yes | S each |
| #1164 bombchu drops before the bomb bag | none (our drop code equals upstream and is correct; no defect found); siblings SoH `2ba5c40da` (#7134), `483849b97` (#6717), `44909ab23` (#6949), `cc63cc4ab` (#7117) + `c42004bc4` (#7274) | same family (bombchu accounting) | absent | no | S each |
| #607 MM blue-textbox icon for foreign items | none needed | | | **already resolved on main** by PR #761 (`b85d62e1`, "Refs #607") | |
| #1063 texture pack crash | LUS `62d22224`, `a93ee524` (half), `c5a5a934`, `2492cc35`; 2S2H `26146e975`, `f86e04b4d`, `d6ac2550b` | same family | absent / partial | cause unknown; hardening set | S each |
| #1127 MM tracker grouping differs from native | 2S2H `f8bf9bb3b` (#1561), `cfd1116a4` (#1775) | same family / reference | absent | no (adapter is ours) | L / S |
| #1131 tracker auto-scroll and current area | 2S2H `f8bf9bb3b` (#1561), `079f44bae` (#1492) | reference | absent | partially (scene fold only) | M |
| #1142 OoT item tracker rows | SoH `eea5129a8` (#7361), `18f8770ea` (#7151), `ec2a3d7aa` (#6750), `0bae4961a`, `0821c2e31`, `7a7f942b8` (#7103) | template + siblings | absent | no (overlay is ours); they overturn the issue's premises | S to L |
| #1158 missing OoT items in combo tracker | same as #1142 | template | absent | no (mirror `bf645378` for OoT) | M |
| #1161 no logic tooltip in combo tracker | SoH `8b001da31` (#3972, pre-base), 2S2H `f0670114d` (pre-base) | both native trackers have it | ported | no (combo-layer work) | M |
| #1166 combo tracker hides shop contents | SoH `2845baad6` (#6641, ported), `2ba368e7e` (#6822), `6bf5ec514` (#6742) | reference + siblings | ported / absent | no (adapter is ours) | M + S |
| #1170 Link's House Cow available early | none in SoH; 2S2H `9b699dee1` (#1506), `29e30e6b4` (#1864) are MM siblings | same family (MM) | absent | no (design decision) | S / M |
| #1172 enhanced models between games | 2S2H `5d96fd84f` (#1881) + `f56c7939e` (#1891); SoH `e50d03dab` (#6766) | partially fixes (keys) | absent | key half yes; chest half is RSBS-local | M + S |
| #1173 MM reticle lacks SoH's features | SoH `6340ad3d5` (#6279) | reference (OoT-only) | absent | no (new MM code) | S (OoT) + M (MM) |

The issues not in the table either have only a low-confidence lead (#978, #993, #1070, #1141, #1143) or no upstream
counterpart at all (listed at the end with the reason).

## Direct fixes

### #1162 Some gossip stones say "No Hint"

Upstream Ship of Harkinian fixed exactly this in `2f262f492` (#6824 "Junk stone hints not generated"), one line in
`DistributeAndPlaceHints`: `junkIdx = distTable.size()` becomes `distTable.size() - 1`. Our copy still has the pre-fix
line at `games/oot/soh/Enhancements/randomizer/3drando/hints.cpp:525`, inherited from the hint refactor `5d8c3c888`
(#6540) through our `b500ddd1` (#314). `CreateStoneHints` pushes Junk as the last `distTable` entry (`:668`), so the
weighted roll at `:570-579` can never equal `junkIdx` and the `GetRandomJunkHint` branch never runs. Every junk roll
becomes a keyless `HINT_TYPE_HINT_KEY` hint that renders `RHT_NONE`, "No Hint": about six weight units of each
Balanced (default) seed, and every stone under Useless.

Port: change `:525` to `static_cast<uint8_t>(distTable.size() - 1)`; no prefix or single-exe guard (OoT-only C++).
The OoT hint stream and stone assignment change for every seed that rolls junk, so the hint-text goldens re-pin. Worth
adding a "no stone hint renders No Hint" assertion to `ValidateGeneratedHints`.

Same family, absent, can ride the same lane:

- `af485bf64` (#7046 "Fix bugs in hints"): with 1-3 required trials every trial is hinted because the filtered list is
  discarded (`hints.cpp:471`), and dual item hints lose the first location's name variant (`hint.cpp:254-255`). Two
  two-line hunks; moves goldens for seeds with `RSK_TRIAL_COUNT` 1-3 and the dual-hint spoiler fields.
- `983d04d36` (#6194 "Restores missing hint types that got lost in a merge"): `BuildMidoMessage` and
  `BuildBossKeyHintMessage` plus their `COND_ID_HOOK`s are missing from `Messages/StaticHints.cpp`, so Mido's Hint and
  Boss Door Hints show vanilla text, and `BuildMapMessage` can print a literal `[[name]]`. Our #5101 port carried the
  same merge loss upstream made. Effort M; use `OoT_gPlayState`, keep our foreign-item branch in `BuildMapMessage`.
- `0dcd52e5c` (#6551 "Fix warp shuffle without warp hint text"): the `TEXT_WARP_*` hooks are gated on
  `RSK_WARP_SONG_HINTS` (`StaticHints.cpp:390-395`), the same generated-text-never-shown class.
- Already ported, for the record: `dc4b27d65` (#6648 gossip stone Mask of Truth gate, `GossipStoneHints.cpp:19`) and
  the misaligning-erase half of `fd696b5e5` (#6789 Ganondorf hint).

### #1168 Some OoT checks lack the proper `RG_CAN_SPEAK` clause

The issue's example is upstream's `c906c71f7` (#6357 "Song from Malon: requires hylian jabbernut"): one `HasItem(RG_SPEAK_HYLIAN)`
added to `RC_SONG_FROM_MALON`. Ours lacks it at `location_access/overworld/lon_lon_ranch.cpp:14`; the rest of the line
is byte-identical. The verifier compared the `RG_SPEAK_*` sets of every `LOCATION` between upstream develop and our
tree: Song From Malon is the only location missing a speak item, but two more fixes cover "some checks":

- `1413ceb33` (#7121 "Fixed carpenter guard hylian jabbernut requirement"): the four Thieves' Hideout carpenter
  checks (`thieves_hideout.cpp:15/33/57/71`) still use `CanKillEnemy(RE_GERUDO_WARRIOR)` alone while their events at
  `:11/29/53/67` already require Speak Hylian, so the tracker shows the four carpenter keys as available without the
  nut. Points the checks at `LOGIC_TH_COULD_FREE_*`; its timesaver hunk goes to `timesaver_hook_handlers.cpp`.
- `0702b11b9` (#6285 "fix carpet man logic"): `haunted_wasteland.cpp:26` reads
  `CanJumpslash() || CanUse(RG_HOVER_BOOTS) && HasItem(RG_SPEAK_HYLIAN) && price <= wallet`; `&&` binds tighter than
  `||`, so a jumpslash alone satisfies the check. Upstream adds the parentheses. This one also changes placements with
  Shuffle Speak off (the price term is no longer bypassed). The precedence came in with upstream's Shuffle Speak
  `e2db315ff` (#5538), which our `de86df7b2` ported, so it is inherited; filed separately as #1182.

Porting all three should close the issue; only `0702b11b9` moves goldens when Shuffle Speak is off (`logic.cpp:2946-2953`
grants every speak flag when the shuffle is off).

Same family (speak clauses that disagree between logic and game), all absent:

- `ea76550fc` (#6564): the owl flights accept Kokiri in the game (`ShuffleSpeak.cpp` tests all six flags) but logic
  omits `RG_SPEAK_KOKIRI` at `death_mountain_trail.cpp:60` and `lake_hylia.cpp:86/90`. Edit by hand (we use the older
  `Entrance(..., []{...})` lambda form).
- `63597ec63` (#6324): the Diving Game actor is missing from `ShuffleSpeak.cpp`'s Zora case (logic already requires
  Speak Zora for `RC_ZD_DIVING_MINIGAME`), Talon's chickens lack `HasItem(RG_POWER_BRACELET)` at `lon_lon_ranch.cpp:39`
  (Shuffle Grab family), and `z_en_ta.c` lacks the `VB_PREVENT_STRENGTH` wrap. The infrastructure is present
  (`GIVanillaBehavior.h:1887`, `hook_handlers.cpp:1413`).
- `e6947c733` (#6255): Business Scrubs start talking on their own within 130 units, bypassing `VB_SPEAK`
  (`z_en_dns.c:324`). Adds `VB_BUSINESS_SCRUB_SPEAK`. Game side only.
- `c63f57850` (#6265): Deku Theater prize handling when the timesaver is off (`z_en_dnt_jiji.c:262` still offers the
  item unconditionally). Effort M, six files.
- `2bc50f605` (#7220): the Double Cell and Steep Slope carpenters have swapped actor params and flags in our
  check table; take it with `1413ceb33`.
- `a393f48c7` (#6299 "Fix missable Malon check"): `z_en_ma1.c:332-333` still removes Malon unconditionally; adds
  `VB_SEND_MALON_HOME`. Same Song From Malon subsystem, game side.
- `1ea720607` (#6938 Shuffle Zelda's Letter) later rewrites `RC_SONG_FROM_MALON` to
  `LOGIC_MALON_RETURNED_FROM_CASTLE && HasItem(RG_SPEAK_HYLIAN)`; a larger feature, noted so the one-line fix is not
  mistaken for the current upstream state.

### #1164 Bombchu drops do not work until the bomb bag (unconfirmed)

No upstream fix. The OoT drop conversion is identical in our tree (`games/oot/src/code/z_en_item00.c:1549-1553`) and
upstream develop (`:1541-1545`), and so is `EnItem00_ConvertBombDropToBombchu` (ours `:1504-1511`). The rule: with
Bombchu Drops on and any bombchu-bag mode, a bomb drop converts to a bombchu drop only while `INV_CONTENT(ITEM_BOMBCHU)`
is set, and with no bomb bag it then always converts. No upstream commit since our base touches that path except the
unrelated `bf7177256` (#7047 FixFlexDrops). Correction (planning session's triage, 2026-10-26): the code is vendored
and correct. Under Single Bag the drop needs only the Bombchu Bag, and the operator's bag came from the Kakariko
skulltula tree through SoH's own give path, so the earlier lead that a foreign-item or shared-pool grant never set the
`ITEM_BOMBCHU` slot does not fit that run. No defect was found; the report stays unconfirmed. On the MM side 2S2H's
chu-drop enhancement (`Equipment/ChuDrops.cpp`) is not linked in single-exe (`games/mm/CMakeLists.txt:464-465`), so MM
never converts a drop at all.

Same family (bombchu accounting in logic and obtainability), all absent:

- `2ba5c40da` (#7134): chu packs set the `ITEM_BOMBCHU` slot in logic only in bag mode None; ours sets it
  unconditionally (`logic.cpp:2298-2301`), so logic can count a pack as granting chus the player cannot hold (the arms
  dealer case upstream names).
- `483849b97` (#6717): single-bag mode refill obtainability; ours (`randomizer.cpp:451-454`) still answers
  `CANT_OBTAIN_ALREADY_HAVE` once the slot is filled, so refills stop being obtainable.
- `44909ab23` (#6949): bombchu refills enter the junk pool when drops are on.
- `cc63cc4ab` (#7117) with `c42004bc4` (#7274): the inverted `IsBombchus(ignore)` in `fill.cpp:474` and the bag/pack
  effects in `ApplyItemEffect`; confirmed absent by the 2026-10-18 audit's verifier.

## Already resolved on main

### #607 MM blue-textbox icon for foreign items

PR #761 (`b85d62e1`, 2026-09-27, "Show a foreign item's OoT icon in MM's blue get-item textbox") did what the issue
asks: one fenced `MM_ForeignTextboxIcon_OnDecodeHeader(play)` call at the end of `Message_DecodeHeader` in
`games/mm/src/code/z_message.c`, the side-channel fields on `CustomMessage::Entry`, the `src/common/foreign_textbox_icon`
registry and a `ForeignTextboxIcon` CTest row. Its commits said "Refs #607", not "Fixes", so the issue stayed open. The
operator can close it. The proposed upstream match, SoH `5c8ff7655` (#6343 "Custom Item Icons in Textboxes"), solves a
different problem (custom icons in OoT's own textbox) and would renumber `ITEM_ROCS_FEATHER`, which our saves store
(`randomizer.cpp:4211`); do not port it for #607. One sibling is useful: `eb272ef68` (#6783) adds Climb, Crawl and Open
Chests textures, which would let those three leave our deliberate no-icon list.

## Same family, not a direct fix

### #1063 OoT texture overhaul pack crashed after a while

Main does not resolve it and the cause is still unknown (no dump, pack name or repro). The issue's first suspect, the
#557 GL texture-name overflow, was already fixed in the build the operator crashed on (fork commit `477633a`, the
growable `mTextures` in `libultraship/include/fast/backends/gfx_opengl.h:105-123`). Unported siblings in the same
texture-cache path, all small, suitable as one fork hardening PR:

| Commit | What it closes | Our state |
|---|---|---|
| LUS `62d22224` (#1041) | nulls dangling `mRenderingState.mTextures[j]` on cache clear, LRU eviction and delete; `map.reserve` to stop rehash. OoT clears the cache on every scene change while alt assets are on | absent (`src/fast/interpreter.cpp:408-413` clears without nulling) |
| LUS `a93ee524` (#1006) | `TEXTURE_CACHE_MAX_SIZE` 500 to 1024 (the vector half we already carry as `477633a`) | half ported |
| LUS `c5a5a934` (#1008) | null guard after address resolution in eleven `ImportTexture*` functions | absent (`:483` to `:811` dereference unchecked) |
| LUS `2492cc35` (#1043) | null-palette guards in CI4/CI8 (`ImportTextureCi4` `:715-735` dereferences the palette) plus the stride refactor | absent |
| LUS `67861a90` (#1042) | reject wild addresses in `gfx_check_image_signature` (ours rejects only odd and zero pointers) | absent |
| LUS `49d640d3` (#1049) | ASan/UBSan fixes incl. a write through a dangling reference in `gfx_end_dl_handler_common` (`:3185`) | absent |
| LUS `adaad35d` (#1040) | backend null and zero-dimension guards | partially (GL half present, DX11/Metal absent) |
| 2S2H `26146e975` (#1980), `f86e04b4d` (#1800) | unregister the player's and Kafei's skeletons on destroy (`MM_Player_Destroy` `z_player.c:13506-13522`, `EnTest3_Destroy` `z_en_test3.c:559-572` have no unregister) | absent |
| 2S2H `d6ac2550b` (#1689) | de-duplicate `RegisterSkeleton` and erase every match in unregister | absent; must go into OoT's shared patcher `games/oot/soh/resource/type/Skeleton.cpp`, since MM's copy is not compiled |

The skeleton commits only matter on an alt-asset toggle (Tab). Not plausible causes and large: LUS `9a1a2bd9`
(.meta alias priority, L) and `ec194d6b` (HD texture clamping, L, sits on `2492cc35`).

**RSBS-only defect found on the way (not upstream):** in single-exe MM registers its skeletons into OoT's patcher, and
OoT's `UpdateSkeletons` (`games/oot/soh/resource/type/Skeleton.cpp:108-124`) writes `skeleton` plus the OoT-only
`skeletonHeader` with no type switch, past every MM `SkelAnime`. 2S2H's `4243fd92f` (#1623) shows the shape of a fix
(skip null `skelAnime`, switch on skeleton type). This deserves its own issue.

### #1127 MM tracker grouping differs from MM's native tracker (deferred by D9)

The gap is our adapter's: `MMCheckScenes` (`games/mm/2s2h/Rando/TrackerAdapterSingleExe.cpp:141-191`) gives an enemy
drop its first region only (`:174-181`) and falls back to the raw static scene for unresolved grottos (`:186-190`).
Upstream's native tracker still drops unconnected grottos and lists multi-region checks under every region, so nothing
upstream closes the issue. Two absent siblings define the reference the lane must choose between:

- `f8bf9bb3b` (#1561): rewrites the native grouping as one walk over `Regions`, folds 27 interior scenes into their
  parent (`scenesToCheckParent`), multi-lists nine more checks, and changes the grotto logic graph and hint text. L,
  moves goldens if taken whole; port only the scene fold into the adapter if the lane targets upstream HEAD.
- `cfd1116a4` (#1775): header-name overrides (Southern Swamp, Mountain Village, Goron Village, Path to Goron Village,
  Lone Peak Shrine). S, belongs in `MMTrackerAreaName`; the Lone Peak label is only right once unconnected grottos are
  dropped.

The live divergence today is first-region-only enemy drops; the unconnected-grotto fallback looks latent on the current
graph.

### #1131 Tracker auto-scroll and Only Show Current Area (lane L4)

Not on main: `src/common/ComboTrackerWindow.cpp` has no scroll code and no current-area accessor. Upstream 2S2H
`f8bf9bb3b` is the useful reference: `GetScrollTargetScene` resolves a grotto or interior to its parent scene via
`regionParentSceneMap`. Our MM adapter already groups grotto checks that way, so the MM accessor needs the same mapping,
keyed like `MMTrackerAreaName`. Our regions cannot resolve shared-entrance grottos from `save.entrance`; resolve the
parent from `respawn[RESPAWN_MODE_DOWN]` or take the region rewrite (moves goldens). `079f44bae` (#1492) is a design
reference only (scroll after the header separator, a manual scroll button); it changes MM's native window.

### #1142 and #1158 OoT rows in the combo Item Tracker

Not on main: `games/oot/soh/Enhancements/randomizer/ItemAdapterSingleExe.cpp` `kRows` (`:259-332`) still holds only SoH's
default sections; the comment at `:255-258` lists the groups left out. Nothing upstream changes our overlay, but the
cited SoH commits overturn the issue's premises and are the template the IT-2 lane should follow instead of the
base-era code the issue cites:

- `eea5129a8` (#7361, after the audit snapshot): dungeon items as fixed `ItemTrackerDungeonColumn` arrays, TCG keys,
  `ShowDungeonItem` rules. Our `ITEM_TRACKER_ITEM` takes three arguments, upstream's four (drop the `""`).
- `ec2a3d7aa` (#6750) then `0bae4961a`: key counts from held keys plus opened-door bits (`GetSceneSmallKeyMax`,
  `GetSceneTotalSmallKeys`), replacing `ship.stats.dungeonKeys` and the `*_SMALL_KEY_MAX` macros; also removes the
  archive-dependent `GetDungeonSmallKeyDoors` (the #330 headless bail). L.
- `18f8770ea` (#7151): hide a section unless the seed shuffles it (`GetGatedSectionDisplay`); needs the frozen OoT
  options at read time. Drop the Silver Rupee and `RSK_GANONS_SOUL` gates (both come from features we lack).
- `0821c2e31` (#6142): `HasBossSoul`, an unshuffled soul counts as held (ours reads `RAND_INF_*_SOUL` raw at
  `randomizer_item_tracker.cpp:940-985`).

Two siblings are live bugs in our SoH tracker, not just templates:

- `7a7f942b8` (#7103): `DrawItem` runs `INV_CONTENT(item.id)` for every row (`randomizer_item_tracker.cpp:827`),
  an out-of-bounds read of `OoT_gItemSlots` (sized 56) for `RG_` and high `ITEM_` rows. Guard on
  `item.id < ARRAY_COUNT(OoT_gItemSlots)`; same family as #983/#984 (custom ItemID table bounds).
- `29c7e7568`: `Item_GetSlot` returns `SLOT_NONE` past `gItemSlots`, stopping `Item_Give` from writing an unrelated
  inventory slot (Gold Skulltula tokens overwrote the Bow slot upstream). Same bounds family.
- `4d7b885db` (#7298): `TrackerHasSave` lets the tracker read the save during the credits.

### #1161 Hovering a combo tracker row shows no logic

Both native trackers already show logic on hover: SoH through `8b001da31` (#3972, before our base; `ShowLogic` CVar,
`LocationAccess::GetConditionStr`) and 2S2H through `accessLogicFuncs` (`f0670114d`, before our base; in our
`games/mm/2s2h/Rando/CheckTracker.cpp:130/413`). The combo window's `DrawCheckRow` (`src/common/ComboTrackerWindow.cpp:267-331`)
has no tooltip and neither adapter interface carries a logic string. Combo-layer work (M): add a per-check logic op to
`ComboOoTTrackerOps` and `ComboMMTrackerDesc`, draw it behind SoH's `ShowLogic`. SoH `077fda874` (#6031) stringifies
entrances and events nothing displays; not needed.

### #1166 Combo tracker does not show shop contents

Our adapter's rule: `OoTTrackerCheckAt` (`games/oot/soh/Enhancements/randomizer/TrackerAdapterSingleExe.cpp:384-425`)
names an item only once found. SoH's `DrawLocation` names a seen shop item unless Mysterious Shuffle is on and adds the
price once identified; that logic (`2845baad6`, #6641) is already in our SoH tracker. Fix RSBS-side (M): mirror
`revealItemName` and the ice-trap naming in the adapter, add a price to `ComboTrackerCheckRow`. 2S2H's tracker names
only obtained checks, so MM has no reference. Siblings to take first so the combo pane does not inherit them:

- `2ba368e7e` (#6822): `ItemLocation::CanBePurchased` gates the price line; absent, so identified skull-house and
  similar checks show "- 0" today. Three clean hunks, S.
- `6bf5ec514` (#6742): the bean salesman is listed when unshuffled (`randomizer_check_tracker.cpp:1797` lacks the
  `rc != RC_ZR_MAGIC_BEAN_SALESMAN` clause; its companion `31cbeb929` is already ported at `:1797-1798`).
- `783139310`: hinted checks marked SEEN (needs #6906); optional.

### #1170 Link's House Cow available before the side quest

No SoH fix. Upstream's cow logic (`kokiri_forest.cpp:199`, `lon_lon_ranch.cpp:11`) matches ours (`kokiri_forest.cpp:127`,
`lon_lon_ranch.cpp:11`) except for macro form: `LOGIC_LINKS_COW` has the same terms as `LOGIC_FREED_EPONA`, so SoH's logic
assumes Malon's obstacle race can be done and SoH's own tracker shows the cow the same way. Changing that is an RSBS
design decision (a tracker-only gate on `EVENTCHKINF_WON_COW_IN_MALONS_RACE`, for example); the Shared Epona's Song
setting may also surface the cow early. Same-kind siblings, absent:

- SoH `63597ec63` (#6324, Talon's chickens need grab) and `c906c71f7` (#6357, Malon's song needs Speak Hylian), in the
  same Lon Lon Ranch logic file (see #1168).
- 2S2H `9b699dee1` (#1506 "Some time logic fixes"): the Romani Ranch barn and field cows, Epona's Song and the alien
  drop are gated on breaking the Milk Road boulder (Goron + Powder Keg in the Day 1 or Night 1 window) or
  `RE_COWS_FROM_ALIENS`; applies beside our `MMRT_GORON_BOMB_JUMP` block (`MilkRoad.cpp:126-141`). S, moves MM goldens.
- 2S2H `29e30e6b4` (#1864 Kafei's quest and time shuffle): adds `RE_MAIL_LETTER_TO_KAFEI`, which grows
  `randoEvents[RE_MAX]` inside MM's `SaveContext` (`games/mm/include/z64save.h:389`) and shifts every later field;
  the frozen blob and `.redsave` layout must be checked. M.
- 2S2H `b754231c3` (#1628 nnl-cows): Nearly-No-Logic could place Epona's Song on a cow; absent from
  `NearlyNoLogic.cpp`.
- Already ported: 2S2H `e87243caa` (#1621 aliens require Night 1), via our `473c030c` (#296).

### #1172 Enhanced models between games

Two halves. Keys: 2S2H `5d96fd84f` (#1881 "Add Dungeon Specific Keys") adds custom small and boss key models with
per-temple emblems under `mm/assets/custom/objects/object_gi_key` and draws them in `Rando/DrawItem.cpp`; our
`DrawSmallKey`/`DrawBossKey` only tint the vanilla keys. M; take its follow-up `f56c7939e` (#1891), which removes the
toggle and routes colours through the Cosmetics Editor; prefix the GBI calls (`MM_gSPDisplayList`, `MM_gPlayState`) and
make `DefaultKeyColors`/`emblemDLs` static (ODR tripwire). Chests: no port needed. SoH `a134d2c59` (in our base) already
ships mask and stray-fairy chest models in `games/oot/assets/custom/objects/object_box`, but they are never declared or
selected: `OoT_Rando_Foreign_ChestCategory` turns the FAIRY and MASK grades into MAJOR, while heart chests for health
items already work. Wiring the other two is a small RSBS-local change. Sibling: SoH `e50d03dab` (#6766) re-exports the
CMC chest display lists so chest fade-in works; absent.

### #1173 MM aiming reticle lacks SoH's features

No upstream fix: 2S2H at `b30a286c5` still draws the fixed-colour `gHookshotReticleDL`
(`games/mm/src/code/z_player_lib.c:2214`, `BowReticle.cpp`). SoH's reticle turns green only on a hookshot-surface
collision, never on enemies, and `6340ad3d5` (#6279) moved it into `Items/HookshotReticle.cpp` on
`VB_TARGETABLE_HOOKSHOT_RETICLE`, fixing a bug our inline copy still has (`games/oot/src/code/z_player_lib.c:1720`: Bow,
Slingshot and Boomerang reticles also turn green on hookshot surfaces). That OoT port is S. Closing the issue needs new
MM code (M): colour `MM_Player_DrawHookshotReticle` and `BowReticle.cpp` through `MM_SurfaceType_IsHookshotSurface`,
driven by the shared OoT CVar. The "green on enemies" half has no upstream reference. Related refactor `d9ebce9f9`
(#6278) and its regression fix `412b60a02` (#6520) only matter if that refactor is ported.

## Low-confidence leads (not verified)

| Issue | Lead | Note |
|---|---|---|
| #978 shared masks P2 | SoH `eca767dcb` (#7032 Logical bunny hood) | our tree already has `HasItem`/`CanUse` cases for Keaton, Bunny, Goron and Zora masks (`logic.cpp:322-347`) |
| #993 reachability projection | SoH `f5264be19` (#6165 Recalculate Available Checks from Current Region) | SoH-side mechanism only; the combo projection is ours |
| #1070 Woodfall does not rise | 2S2H `ec9e755bb` (#1598 Custom Ocarina Controls) | not a fix; `Dm_Char01` and `Rando/DmChar01.cpp` are unchanged upstream since our base |
| #1141 Item Tracker settings keys | SoH `18f8770ea` (#7151) | the gating helper, see #1142 |
| #1143 count and metric parity | SoH `ec2a3d7aa` (#6750), 2S2H `8e7ecc2e7` (#1831) | key-count and number-visibility references, see #1142 |

## No upstream counterpart

Checked and deliberately not matched, with the matcher's reason:

- **#1157 jabber stones only in OoT**: 2S2H has no speak or jabber shuffle (no matching `RO_SHUFFLE_*` in `Rando/Types.h`).
- **#1159 heart pieces, #1160 bombchus, #1163 rupees not shared, #1145 shared Kokiri Sword**: RSBS shared-resource
  accounting (`Combo_ItemClassSharedKind`, pickup credit); no upstream equivalent.
- **#1165 stray fairies not animated in OoT**: MM's native `DrawStrayFairy` is already animated (`AnimatedMat`); the
  OoT-side draw is our curated-model code, and libultraship has nothing for it.
- **#1167 MM songs in OoT shops show `?`, #1169 cross-game warp songs, #1171 Gerudo mask texture in MM**: foreign-model
  and cross-game paths. For #1171 neither the CI4 palette fix (`709efa73`) nor the TEXEL1 fix (`f7600e02`) applies.
  Correction (planning session's triage, 2026-10-26): the broken model is the get-item model, not the worn
  `gLinkChildGerudoMaskDL` this report first named; no rule key authors `RSBS_XITEM_GERUDO_MASK_MM`, so the worn path is
  unreachable. The fault is MM's host emitting MM's own setup display list 26 (2-cycle, `games/mm/src/code/z_rcp.c:297-306`)
  for OoT's 1-cycle recipe (`games/oot/src/code/z_rcp.c:296-305`); the six medallions share it. Game side, not
  libultraship. #1169 is also a documentation defect: `known-issues.md` and `release-notes-next.md` describe cross-game
  warp songs as existing (from #941 and #972) while no code implements them.
- **#1070 Woodfall Temple does not rise**: `Dm_Char01` and `Rando/DmChar01.cpp` unchanged upstream.
- **#824, #925 flaky test rows**: the rows are RSBS C tests exiting through `rsbs/src/main.cpp:556-561`; the slot-0
  `.redsave` writer already writes atomically (`src/common/save.cpp:507-529`). LUS `d4f9b849` (atomic `Config::Save`)
  is the same idea applied to `shipofharkinian.json`, not this failure.
- **#694 MM kaleido game-over art, #966 MM settings not honoured, #1133 Combo header retirement**: single-exe menu and
  linkage questions; nothing upstream.
- **#310, #460, #492, #497, #500, #564, #573, #574, #577, #645, #975, #976, #977, #979, #980, #1132, #1134, #1146,
  #1151**: epics, trackers, QA passes and design decisions.

## Fix families worth taking as lanes

In order of how much they close:

1. **Hint generation** (closes #1162): `2f262f492`; with `af485bf64`, `983d04d36`, `0dcd52e5c`. One OoT lane, one
   golden re-pin.
2. **Shuffle Speak logic clauses** (closes #1168): `c906c71f7`, `1413ceb33` + `2bc50f605`, `0702b11b9`; with
   `ea76550fc`, `63597ec63`, `e6947c733`, `a393f48c7`, `c63f57850`. One OoT lane; goldens move only through
   `0702b11b9` with Shuffle Speak off.
3. **Item-table bounds** (same family as #983/#984): `7a7f942b8`'s `gItemSlots` guard and `29c7e7568`'s `Item_GetSlot`.
   Two small OoT fixes with a ROM-free lock each.
4. **libultraship texture-cache hardening** (#1063 family): `62d22224`, the cache-size half of `a93ee524`, `c5a5a934`,
   `2492cc35`'s palette guards, `67861a90`, `49d640d3`, `adaad35d`'s DX11/Metal half. A fork PR; the fork's `main`
   lags our pin by 12 commits on `claude/*` branches, so settle the fork branch first.
5. **MM skeleton registration** (#1063 family, plus the RSBS-only `UpdateSkeletons` overwrite): `26146e975`,
   `f86e04b4d`, `d6ac2550b` into OoT's shared patcher.
6. **Check tracker pricing and listing** (prerequisite for #1166): `2ba368e7e`, `6bf5ec514`.
7. **MM logic holes** (#1170 family): `cc5d87143` (Romani's Mask for the Circus Leader, `Central.cpp:403`),
   `9b699dee1`, `b754231c3`; `29e30e6b4` only after the save-layout check. MM goldens move.
8. **MM dungeon key models** (#1172 key half): `5d96fd84f` + `f56c7939e`.
9. **Item tracker counting** (template for #1142/#1158): `0821c2e31`, `0bae4961a`, `ec2a3d7aa`, `18f8770ea`,
   `eea5129a8`, `4d7b885db`.
10. **Reticles** (#1173 OoT half): `6340ad3d5`.
