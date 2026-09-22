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
 * Two paragraphs used to stand here describing the rows caught in that trap.
 * Both were accurate when written and both are now HISTORY, kept only because
 * the SHAPES they name are what to recognise again — not the rows. One:
 * "the pure form of that trap", an identify-the-check helper with no call site
 * outside an undispatched OnActorInit (the three drop rows). Two: "worse than
 * inert", a VB leg forcing a permissive verdict while the text leg that would
 * gate it is dead (the gossip-stone pair, RO_ACCESS_TRIALS). Every hook under
 * both descriptions dispatches today. See the two re-measures below.
 *
 * #438 IS STALE IN ONE DIRECTION and this table follows the tree, not the
 * issue: `OnSceneInit` IS dispatched (MM_GameHooks_ExecuteOnSceneInit,
 * GameExports_SingleExe.cpp, called from z_play.c), so RO_ACCESS_DUNGEONS is
 * fully live. Re-measure before trusting either document.
 *
 * THE THREE DROP ROWS WERE THAT STALENESS, AND ARE NOW RE-MEASURED (#438
 * remainder). This table was written before #512 wired OnActorInit /
 * OnActorDraw / OnOpenText, so `kReasonActorInitDrop` went on naming a blocker
 * that no longer existed. Promoting a row ENABLES it, which ADR 0004 calls the
 * high-stakes direction, so the flip was held until every leg could be checked
 * rather than ridden on someone else's fix. Done leg by leg, per row, at the
 * dispatchers in games/mm/2s2h/GameExports_SingleExe.cpp:
 * ObjKibako/ObjTaru/ObjGrass identify their checks through id-keyed OnActorInit
 * (ExecuteForID leg present, #512); grass's non-actor elements ride id-keyed
 * OnActorKill with OnActorDestroy as their reaper (both #515); every remaining
 * leg is ShouldVanillaBehavior (#392). All three behaviour TUs sit under
 * 2s2h/Rando/, which links WHOLE_ARCHIVE, so the TU-links and registrar-runs
 * legs of ADR 0004's test were never in doubt. The rows are LIVE and
 * kReasonActorInitDrop is retired. The same standing instruction applies to
 * whoever reads this next: re-measure, do not trust the history.
 *
 * THE OnOpenText BATCH IS NOW RE-MEASURED TOO (#669), AND THIRTEEN ROWS ARE
 * PROMOTED. The paragraph that stood here said those rows were left stale on
 * purpose, because the hook was never their only question: each needed its own
 * behaviour TU checked. That check is what #669 was. What it found, leg by leg:
 *
 *   - THE HOOK SURFACE IS NARROWER THAN THE REASONS IMPLIED. Every behaviour TU
 *     behind these rows registers on exactly five hook types: OnOpenText,
 *     OnActorInit, ShouldActorInit, ShouldActorUpdate and ShouldVanillaBehavior.
 *     All five have an MM dispatch point (MM_GameHooks_ExecuteOnOpenText /
 *     ...OnActorInit / ...ShouldActorInit / ...ShouldActorUpdate / ...VBShould,
 *     GameExports_SingleExe.cpp), and their actor-side call sites in z_actor.c
 *     are live and unguarded. There is no sixth hook type hiding in this batch.
 *
 *   - THE CustomMessage QUESTION, WHICH WAS THE ONE WORTH NOT ASSUMING, ANSWERS
 *     ITSELF IN THE HANDLERS. The hint handlers do NOT ride the
 *     CUSTOM_MESSAGE_ID registrant that #520 revived. Each one calls
 *     CustomMessage::LoadCustomMessageIntoFont() ITSELF and then sets
 *     *loadFromMessageTable = false; MM_Message_OpenText (z_message.c) has a
 *     live `if (!loadFromMessageTable)` leg that leaves the font buffer the
 *     handler just wrote untouched. That is load-bearing, not incidental: the
 *     dispatcher's ExecuteForID leg is keyed ONCE per call, so a handler that
 *     merely staged a message and rewrote textId would never be re-entered and
 *     the staged text would never be drawn. These handlers are written the way
 *     that works.
 *
 *   - THE REGISTRAR-RUNS LEG IS ONE SHARED ANSWER. Every registrar here is
 *     called from Rando::ActorBehavior::OnFileLoad() (or
 *     Rando::ClockShuffle::OnFileLoad()), reached from OnSaveLoadHandler, which
 *     Rando::Init registers on OnSaveLoad — and GameInteractor_ExecuteOnSaveLoad
 *     is MM-defined over S2H::GameHooks and called from z_play.c and
 *     z_sram_NES.c. Every behaviour TU named below is present in the operator's
 *     redship.map, so the TU-links leg is ground truth, not inference.
 *
 *   - THE HINT ROWS CANNOT WIDEN THE POOL AT ALL. No RO_HINTS_* id appears in
 *     Logic/GeneratePools.cpp: hints only READ a placement the fill already
 *     made. ADR 0004's unwinnable-seed hazard — the reason promotion is the
 *     high-stakes direction — simply does not apply to six of these thirteen.
 *
 *   - THE THREE THAT DO WIDEN THE POOL WERE CHECKED FOR A PAYOUT, NOT JUST A
 *     HOOK. FROGS, SHOPS and TINGLE each have a leg that sets
 *     RANDO_SAVE_CHECKS[...].eligible through a dispatched hook. For SHOPS that
 *     meant confirming all 25 RCTYPE_SHOP checks are served — EnIn, EnTab,
 *     EnGirlA and EnSob1, all four in the link — rather than only the two the
 *     row's own reason happened to mention.
 *
 * `kReasonOpenText` is retired with the rows it gated. ONE row survived that
 * re-measure still gated — RO_CLOCK_SHUFFLE — with its reason finally naming
 * the real blocker instead of a hook: a link ELISION, not a dispatch gap.
 *
 * THAT ROW IS NOW LIVE TOO (#678), BECAUSE THE BLOCKER WAS FIXED RATHER THAN
 * RE-WORDED. The two Enhancements TUs holding its ownership checks are carved
 * into 2ship_enh_clockshuffle and linked WHOLE_ARCHIVE (games/mm/CMakeLists.txt),
 * so the objects, their registrars and the three ClockShuffle functions they
 * call are all in the binary. Read its row comment for the three-part evidence
 * and for what the previous reason over-claimed. NO ROW IN THIS TABLE IS GATED
 * ON AN UNFINISHED PORT ANY MORE: the only non-LIVE entry left is
 * RO_ACCESS_MAJORA_REMAINS, and that is an operator retirement (ADR 0010 answer
 * O1), not a blocker.
 *
 * Same standing instruction as the paragraphs above, now earned four times:
 * re-measure against the tree, do not trust this history.
 *
 * THE HAZARD THAT WAS NOT PER-ROW, AND IS NOW GONE (#514). This table used to
 * be read alongside a pane-wide banner: `BeforeEndOfCycleSave` /
 * `AfterEndOfCycleSave` are registered unconditionally under IS_RANDO, were
 * dormant, and their body (Rando/MiscBehavior/OnCycleSave.cpp) is what carries
 * dungeon items, keys, stray fairies and trade slots across a cycle reset and
 * clears the per-cycle check flags. With it dead a cycle reset degraded rando
 * state for EVERY option, which is why it was one banner rather than 47
 * identical reason strings. #514 gave both halves MM dispatch
 * (MM_GameHooks_ExecuteBefore/AfterEndOfCycleSave), so the banner is retired
 * and RO_SHUFFLE_GOLD_SKULLTULAS — whose PARTIAL reason named this gap and
 * nothing else — is LIVE. Same standing instruction as the paragraph above:
 * re-measure against the tree, do not trust this history.
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
// The trick table's half of the same seam (#578 part 1).
#include "combo_mm_tricks_view.h"

#include <map>
#include <string>

namespace {

// ---------------------------------------------------------------------------
// Value labels for the four combo-valued options. Order is ENUMERATOR order
// (index 0 first) because the pane writes the selected index straight into the
// CVar and MM reads it back as the enumerator — a re-ordered array here would
// silently map "Glitchless" onto RO_LOGIC_NO_LOGIC.
// ---------------------------------------------------------------------------

const char* const kLogicLabels[] = {
    "Glitchless",      // RO_LOGIC_GLITCHLESS
    "No Logic",        // RO_LOGIC_NO_LOGIC
    "Nearly No Logic", // RO_LOGIC_NEARLY_NO_LOGIC
    "Vanilla",         // RO_LOGIC_VANILLA
};

const char* const kDungeonAccessLabels[] = {
    "Requires Transformation & Song",  // RO_ACCESS_DUNGEONS_FORM_AND_SONG
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

// No shared reason strings remain. Both that existed are retired by re-measure:
// kReasonActorInitDrop (crate, barrel, grass — #438's remainder) and
// kReasonOpenText (the hint family and the OnOpenText batch — #669). Each named
// a hook type that has had an MM dispatch point since #512. Exactly ONE row is
// still gated — RO_ACCESS_MAJORA_REMAINS, retired by operator ruling — and it
// carries its own reason naming that ruling, not a blocker. (RO_CLOCK_SHUFFLE,
// the other row that used to be here, went LIVE in #678 when its elision was
// fixed.) The MMRandoOptions lock REFUSES any reason naming a dispatched hook
// type, so a shared "waiting on dispatch" constant cannot be reintroduced
// without the test going red.

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
      // Was PARTIAL on "trials read as Open regardless", which was true while
      // the gate was dead: EnJs.cpp's VB_JS_OVERRIDE_MASK_CHECK reports jsType
      // 1-4 unlocked unconditionally, so the ONLY thing enforcing a trial
      // requirement is the text leg. That leg is OverrideSubJsText on
      // OnOpenText[0x2215], which dispatches (#512) — it reads
      // RO_ACCESS_TRIALS and, when the requirement is unmet, swaps in the
      // refusal message and routes to 0x2216, which ends the conversation
      // without starting the trial. All three modes (20_MASKS, REMAINS, FORMS)
      // are handled there; OPEN falls through by design. Both legs are
      // dispatched hook types, EnJs.cpp is in the link, so the gate this row
      // sets is the gate the player gets.
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_ACCESS_MOON_MASKS_COUNT, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_SLIDER,
      "Moon Access: Masks Required", "How many masks are needed to enter the Moon.",
      0, 20, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_ACCESS_MOON_REMAINS_COUNT, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_SLIDER,
      "Moon Access: Remains Required", "How many boss remains are needed to enter the Moon.",
      0, 4, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    // Both Majora sliders were PARTIAL on "logic only: the gate needs
    // OnOpenText". They share one gate and it is live: EnJs.cpp's
    // OverrideMainJsText, on OnOpenText[0x21FC], compares
    // Rando::Logic::MoonMaskCount() and RemainsCount() against these two
    // options and substitutes "You are not strong enough to play with me..."
    // when either is short, routing to 0x21FD to end the exchange. Nothing else
    // gates them, so "logic only" stopped being true when #512 dispatched
    // OnOpenText.
    { RO_ACCESS_MAJORA_MASKS_COUNT, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_SLIDER,
      "Majora Access: Masks Required", "How many masks are needed to reach Majora.",
      0, 20, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_ACCESS_MAJORA_REMAINS_COUNT, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_SLIDER,
      "Majora Access: Remains Required", "How many boss remains are needed to reach Majora.",
      0, 4, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_ACCESS_MAJORA_REMAINS, COMBO_MM_GROUP_LOGIC, COMBO_MM_WIDGET_CHECKBOX,
      "Majora Access: Remains (retired)",
      "Retired: this option never had a consumer and, by operator ruling (ADR 0010 answer O1), never will. "
      "The row remains as a save-format tombstone.",
      0, 0, nullptr, 0,
      // The 47th id. Given a StaticData row so the option id space is total
      // (#499 step 5). RETIRED by the operator (ADR 0010 answer O1,
      // 2026-07-31): it never gains a consumer and never becomes a working
      // control — the accepted answer keeps it drawn disabled-with-reason
      // (ADR 0004 §5) rather than hidden, and keeps the always-zero StaticData
      // row for id-space totality and seed/digest-string stability (see the
      // tombstone note at StaticData/Options.cpp). A future edit that gives
      // this row a consumer or a live widget is overturning an operator
      // ruling, not finishing a TODO; the mm-rando-options lock pins this.
      COMBO_MM_LIVENESS_DORMANT, "Retired (ADR 0010 O1): no code will ever read this option" },

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
    // LIVE as of #514: the only leg this row was ever missing was the cycle-save
    // restore (AfterEndOfCycleSave copies skullTokenCount back under exactly
    // this option), and that now dispatches.
    { RO_SHUFFLE_GOLD_SKULLTULAS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Gold Skulltula Tokens", "Adds the Spider House tokens to the check pool.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_MINIMUM_SKULLTULA_TOKENS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_SLIDER,
      "Minimum Gold Skulltula Tokens",
      "Tokens needed for the Spider House reward. Forced to the vanilla value when tokens are not shuffled.",
      1, SPIDER_HOUSE_TOKENS_REQUIRED, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_MINIMUM_STRAY_FAIRIES, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_SLIDER,
      "Minimum Stray Fairies",
      "Stray Fairies needed for a Great Fairy reward. Does not affect the Clock Town fairy.",
      1, STRAY_FAIRY_SCATTERED_TOTAL, nullptr, 0,
      // Was PARTIAL on "custom fairy counts need OnActorInit". The count is
      // applied by EnElfgrp.cpp's id-keyed OnActorInit registrant on
      // ACTOR_EN_ELFGRP, which compares the held fairies against
      // RO_MINIMUM_STRAY_FAIRIES and swaps the Great Fairy's actionFunc to the
      // reward one. That hook type dispatches (#512, ExecuteForID leg present)
      // and z_actor.c's call site is live. Its other legs are VB.
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_FROGS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Frogs", "Adds the Frog Choir frogs to the check pool.",
      0, 0, nullptr, 0,
      // Pool-widening, so the payout was checked rather than assumed:
      // EnMinifrog.cpp's id-keyed OnActorInit swaps the frog to a
      // cutscene-free actionFunc, whose successor sets
      // RANDO_SAVE_CHECKS[frogCheck].eligible — the same eligible-then-award
      // path every LIVE check rides. VB_DESPAWN_FROG keeps an obtained frog
      // gone and OnOpenText[0xD81] carries the text. All three dispatch.
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_SHOPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Shops", "Adds purchaseable shop slots to the check pool.",
      0, 0, nullptr, 0,
      // The widest of the three pool-widening promotions, and the one whose old
      // reason undersold the surface: it named EnIn/EnTab (Gorman milk, the two
      // Milk Bar slots) but RCTYPE_SHOP is 25 checks. The other 22 — Trading
      // Post, Bomb Shop, Curiosity, Goron, Hags, Zora — are served by
      // EnGirlA.cpp (id-keyed OnActorInit on ACTOR_EN_GIRLA plus ~18 OnOpenText
      // registrants for the shelf descriptions, prices and refusals) and
      // EnSob1.cpp (VB_DRAW_ITEM_FROM_SOB1, id-keyed OnActorDraw on
      // ACTOR_EN_OSSAN). All four TUs are in redship.map; every leg is
      // OnActorInit / OnActorDraw / OnOpenText / VB, all dispatched. The give
      // is VB_GIVE_ITEM_FROM_OFFER setting .eligible, and pricing rides
      // VB_EXEC_MSG_EVENT. Note the two checks GeneratePools shuffles even with
      // this option OFF (RC_CURIOSITY_SHOP_SPECIAL_ITEM,
      // RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM): that machinery has been
      // carrying real checks all along, which is corroboration the path works.
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_TINGLE_SHOPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Tingle Maps", "Adds the maps Tingle sells to the check pool.",
      0, 0, nullptr, 0,
      // EnBal.cpp: VB_TINGLE_GIVE_MAP_UNLOCK sets .eligible for the map the
      // player bought, VB_NOT_AFFORD_TINGLE_MAP / VB_ALREADY_HAVE_TINGLE_MAP
      // apply the generated price and obtainability, and seven OnOpenText
      // registrants (0x1D09-0x1D16) carry the shop text. VB plus OnOpenText,
      // both dispatched; no third hook type.
      COMBO_MM_LIVENESS_LIVE, "" },
    // Re-measured leg by leg for #438's remainder (see the file header). All
    // three rode OnActorInit, which #512 dispatched; grass additionally rode
    // OnActorKill and OnActorDestroy, which #515 dispatched. Every behaviour TU
    // is under 2s2h/Rando/, which links WHOLE_ARCHIVE, and every remaining leg
    // is ShouldVanillaBehavior, live since #392. Nothing left to block them.
    { RO_SHUFFLE_CRATE_DROPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Crate Drops", "Adds the item dropped by breaking a crate to the check pool.",
      0, 0, nullptr, 0,
      // Was DORMANT on "the drop's OnActorInit dispatch is not placed". It is
      // placed: ObjKibako.cpp keys COND_ID_HOOK(OnActorInit) on ACTOR_OBJ_KIBAKO
      // and ACTOR_OBJ_KIBAKO2, and MM_GameHooks_ExecuteOnActorInit carries the
      // ExecuteForID leg those need. Its other two legs
      // (VB_CRATE_DRAW_BE_OVERRIDDEN, VB_BARREL_OR_CRATE_DROP_COLLECTIBLE) are
      // ShouldVanillaBehavior.
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_BARREL_DROPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Barrel Drops", "Adds the item dropped by breaking a barrel to the check pool.",
      // ObjTaru.cpp, same two hook types as the crate row: id-keyed OnActorInit
      // on ACTOR_OBJ_TARU plus VB_BARREL_OR_CRATE_DROP_COLLECTIBLE.
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
    { RO_SHUFFLE_GRASS_DROPS, COMBO_MM_GROUP_SHUFFLE, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Grass Drops", "Adds the item dropped by cutting grass to the check pool.",
      // The one that needed two separate fixes, and the reason this row was held
      // back longest. ObjGrass.cpp identifies actor-backed bushes through
      // id-keyed OnActorInit (ACTOR_EN_KUSA) -- live since #512 -- but the
      // NON-actor grass elements, which are the bulk of the pool (216 in Termina
      // Field alone), get their RandoCheckIds only from its id-keyed OnActorKill
      // registrant on ACTOR_OBJ_GRASS_UNIT, and the entries that creates are
      // freed only by its id-keyed OnActorDestroy registrants. #515 dispatched
      // that pair together, for exactly that reason. Five VB legs, all live.
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },

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
    // LIVE as of #515. The one leg this row was missing was the kill-drop path:
    // EnemyDrops.cpp's unkeyed OnActorKill registrant, which covers the 18
    // DROP_TYPE_KILL enemies, and which now dispatches
    // (MM_GameHooks_ExecuteOnActorKill). Its other three legs were already live
    // — VB_ENEMY_DROP_COLLECTIBLE for DROP_TYPE_NORMAL, and OnFlagSet /
    // OnSceneFlagSet for the two cutscene deaths (Captain Keeta, Igos) — and the
    // payout itself is a CustomItem::Spawn, not a hook, so nothing else gates it.
    { RO_SHUFFLE_ENEMY_DROPS, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_CHECKBOX,
      "Enemy Drops", "Shuffles the first drop from a non-boss enemy.",
      0, 0, nullptr, 0, COMBO_MM_LIVENESS_LIVE, "" },
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
    // THE ROW THE #669 RE-MEASURE LEFT GATED, NOW PROMOTED BY FIXING WHAT IT
    // NAMED (#678). Worth reading as the whole arc, because it is the one row
    // where each layer of reason was wrong in a different way.
    //
    //   1. The ORIGINAL reason ("half-day prompts need OnOpenText dispatch")
    //      was stale: ClockShuffle.cpp's own legs are an id-keyed
    //      ShouldActorUpdate on ACTOR_EN_TEST4, six OnOpenText registrants and
    //      two VB flags, and all of those have dispatched since #512.
    //   2. #669/#677 replaced it with the real blocker, an ELISION rather than
    //      a dispatch gap: Rando::ClockShuffle::IsTimeOwnedForClockShuffle,
    //      GetTimeDescriptionForMessage and SetTimeToHalfDayStart have their
    //      only callers in 2s2h/Enhancements/Songs/BetterSongOfDoubleTime.cpp
    //      and SkipSoTCutscenes.cpp, both of which register purely through
    //      file-scope RegisterShipInitFunc objects and were dropped from the
    //      plain-archive 2ship_enh — the #516 elided-registrar class exactly.
    //   3. #678 fixes that. Both TUs are carved into 2ship_enh_clockshuffle,
    //      which links WHOLE_ARCHIVE (games/mm/CMakeLists.txt), so both objects
    //      are in the binary, both registrars run, and the three ClockShuffle
    //      functions they call have live callers again.
    //
    // THE THREE-PART EVIDENCE (ADR 0004 §5), measured at #678:
    //   TU links      — BetterSongOfDoubleTime.cpp.obj and SkipSoTCutscenes.cpp.obj
    //                   are present in redship.map, along with
    //                   ?IsTimeOwnedForClockShuffle@..., ?GetTimeDescriptionForMessage@...
    //                   and ?SetTimeToHalfDayStart@... — all five were 0-hit
    //                   before. ClockShuffle.cpp itself was always in the link
    //                   (2s2h/Rando/ is WHOLE_ARCHIVE'd).
    //   Registrars run — Rando::ClockShuffle::OnFileLoad() ← OnSaveLoadHandler
    //                   ← Rando::Init on OnSaveLoad, and the two Songs
    //                   registrars through S2H::ShipInit (their ShipInit map
    //                   entries and their live hook registrations are both
    //                   asserted by the MMClockShuffleSongs ctest row).
    //   Dispatch       — ShouldActorUpdate, OnOpenText, ShouldVanillaBehavior,
    //                   OnActorUpdate and OnActorKill all have MM dispatch
    //                   points in GameExports_SingleExe.cpp, with live
    //                   unguarded call sites.
    //
    // WHAT THE OLD REASON OVER-CLAIMED, recorded so the next re-measure does not
    // re-learn it. "Song of Double Time will warp into a half-day the player has
    // not unlocked" was the consequence of the elision only for a player who had
    // BetterSongOfDoubleTime ON — and while that TU was elided, nobody could:
    // the CVar existed but its whole enhancement was absent, so vanilla Song of
    // Double Time ran, and ClockShuffle's OWN proactive skip
    // (CheckAndSkipUnownedTime on ShouldActorUpdate[ACTOR_EN_TEST4]) still
    // pushed an unowned destination forward. The accurate statement of the
    // defect is the one this fix addresses: the two enhancements were dead, so
    // turning either on did nothing, and their clock-shuffle-aware legs — the
    // selector refusing an unowned half-day by name, and a Song of Time reset
    // landing on the earliest OWNED half-day rather than the vanilla dawn — did
    // not exist to be reached.
    { RO_CLOCK_SHUFFLE, COMBO_MM_GROUP_ITEMS, COMBO_MM_WIDGET_CHECKBOX,
      "Shuffle Time", "Breaks the three-day cycle into six half-days that must be unlocked as items.",
      0, 0, nullptr, 0,
      COMBO_MM_LIVENESS_LIVE, "" },
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
    // The whole family was gated on "OnOpenText has no MM dispatch point"
    // (#438's largest dormant surface, 98 registrations). It has one, since
    // #512. All six rows are re-measured LIVE for #669, and they are the
    // CHEAPEST promotions in the table for a reason worth stating once here:
    // no RO_HINTS_* id appears in Logic/GeneratePools.cpp. A hint row adds
    // nothing to the check pool and places no item; it reads a placement the
    // fill already made and renders a sentence about it. The unwinnable-seed
    // hazard that makes promotion the high-stakes direction cannot reach them.
    //
    // The shared mechanism, which is what actually had to be verified: every
    // handler below builds a CustomMessage::Entry, calls
    // CustomMessage::LoadCustomMessageIntoFont() ITSELF, and sets
    // *loadFromMessageTable = false — so the text reaches the screen through
    // z_message.c's live `if (!loadFromMessageTable)` leg, NOT through a
    // second dispatch into the CUSTOM_MESSAGE_ID registrant. That distinction
    // is the one that could have gone wrong: the dispatcher keys ExecuteForID
    // once per call, so a handler that only staged text and rewrote textId
    // would never be re-entered.
    { RO_HINTS_GOSSIP_STONES, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Gossip Stone Hints", "Each gossip stone gives a fixed hint about one location's contents.",
      0, 0, nullptr, 0,
      // Was PARTIAL as "breaks stones": VB_GS_CONSIDER_MASK_OF_TRUTH_EQUIPPED
      // forced the permissive verdict while the text that justified it was
      // dead. EnGs.cpp's OnOpenText[FIRST_GS_MESSAGE] handler now runs, so the
      // stone says what the forced verdict promised. VB_GS_CONTINUE_TEXTBOX
      // routes to SECOND_GS_MESSAGE, also dispatched.
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_HINTS_PURCHASEABLE, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Purchaseable Gossip Hints", "Gossip stones sell a hint for a scaling rupee cost.",
      0, 0, nullptr, 0,
      // Same TU and the same two OnOpenText ids as the row above; this one adds
      // the choice prompt to the first message and does the rupee deduction in
      // the SECOND_GS_MESSAGE handler. Both are live.
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_HINTS_SPIDER_HOUSES, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Spider House Hints", "Hints for the two Spider House rewards.",
      0, 0, nullptr, 0,
      // EnSsh.cpp, OnOpenText[0x915 / 0x1130 / 0x1131]. Reads the placed item
      // off RANDO_SAVE_CHECKS and names it; no other hook type involved.
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_HINTS_HOOKSHOT, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Hookshot Hint", "The Zora on Great Bay Coast hints where the Hookshot is.",
      0, 0, nullptr, 0,
      // EnZow.cpp, six OnOpenText ids onto one handler. The purest case in the
      // batch: OnOpenText is the TU's only hook type, so dispatch was the
      // entire question and it is answered.
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_HINTS_BOSS_REMAINS, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Boss Remains Hints", "The Clock Town recruitment posters hint where the Boss Remains are.",
      0, 0, nullptr, 0,
      // EnTalk.cpp registers exactly one hook, OnOpenText[0x1C06], which walks
      // the four remains across successive poster reads via nextMessageID.
      COMBO_MM_LIVENESS_LIVE, "" },
    { RO_HINTS_OATH_TO_ORDER, COMBO_MM_GROUP_HINTS, COMBO_MM_WIDGET_CHECKBOX,
      "Oath to Order Hint", "Skull Kid hints where Oath to Order is once the Moon is reachable.",
      0, 0, nullptr, 0,
      // DmStk.cpp pairs OnOpenText[0x2013] with an id-keyed ShouldActorUpdate
      // on ACTOR_DM_STK that makes Skull Kid offer the talk in the first place.
      // Both hook types dispatch, so neither half is left half-armed — which
      // mattered here, since the text alone with no talk offer is a hint the
      // player can never trigger.
      COMBO_MM_LIVENESS_LIVE, "" },
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

// ============================================================================
// THE TRICK TABLE'S DESCRIPTORS (#578 part 1)
// ============================================================================
//
// The same seam, one table over: MM owns the `MMRT_*` id space and its strings,
// src/common gets flat data (combo_mm_tricks_view.h explains why this is a
// SEPARATE table rather than extra option rows).
//
// EVERY FIELD EXCEPT THE TWO HONESTY FLAGS IS COPIED from the
// `Rando::StaticData::Tricks` row, not restated — same rule as the option table,
// and the `mm-trick-table` lock asserts the copy is exact. What is NOT in the MM
// row and is decided here is `bound`: whether some region condition in THIS
// BUILD consults the key.
//
// `kBoundTricks` IS THE PART THAT MUST NOT ROT, and it is the same anti-staleness
// problem the option table's reason strings had twice (#438's remainder, then
// #669: a row went on citing a blocker that had been fixed months earlier). The
// shape of the fix there was "do not transcribe a claim you can prove" — take the
// address of the dispatcher and let the linker prove it. There is no equivalent
// address to take here: a binding is a token inside a `std::function` in a region
// definition, not a symbol. So the claim is transcribed, and the lock that keeps
// it honest is different in kind: `mm-trick-table` EVALUATES part 1's two bound
// keys' real conditions with the trick off and then on, and `mm-trick-bindings`
// (#578 part 2) does the same, row by row, for every key part 2 bound — both fail
// if the verdict does not move. A key listed here whose binding was deleted turns
// one of those red; a key bound in a region file and not listed here draws
// disabled-with-reason, which is wrong-but-visible rather than wrong-and-silent.
//
// Part 3 adds keys to this list as it authors their bindings. Keep it sorted the
// way the table is (id order) so the diff reads as an append.
namespace {

// ID ORDER, not the order the parts landed in — one list a reader can diff
// against the table, rather than two appended blocks that interleave.
const MMRandoTrickId kBoundTricks[] = {
    // part 2. Logic/Regions/{BeneathTheWell,East,IkanaCastle,North,
    // SnowheadTemple}.cpp — 16 `ITEM_LENS_OF_TRUTH` terms, i.e. every one in
    // Regions/ except the two the trick's own definition excludes (North.cpp's
    // header note names them).
    MMRT_LENS,
    // part 2. Logic/Regions/South.cpp — RR_DEKU_PALACE_INSIDE_LOWER's new
    // connection to RR_DEKU_PALACE_INSIDE_UPPER_CELL_SIDE.
    MMRT_PALACE_BEAN_SKIP,
    // part 2. Logic/Regions/North.cpp — the Mountain Village -> Goron Graveyard
    // exit, the site MMRT_LENS excludes by name.
    MMRT_DARMANI_WALL,
    // part 3. Logic/Regions/West.cpp — RR_PINNACLE_ROCK_ENTRANCE's connection to
    // RR_PINNACLE_ROCK_INNER; the seahorse term only, CAN_BE_ZORA kept.
    MMRT_NO_SEAHORSE,
    // part 2. Logic/Regions/West.cpp — the four Zora Hall back-room doors.
    MMRT_ZORA_HALL_HUMAN,
    // part 3. Logic/Regions/East.cpp — RR_IKANA_CANYON_LOWER's connection to
    // RR_IKANA_CANYON_UPPER; the Ice Arrow term only, the Hookshot kept.
    MMRT_ICELESS_IKANA,
    // part 2. Logic/Regions/MilkRoad.cpp — RR_MILK_ROAD <->
    // RR_MILK_ROAD_BEHIND_FENCE, both directions.
    MMRT_GORON_BOMB_JUMP,
    // part 3. Logic/Regions/Central.cpp — RE_BOMBER_CODE, which is what both
    // RC_CLOCK_TOWN_BOMBERS_NOTEBOOK rows already read.
    MMRT_BOMBER_GUESS,
    // part 3 (second pass). Logic/Regions/South.cpp — RR_DEKU_PALACE_OUTSIDE's
    // connection to RR_DEKU_PALACE_INSIDE_LOWER, the bare Deku term that models
    // the guards (the neighbouring water terms all carry a cleared-swamp leg).
    MMRT_PALACE_GUARD_SKIP,
    // part 3. Logic/Regions/SnowheadTemple.cpp —
    // RR_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER's connection up, that room only.
    MMRT_SHT_PILLAR_ROOM_HOOKSHOT,
    // part 1 (finding (a)). Logic/Logic.h's CAN_USE_EXPLOSIVE — 32 uses across
    // Regions/ plus the CanKillEnemy table.
    MMRT_KEG_EXPLOSIVES,
    // part 2. Logic/Regions/MilkRoad.cpp — RC_DOGGY_RACETRACK_CHEST.
    MMRT_DOG_RACE_CHEST_NOTHING,
    // part 3. Logic/Regions/South.cpp — RC_SOUTHERN_SWAMP_PIECE_OF_HEART's
    // CAN_BE_DEKU term; the Land Title Deed flag kept.
    MMRT_SOUTHERN_SWAMP_SCRUB_HP_GORON,
    // part 3. Logic/Regions/West.cpp — RC_ZORA_HALL_SCRUB_PIECE_OF_HEART's
    // CAN_BE_DEKU term; the Mountain Title Deed flag and CAN_BE_GORON kept.
    MMRT_ZORA_HALL_SCRUB_HP_NO_DEKU,
    // part 3 (second pass). Logic/Regions/IkanaCastle.cpp — a NEW one-way edge
    // from RR_IKANA_CASTLE_INNER_ROOF to RR_IKANA_CASTLE_OUTER_ROOF.
    MMRT_IKANA_ROOF_PARKOUR,
    // part 2. Logic/Regions/Central.cpp — RC_CLOCK_TOWN_WEST_POSTMAN_MINIGAME.
    MMRT_POST_OFFICE_GAME,
    // part 3. Logic/Regions/BeneathTheWell.cpp — the Dexihand room's
    // RE_ACCESS_HOT_SPRING_WATER.
    MMRT_WELL_HSW,
    // part 3 (second pass). Logic/Regions/GreatBayTemple.cpp — the water-wheel
    // room's connection to the central room; CAN_BE_GORON is the trick's own term.
    MMRT_GBT_WATERWHEEL_GORON,
    // part 3. Logic/Regions/GreatBayTemple.cpp —
    // RC_GREAT_BAY_TEMPLE_ENTRANCE_CHEST, with the Bow as the disjunct's own term.
    MMRT_GBT_ENTRANCE_BOW,
    // part 3 (second pass). Logic/Regions/StoneTowerTemple.cpp — the six
    // Deku-gated checks in the inverted wind room whose RC names say UPDRAFTS.
    MMRT_ST_UPDRAFTS,
    // part 3. Logic/Regions/Central.cpp — the two Clock Town West bank rewards
    // that carry a wallet term.
    MMRT_BANK_NO_WALLET,
    // part 3 (second pass). Logic/Regions/WoodfallTemple.cpp — all six
    // freestanding rupees in the room outside Odolwa's door.
    MMRT_WFT_RUPEES_ICE,
    // part 3. Logic/Regions/StoneTowerTemple.cpp — the eight Zora-gated
    // pre-Twinmold floating rupees.
    MMRT_ISTT_RUPEES_GORON,
    // part 3. Logic/Regions/Central.cpp — North Clock Town's two Tingle maps.
    MMRT_NCT_TINGLE,
    // part 2. Logic/Regions/WoodfallTemple.cpp (the water-room hive) and
    // Logic/Regions/PiratesFortress.cpp (RE_PIRATE_FORTRESS_BEEHIVE_HIT).
    MMRT_HIVE_BOMBCHU,
    // part 1 (finding (b)). Logic/Regions/GreatBayTemple.cpp — the compass-room
    // boss-key connection.
    MMRT_GBT_BOSS_KEY_ICE,
};

bool IsBoundTrick(MMRandoTrickId mmRandoTrickId) {
    for (MMRandoTrickId bound : kBoundTricks) {
        if (bound == mmRandoTrickId) {
            return true;
        }
    }
    return false;
}

/** The tag set as one human string. Joined here so src/common never learns
 *  MM's tag enum; the bitmask travels too, for a future filter UI.
 *
 *  The cache is a std::map, NOT a vector, and that is load-bearing rather than
 *  taste. This function hands out a `const char*` into a stored string and is
 *  then called again for the next row: a vector reallocates as it grows and
 *  MOVES its elements, so every pointer handed out before the growth dangles —
 *  and with the short-string optimization the character buffer lives INSIDE the
 *  moved string object, so a heap buffer does not save it either. A map's nodes
 *  never move, so a returned pointer stays valid for the process's life. Keying
 *  by the bitmask also collapses the 86 rows onto the handful of distinct tag
 *  sets. The vector version of this function is what `mm-trick-table` check (c)
 *  caught: row index 2's tag summary read empty once a later row grew the
 *  vector out from under it. */
const char* TagSummary(uint32_t tags) {
    static std::map<uint32_t, std::string> sTagStrings;
    auto cached = sTagStrings.find(tags);
    if (cached != sTagStrings.end()) {
        return cached->second.c_str();
    }
    std::string s;
    static const MMRandoTrickTag kAllTags[] = { MMRTT_NOVICE,       MMRTT_INTERMEDIATE, MMRTT_ADVANCED, MMRTT_EXPERT,
                                                MMRTT_EXPERIMENTAL, MMRTT_GLITCH,       MMRTT_COMBO };
    for (MMRandoTrickTag tag : kAllTags) {
        if ((tags & (uint32_t)tag) == 0) {
            continue;
        }
        if (!s.empty()) {
            s += ", ";
        }
        s += Rando::StaticData::GetTrickTagName(tag);
    }
    if (s.empty()) {
        // The lock forbids an empty tag set; this keeps a broken table from
        // handing the pane a NULL-adjacent empty label.
        s = "(untagged)";
    }
    return sTagStrings.emplace(tags, s).first->second.c_str();
}

std::vector<ComboMMTrickDesc>& TrickDescriptorTable() {
    static std::vector<ComboMMTrickDesc> sTrickDescriptors;
    if (!sTrickDescriptors.empty()) {
        return sTrickDescriptors;
    }
    // Reserve up front: the descriptors are handed to src/common as a raw
    // pointer, so a reallocation mid-build would leave the registry pointing at
    // freed memory. (TagSummary's own strings are kept in a node-based map for
    // the same reason; see its comment.)
    sTrickDescriptors.reserve((size_t)MMRT_MAX);
    for (auto& [mmRandoTrickId, row] : Rando::StaticData::Tricks) {
        ComboMMTrickDesc desc = {};
        desc.id = (uint16_t)mmRandoTrickId;
        desc.name = row.name;
        desc.cvar = row.cvar;
        desc.label = row.displayName;
        desc.tooltip = row.tooltip;
        desc.area = (uint8_t)row.area;
        desc.areaName = Rando::StaticData::GetTrickAreaName(row.area);
        desc.tags = row.tags;
        desc.tagSummary = TagSummary(row.tags);
        desc.reserved = row.reserved;
        desc.bound = IsBoundTrick(mmRandoTrickId);
        if (row.reserved) {
            // The MM row's own reason, which names the OoT item. Reserved beats
            // unbound in the message because it is the harder blocker: no
            // region-file pass can bind these at all until increment 3.
            desc.disabledReason = row.reservedReason;
        } else if (!desc.bound) {
            desc.disabledReason = "No logic binding yet (#578 part 3)";
        } else {
            desc.disabledReason = "";
        }
        sTrickDescriptors.push_back(desc);
    }
    return sTrickDescriptors;
}

} // namespace

/** Publish the trick table to src/common. Same not-a-registrar reasoning as
 *  MM_RandoOptionsUi_Register above, for the same reason: it reads a
 *  namespace-scope std::map in another TU. */
extern "C" void MM_RandoTricksUi_Register(void) {
    auto& table = TrickDescriptorTable();
    Combo_RegisterMMTrickTable(table.data(), (int)table.size());
}

#endif // RSBS_SINGLE_EXECUTABLE
