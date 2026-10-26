/**
 * @file combo_tracker_view.h
 * @brief View model for the combo tracker: both games' check progress through
 *        per-game adapters (#458; ADR 0002, ADR 0008).
 *
 * WHAT PROBLEM THIS SOLVES. Every tracker in the binary shows exactly one
 * game: OoT's trackers read the live heap Rando::Context, MM's read the live
 * MM gSaveContext behind an active-game gate. Nothing can show the INACTIVE
 * game's progress, even though the data is resident the whole time — OoT is
 * suspended (not shut down) during MM so its heap survives, and MM's check
 * completion travels inside the frozen shadow blob as in-save POD. This model
 * is the projection of both, plus the cross-game identity and placements
 * already in gComboCtx. It is read-only but for one write, the skip toggle
 * (#458 U5), which only the LIVE panel takes.
 *
 * PER-GAME ADAPTERS, NEVER A MERGED ID SPACE (ADR 0002). The two check models
 * are irreconcilable by construction: MM keys an in-save POD table by
 * RandoCheckId, OoT keys a heap array by RandomizerCheck, and the raw u16s
 * collide freely. So each game registers its own adapter from a TU where its
 * layout/enums are in scope — the RsbsGameMetaDesc offset-descriptor pattern
 * (save.h) for MM, an accessor vtable for OoT — and every read below takes the
 * GameId. A ComboTrackerCheckRow.checkId is game-local: it is only meaningful
 * inside the panel of the game it came from and must never be compared across
 * games. The game argument IS the tag.
 *
 * FRESHNESS IS A FIELD, NOT A COMMENT. The tracker's whole point is showing
 * data that may be stale, so every summary carries an explicit freshness the
 * window acts on (no skip writes and no found state from data it cannot read):
 *   - MM reads its live save while MM is the active game AND its play state
 *     is loaded (the descriptor's liveSave hook answers non-NULL) AND that
 *     save carries the 'ZELDA3' marker: LIVE. Otherwise it reads the frozen
 *     shadow blob (Context_GetMMSaveContext), written at freeze/save time:
 *     STALE. The shadow path is never reported live — it lags the live save,
 *     and on arrival in Termina it is consumed and zeroed until MM's first
 *     save or the departure freeze refills it (#799).
 *   - OoT reads the heap Rando::Context, which is live while OoT runs and
 *     exactly as-of-suspend while MM runs (the suspend machinery is
 *     audio+graph only; the heap is not torn down).
 *   - A game with nothing to read (never booted, no adapter registered, MM
 *     shadow still all-zero) reports UNAVAILABLE — distinct from "a world
 *     with zero checks collected", which the window must not conflate.
 *
 * Locked ROM-free by the ComboTrackerView CTest
 * (src/common/tests/test_combo_tracker_view.c): adapters driven over an
 * authored MM shadow blob and an authored OoT heap context, plus the
 * unregistered/never-booted null-safety.
 */

#ifndef RSBS_COMMON_COMBO_TRACKER_VIEW_H
#define RSBS_COMMON_COMBO_TRACKER_VIEW_H

#include "foreign_items.h" // SharedItem, gComboCtx, placement accessors, GameId

