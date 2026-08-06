/**
 * ForeignItemsSingleExe.cpp — the MM-side redemption give for cross-game items
 * (Lane 6 / #502; ADR 0002, ADR 0005).
 *
 * The MM twin of soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp. It is
 * the ONE MM translation unit where the cross-game item class may name real
 * RI_* enumerators, because everything leaves it as an origin-tagged SharedItem
 * (or a display string) and a raw RI_* therefore never crosses a game boundary
 * (ADR 0002, the #356 bug class). It holds BOTH halves of MM's side of the
 * cross-game item class: the redemption give (below) and — since #510 —
 * kForeignPoolMMV1, the reverse-direction SOURCE pool that OoT's placement pass
 * draws from, with its Combo_RegisterForeignItemPool file-scope registrar.
 *
 * Lives in 2s2h/Rando/, glob-collected into `2ship_rando`, which links
 * WHOLE_ARCHIVE — so this TU survives the link with no CMake edit.
 *
 * ============================================================================
 * WHY THE GIVE IS DEFERRED RATHER THAN IMMEDIATE
 * ============================================================================
 *
 * MM's redemption point runs BEFORE MM_gPlayState is assigned:
 * MM_ConsumeSharedItems() is called from MM_Play_ConsumeStartupEntrance at
 * z_play.c:2406, while `MM_gPlayState = this` happens at z_play.c:2468. That is
 * not an accident to be tidied away — the consume has to run after the frozen
 * save is restored and before gameplay observes it, which is exactly where it
 * sits.
 *
 * MM's give path is not NULL-play tolerant the way OoT's starting-item give is.
 * Rando::GiveItem's default branch is `MM_Item_Give(MM_gPlayState, itemId)`,
 * and Item_GiveImpl (z_parameter.c:4136) carries only PARTIAL 2S2H nullptr
 * guards — the sword and shield branches, added so OnFileCreate could grant
 * starting items outside gameplay. Three legs still deref `play` unguarded:
 *
 *   - Inventory_IncrementSkullTokenCount(play->sceneId)   (:4150, ITEM_SKULL_TOKEN)
 *   - Interface_LoadItemIconImpl(play, EQUIP_SLOT_C_*) and its Dpad twin, in
 *     the bottle-content loop                            (:4524-4550)
 *   - the same pair in the trade-item branch             (:4575, :4583)
 *
 * The last two are the ones that make an item-by-item audit treacherous. They
 * are reached only when the player ALREADY holds a bottle / trade item in that
 * slot and has it C- or D-equipped — which is impossible at file creation and
 * entirely ordinary on a mid-game arrival. That is precisely why
 * Rando::GrantStartingItems' NULL-play gives have never tripped them, and why
 * its success proves a NULL-play give works for SOME items in SOME save states
 * rather than for any item. A redemption is the mid-game case by definition.
 *
 * The lane brief offers two ways out: restrict the pool to a hand-audited
 * NULL-play-safe RI_* set, or move redemption to the gameplay-gated frame-tick
 * safe point shared_items.h:52-64 already defines. This TU takes the second,
 * for a reason specific to how the work is split: the reverse-direction pool
 * (kForeignPoolMMV1) is authored by a DIFFERENT lane, later. An allowlist
 * audited against today's pool would be a correctness guarantee that silently
 * expires the moment somebody adds a row — the failure mode being a crash on
 * the arrival path, i.e. the worst place to learn about it. Deferral is
 * item-agnostic and O(1) in pool size, so it cannot rot that way.
 *
 * The deferral is scoped as tightly as it can be: MM_AwardSharedItem still
 * runs at the presence-gated arrival point and still consumes the redemption
 * there (Combo_RedeemSharedItemsForGame sets RSBS_SHARED_ITEM_REDEEMED on
 * return, and we deliberately do NOT clear the entry). Only the give itself
 * waits, in a process-global RAM queue, for the first gameplay frame with a
 * live PlayState — a handful of frames later, in the same boot, with no player
 * input possible in between.
 *
 * DURABILITY OF THAT WINDOW. The obvious objection is "redeemed in gComboCtx
 * but not yet given — a crash there loses the item". It does not: the redeemed
 * bit only becomes durable when a .redsave is WRITTEN, and no save can be taken
 * between MM_Play_ConsumeStartupEntrance and the first Play frame. If the
 * process dies inside that window, the .redsave on disk still shows the entry
 * un-redeemed and the next arrival re-awards it. The queue therefore never has
 * to survive a process, which is why it is RAM-only by design rather than by
 * omission.
 *
 * ONE DEPENDENCY WORTH NAMING. The drain hangs off
 * Rando::MiscBehavior::CheckQueue, which is a COND_ID_HOOK gated on IS_RANDO
 * (MiscBehavior.cpp:25). A save whose IS_RANDO hooks are not armed therefore
 * never drains — the #439/#487 class of bug, where an arrival path leaves the
 * hooks matched against the wrong save. That is the correct coupling rather
 * than a gap to paper over: a save with no randomizer identity has no business
 * receiving cross-game randomizer items, and if the hooks are unarmed the
 * player has much larger problems than one un-given item. It does mean this
 * give inherits those locks' guarantees, so it is written down here.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include <string>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/StaticData/StaticData.h"
// The arrival toast (#494), sent through MM_Notify_Emit — the explicit #427
// item-1 bridge to OoT's shared overlay. MM's own BenGui/Notification.cpp is
// excluded from single-exe builds and Notification::Emit is deliberately NOT
// declared there; the layouts both sides compile against are locked by
// src/common/notification_layout_probe.h and mm_notification_binding_test.cpp.
#include "2s2h/BenGui/Notification.h"

extern "C" {
#include "variables.h" // MM_gPlayState
#include "functions.h"
}

// src/common. Included OUTSIDE any extern "C" block: the header manages its
// own linkage and pulls in <stdbool.h>/<stdint.h> (matching Foreign.cpp).
#include "foreign_items.h"
#include "shared_items.h" // RSBS_SHARED_ITEM_CAP (via context.h)

// ============================================================================
// kForeignPoolMMV1 — the reverse-direction SOURCE pool (#510)
// ============================================================================
//
// The MM items OoT's placement pass (OoT_PlaceForeignItems) may host in an OoT
// check. The OoT twin of this table is kForeignPoolV1 in
// soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp; both are defined in
// the one TU where their own enum is in scope and leave only as origin-tagged
// SharedItems (ADR 0002).
//
// SCOPE: this is deliberately a BROAD pool, not a pinned handful.
// RSBS_FOREIGN_PLACEMENT_CAP (8) caps PLACEMENTS PER SEED, not candidates — the
// placement pass draws a random subset — so a wide pool buys seed-to-seed
// variety at zero runtime cost. There is intentionally no
// static_assert(count <= CAP) here; the OoT pool has one only because it
// predates the reverse direction and happens to be smaller than the cap.
//
// ---------------------------------------------------------------------------
// THE MEMBERSHIP RULE (and why each exclusion is a rule, not a taste)
// ---------------------------------------------------------------------------
// An MM item is a v1 candidate iff ALL of:
//
//  (1) It is a real item — not a sentinel. RI_UNKNOWN (enumerator 0, i.e. a
//      zero-initialised slot), RI_NONE ("literally nothing") and RI_JUNK are
//      excluded; MM_ForeignItem_Give's IsGiveableItemId refuses the first two
//      outright (the #488 sentinel trap).
//
//  (2) Its randoItemType is not RITYPE_JUNK. The junk class is what a foreign
//      HOST check degrades to when the placement table is absent — the OoT host
//      physically holds an OoT junk item — so crossing MM junk would be a
//      strictly worse duplicate of what is already there, and would burn one of
//      at most 8 slots on a green rupee. Excluded with it: RI_GOLD_DUST_REFILL,
//      which is typed RITYPE_LESSER but is semantically a bottle refill.
//
//  (3) Its give in Rando::GiveItem is UNCONDITIONALLY effectful — it changes MM
//      save state whatever options the paired MM world was generated under.
//      This is the load-bearing one, and it is what excludes the enemy/boss
//      SOULS, the OCARINA BUTTONS, RI_ABILITY_SWIM, and the RI_TIME_* /
//      RI_TIME_PROGRESSIVE clock items: each of those gives is a bare
//      rando-inf/week-event flag whose only meaning comes from a specific MM
//      shuffle setting, and OoT's placement pass runs at OoT generation time —
//      possibly before the paired MM world exists at all — so it CANNOT read
//      MM's option profile to find out. A settings-gated entry would be a
//      crossing the player is promised in OoT ("it will be awarded there!") and
//      then silently never receives in Termina, which is worse than no crossing.
//      (Souls are also ~55 rows; admitting them would make a uniform draw of 8
//      mostly souls and crowd out everything else. That is the secondary reason,
//      not the reason.)
//
//  (4) Its give fires no GLOBAL WORLD EVENT. This excludes RI_TRIFORCE_PIECE and
//      RI_TRIFORCE_PIECE_PREVIOUS: Rando::GiveItem increments
//      foundTriforcePieces and, on reaching RO_TRIFORCE_PIECES_REQUIRED,
//      recursively gives RI_SOUL_BOSS_MAJORA, fires
//      GameInteractor_ExecuteOnGameCompletion() and QUEUES A FORCED SCENE
//      TRANSITION (GiveItem.cpp:109-125). Detonating game completion from the
//      foreign-redemption flush — the first gameplay frame after an arrival — is
//      exactly what a cross-game seam must not do. The piece count is also a
//      per-world goal quantity that extra crossings would silently inflate.
//
//  (5) It is a reward, not a punishment: RI_TRAP is excluded. The OoT-side
//      pickup text promises an award in Termina; delivering MM's trap machinery
//      instead would make that text a lie.
//
//  (6) It is NOT a shared cross-game resource (#525). Rupees, hearts, magic and
//      ammo are one quantity spanning both games now — one wallet, one health
//      bar, one magic meter, one quiver, capacity AND current value
//      (src/common/shared_resources.h) — so there is nothing left for a
//      wallet, heart, magic or ammo item to CROSS. Shipping one anyway would
//      hand the player a second copy of a capacity they already have, or worse,
//      a rupee award that the next harvest reconciles straight back out. Six
//      rows left with the sharing that replaced them: RI_PROGRESSIVE_WALLET,
//      RI_WALLET_ADULT, RI_WALLET_GIANT, RI_DOUBLE_DEFENSE, RI_HEART_CONTAINER
//      and RI_HEART_PIECE. Three more left when shared magic landed (#525's
//      optional tier): RI_PROGRESSIVE_MAGIC, RI_SINGLE_MAGIC and
//      RI_DOUBLE_MAGIC. Eight more left with shared ammo: RI_BOMB_BAG_20,
//      RI_BOMB_BAG_30, RI_BOMB_BAG_40, RI_QUIVER_40, RI_QUIVER_50,
//      RI_PROGRESSIVE_BOMB_BAG, RI_BOW and RI_PROGRESSIVE_BOW. And one more
//      with the shared hookshot: RI_HOOKSHOT. Both games keep the hookshot in a
//      single inventory byte, which makes it a monotonic TIER (0 none, 1
//      hookshot, 2 longshot) rather than an item to hand over once — see
//      RSBS_SHARED_RES_HOOKSHOT_TIER.
//
//      THE BOW ROWS LEAVE FOR A REASON WORTH SPELLING OUT, because "a bow is
//      not ammo" is the obvious objection. MM does not separate the two: its
//      own give for the bow sets UPG_QUIVER to 1 (z_parameter.c), so in MM
//      owning the bow IS quiver tier 1 — and that tier is now the shared
//      resource. A bow crossing would therefore mutate the shared quantity,
//      which is precisely what this criterion excludes. The same logic retires
//      OoT's RG_FAIRY_BOW from its own pool.
//
//      Note RI_DOUBLE_DEFENSE was NOT in the "Health" block — it sat under core
//      equipment, so a block delete misses it. The block-shaped grouping below
//      is presentational; criterion 6 is not.
//
// Everything else is IN — every mask, every song, every bottle, both shields,
// the sword upgrades, the
// progressives (which resolve against the LIVE MM save, so they are the single
// best-behaved cross-game gift, and the OoT pool sets the precedent with
// RG_PROGRESSIVE_STRENGTH), the boss remains, the
// deeds and letters and other sidequest items, the owl statues, the Tingle maps,
// and the dungeon keys/maps/compasses/stray fairies.
//
// One property makes that breadth safe rather than reckless: a crossing is
// ADDITIVE. The MM world's own fill is untouched — nothing is removed from it to
// pay for a foreign placement (the OoT host keeps its own junk item too) — so no
// crossing can make MM unwinnable. Extra keys, extra fairies and extra remains
// can only ever trivialize, never block.
//
// NULL-play safety is NOT a membership criterion, deliberately. MM's give is
// deferred to the first frame with a live MM_gPlayState (see the file header),
// which is item-agnostic and O(1) in pool size — the #502 design note says in as
// many words that an allowlist audited against today's pool would expire the
// moment somebody added a row. This pool is that row, 116 times over, and it
// relies on the deferral instead of re-auditing it.
//
// ---------------------------------------------------------------------------
// THE CLASS COLUMN (#495, ADR 0011 decision 3)
// ---------------------------------------------------------------------------
// Every row names the ONE RSBS_ITEMCLASS_* bit it belongs to, and the pool draw
// is a RULE EVALUATION over those bits rather than "the whole array, in order":
// OoT's placement pass calls Combo_ForeignPoolDrawFor, which filters this table
// by the FROZEN itemClassMM bitset and preserves pool order.
//
// THE CRITERIA RUN FIRST, THE BITSET SELECTS AMONG SURVIVORS. Every row below
// already passed the six criteria above; a class bit can only ever narrow that
// set, never widen it, so no bit can readmit a #525 shared cross-game resource.
// That ordering is the invariant, not a convention (see foreign_items.h).
//
// NO SEED TERM (accepted answer O3). The class is a pure function of (this
// table, the frozen bitset). Seed-to-seed variety already comes from the
// placement draw, which picks pool entry AND host without replacement from an
// identity-seeded stream; a second randomisation of the same axis would buy
// nothing and would make Combo_GetForeignItemByNameFor PARTIAL on the
// spoiler-LOAD path, which runs in processes that never generated.
//
// WHERE THE BLOCK HEADINGS AND THE CLASSES DISAGREE, THE CLASS WINS -- the
// headings below are presentational groupings that predate the rule:
//   - "Progressive Goron Lullaby" sits under progressives but is a SONG.
//   - "Bomber's Notebook" sits under core equipment but is the sidequest
//     tracker, so it is SIDEQUEST.
//   - "Ocarina of Time" sits under quest items but is PROGRESSION: every song
//     in the game is unusable without it.
//   - The boss REMAINS are DUNGEON_REWARD (MM's medallion analogue), while the
//     keys/maps/compasses are DUNGEON_ITEMS -- the bit table's own split.
//   - Stray fairies are SIDEQUEST, not DUNGEON_ITEMS: the bit's published
//     meaning is "small/boss keys, maps, compasses", and the fairy chain is a
//     Great Fairy reward sidequest that happens to be dungeon-scoped.
//
// Display names are MM's own (Rando::StaticData::Items) verbatim. They are a
// PERSISTENCE KEY, not decoration: Combo_GetForeignItemByNameFor is the
// spoiler-LOAD inverse, so renaming an entry breaks spoiler round-trips for
// already-generated worlds. "Lens of Truth" is deliberately identical to the
// OoT pool's row of the same name — that collision is real, is the reason the
// lookups are keyed on (originGame, name), and is asserted in
// test_foreign_items.c rather than dodged. It inherited that job from "Bomb
// Bag", which was the colliding pair until shared ammo retired both halves;
// OoT's pool gained its Lens row in the same commit that deleted them, because
// the collision test goes red the instant one side exists without the other.
static const ComboForeignItemDef kForeignPoolMMV1[] = {
    // --- Progressive upgrades: resolve against the live MM save at give time.
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_PROGRESSIVE_SWORD }, "Progressive Sword", "a ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_PROGRESSIVE_LULLABY },
      "Progressive Goron Lullaby",
      "",
      RSBS_ITEMCLASS_SONGS },

    // --- Core equipment, abilities and capacity upgrades.
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_LENS }, "Lens of Truth", "the ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_ARROW_FIRE }, "Fire Arrows", "", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_ARROW_ICE }, "Ice Arrows", "", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_ARROW_LIGHT }, "Light Arrows", "", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_POWDER_KEG }, "Powder Keg", "a ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_PICTOGRAPH_BOX }, "Pictograph Box", "a ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MAGIC_BEAN }, "Magic Bean", "a ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_BOMBERS_NOTEBOOK }, "Bomber's Notebook", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SHIELD_HERO }, "Hero's Shield", "the ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SHIELD_MIRROR }, "Mirror Shield", "the ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SWORD_KOKIRI }, "Kokiri Sword", "the ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SWORD_RAZOR }, "Razor Sword", "the ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SWORD_GILDED }, "Gilded Sword", "the ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_GREAT_FAIRY_SWORD },
      "Great Fairy's Sword",
      "the ",
      RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_GREAT_SPIN_ATTACK },
      "Great Spin Attack",
      "the ",
      RSBS_ITEMCLASS_PROGRESSION },

    // --- Bottles (the bottle itself, not its refills).
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_BOTTLE_EMPTY }, "Empty Bottle", "an ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_BOTTLE_MILK }, "Bottle of Milk", "a ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_BOTTLE_RED_POTION },
      "Bottle with Red Potion",
      "a ",
      RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_BOTTLE_GOLD_DUST },
      "Bottle With Gold Dust",
      "a ",
      RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_BOTTLE_CHATEAU_ROMANI },
      "Bottle of Chateau Romani",
      "a ",
      RSBS_ITEMCLASS_PROGRESSION },

    // --- Masks. All 24, transformation masks included.
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_DEKU }, "Deku Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_GORON }, "Goron Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_ZORA }, "Zora Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_FIERCE_DEITY }, "Fierce Deity Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_ALL_NIGHT }, "All-Night Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_BLAST }, "Blast Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_BREMEN }, "Bremen Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_BUNNY }, "Bunny Hood", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_CAPTAIN }, "Captain's Hat", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_CIRCUS_LEADER }, "Circus Leader's Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_COUPLE }, "Couples Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_DON_GERO }, "Don Gero Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_GARO }, "Garo's Mask", "", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_GIANT }, "Giant's Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_GIBDO }, "Gibdo Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_GREAT_FAIRY }, "Great Fairy Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_KAFEIS_MASK }, "Kafei's Mask", "", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_KAMARO }, "Kamaro's Mask", "", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_KEATON }, "Keaton Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_POSTMAN }, "Postman's Hat", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_ROMANI }, "Romani's Mask", "", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_SCENTS }, "Mask of Scents", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_STONE }, "Stone Mask", "the ", RSBS_ITEMCLASS_MASKS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MASK_TRUTH }, "Mask of Truth", "the ", RSBS_ITEMCLASS_MASKS },

    // --- Songs.
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_SONATA }, "Sonata of Awakening", "the ", RSBS_ITEMCLASS_SONGS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_LULLABY }, "Goron Lullaby", "the ", RSBS_ITEMCLASS_SONGS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_LULLABY_INTRO }, "Goron Lullaby Intro", "the ", RSBS_ITEMCLASS_SONGS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_NOVA }, "New Wave Bossa Nova", "the ", RSBS_ITEMCLASS_SONGS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_ELEGY }, "Elegy of Emptiness", "the ", RSBS_ITEMCLASS_SONGS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_OATH }, "Oath to Order", "the ", RSBS_ITEMCLASS_SONGS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_SOARING }, "Song of Soaring", "the ", RSBS_ITEMCLASS_SONGS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_HEALING }, "Song of Healing", "the ", RSBS_ITEMCLASS_SONGS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_EPONA }, "Epona's Song", "", RSBS_ITEMCLASS_SONGS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_SUN }, "Sun's Song", "the ", RSBS_ITEMCLASS_SONGS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_TIME }, "Song of Time", "the ", RSBS_ITEMCLASS_SONGS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SONG_STORMS }, "Song of Storms", "the ", RSBS_ITEMCLASS_SONGS },

    // --- Quest and sidequest items.
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_OCARINA }, "Ocarina of Time", "the ", RSBS_ITEMCLASS_PROGRESSION },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MOONS_TEAR }, "Moon's Tear", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_DEED_LAND }, "Land Title Deed", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_DEED_SWAMP }, "Swamp Title Deed", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_DEED_MOUNTAIN }, "Mountain Title Deed", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_DEED_OCEAN }, "Ocean Title Deed", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_ROOM_KEY }, "Room Key", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_LETTER_TO_KAFEI }, "Letter to Kafei", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_LETTER_TO_MAMA }, "Letter to Mama", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_PENDANT_OF_MEMORIES },
      "Pendant of Memories",
      "the ",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_MUSHROOM }, "Magic Mushroom", "a ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_FROG_BLUE }, "Blue Frog", "a ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_FROG_CYAN }, "Cyan Frog", "a ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_FROG_PINK }, "Pink Frog", "a ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_FROG_WHITE }, "White Frog", "a ", RSBS_ITEMCLASS_SIDEQUEST },

    // --- Boss remains.
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_REMAINS_ODOLWA }, "Odolwa's Remains", "", RSBS_ITEMCLASS_DUNGEON_REWARD },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_REMAINS_GOHT }, "Goht's Remains", "", RSBS_ITEMCLASS_DUNGEON_REWARD },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_REMAINS_GYORG }, "Gyorg's Remains", "", RSBS_ITEMCLASS_DUNGEON_REWARD },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_REMAINS_TWINMOLD }, "Twinmold's Remains", "", RSBS_ITEMCLASS_DUNGEON_REWARD },

    // --- Health: EMPTY since #525. Heart containers, heart pieces and double
    // defense are a SHARED RESOURCE now (criterion 6 below), not a crossing.

    // --- Skulltula tokens.
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_GS_TOKEN_SWAMP },
      "Swamp Gold Skulltula Token",
      "a ",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_GS_TOKEN_OCEAN },
      "Ocean Gold Skulltula Token",
      "an ",
      RSBS_ITEMCLASS_SIDEQUEST },

    // --- Owl statues (Sram_ActivateOwl: always a real warp unlock).
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_OWL_CLOCK_TOWN_SOUTH },
      "Clock Town Owl Statue",
      "the ",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_OWL_MILK_ROAD }, "Milk Road Owl Statue", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_OWL_SOUTHERN_SWAMP },
      "Southern Swamp Owl Statue",
      "the ",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_OWL_WOODFALL }, "Woodfall Owl Statue", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_OWL_MOUNTAIN_VILLAGE },
      "Mountain Village Owl Statue",
      "the ",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_OWL_SNOWHEAD }, "Snowhead Owl Statue", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_OWL_GREAT_BAY_COAST },
      "Great Bay Coast Owl Statue",
      "the ",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_OWL_ZORA_CAPE }, "Zora Cape Owl Statue", "the ", RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_OWL_IKANA_CANYON },
      "Ikana Canyon Owl Statue",
      "the ",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_OWL_STONE_TOWER },
      "Stone Tower Owl Statue",
      "the ",
      RSBS_ITEMCLASS_SIDEQUEST },

    // --- Tingle maps.
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_TINGLE_MAP_CLOCK_TOWN },
      "Tingle's Clock Town Map",
      "",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_TINGLE_MAP_WOODFALL },
      "Tingle's Woodfall Map",
      "",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_TINGLE_MAP_SNOWHEAD },
      "Tingle's Snowhead Map",
      "",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_TINGLE_MAP_ROMANI_RANCH },
      "Tingle's Romani Ranch Map",
      "",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_TINGLE_MAP_GREAT_BAY },
      "Tingle's Great Bay Map",
      "",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_TINGLE_MAP_STONE_TOWER },
      "Tingle's Stone Tower Map",
      "",
      RSBS_ITEMCLASS_SIDEQUEST },

    // --- Dungeon items. Additive, so an extra key can only ever trivialize.
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_WOODFALL_BOSS_KEY },
      "Woodfall Boss Key",
      "the ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_WOODFALL_SMALL_KEY },
      "Woodfall Small Key",
      "a ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_WOODFALL_MAP }, "Woodfall Map", "the ", RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_WOODFALL_COMPASS },
      "Woodfall Compass",
      "the ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SNOWHEAD_BOSS_KEY },
      "Snowhead Boss Key",
      "the ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SNOWHEAD_SMALL_KEY },
      "Snowhead Small Key",
      "a ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SNOWHEAD_MAP }, "Snowhead Map", "the ", RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SNOWHEAD_COMPASS },
      "Snowhead Compass",
      "the ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_GREAT_BAY_BOSS_KEY },
      "Great Bay Boss Key",
      "the ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_GREAT_BAY_SMALL_KEY },
      "Great Bay Small Key",
      "a ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_GREAT_BAY_MAP }, "Great Bay Map", "the ", RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_GREAT_BAY_COMPASS },
      "Great Bay Compass",
      "the ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_STONE_TOWER_BOSS_KEY },
      "Stone Tower Boss Key",
      "the ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_STONE_TOWER_SMALL_KEY },
      "Stone Tower Small Key",
      "a ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_STONE_TOWER_MAP }, "Stone Tower Map", "the ", RSBS_ITEMCLASS_DUNGEON_ITEMS },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_STONE_TOWER_COMPASS },
      "Stone Tower Compass",
      "the ",
      RSBS_ITEMCLASS_DUNGEON_ITEMS },

    // --- Stray fairies.
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_CLOCK_TOWN_STRAY_FAIRY },
      "Clock Town Stray Fairy",
      "a ",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_WOODFALL_STRAY_FAIRY },
      "Woodfall Stray Fairy",
      "a ",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_SNOWHEAD_STRAY_FAIRY },
      "Snowhead Stray Fairy",
      "a ",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_GREAT_BAY_STRAY_FAIRY },
      "Great Bay Stray Fairy",
      "a ",
      RSBS_ITEMCLASS_SIDEQUEST },
    { { (uint8_t)GAME_MM, 0, (uint16_t)RI_STONE_TOWER_STRAY_FAIRY },
      "Stone Tower Stray Fairy",
      "a ",
      RSBS_ITEMCLASS_SIDEQUEST },
};

static constexpr int kForeignPoolMMCount = sizeof(kForeignPoolMMV1) / sizeof(kForeignPoolMMV1[0]);

// ----------------------------------------------------------------------------
// THE EXCLUSIONS, WITH ATTRIBUTION (#495, ADR 0011 decision 3.4)
// ----------------------------------------------------------------------------
//
// The six criteria are numbered in src/common/foreign_items.h. Every id below
// was considered for this pool and REJECTED, and each names the criterion that
// rejected it. This is the prose above promoted to code, and it is what makes
// the class lock a test of the RULE rather than of a table that happens to look
// right: without an attributed exclusion set, a row that quietly drifted back
// into the pool would be invisible to CI.
//
// It is the ADJUDICATED set, not a machine sweep of RI_UNKNOWN..RI_MAX. Two
// criteria are not decidable from any table this build can read: criterion 3 is
// a property of the PAIRED world's option profile, which OoT's placement pass
// cannot see (ADR 0011 decision 3.5 / answer O8 keep it blanket until the MM
// freeze moves ahead of Fill() AND a values-publishing src/common surface
// exists), and criterion 4 is a property of a give's control flow. Guessing
// either is exactly the promise -- "it will be awarded there!" -- that criteria
// 3 and 4 exist to protect.
//
// The soul rows are REPRESENTED rather than enumerated: there are ~55 of them,
// all excluded by criterion 3 for one reason (a bare rando-inf flag whose
// meaning comes from an MM shuffle setting), and listing every one would make
// this table a second copy of the enum instead of a record of decisions.
namespace {
struct ForeignExclusionMM {
    uint16_t id;
    uint8_t criterion;
};

const ForeignExclusionMM kForeignExclusionsMM[] = {
    // (1) Sentinels -- the #488 trap. RI_UNKNOWN is enumerator 0, i.e. a
    //     zero-initialised slot; RI_NONE is "literally nothing".
    { (uint16_t)RI_UNKNOWN, (uint8_t)RSBS_FOREIGN_CRIT_REAL_ITEM },
    { (uint16_t)RI_NONE, (uint8_t)RSBS_FOREIGN_CRIT_REAL_ITEM },
    // (2) Junk-class, plus the row that is typed RITYPE_LESSER but is
    //     semantically a bottle refill.
    { (uint16_t)RI_JUNK, (uint8_t)RSBS_FOREIGN_CRIT_NOT_JUNK },
    { (uint16_t)RI_GOLD_DUST_REFILL, (uint8_t)RSBS_FOREIGN_CRIT_NOT_JUNK },
    // (3) Settings-armed gives. Each is a bare rando-inf / week-event flag whose
    //     only meaning comes from a specific MM shuffle setting.
    { (uint16_t)RI_SOUL_ENEMY_DINOLFOS, (uint8_t)RSBS_FOREIGN_CRIT_UNCONDITIONAL_GIVE }, // representative soul row
    { (uint16_t)RI_OCARINA_BUTTON_A, (uint8_t)RSBS_FOREIGN_CRIT_UNCONDITIONAL_GIVE },
    { (uint16_t)RI_OCARINA_BUTTON_C_DOWN, (uint8_t)RSBS_FOREIGN_CRIT_UNCONDITIONAL_GIVE },
    { (uint16_t)RI_OCARINA_BUTTON_C_RIGHT, (uint8_t)RSBS_FOREIGN_CRIT_UNCONDITIONAL_GIVE },
    { (uint16_t)RI_OCARINA_BUTTON_C_LEFT, (uint8_t)RSBS_FOREIGN_CRIT_UNCONDITIONAL_GIVE },
    { (uint16_t)RI_OCARINA_BUTTON_C_UP, (uint8_t)RSBS_FOREIGN_CRIT_UNCONDITIONAL_GIVE },
    { (uint16_t)RI_ABILITY_SWIM, (uint8_t)RSBS_FOREIGN_CRIT_UNCONDITIONAL_GIVE },
    { (uint16_t)RI_TIME_PROGRESSIVE, (uint8_t)RSBS_FOREIGN_CRIT_UNCONDITIONAL_GIVE },
    // (4) Global world event: Rando::GiveItem's Triforce branch fires
    //     GameInteractor_ExecuteOnGameCompletion() and queues a forced scene
    //     transition once the required count is reached.
    { (uint16_t)RI_TRIFORCE_PIECE, (uint8_t)RSBS_FOREIGN_CRIT_NO_WORLD_EVENT },
    { (uint16_t)RI_TRIFORCE_PIECE_PREVIOUS, (uint8_t)RSBS_FOREIGN_CRIT_NO_WORLD_EVENT },
    // (5) Reward, not punishment.
    { (uint16_t)RI_TRAP, (uint8_t)RSBS_FOREIGN_CRIT_REWARD },
    // (6) #525 shared cross-game resources -- the eighteen rows that LEFT this
    //     table across the three sharing tiers. Listed here rather than merely
    //     deleted, so the reason survives the deletion.
    { (uint16_t)RI_PROGRESSIVE_WALLET, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_WALLET_ADULT, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_WALLET_GIANT, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_DOUBLE_DEFENSE, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_HEART_CONTAINER, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_HEART_PIECE, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_PROGRESSIVE_MAGIC, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_SINGLE_MAGIC, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_DOUBLE_MAGIC, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_BOMB_BAG_20, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_BOMB_BAG_30, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_BOMB_BAG_40, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_PROGRESSIVE_BOMB_BAG, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_QUIVER_40, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_QUIVER_50, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    // MM's own bow give sets UPG_QUIVER to 1, so in MM owning the bow IS quiver
    // tier 1 -- a bow crossing would mutate the shared quantity itself.
    { (uint16_t)RI_BOW, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_PROGRESSIVE_BOW, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RI_HOOKSHOT, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
};

constexpr int kForeignExclusionsMMCount = sizeof(kForeignExclusionsMM) / sizeof(kForeignExclusionsMM[0]);
} // namespace

// Publish into src/common's origin-indexed registry (ADR 0009 decision 3), the
// same way the OoT pool does. File-scope initializer, so it runs before main()
// and before any gameplay or test code can ask for the pool; this TU lives in
// 2ship_rando, which links WHOLE_ARCHIVE, so it is never dropped as unreferenced
// (the Mode-B elision class — see .github/scripts/check-registrar-elision.sh).
namespace {
struct ForeignPoolMMV1Registrar {
    ForeignPoolMMV1Registrar() {
        Combo_RegisterForeignItemPool((uint8_t)GAME_MM, kForeignPoolMMV1, kForeignPoolMMCount);
    }
};
const ForeignPoolMMV1Registrar gForeignPoolMMV1Registrar;
} // namespace

namespace {

// The pending-give queue. Capacity matches the durable array's, so a full
// gComboCtx redeeming entirely into MM in one arrival can never overflow it —
// the queue is not allowed to be the thing that loses an item.
uint16_t sPendingGives[RSBS_SHARED_ITEM_CAP];
int sPendingCount = 0;

/**
 * Is `riId` a real, giveable MM item id?
 *
 * RI_UNKNOWN is enumerator 0 — a zero-initialised slot — and RI_NONE is
 * "literally nothing"; both are declared RITYPE_JUNK, so a type-based test
 * accepts them (the #488 sentinel trap, same two values, different surface).
 * An id at or past RI_MAX is out of the table entirely and would index
 * Rando::StaticData::Items out of range inside the give.
 */
