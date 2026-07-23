/**
 * OptionsUiSingleExe.cpp — MM's randomizer options, described for the combo
 * options pane (#497 step 4, #499 step 5; ADR 0004, ADR 0009 decision 3).
 *
 * ============================================================================
 * WHY THIS TABLE EXISTS AND WHY IT LIVES HERE
 * ============================================================================
 *
 * `Rando::StaticData::Options` carries `{id, name, cvar, defaultValue}` and
 * nothing else — the `RO(id, default)` macro stores only the STRINGIFIED
 * enumerator, so the "label" for RO_SHUFFLE_COWS is the literal
 * "RO_SHUFFLE_COWS". Two things a menu needs are therefore missing from it:
 * human labels with value enums, and the 9-group taxonomy MM's own rando menu
 * uses. Both live in `games/mm/2s2h/Rando/Menu.cpp`, which is elided into
 * `2ship_rando_ui` and cannot be linked without dragging `BenMenu` — the exact
 * condition #451 is armed by. So the labels are re-stated here, in a TU that
 * links, rather than the menu being revived.
 *
 * This file is the MM half of the seam; `src/common/combo_mm_options_view.h` is
 * the common half. The split follows ADR 0009 decision 3's rule for the foreign
 * pools: a table whose enums are MM's is defined in the single TU where those
 * enums are in scope, and crosses into common code as flat data. Common code
 * gains no MM header, and the combo pane gains no MM knowledge.
 *
 * Lives in 2s2h/Rando/, glob-collected into `2ship_rando`, which links
 * WHOLE_ARCHIVE — so the file-scope registrar at the bottom runs with no CMake
 * edit, exactly as ForeignItemsSingleExe.cpp's pool registrar does.
 *
 * ============================================================================
 * CAPABILITY GATING IS DATA, NOT DECORATION (ADR 0004 section 5)
 * ============================================================================
 *
 * Every row carries a liveness class measured against the single-exe link on
 * 2026-07-23, and a reason string when it is not live. The measurement is the
 * ADR's three-part test — TU links, registrar runs, hook type has an MM
 * dispatch point — and for this table only the third leg ever fails:
 * `2ship_rando` is WHOLE_ARCHIVE'd so every behaviour TU links, and
 * `MM_Rando_Init` -> `Rando::Init` runs every registrar.
 *
 * The states that matter, and why the distinction is not pedantry:
 *
 *   GENERATION_ONLY — consumed by the fill or by starting state. No runtime
 *     hook, so it cannot be half-armed. Always honest.
 *   LIVE — pool widening plus every hook it rides is dispatched.
 *   PARTIAL — some legs live, at least one dormant. Enabling it widens the
 *     check pool but part of the behaviour never arms.
 *   DORMANT — the hook type it rides has no MM dispatch point (#438).
 *
 * PARTIAL and DORMANT are both drawn disabled BECAUSE OF WHAT THEY DO, not out
 * of caution. `Logic/GeneratePools.cpp:60-130` skips whole check classes when
 * their option is off; turning one on adds those checks to the pool, the fill
 * places real items on them, and then the hook that would award the item never
 * runs. The result is an unwinnable world with items sitting on checks the game
 * cannot pay out — strictly worse than leaving the option off, which is why ADR
 * 0004 calls this the worst state available and why a disabled row with a
 * reason is the honest presentation.
 *
 * Three rows are the pure form of that trap and are flagged as such below:
 * crate, barrel and grass drops. Their identify-the-check helpers have NO call
 * site outside the undispatched OnActorInit hooks, and the surviving VB legs
 * bail on RC_UNKNOWN — so the dormancy is total rather than cosmetic.
 *
 * Two rows are worse than inert: RO_HINTS_GOSSIP_STONES / RO_HINTS_PURCHASEABLE
 * force a mask-of-truth verdict and redirect the textbox to a message whose
 * filler is the undispatched OnOpenText handler, and RO_ACCESS_TRIALS' VB leg
 * returns "unlocked" for every trial while the text leg that would gate it is
 * dead. Enabling those is a REGRESSION against vanilla behaviour, not a no-op.
 *
 * #438 IS STALE IN ONE DIRECTION and this table follows the tree, not the
 * issue: `OnSceneInit` IS dispatched (MM_GameHooks_ExecuteOnSceneInit,
 * GameExports_SingleExe.cpp, called from z_play.c), so RO_ACCESS_DUNGEONS is
 * fully live. Re-measure before trusting either document.
 *
 * ONE HAZARD THAT IS NOT PER-ROW. `BeforeEndOfCycleSave` / `AfterEndOfCycleSave`
 * are registered unconditionally under IS_RANDO and are dormant, and their body
 * (Rando/MiscBehavior/OnCycleSave.cpp) is what carries dungeon items, keys,
 * stray fairies and trade slots across a cycle reset and clears the per-cycle
 * check flags. With it dead, a cycle reset degrades rando state for EVERY
 * option. That belongs in a pane-wide banner rather than 47 identical reason
 * strings; the window draws it as one.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <vector>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/StaticData/StaticData.h"

extern "C" {
// SPIDER_HOUSE_TOKENS_REQUIRED and STRAY_FAIRY_SCATTERED_TOTAL — the two
// slider ceilings that are game constants rather than UI choices. Same include
// StaticData/Options.cpp carries for the same two.
#include "overlays/actors/ovl_En_Sth/z_en_sth.h"
}

// src/common. Included OUTSIDE any extern "C" block: the header manages its own
// linkage and pulls <stdbool.h>/<stdint.h> (matching Foreign.cpp).
#include "combo_mm_options_view.h"

namespace {

// ---------------------------------------------------------------------------
// Value labels for the four combo-valued options. Order is ENUMERATOR order
// (index 0 first) because the pane writes the selected index straight into the
// CVar and MM reads it back as the enumerator — a re-ordered array here would
// silently map "Glitchless" onto RO_LOGIC_NO_LOGIC.
// ---------------------------------------------------------------------------

const char* const kLogicLabels[] = {
    "Glitchless",       // RO_LOGIC_GLITCHLESS
    "No Logic",         // RO_LOGIC_NO_LOGIC
    "Nearly No Logic",  // RO_LOGIC_NEARLY_NO_LOGIC
    "Vanilla",          // RO_LOGIC_VANILLA
};

const char* const kDungeonAccessLabels[] = {
    "Requires Transformation & Song", // RO_ACCESS_DUNGEONS_FORM_AND_SONG
    "Requires Transformation or Song", // RO_ACCESS_DUNGEONS_FORM_OR_SONG
    "Requires Only Transformation",    // RO_ACCESS_DUNGEONS_FORM_ONLY
    "Requires Only Song",              // RO_ACCESS_DUNGEONS_SONG_ONLY
    "Open",                            // RO_ACCESS_DUNGEONS_OPEN
};

const char* const kTrialsAccessLabels[] = {
    "2-6-12-20 Masks",                    // RO_ACCESS_TRIALS_20_MASKS
    "Requires Associated Remains",        // RO_ACCESS_TRIALS_REMAINS
    "Requires Associated Transformation", // RO_ACCESS_TRIALS_FORMS
    "Open",                               // RO_ACCESS_TRIALS_OPEN
};

const char* const kClockProgressiveLabels[] = {
    "Random",                  // RO_CLOCK_SHUFFLE_RANDOM
    "Progressive: Ascending",  // RO_CLOCK_SHUFFLE_ASCENDING
    "Progressive: Descending", // RO_CLOCK_SHUFFLE_DESCENDING
};

/**
 * UI metadata for one option. Deliberately does NOT restate the id's cvar
 * string or its default: both are copied from the `Rando::StaticData::Options`
 * row at registration time, so the pane cannot bind a widget to a key MM does
 * not read, and a default changed in Options.cpp cannot go stale here. The
 * MMRandoOptions lock asserts that equality anyway, because "cannot drift" is
 * worth proving rather than asserting in a comment.
 */