#ifdef __cplusplus
extern "C" {
#endif

/** How current a panel's data is. See the header comment: MM is LIVE only
 *  while MM is played (its live save, never the shadow), OoT's heap is LIVE
 *  only while OoT is the active game. */
typedef enum {
    COMBO_TRACKER_FRESH_UNAVAILABLE = 0, // nothing to read; NOT "zero progress"
    COMBO_TRACKER_FRESH_LIVE = 1,        // reading the running game's live state
    COMBO_TRACKER_FRESH_STALE = 2,       // last freeze/save (MM) or suspend (OoT)
} ComboTrackerFreshness;

/**
 * The caption a game's panel draws under its header for a freshness value:
 * none, "" for every game and freshness (#927, the operator's 2026-10-11 rule:
 * a caption goes unless it is the label of what is drawn, and freshness is not
 * one). The item overlay's labels answer through this one. Never NULL.
 */
const char* Combo_TrackerFreshnessLabel(uint8_t game, uint8_t freshness);

/**
 * The combo identity header: the pairing key and both directions' placement
 * counts. `paired == false` means the worlds were never paired — the other
 * fields are then whatever gComboCtx holds (typically 0) and must not be shown
 * as a real pairing. The per-game panels are independent of pairing: a solo
 * OoT rando session has progress worth showing with no paired MM world.
 */
typedef struct {
    bool paired;                      // Combo_ForeignPairingActive()
    uint32_t sharedRandoSeed;         // gComboCtx.sharedRandoSeed
    uint32_t sharedRandoSettingsHash; // gComboCtx.sharedRandoSettingsHash
    uint32_t mmProfileDigest;         // gComboCtx.mmProfileDigest (0 = identity not frozen, #498/#564)
    int mmHostedForeign;              // OoT items placed into MM checks
    int ootHostedForeign;             // MM items placed into OoT checks
} ComboTrackerIdentity;

/** Fill `out` with the identity header. NULL `out` is ignored. */
void Combo_TrackerIdentity(ComboTrackerIdentity* out);

/**
 * One game's progress summary. `freshness == COMBO_TRACKER_FRESH_UNAVAILABLE`
 * means every other field is zero and the panel should say so; `hasWorld ==
 * false` with data available means the resident save is not a randomized one.
 */
typedef struct {
    uint8_t freshness; // ComboTrackerFreshness; owned by the view, not the adapter
    bool hasWorld;     // a randomized world is resident
    uint32_t seed;     // that game's own final seed (0 when none)
    int totalChecks;   // walkable row indices, [0, totalChecks)
    int shuffled;      // listed checks (OoT: SoH's Check Tracker's list once loaded, #1126)
    int obtained;      // shuffled checks already collected
    int skipped;       // shuffled checks the player marked skipped
    // REACHABILITY (#1129): whether `available` is an answer (see AVAILABILITY
    // below); false means "not known", never "none available".
    bool availabilityKnown;
    int available; // shuffled checks reachable now (0 unless availabilityKnown)
} ComboTrackerGameSummary;

/**
 * A check's status in common terms: the value the Check Tracker's colours key on
 * (#458 U4; ADR 0002's 2026-09-30 amendment, point 2). The values are SoH's
 * check-tracker states, the vocabulary its colour settings are written in. Each
 * game produces only the values it has an analogue for:
 *
 *   - OoT projects its RandomizerCheckStatus in its own TU, with SoH's own
 *     precedence (DrawLocation): RCSHOW_COLLECTED is COLLECTED, RCSHOW_SAVED is
 *     SAVED, then the heap skip flag is SKIPPED, RCSHOW_SEEN and
 *     RCSHOW_IDENTIFIED are SEEN, RCSHOW_SCUMMED is SCUMMED, RCSHOW_UNCHECKED
 *     is UNCHECKED.
 *   - MM has only its obtained and skipped flags, so the view maps them:
 *     obtained is COLLECTED, skipped is SKIPPED, anything else UNCHECKED. MM
 *     never produces SEEN, SCUMMED or SAVED.
 *
 * `obtained` on the row is exactly COLLECTED or SAVED, so the older projection
 * and this one never disagree.
 */
typedef enum {
    COMBO_TRACKER_CHECK_UNCHECKED = 0,
    COMBO_TRACKER_CHECK_SEEN = 1,      // the item there is known, not collected (OoT only)
    COMBO_TRACKER_CHECK_SCUMMED = 2,   // collected, then the game reloaded without saving (OoT only)
    COMBO_TRACKER_CHECK_SKIPPED = 3,   // the player marked it skipped
    COMBO_TRACKER_CHECK_COLLECTED = 4, // obtained (OoT: not saved yet; MM: its obtained flag)
    COMBO_TRACKER_CHECK_SAVED = 5,     // obtained and saved (OoT only)
    COMBO_TRACKER_CHECK_STATUS_COUNT
} ComboTrackerCheckStatus;

/**
 * One check, as its own game's panel renders it. `checkId` is GAME-LOCAL
 * (MM RandoCheckId / OoT RandomizerCheck) and must never cross panels — see
 * the header comment. `name` may be NULL when the game has no name table
 * loaded (e.g. OoT static data before OoT's first boot); render the id then.
 *
 * AREA (#458 U4). The panel groups its rows the way each game's own check
 * tracker does: OoT by its randomizer area (GetRCAreaName), MM by scene (the
 * native tracker's scene headers, grottos under the scene they open from).
 * `areaKey` is as game-local as `checkId`: rows of one game with equal keys
 * share an area, and the panel draws its areas in ascending key order (each
 * game's own tracker order). It is never compared across games.
 *
 * PLACED ITEM (#458 U4). `placedItemName` is the item the game's own check
 * tracker names beside a found check, or beside a seen one where that tracker
 * reveals it (#1166, Combo_TrackerNameSeenItem below), or NULL while the status
 * does not reveal it (the panel never shows more than that tracker does). `placedItemGame` is
 * that item's game: the row's own game, or the other game when the check hosts
 * a crossing, and then the name is the crossed item's real name (#796), not the
 * cover item the host physically holds. The renderer marks the other game's
 * items with the suffix the native trackers print (" (MM)" / " (OoT)").
 *
 * SHORT NAME (#458 U4). `shortName` is the name the game's own check tracker
 * prints for the check under its area header, when that differs from `name`:
 * OoT's location short name (SoH's DrawLocation prints GetShortName(), so
 * "Kokiri Sword Chest" under "Kokiri Forest", where `name` is "KF Kokiri Sword
 * Chest"). NULL when the tracker prints the full name there, as MM's does under
 * its scene headers; the grouped list then draws `name`.
 *
 * AVAILABILITY (#1129, #993 option 1). `available` is whether the check can be
 * collected now, as the game's own check tracker computes it, and
 * `availabilityKnown` whether there is an answer at all:
 *   - LIVE panel: OoT's ItemLocation::IsAvailable() once SoH's Enable Available
 *     Checks has run a recalculation, under the same gate the list has (OoT
 *     played, a randomizer file loaded, SoH's tracker settings loaded); MM's
 *     Rando::CheckTracker::ComputeChecksInLogic on its live save, refreshed at
 *     most every 20 frames as MM's own tracker does, while Enable Available
 *     Checks is on.
 *   - STALE panel: the set captured at that game's last departure freeze
 *     (Combo_TrackerFreezeAvailability), carried by the .redsave; never a live
 *     computation, which would read the other game's state.
 *   - otherwise (the option off, no recalculation yet, no capture, a
 *     .redsave written before #1129): not known. The window then draws no
 *     glyph and never filters the row out.
 * A found check is never available (both trackers drop it from the set).
 *
 * PRICE (#1166). `hasPrice` is whether the row prints a price after its item,
 * as SoH's DrawLocation prints " - <price>" (or the price alone) beside an
 * identified check it can sell (ItemLocation::CanBePurchased, SoH 2ba368e7e);
 * `price` is that price, 0 otherwise. OoT only: MM's tracker prints none.
 */
typedef struct {
    uint16_t checkId;
    const char* name; // may be NULL; storage is the owning game's static table
    bool shuffled;
    bool obtained;
    bool skipped;
    uint8_t status;             // ComboTrackerCheckStatus
    uint16_t areaKey;           // game-local area id; see AREA above
    const char* areaName;       // may be NULL; storage is the owning game's
    const char* placedItemName; // NULL unless the status reveals it; storage is the owning game's
    uint8_t placedItemGame;     // GameId of placedItemName's item; GAME_NONE when it is NULL
    const char* shortName;      // name under the area header, or NULL for `name`; see SHORT NAME above
    bool availabilityKnown;     // see AVAILABILITY above
    bool available;             // reachable now; meaningful only when availabilityKnown
    bool hasPrice;              // see PRICE above
    uint16_t price;
} ComboTrackerCheckRow;

/**
 * A SEEN check's item (#1166): what SoH's DrawLocation prints beside a check it
 * has seen (RCSHOW_SEEN, a shop slot looked at) or identified (RCSHOW_IDENTIFIED,
 * its price read too), as facts the OoT TU reads in its own terms. The rule is
 * here, free of OoT's enums, so the ROM-free lock holds it:
 *   - The item is named unless SoH hides it: a merchant's without Merchant Hint
 *     Text or under Mysterious Shuffle, a scrub's without Scrub Hint Text or
 *     under Mysterious Shuffle, a shop slot's under Mysterious Shuffle when it
 *     holds a randomized item (IsMysteryShopItem).
 *   - A named ice trap goes by its disguise once seen and by its trick name once
 *     identified, as SoH names it; any other item by the name SoH's tracker
 *     gives it (PlacedItemTrackerName: a crossing host's crossed item).
 *   - The price follows an identified check SoH lists and can sell
 *     (CanBePurchased: a shop, scrub or merchant), whether or not the item is
 *     named.
 */
typedef enum {
    COMBO_TRACKER_SALE_NONE = 0,     // not sold: no price, never hidden by the sale rules
    COMBO_TRACKER_SALE_SHOP = 1,     // RCTYPE_SHOP
    COMBO_TRACKER_SALE_SCRUB = 2,    // RCTYPE_SCRUB
    COMBO_TRACKER_SALE_MERCHANT = 3, // RCTYPE_MERCHANT
} ComboTrackerSaleKind;

typedef struct {
    uint8_t game;              // the row's game: an ice trap's disguise and trick name are its items
    uint8_t saleKind;          // ComboTrackerSaleKind
    bool identified;           // RCSHOW_IDENTIFIED (else RCSHOW_SEEN)
    bool listed;               // on the game's tracker list (SoH's IsVisibleInCheckTracker)
    bool canBePurchased;       // ItemLocation::CanBePurchased
    bool merchantTextHint;     // the seed's Merchant Hint Text
    bool scrubTextHint;        // the seed's Scrub Hint Text
    bool mystery;              // Mysterious Shuffle
    bool mysteryShopItem;      // IsMysteryShopItem; read only for a shop slot under Mysterious Shuffle
    bool iceTrap;              // the check holds an ice trap
    const char* itemName;      // the item's tracker name (not an ice trap); NULL or "" for none
    uint8_t itemGame;          // GameId of itemName's item
    const char* looksLikeName; // an ice trap's disguise; NULL or "" for none
    const char* trickName;     // an ice trap's trick name; NULL or "" for none
    uint16_t price;
} ComboTrackerSeenItem;

/**
 * Name `seen`'s item on `row` by the rule above: `placedItemName` and
 * `placedItemGame` (NULL and GAME_NONE when hidden or unnamed), `hasPrice` and
 * `price`. Every other field is left alone. NULL arguments are ignored.
 */
void Combo_TrackerNameSeenItem(const ComboTrackerSeenItem* seen, ComboTrackerCheckRow* row);

// ============================================================================
// MM adapter: an offset descriptor over the frozen MM shadow blob
// ============================================================================
//
// MM's check completion is in-save POD (RANDO_SAVE_CHECKS inside
// ShipSaveInfo), so the whole table rides the shadow blob Context_
// GetMMSaveContext() hands out. Common code walks that blob at offsets the MM
// TU registers — the RsbsGameMetaDesc pattern — so this file never includes
// z64save.h and a layout change on MM's side updates the descriptor and its
// static_assert tripwires in the same TU (games/mm/2s2h/Rando/
// TrackerAdapterSingleExe.cpp). The live save is the same SaveContext layout,
// so the same offsets walk it while MM is played (liveSave below, #799).

typedef struct ComboMMTrackerDesc {
    // 'ZELDA3' new-file marker: mismatch means the shadow holds no MM save at
    // all (all-zero until the first freeze), which reads as UNAVAILABLE.
    uint32_t newfOffset;
    uint32_t newfLen; // <= 8
    uint8_t newf[8];
    uint32_t saveTypeOffset; // u32 read; == saveTypeRando means a rando save
    uint32_t saveTypeRando;  // MM's SAVETYPE_RANDO value
    uint32_t finalSeedOffset;
    uint32_t checkTableOffset; // randoSaveChecks[0]
    uint32_t checkStride;      // sizeof(RandoSaveCheck)
    uint32_t checkCount;       // RC_MAX
    uint32_t shuffledOffset;   // one-byte flags within a check row
    uint32_t obtainedOffset;
    uint32_t skippedOffset;
    // Display name for a check id, or NULL. Supplied by the MM TU (it resolves
    // through Rando::StaticData) so the id->name table never crosses into
    // common code. May itself be NULL.
    const char* (*checkName)(uint16_t checkId);
    // The area a check is listed under (#458 U4): its scene's name, as MM's
    // check tracker heads it, or NULL; `*outKey` receives the game-local area
    // key (ComboTrackerCheckRow.areaKey). Defined in the MM TU, which owns the
    // scene table. May itself be NULL: rows then carry no area.
    const char* (*areaName)(uint16_t checkId, uint16_t* outKey);
    // The item an obtained check's row names (#458 U4), read from `save` (the
    // live save or the shadow, whichever the view picked; same layout), with
    // its game in `*outGame`: the crossed OoT item for a check that hosts one,
    // as MM's own check tracker names it (#796), else the item MM's table
    // stores there. NULL when there is nothing to name. The view calls it only
    // for obtained rows. May itself be NULL: rows then name no item.
    const char* (*placedItemName)(const void* save, uint16_t checkId, uint8_t* outGame);
    // MM's live save, laid out exactly like the shadow blob (every offset above
    // applies to it), or NULL when it must not be read: the MM TU answers
    // &gSaveContext only while MM's play state is loaded, which excludes the
    // title screen (an unmarked bootstrap save) and file select (which scans
    // slots through the live buffer). The view calls it only while MM is the
    // active game (ADR 0008 rule 5's amendment) and uses the answer only when
    // it carries the 'ZELDA3' marker; otherwise it falls back to the shadow
    // (#799). May itself be NULL: the shadow is then the only source.
    const void* (*liveSave)(void);
    // The skip toggle's write (#458 U5), MM's own op: set check `checkId`'s
    // RANDO_SAVE_CHECKS[].skipped in MM's live save, the byte MM's own check
    // tracker flips on a row click (CheckTracker.cpp) and MM's next save
    // persists. The MM TU makes the write itself, under the LIVE read's
    // conditions (MM is the active game, its play state is loaded, the save is
    // a marked rando save), so common code never writes MM's save (ADR 0008
    // rule 5). False, with nothing written, outside those conditions or for a
    // check its tracker's button is not drawn on (not part of the seed, or
    // found). The view calls it only when its own source pick reads LIVE, never
    // for the shadow. May itself be NULL: MM's panel then offers no toggle.
    bool (*setLiveSkipped)(uint16_t checkId, bool skipped);
    // Reachability on the LIVE panel (#1129): 1 when check `checkId` is in
    // logic on MM's live save now (Rando::CheckTracker::ComputeChecksInLogic,
    // cached by the MM TU at its own tracker's 20-frame cadence), 0 when not,
    // -1 when there is no answer (Enable Available Checks off, no MM play
    // state). The answer for any id says whether the set is known at all. The
    // view calls it only when its source pick reads LIVE; a STALE panel reads
    // the departure capture instead. May itself be NULL: no availability.
    int (*liveAvailable)(uint16_t checkId);
    // The logic a check's row shows on hover (#1161): the condition MM's own
    // check tracker draws as that row's tooltip (CheckTracker.cpp's
    // accessLogicFuncs, the Regions' CHECK macro stringified), or NULL where it
    // draws none. The string is static storage. May itself be NULL: no logic.
    const char* (*logicText)(uint16_t checkId);
    // The area the player is in (#1131), as the area key MM's rows carry
    // (areaName's *outKey): the scene MM is played in, the one MM's own check
    // tracker scrolls to on OnSceneInit and filters by under Only Show Current
    // Scene (CheckTracker.cpp), through Play_GetOriginalSceneId; in a grotto,
    // the scene it was entered from, since a grotto's checks are listed there.
    // False with no MM play state. The view calls it only while MM is the
    // active game (Combo_TrackerCurrentArea). May itself be NULL: no area.
    bool (*currentArea)(uint16_t* outKey);
} ComboMMTrackerDesc;

/**
 * Install MM's descriptor (copied; the caller's storage is not retained).
 * Rejects, with a stderr complaint, geometry that would read outside the
 * MM_SAVE_CONTEXT_SIZE blob or outside a row's stride — belt and braces under
 * the MM TU's static_asserts. Passing NULL un-registers, so a test can restore
 * the registry rather than leave process-global state behind.
 */
void Combo_Tracker_RegisterMM(const ComboMMTrackerDesc* desc);

/** The registered MM descriptor, or NULL. Read-only; exposed so the ROM-free
 *  lock can author a shadow blob at the REAL registered offsets. */
const ComboMMTrackerDesc* Combo_Tracker_GetMMDesc(void);

/**
 * Build and register MM's descriptor. DEFINED MM-SIDE
 * (games/mm/2s2h/Rando/TrackerAdapterSingleExe.cpp), declared here because
 * the combo entry point is what calls it. A call rather than a file-scope
 * registrar for the OptionsUiSingleExe reason: the name resolver reads
 * Rando::StaticData::CheckNames, whose population must not race static init.
 */
void MM_TrackerAdapter_Register(void);

/**
 * The departure freeze's reachability capture (#1129, ruling D6). DEFINED
 * MM-SIDE beside MM_TrackerAdapter_Register; MM_Game_Suspend calls it right
 * after MM_HarvestSharedResources, while gSaveContext is still MM's. With MM
 * playing a randomizer file it stores MM's in-logic set, freshly computed, through
 * Combo_TrackerFreezeAvailability, or clears the record when Enable Available
 * Checks is off; with no file played (title screen, file select) it leaves the
 * last capture as it is.
 */
void MM_TrackerAdapter_CaptureAvailability(void);

// ============================================================================
// OoT adapter: an accessor vtable over the heap Rando::Context
// ============================================================================
//
// OoT's check status lives on the HEAP (Rando::Context), not in the
// SaveContext blob — and OOT_SAVE_CONTEXT_SIZE has ~1KB slack, so the blob
// route is not available even in principle. OoT is suspended, not shut down,
// during MM, so the heap survives and these accessors stay valid while MM
// runs. Every function must be null-safe for the never-booted case (the heap
// context is created lazily; Rando::Context::GetInstance() is NULL until
// something creates it).

typedef struct ComboOoTTrackerOps {
    // Fill everything except `freshness` (the view owns freshness). Returns
    // false — leaving `out` untouched — when no heap context exists.
    bool (*summary)(ComboTrackerGameSummary* out);
    // Walkable row indices; 0 when no heap context exists.
    int (*checkCount)(void);
    // Row `index`; false when out of range or no heap context exists. Fills
    // every field of the row, the U4 ones included (status, area, placed item:
    // OoT derives them in its own TU, ADR 0002's amendment point 2); the view
    // zeroes the row first, so a field left alone reads as "none".
    bool (*checkAt)(int index, ComboTrackerCheckRow* out);
    // Display name for a check id, or NULL (never-initialized static data).
    const char* (*checkName)(uint16_t checkId);
    // ---- The skip toggle (#458 U5): the one write, LIVE panel only ----------
    // Both NULL (a read-only registrant: the panel then offers no toggle) or
    // both set. The view calls them only while OoT is the active game.
    //
    // Whether OoT takes a skip write now: a rando save is loaded, so the heap
    // belongs to the file being played. (Close to, not the same as, SoH's own
    // Check Tracker draw condition; the OoT TU states the difference.)
    bool (*skipWritable)(void);
    // Set check `checkId`'s skip flag on the heap and persist it, as SoH's Check
    // Tracker's skip button does (randomizer_check_tracker.cpp, DrawLocation):
    // its tracker-data save section is written and its own area counts follow.
    // False, with nothing written, for a check its button is not drawn on (not
    // part of the seed, or found) or when skipWritable answers false.
    bool (*setSkipped)(uint16_t checkId, bool skipped);
    // The logic a check's row shows on hover (#1161): the condition SoH's Check
    // Tracker draws as that row's tooltip under Show Logic (DrawLocation:
    // LocationAccess::GetConditionStr of the check in its parent region), or
    // NULL for none. Valid until the next call. May itself be NULL: no logic.
    const char* (*logicText)(uint16_t checkId);
    // The area the player is in (#1131), as the area key OoT's rows carry
    // (RandomizerCheckArea): SoH's Check Tracker's currentArea, the area its
    // window scrolls to on a transition (randomizer_check_tracker.cpp,
    // CheckTrackerTransition / GetCheckArea). False while OoT plays no
    // randomizer file or before the first transition. The view calls it only
    // while OoT is the active game. May itself be NULL: no area.
    bool (*currentArea)(uint16_t* outKey);
} ComboOoTTrackerOps;

/**
 * Install OoT's accessor vtable (the pointer is retained; the OoT TU passes a
 * static). Passing NULL un-registers. A vtable with a NULL read member, or with
 * exactly one of the two skip members, is rejected with a stderr complaint — a
 * half-registered adapter would turn "unavailable" into a null call through
 * the window's draw path.
 */
void Combo_Tracker_RegisterOoT(const ComboOoTTrackerOps* ops);

/**
 * Register OoT's accessor vtable. DEFINED OoT-SIDE
 * (games/oot/soh/Enhancements/randomizer/TrackerAdapterSingleExe.cpp),
 * declared here because the combo entry point is what calls it.
 */
void OoT_TrackerAdapter_Register(void);

/**
 * OoT's departure capture (#1129), the twin of MM_TrackerAdapter_CaptureAvailability:
 * DEFINED OoT-SIDE, called by OoT_Game_Suspend after OoT_HarvestSharedResources.
 * With OoT playing a randomizer file it stores the checks SoH's tracker lists and
 * IsAvailable() marks available, or clears the record when that set is not known
 * (Enable Available Checks off, no recalculation yet, SoH's tracker not loaded);
 * with no randomizer file played it leaves the last capture as it is.
 */
void OoT_TrackerAdapter_CaptureAvailability(void);

// ============================================================================
// The reads the window performs (all null-safe; game is GAME_OOT or GAME_MM)
// ============================================================================

/** Fill `out` with `game`'s summary. An unregistered adapter, a never-booted
 *  OoT, or an MM shadow with no save all yield UNAVAILABLE zeros, never a
 *  crash. NULL `out` is ignored. */
void Combo_TrackerGameSummary(uint8_t game, ComboTrackerGameSummary* out);

/** Walkable row indices for `game`; 0 whenever its data is UNAVAILABLE. */
int Combo_TrackerCheckCount(uint8_t game);

/** Row `index` of `game`'s check table (raw table order, unshuffled rows
 *  included — the renderer filters). False out of range / unavailable. */
bool Combo_TrackerCheckAt(uint8_t game, int index, ComboTrackerCheckRow* out);

/** Display name for `game`'s check `checkId`, or NULL. */
const char* Combo_TrackerCheckName(uint8_t game, uint16_t checkId);

/** The logic `game`'s own check tracker shows on hovering check `checkId` (#1161;
 *  the adapters' logicText), or NULL for none or no adapter. Valid until the next call. */
const char* Combo_TrackerCheckLogic(uint8_t game, uint16_t checkId);

/**
 * The area `game`'s player is in (#1131), as that game's area key (the
 * ComboTrackerCheckRow.areaKey space, never compared across games), in
 * `*outKey` (which may be NULL); false when no area is known. The ACTIVE game's
 * adapter answers (its currentArea op) and the view keeps the answer; the
 * inactive game answers the area kept from its last live answer (its last
 * frame of play, since the window asks every frame), never its adapter, whose
 * state belongs to the other game then. A game never asked while active, or
 * whose last live answer was "none", answers false.
 */
bool Combo_TrackerCurrentArea(uint8_t game, uint16_t* outKey);

// ============================================================================
// Reachability of the INACTIVE game (#1129, ruling D6: captured at the freeze)
// ============================================================================
//
// The far game's panel is a snapshot (MM's frozen shadow, OoT's suspended
// heap), so a live computation cannot serve it: MM's crawl reads gSaveContext,
// which belongs to the other game then, and OoT's set is only as current as
// its last recalculation. Each game's suspend path therefore captures its own
// set at the departure freeze into gComboCtx (context.h, the tracker
// availability records), the crossing commit that follows writes it to the
// .redsave, and the STALE panel reads it back. Refreshing that set live as the
// active game obtains things is the follow-up.

/**
 * Store `game`'s departure capture: one BIT per check id, LSB first within each
 * byte (id i is bits[i >> 3] >> (i & 7) & 1), for ids [0, checkCount). NULL
 * `bits` or a zero `checkCount` clears the record ("not known"); a `checkCount`
 * past the record's capacity is refused with a stderr complaint and clears it
 * too, never truncates. Other games are ignored.
 */
void Combo_TrackerFreezeAvailability(uint8_t game, const uint8_t* bits, uint32_t checkCount);

/** `game`'s captured answer for `checkId`: 1 available, 0 not, -1 no capture
 *  (or an id past it). */
int Combo_TrackerFrozenAvailability(uint8_t game, uint16_t checkId);

// ============================================================================
// The skip toggle (#458 U5): the view's only write, made by the game's adapter
// ============================================================================
//
// LIVE PANEL ONLY. A check is marked skipped in the game's own live state, the
// same flag that game's own check tracker toggles. The view writes nothing
// itself: it asks the active game's adapter, whose TU makes the write
// (ComboOoTTrackerOps.setSkipped, ComboMMTrackerDesc.setLiveSkipped): OoT's heap ItemLocation
// (persisted to its tracker-data save section, as SoH's skip button does), MM's
// RANDO_SAVE_CHECKS[].skipped in the live save (persisted by MM's next save, as
// MM's tracker's row click is). The other game's panel is a snapshot (MM's
// frozen shadow, OoT's suspended heap) and is never written: the next arrival
// replaces a shadow edit, and a suspended heap edit would land behind the back
// of the save it belongs to.
//
// WHICH CHECKS. SoH's rule (DrawLocation draws the button only on a check that
// is not found): a check of the seed, not collected. MM's own tracker also
// flips a found check's flag, with no visible effect there; one window keeps
// one rule.

/** Whether `game`'s panel takes skip writes now: its data is LIVE and its
 *  adapter takes the write (MM: its live save; OoT: a loaded save). */
bool Combo_TrackerSkipWritable(uint8_t game);

/** Whether the window offers the skip toggle on `row` of `game`'s panel: the
 *  panel takes writes, and the check is part of the seed and not found. */
bool Combo_TrackerRowSkippable(uint8_t game, const ComboTrackerCheckRow* row);

/**
 * Set `game`'s check `checkId` skipped (or not) in that game's live state.
 * True when the flag now holds `skipped` (written by this call, or already
 * so); false, with nothing written anywhere, when the panel does not take
 * writes (a snapshot, no adapter, no loaded save) or the check is not
 * skippable.
 */
bool Combo_TrackerSetSkipped(uint8_t game, uint16_t checkId, bool skipped);

// ============================================================================
// Cross-game crossings: the rows both panes draw (#755, #757)
// ============================================================================
//
// WHERE THE ROWS COME FROM. Under the single bag (ADR 0010 increment 3, PR #743)
// the crossing store (crossing_store.h) is the ONLY record of which host holds
// which foreign item; the two pinned tables in gComboCtx are written by the
// legacy spoiler load alone. So a crossing row is exactly what the host's give
// path would yield (Combo_GetForeignPlacementForCheck and its OoT twin): the
// pinned rows of `hostGame`'s table first, in slot order, then the store's rows
// for `hostGame` in insertion order, skipping a store row whose host already has
// a pinned row (the give path reads the pinned row there, so that store row is
// unreachable and must not be listed twice). On a single-bag world the pinned
// tables are empty and the rows are the store's, in the order the one spoiler's
// combo.crossingStore section prints them.
//
// NAMES. The host check is named by the HOST game's tracker adapter (the same
// name the per-game panel prints for that check; for MM that is the readable
// CheckNames spelling, not the RC_* spelling MM's describer serves). The item is
// named by its ORIGIN game's describer, with that describer's article.
//
// FOUND. A crossing is found once its host check is collected in the host
// game's own save: MM's RANDO_SAVE_CHECKS[host].obtained (the bit MM's give path
// gates the crossing's delivery on), OoT's check status for the host. That is
// per host, so two copies of one item on two hosts are counted apart; the
// shared-item array cannot say that (its entries carry no host and are recycled
// once redeemed). The host game's data carries the per-game panel's freshness:
// each game's is live while that game is played; otherwise MM's is the last
// game switch or save and OoT's the last game switch. When the
// host game has nothing to read, found is UNKNOWN, never "not found".

/** Whether a crossing's host check has been collected, per the host game's save. */
typedef enum {
    COMBO_TRACKER_FOUND_UNKNOWN = 0, // the host game's data is unavailable, or it has no row for the check
    COMBO_TRACKER_FOUND_NO = 1,
    COMBO_TRACKER_FOUND_YES = 2,
} ComboTrackerFound;

/**
 * One crossing: host check `hostCheckId` of `hostGame` yields the foreign item
 * `(originGame, itemId)`. `itemName` and `itemArticle` are never NULL
 * (`itemArticle` is "" when the describer has none; it carries its own trailing
 * space, as Combo_GetForeignItemArticle's does); `hostCheckName` may be NULL,
 * and the renderer then prints the id.
 */
typedef struct {
    uint8_t hostGame;          // game whose world holds the check
    uint16_t hostCheckId;      // that game's check id
    const char* hostCheckName; // resolved via the host game's adapter; may be NULL
    uint8_t originGame;        // the item's id-space owner (the other game)
    uint16_t itemId;
    const char* itemName;    // never NULL
    const char* itemArticle; // never NULL
    uint8_t found;           // ComboTrackerFound
    bool availabilityKnown;  // the host check's row's (#1129); see AVAILABILITY
    bool available;
} ComboTrackerForeignRow;

/** Crossings hosted by `hostGame` (see "WHERE THE ROWS COME FROM"); 0 when the
 *  worlds are not paired (same honesty rule as the spoiler view: "not paired"
 *  must never render as "no crossings"). */
int Combo_TrackerForeignCount(uint8_t hostGame);

/** Fill `out` with `hostGame`'s crossing `index`, in the order above. False for
 *  a NULL `out`, an out-of-range index, or an unpaired world. */
bool Combo_TrackerForeignRowAt(uint8_t hostGame, int index, ComboTrackerForeignRow* out);

/** One direction's totals: the counts the panes put in their notes. */
typedef struct {
    int total;         // == Combo_TrackerForeignCount(hostGame)
    int found;         // rows whose found == COMBO_TRACKER_FOUND_YES
    uint8_t freshness; // ComboTrackerFreshness of the host game's data; UNAVAILABLE = found is not known
} ComboTrackerForeignProgress;

/** Fill `out` with `hostGame`'s totals. NULL `out` is ignored. */
void Combo_TrackerForeignProgress(uint8_t hostGame, ComboTrackerForeignProgress* out);

/** Fallback ComboTrackerForeignRow.itemName for a placement whose item its
 *  origin's describer cannot name (the spoiler view's placeholder rule). */
#define RSBS_TRACKER_UNKNOWN_ITEM_NAME "Unknown Foreign Item"

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_TRACKER_VIEW_H