bool IsGiveableItemId(uint16_t riId) {
    return riId != (uint16_t)RI_UNKNOWN && riId != (uint16_t)RI_NONE && riId < (uint16_t)RI_MAX;
}

/**
 * The WHOLE arrival toast for `riId`, read from MM's OWN item table.
 *
 * Returns the complete Notification::Options rather than the item name alone,
 * so that the ROM-free tier's assertion covers every field the player can read
 * (MM_ForeignItem_TestArrivalText at the bottom of this file renders it the way
 * the overlay does). A bridge that exposed only the name would let a re-added
 * cross-game tell — an origin badge in `.prefix`, "from Ocarina of Time"
 * appended to the verb — pass the lock untouched, which is exactly the
 * regression #510 exists to prevent.
 *
 * Icon and name come from the same two accessors MM's native rando pickup toast
 * calls (Rando/MiscBehavior/CheckQueue.cpp) — this is an MM item being received
 * in MM, so nothing here is cross-game and nothing needs an archive that is not
 * mounted. `.itemIcon` may be nullptr; Emit then renders text-only.
 *
 * Field arrangement matches that native toast exactly (verb in `.message`, item
 * in `.suffix`, no `.prefix`), because Options colours each field differently
 * and a bespoke arrangement would itself be a "this one is special" tell.
 */