struct OptionUi {
    RandoOptionId id;
    ComboMMOptionGroup group;
    ComboMMOptionWidget widget;
    const char* label;
    const char* tooltip;
    int32_t minValue;
    int32_t maxValue;
    const char* const* valueLabels;
    uint8_t valueCount;
    ComboMMOptionLiveness liveness;
    const char* disabledReason;
};

#define UI_COUNT(a) (uint8_t)(sizeof(a) / sizeof((a)[0]))

// Reason strings shared by several rows, so a wording change stays one edit.
constexpr const char* kReasonOpenText = "Not yet available: MM OnOpenText dispatch not placed (#438)";
constexpr const char* kReasonActorInitDrop =
    "Would strand items: the drop's OnActorInit dispatch is not placed (#438)";

// clang-format off
const OptionUi kOptionUi[] = {
    // ---- Logic & Conditions ------------------------------------------------
    { RO_LOGIC, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_COMBO,
      "Logic",
      "Glitchless guarantees a beatable seed; No Logic and Nearly No Logic place freely; Vanilla does not shuffle.",
      0, 0, kLogicLabels, UI_COUNT(kLogicLabels),
      COMBO_MM_LIVENESS_GENERATION_ONLY, "" },
    { RO_ACCESS_DUNGEONS, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_COMBO,
      "Dungeon Access",
      "What a dungeon entrance requires: form and song, form or song, form only, song only, or nothing.",
      0, 0, kDungeonAccessLabels, UI_COUNT(kDungeonAccessLabels),
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_ACCESS_TRIALS, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_COMBO,
      "Trials Access",
      "What the Moon trials require. Mask counts, associated remains, associated transformation, or open.",
      0, 0, kTrialsAccessLabels, UI_COUNT(kTrialsAccessLabels),
      // Worse than inert: the VB leg reports every trial unlocked while the
      // text leg that would gate it is dead, so a setting here reads as "Open"
      // whatever it says.
      COMBO_MM_LIVENESS_PARTIAL, "Trials read as Open regardless: gating needs MM OnOpenText dispatch (#438)" },
    { RO_ACCESS_MOON_MASKS_COUNT, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_SLIDER,
      "Moon Access: Masks Required", "How many masks are needed to enter the Moon.",
      0, 20, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_ACCESS_MOON_REMAINS_COUNT, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_SLIDER,
      "Moon Access: Remains Required", "How many boss remains are needed to enter the Moon.",
      0, 4, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_ACCESS_MAJORA_MASKS_COUNT, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_SLIDER,
      "Majora Access: Masks Required", "How many masks are needed to reach Majora.",
      0, 20, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Logic only: the Majora gate needs MM OnOpenText dispatch (#438)" },
    { RO_ACCESS_MAJORA_REMAINS_COUNT, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_SLIDER,
      "Majora Access: Remains Required", "How many boss remains are needed to reach Majora.",
      0, 4, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Logic only: the Majora gate needs MM OnOpenText dispatch (#438)" },
    { RO_ACCESS_MAJORA_REMAINS, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_CHECKBOX,
      "Majora Access: Remains (unimplemented)",
      "Declared but never implemented: no code in MM's randomizer reads this option.",
      0, 0, nullptr, 0,
      // The 47th id. Given a StaticData row so the option id space is total
      // (#499 step 5), and shown disabled because it has no consumer at all —
      // a control that writes a CVar nothing reads is the vacuous gate ADR
      // 0004 section 5 forbids.
      COMBO_MM_LIVENESS_DORMANT, "Not implemented: no code reads this option" },

    // ---- Shuffle Options ---------------------------------------------------
    { RO_SHUFFLE_COWS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Cows", "Adds the reward for playing Epona's Song for a cow to the check pool.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_OWL_STATUES, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Owl Statues", "Adds activating each Owl Statue to the check pool.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_FREESTANDING_ITEMS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Freestanding Items", "Adds collectibles lying loose in the world to the check pool.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_POT_DROPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Pot Drops", "Adds the item dropped by breaking a pot to the check pool.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_SNOWBALL_DROPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Snowball Drops", "Adds the item dropped by breaking a snowball to the check pool.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_BOSS_REMAINS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Boss Remains", "Shuffles the four Boss Remains into the item pool.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_GOLD_SKULLTULAS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Gold Skulltula Tokens", "Adds the Spider House tokens to the check pool.",
      0, 0, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Token count is lost on a cycle reset: EndOfCycleSave dispatch missing (#438)" },
    { RO_MINIMUM_SKULLTULA_TOKENS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_SLIDER,
      "Minimum Gold Skulltula Tokens",
      "Tokens needed for the Spider House reward. Forced to the vanilla value when tokens are not shuffled.",
      1, SPIDER_HOUSE_TOKENS_REQUIRED, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_MINIMUM_STRAY_FAIRIES, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_SLIDER,
      "Minimum Stray Fairies",
      "Stray Fairies needed for a Great Fairy reward. Does not affect the Clock Town fairy.",
      1, STRAY_FAIRY_SCATTERED_TOTAL, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Custom fairy counts need MM OnActorInit dispatch (#438)" },
    { RO_SHUFFLE_FROGS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Frogs", "Adds the Frog Choir frogs to the check pool.",
      0, 0, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Frog checks need MM OnActorInit/OnOpenText dispatch (#438)" },
    { RO_SHUFFLE_SHOPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Shops", "Adds purchaseable shop slots to the check pool.",
      0, 0, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Shop text and pricing need MM OnOpenText/OnActorInit dispatch (#438)" },
    { RO_SHUFFLE_TINGLE_SHOPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Tingle Maps", "Adds the maps Tingle sells to the check pool.",
      0, 0, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Tingle shop text needs MM OnOpenText dispatch (#438)" },
    { RO_SHUFFLE_CRATE_DROPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Crate Drops", "Adds the item dropped by breaking a crate to the check pool.",
      0, 0, nullptr, 0,
      // Total dormancy, not cosmetic: the identify-the-check helper has no
      // call site outside the undispatched hook, and the surviving VB legs
      // bail on RC_UNKNOWN. Items placed here can never be collected.
      COMBO_MM_LIVENESS_DORMANT, kReasonActorInitDrop },
    { RO_SHUFFLE_BARREL_DROPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Barrel Drops", "Adds the item dropped by breaking a barrel to the check pool.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_DORMANT, kReasonActorInitDrop },
    { RO_SHUFFLE_GRASS_DROPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Grass Drops", "Adds the item dropped by cutting grass to the check pool.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_DORMANT, kReasonActorInitDrop },

    // ---- Items -------------------------------------------------------------
    { RO_PLENTIFUL_ITEMS, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_CHECKBOX,
      "Plentiful Items", "Major items, masks and keys get an extra copy in the pool.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_GENERATION_ONLY, "" },
    { RO_SHUFFLE_TRAPS, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Traps", "Mixes Ice Traps into the item pool.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_TRAP_AMOUNT, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_SLIDER,
      "Trap Count", "How many traps are shuffled into the item pool. Only used when traps are shuffled.",
      1, 100, nullptr, 0, COMBO_MM_LIVENESS_GENERATION_ONLY, "" },
    { RO_SHUFFLE_BOSS_SOULS, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_CHECKBOX,
      "Boss Souls", "Boss Souls enter the item pool; a boss does not spawn until its soul is found.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_ENEMY_SOULS, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_CHECKBOX,
      "Enemy Souls", "Enemy Souls enter the item pool; an enemy is immune until its soul is found.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_ENEMY_DROPS, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_CHECKBOX,
      "Enemy Drops", "Shuffles the first drop from a non-boss enemy.",
      0, 0, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Partly live: the kill-drop leg needs MM OnActorKill dispatch (#438)" },
    { RO_SHUFFLE_OCARINA_BUTTONS, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Ocarina Buttons", "Ocarina buttons become items; a song is unplayable until its notes are found.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_SWIM, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Swim", "Swimming becomes an item; deep water respawns Link until it is found.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_TRIFORCE_PIECES, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_CHECKBOX,
      "Triforce Hunt", "Scatters Triforce Pieces through the pool; collecting enough of them wins the seed.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_TRIFORCE_PIECES_MAX, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_SLIDER,
      "Triforce Pieces Shuffled",
      "How many pieces are placed. Reduced automatically if the pool cannot hold that many.",
      1, 1000, nullptr, 0, COMBO_MM_LIVENESS_GENERATION_ONLY, "" },
    { RO_TRIFORCE_PIECES_REQUIRED, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_SLIDER,
      "Triforce Pieces Required", "How many pieces win the seed. Capped at the number shuffled.",
      1, 15, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_CLOCK_SHUFFLE, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Time", "Breaks the three-day cycle into six half-days that must be unlocked as items.",
      0, 0, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Partly live: half-day prompts need MM OnOpenText dispatch (#438)" },
    { RO_CLOCK_SHUFFLE_PROGRESSIVE, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_COMBO,
      "Time Progression", "Random shuffles all six half-days; Ascending and Descending unlock them in order.",
      0, 0, kClockProgressiveLabels, UI_COUNT(kClockProgressiveLabels),
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_CLOCK_TERMINAL_TIME, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_TIME,
      "Final Hours Start Time",
      "When the Final Hours begin, 00:00 to 05:59. Baked into the seed and fixed once generated.",
      0, 359, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },

    // ---- Starting Items ----------------------------------------------------
    { RO_STARTING_HEALTH, COMBO_MM_GROUP_STARTING, COMBO_MM_WIDGET_SLIDER,
      "Starting Hearts", "How many hearts a new file begins with.",
      1, 20, nullptr, 0, COMBO_MM_LIVENESS_GENERATION_ONLY, "" },
    { RO_STARTING_CONSUMABLES, COMBO_MM_GROUP_STARTING, COMBO_MM_WIDGET_CHECKBOX,
      "Start With Full Consumables", "Begin with full Deku Sticks and Deku Nuts.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_GENERATION_ONLY, "" },
    { RO_STARTING_RUPEES, COMBO_MM_GROUP_STARTING, COMBO_MM_WIDGET_CHECKBOX,
      "Start With Full Wallet", "Begin with a full wallet.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_GENERATION_ONLY, "" },
    { RO_STARTING_MAPS_AND_COMPASSES, COMBO_MM_GROUP_STARTING, COMBO_MM_WIDGET_CHECKBOX,
      "Start With Maps & Compasses", "Begin with every dungeon map and compass.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_GENERATION_ONLY, "" },

    // ---- Hints -------------------------------------------------------------
    // Every hint family registers on OnOpenText, which has no MM dispatch
    // point at all (#438's largest dormant surface, 98 registrations). None of
    // these can do anything today, and two of them make things worse.
    { RO_HINTS_GOSSIP_STONES, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Gossip Stone Hints", "Each gossip stone gives a fixed hint about one location's contents.",
      0, 0, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Breaks stones: the verdict fires but the hint text needs OnOpenText (#438)" },
    { RO_HINTS_PURCHASEABLE, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Purchaseable Gossip Hints", "Gossip stones sell a hint for a scaling rupee cost.",
      0, 0, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Breaks stones: the purchase prompt needs OnOpenText dispatch (#438)" },
    { RO_HINTS_SPIDER_HOUSES, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Spider House Hints", "Hints for the two Spider House rewards.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_DORMANT, kReasonOpenText },
    { RO_HINTS_HOOKSHOT, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Hookshot Hint", "The Zora on Great Bay Coast hints where the Hookshot is.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_DORMANT, kReasonOpenText },
    { RO_HINTS_BOSS_REMAINS, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Boss Remains Hints", "The Clock Town recruitment posters hint where the Boss Remains are.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_DORMANT, kReasonOpenText },
    { RO_HINTS_OATH_TO_ORDER, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Oath to Order Hint", "Skull Kid hints where Oath to Order is once the Moon is reachable.",
      0, 0, nullptr, 0,
      COMBO_MM_LIVENESS_PARTIAL, "Partly live: the hint text needs MM OnOpenText dispatch (#438)" },
};
// clang-format on

#undef UI_COUNT

/**
 * The descriptor table handed to src/common, built once at registration.
 *
 * Built rather than written out because `cvar` and `defaultValue` are COPIED
 * from the matching `Rando::StaticData::Options` row: restating them here would
 * create a second place for a key name to live, and the failure mode of that
 * drift is a pane whose widgets write CVars the generator never reads — which
 * looks exactly like a working menu.
 *
 * A `RO_*` id present in kOptionUi but absent from StaticData::Options is
 * skipped rather than guessed at; the MMRandoOptions lock turns that skip into
 * a test failure, since a silently shorter table is the same class of quiet
 * incompleteness the 47-vs-46 skew was.
 */
std::vector<ComboMMOptionDesc>& DescriptorTable() {
    static std::vector<ComboMMOptionDesc> sDescriptors;
    if (!sDescriptors.empty()) {
        return sDescriptors;
    }
    sDescriptors.reserve(sizeof(kOptionUi) / sizeof(kOptionUi[0]));
    for (const OptionUi& ui : kOptionUi) {
        auto it = Rando::StaticData::Options.find(ui.id);
        if (it == Rando::StaticData::Options.end()) {
            continue;
        }
        const Rando::StaticData::RandoStaticOption& row = it->second;

        ComboMMOptionDesc desc = {};
        desc.id = (uint16_t)ui.id;
        desc.name = row.name;
        desc.cvar = row.cvar;
        desc.label = ui.label;
        desc.tooltip = ui.tooltip;
        desc.group = (uint8_t)ui.group;
        desc.widget = (uint8_t)ui.widget;
        desc.defaultValue = (int32_t)row.defaultValue;
        desc.minValue = ui.minValue;
        desc.maxValue = ui.maxValue;
        desc.valueLabels = ui.valueLabels;
        desc.valueCount = ui.valueCount;
        desc.liveness = (uint8_t)ui.liveness;
        desc.disabledReason = ui.disabledReason;
        sDescriptors.push_back(desc);
    }
    return sDescriptors;
}

} // namespace

/**
 * Publish the table to src/common.
 *
 * DELIBERATELY NOT a file-scope registrar, which is what the foreign-item pools
 * use. Those register a table of literals; this one READS
 * `Rando::StaticData::Options`, a namespace-scope std::map in another
 * translation unit. Static initialization order across TUs is unspecified, so a
 * file-scope registrar could publish a table built from an empty map — and it
 * would fail as "the pane has no rows", with nothing pointing at the cause.
 *
 * Called instead from Combo_MMOptionsWindow_Init(), which runs from the combo
 * entry point after both games' statics are constructed. Idempotent: the
 * descriptor vector is built once and re-registering the same pointer is
 * harmless (the registry logs a replacement, which is why the window calls this
 * once rather than per frame).
 */
extern "C" void MM_RandoOptionsUi_Register(void) {
    auto& table = DescriptorTable();
    Combo_RegisterMMOptionTable(table.data(), (int)table.size());
}

#endif // RSBS_SINGLE_EXECUTABLE