Notification::Options BuildArrivalToast(RandoItemId randoItemId) {
    return Notification::Options{
        .itemIcon = Rando::StaticData::GetIconTexturePath(randoItemId),
        .message = "You got",
        .suffix = Rando::StaticData::GetItemName(randoItemId), // MM's article + name, e.g. "the Bunny Hood"
    };
}

/**
 * The toast as one line of text, joined the way Notification::Window::Draw lays
 * it out: every non-empty field on the same line, one gap between neighbours
 * (soh/Notification/Notification.cpp's ImGui::SameLine). The ROM-free lock's
 * observable — see MM_ForeignItem_TestArrivalText.
 */
std::string RenderToastLine(const Notification::Options& toast) {
    std::string line;
    const std::string* const fields[] = { &toast.prefix, &toast.message, &toast.suffix };
    for (const std::string* field : fields) {
        if (field->empty()) {
            continue;
        }
        if (!line.empty()) {
            line += ' ';
        }
        line += *field;
    }
    return line;
}

/** The actual give. Precondition: MM_gPlayState != NULL and the id is real. */
void GiveNow(uint16_t riId) {
    const RandoItemId randoItemId = (RandoItemId)riId;
    Rando::GiveItem(randoItemId);

    // #494: the arrival is the player's ONLY signal that this item landed.
    // Until now it was silent — the item appeared in the inventory an arbitrary
    // number of scenes after the OoT check that granted it, with no in-game
    // feedback at all. That is the same gap the OoT arrival closed for the other
    // direction (OoT_AwardSharedItem), here on MM's side.
    //
    // PRESENTED AS AN ORDINARY MM PICKUP: MM's own toast, MM's own icon, MM's
    // own "article + name" sentence, and no mention of where the item came from
    // (BuildArrivalToast, above).
    //
    // A toast rather than the get-item cutscene: this fires on a gameplay frame
    // the player did not initiate, where seizing the camera and the message
    // context would be a hijack. The toast is exactly what MM already falls back
    // to for its own pickups when the cutscene is skipped.
    //
    // EMITTED HERE AND NOT IN MM_AwardSharedItem (GameExports_SingleExe.cpp)
    // because this is the one point that is gameplay-gated by construction: both
    // callers reach it only with a live MM_gPlayState. The award callback runs at
    // the presence-gated arrival point, which the ROM-free rows drive headlessly
    // (src/common/tests/test_foreign_award.c calls the real MM_ConsumeSharedItems
    // with no PlayState and no Gui), so a Notification::Emit there would put an
    // ImGui/audio call on a display-free CI path.
    Notification::MM_Notify_Emit(BuildArrivalToast(randoItemId));
}

} // namespace

// ============================================================================
// Redemption give (called by MM_AwardSharedItem, the A1 consumer callback)
// ============================================================================

/**
 * Award one MM-origin foreign item. Gives immediately when a PlayState is live,
 * otherwise queues for MM_ForeignItem_FlushPending (see the file header for why
 * that is the safe shape here).
 *
 * @return 1 if the item was given or queued for the next gameplay frame,
 *         0 if the id names no real MM item (logged) or the queue is full.
 *         The caller's consumer sets RSBS_SHARED_ITEM_REDEEMED either way — a
 *         give we could not perform is a loud log, not a stalled arrival.
 */
extern "C" int MM_ForeignItem_Give(uint16_t riId) {
    if (!IsGiveableItemId(riId)) {
        fprintf(stderr, "[MM] foreign give: RI id=%u names no real MM item — not given\n", (unsigned)riId);
        return 0;
    }

    if (MM_gPlayState != NULL) {
        GiveNow(riId);
        return 1;
    }

    if (sPendingCount >= (int)RSBS_SHARED_ITEM_CAP) {
        // Unreachable while the queue is sized to the durable array (one
        // arrival cannot redeem more entries than the array holds), so this is
        // a guard against a future resize, not an expected path.
        fprintf(stderr, "[MM] foreign give: pending queue full, dropping RI id=%u\n", (unsigned)riId);
        return 0;
    }
    sPendingGives[sPendingCount++] = riId;
    return 1;
}

/**
 * Drain the pending queue once a PlayState is live. Called every frame from
 * Rando::MiscBehavior::CheckQueue (the gameplay-gated per-frame rando hook),
 * BEFORE its own early-out, so a queued give is not held behind a queued check.
 *
 * Order is preserved: Combo_RedeemSharedItemsForGame awards in slot order and
 * that order is a contract (progressive items resolve against the live save, so
 * order changes WHAT the player receives — shared_items.h). The queue is FIFO
 * for the same reason.
 *
 * @return the number of items given this call (0 when there is nothing pending
 *         or no live PlayState).
 */
extern "C" int MM_ForeignItem_FlushPending(void) {
    if (sPendingCount == 0 || MM_gPlayState == NULL) {
        return 0;
    }

    const int count = sPendingCount;
    // Clear the queue BEFORE giving. A give re-entering this function (an item
    // whose give path spins the frame loop) must not see the same entries
    // again; re-entrancy that re-gave would be a duplicate the redeemed bit
    // cannot catch, because the crossing was already consumed.
    uint16_t drained[RSBS_SHARED_ITEM_CAP];
    for (int i = 0; i < count; i++) {
        drained[i] = sPendingGives[i];
    }
    sPendingCount = 0;

    for (int i = 0; i < count; i++) {
        fprintf(stderr, "[MM] foreign give (deferred to gameplay): RI id=%u\n", (unsigned)drained[i]);
        GiveNow(drained[i]);
    }
    return count;
}

// ============================================================================
// ROM-free test bridge (redship tier; src/common/tests/test_foreign_award.c).
//
// The give itself needs a PlayState and a loaded save, so the ROM-free tier
// cannot observe it directly. What it CAN observe — and what the #502 lock is
// actually about — is that the real consumer walk reaches the real
// MM_AwardSharedItem, that the award lands here exactly once per crossing, and
// that a NULL PlayState defers instead of dereferencing. These two accessors
// are the observable for that; neither is reachable from gameplay.
// ============================================================================

/** How many gives are waiting for a live PlayState. */
extern "C" int MM_ForeignItem_TestPendingCount(void) {
    return sPendingCount;
}

/** The queued id at `index`, or 0 (RI_UNKNOWN) if out of range. */
extern "C" uint16_t MM_ForeignItem_TestPendingAt(int index) {
    if (index < 0 || index >= sPendingCount) {
        return (uint16_t)RI_UNKNOWN;
    }
    return sPendingGives[index];
}

/** Drop the queue without giving. Test isolation only — `--test all` runs every
 *  row in one process, and a queue left behind would leak into the next row. */
extern "C" void MM_ForeignItem_TestResetPending(void) {
    sPendingCount = 0;
}

/** RI_MAX — the exclusive upper bound for a RandoItemId walk, so a src/common
 *  test can find real ids without importing MM's enum (the #488 bridge's
 *  MM_Rando_Foreign_TestCheckIdMax, one enum over). */
extern "C" int MM_ForeignItem_TestItemIdMax(void) {
    return (int)RI_MAX;
}

/** The REAL id predicate MM_ForeignItem_Give gates on. Exposed rather than
 *  re-derived in the test for the same reason MM_Rando_Foreign_IsEligibleHost
 *  is: a lock that paraphrases the rule stops testing it the moment the rule
 *  moves. */
extern "C" int MM_ForeignItem_TestIsGiveableId(uint16_t riId) {
    return IsGiveableItemId(riId) ? 1 : 0;
}

/**
 * Walk the CRITERION-ATTRIBUTED EXCLUSION table (#495, ADR 0011 decision 3.4):
 * entry `index`'s rejected RI_* id and the criterion number that rejected it.
 *
 * The observable that makes the class rule testable. src/common has no MM enum
 * in scope by design, so the lock cannot name RI_TRAP itself; it walks this
 * bridge instead and asserts each excluded id is absent from the pool and from
 * the name inverse. Exposed rather than re-listed in the test for the standing
 * reason a lock never keeps its own copy of the rule.
 *
 * @return 1 while `index` names an entry, 0 once it is past the end.
 */
extern "C" int MM_ForeignItem_TestExclusionAt(int index, uint16_t* outId, uint8_t* outCriterion) {
    if (index < 0 || index >= kForeignExclusionsMMCount) {
        return 0;
    }
    if (outId != nullptr) {
        *outId = kForeignExclusionsMM[index].id;
    }
    if (outCriterion != nullptr) {
        *outCriterion = kForeignExclusionsMM[index].criterion;
    }
    return 1;
}

/** Is `riId` declared RITYPE_JUNK in MM's item table? (#510)
 *
 *  The observable for kForeignPoolMMV1's membership rule (2) — "no junk-class
 *  item may be a cross-game SOURCE, because junk is what a foreign HOST degrades
 *  to". Reported from MM's real table rather than re-derived in the test, so a
 *  row whose type changes upstream moves the lock with it instead of leaving it
 *  asserting a stale copy.
 *
 *  @return 1 if junk-class, 0 if a real non-junk item, -1 if the id names no row. */
extern "C" int MM_ForeignItem_TestIsJunkClassId(uint16_t riId) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)riId);
    if (it == Rando::StaticData::Items.end()) {
        return -1;
    }
    return (it->second.randoItemType == RITYPE_JUNK) ? 1 : 0;
}

/**
 * The WHOLE arrival toast for `riId` as one line, NUL-terminated into `out`
 * (#494) — every field the player can read, joined the way the overlay draws
 * them.
 *
 * The SAME BuildArrivalToast the give emits, not a paraphrase of it — a lock
 * that rebuilt the string itself would keep passing after the toast changed.
 * What CI can honestly assert is the resolution surface: that MM's own item
 * table names every entry of MM's own pool, and that the toast is that name
 * behind the native verb and NOTHING ELSE. Extra prose anywhere in the toast —
 * "it came from Ocarina of Time" appended to the verb, an origin badge in the
 * unused `.prefix` — breaks the equality, which is the no-tell contract (#510)
 * stated as an assertion. Reporting only the item name would have left both of
 * those regressions invisible to CI.
 *
 * @return the length written, or -1 if the id names no real MM item or `out` is
 *         too small (never a truncated string).
 */
extern "C" int MM_ForeignItem_TestArrivalText(uint16_t riId, char* out, int cap) {
    if (out == nullptr || cap <= 0 || !IsGiveableItemId(riId)) {
        return -1;
    }
    const std::string line = RenderToastLine(BuildArrivalToast((RandoItemId)riId));
    if ((int)line.size() >= cap) {
        return -1;
    }
    memcpy(out, line.c_str(), line.size() + 1);
    return (int)line.size();
}

/** The arrival toast's icon path for `riId`, or nullptr — which is a text-only
 *  toast, not a defect (Notification::Emit skips a null icon). */
extern "C" const char* MM_ForeignItem_TestArrivalIcon(uint16_t riId) {
    if (!IsGiveableItemId(riId)) {
        return nullptr;
    }
    return BuildArrivalToast((RandoItemId)riId).itemIcon;
}

#endif // RSBS_SINGLE_EXECUTABLE
