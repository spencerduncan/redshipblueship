/**
 * ForeignItemsSingleExe.cpp — the OoT-side redemption give, the crossing
 * pickup, and the paired creation event (Phase 3.0 Lane C1, #392; ADR 0002;
 * ADR 0010 increments 2 and 3).
 *
 * This is the ONE translation unit where the cross-game item class is defined,
 * because it is the one place the real RG_* enumerators may appear: everything
 * leaves this TU as an origin-tagged SharedItem (or a plain display string),
 * so a raw RG_* integer never crosses a game boundary (ADR 0002, the #356 bug
 * class). MM and the ROM-free test harness reach the pool through the
 * extern "C" surface declared in src/common/foreign_items.h.
 *
 * The pinned OoT pool this TU used to define (kForeignPoolV1, ~4 rows) and the
 * reverse overlay pass (OoT_PlaceForeignItems) are RETIRED (ADR 0010 increment
 * 3, D3; lane K11): items leave origin pools, and every crossing is a placement
 * the single-bag fill makes at the creation event.
 *
 * Redemption give: progressive entries resolve against the LIVE save via
 * Item::GetGIEntry_Copy() — Logic's save context is pointed at gSaveContext on
 * every save load (SaveManager.cpp / savefile.cpp), so this is the same
 * resolution SoH's own in-game gives use — then dispatch mirrors
 * savefile.cpp's StartingItemGive: MOD_NONE entries through OoT_Item_Give
 * (the NULL-play starting-item precedent), MOD_RANDOMIZER entries through
 * Randomizer_Item_Give.
 *
 * Lives in soh/Enhancements/randomizer/ (soh_rando) which links WHOLE_ARCHIVE,
 * so these definitions always survive the link.
 *
 * The crossing pickup (OoT_Rando_Foreign_RecordPickup) reads the placement
 * table and, behind it, the crossing store, and this file still never sees an
 * RI_*, only origin-tagged SharedItems.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring> // memcpy/memset — the creation event's snapshot bracket
#include <string>
#include <vector>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
// ReachabilitySearch — the reverse pass's reachability gate (#656). The same
// header entrance.cpp reaches it through, from the same directory.
#include "3drando/fill.hpp"

#include "foreign_items.h"       // src/common — SharedItem, the placement tables
#include "combo_single_bag.h" // src/common — the single-bag fill (ADR 0010 increment 3, lane K11)
#include "crossing_store.h"   // src/common — the creation writer of the crossings (ADR 0010 O7)
#include "shared_items.h"        // src/common — Combo_RecordSharedItem (#493)
#include "notification_bridge.h" // src/common — the shared refusal/failure overlay
#include "gen_budget.h"          // src/common — the #582 fill budget + progress surface

extern "C" {
#include <z64.h>
#include "variables.h"
#include "functions.h" // OoT_Item_Give
u16 Randomizer_Item_Give(PlayState* play, GetItemEntry giEntry);
}

// savefile.cpp's rupee helper (C++ linkage there; no header of its own).
void GiveLinkRupees(int numOfRupees);

// ============================================================================
// The pinned pool ("foreign item class v1") — RETIRED (ADR 0010 increment 3,
// D3; lane K11)
// ============================================================================
//
// kForeignPoolV1 (Progressive Strength, Lens of Truth, Boomerang, Megaton
// Hammer) and its criterion-attributed exclusion table fed MM's forward overlay
// pass, which pinned those items onto MM junk as duplicate copies. Under one bag
// every OoT progression item the frozen settings hand to the general pass may
// cross (the O8 owner, src/common/shared_items.h, decides membership), and which
// ones do is the single-bag fill's decision.

// ============================================================================
// Redemption give (called by OoT_AwardSharedItem, the A1 consumer callback)
// ============================================================================

extern "C" int OoT_ForeignItem_Give(uint16_t rgId) {
    // Progressive resolution walks Rando::Context / Logic / OTRGlobals; all
    // three exist once OoT has booted to gameplay, which the presence-gated
    // consumption point guarantees. Guard anyway: a give we cannot perform is
    // logged loudly rather than crashing the arrival path.
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandomizer == nullptr ||
        Rando::Context::GetInstance() == nullptr || Rando::Context::GetInstance()->GetLogic() == nullptr) {
        fprintf(stderr, "[OoT] foreign give unavailable (rando context not live), RG id=%u\n", (unsigned)rgId);
        return 0;
    }

    const GetItemEntry entry = Rando::StaticData::RetrieveItem((RandomizerGet)rgId).GetGIEntry_Copy();

    // Mirror savefile.cpp's StartingItemGive dispatch (NULL play: the
    // starting-item precedent; none of the pool's resolved entries touch play).
    if (entry.modIndex == MOD_NONE) {
        if (entry.itemId >= ITEM_RUPEE_GREEN && entry.itemId <= ITEM_RUPEE_GOLD) {
            static const int kRupeeCounts[] = { 1, 5, 20, 50, 200 };
            GiveLinkRupees(kRupeeCounts[entry.itemId - ITEM_RUPEE_GREEN]);
        } else {
            OoT_Item_Give(NULL, (uint8_t)entry.itemId);
        }
        return 1;
    }
    if (entry.modIndex == MOD_RANDOMIZER) {
        if (entry.getItemId == RG_ICE_TRAP) {
            gSaveContext.ship.pendingIceTrapCount++;
        } else {
            Randomizer_Item_Give(NULL, entry);
        }
        return 1;
    }

    fprintf(stderr, "[OoT] foreign give: unhandled modIndex %d for RG id=%u\n", (int)entry.modIndex, (unsigned)rgId);
    return 0;
}

// ============================================================================
// REVERSE DIRECTION (#510): the OoT placement pass — RETIRED (ADR 0010
// increment 3, D3; lane K11)
// ============================================================================
//
// OoT_PlaceForeignItems, its chest-only host predicate and its reachability gate
// (#656) are gone with the pool they drew from. The host-class half of that
// predicate lives on as the OoT engine's `hostAcceptsForeign`
// (ComboLogicEngineOoT.cpp), which the single-bag fill consults before it draws a
// crossing onto an OoT host; the fill-side half ("the fill put junk here") and
// the reachability gate are the fill's own proof now.

// ============================================================================
// REVERSE DIRECTION (#493): the PICKUP CORE — the OoT twin of
// Rando::Foreign::RecordForeignPickup
// ============================================================================
//
// WHY THIS IS A NAMED FUNCTION AND NOT THE FOUR LINES IT REPLACES. Until now
// the reverse leg's producer lived entirely inside the RC-queue drain lambda in
// hook_handlers.cpp, which needs a live PlayState, a Player and the CheckTracker
// — so nothing ROM-free could enter it, and #493's own Lock section says in as
// many words that a row which pokes the placement table instead "would be
// vacuous". The forward direction has had the extraction since Lane C1
// (Rando::Foreign::RecordForeignPickup + the MM_Rando_Foreign_RecordPickup
// bridge); this is the missing twin, and the ForeignItemGiveReverse row now
// drives it.
//
// The split is deliberately the SAME one MM makes: this function owns the
// DECISION and the durable RECORD, and owns nothing about presentation. The
// toast, the tracker write and the queue pop stay at the hook, because they are
// display/gameplay concerns that a display-free tier cannot honestly assert.
//
// THE PAIRING REFUSAL IS THE #610 RULE, APPLIED TO THE DIRECTION THAT DID NOT
// HAVE IT. Combo_RecordSharedItem takes no identity argument and performs no
// identity check (src/common/shared_items.c): whatever it records is redeemed by
// whichever paired MM world arrives next, blind to which world authored it. MM's
// forward-side core has refused to author such a record since #610; the reverse
// side did not, so an OoT session whose foreignPlacementsOoT survived into an
// unpaired state (a .redsave loaded into a solo session, a future spoiler-drop
// route, a session invalidation that KEEPs the table) could mint a crossing with
// no paired world to receive it. The check DEGRADES to the junk-class OoT item
// the host physically holds — the documented absent-placement behaviour
// (foreign_items.h) — rather than to a crossing nobody can receive.
//
// @return true only when a durable shared-item record was authored. The caller
//         presents the foreign pickup if and only if that happened.
static bool OoT_Foreign_RecordPickupImpl(uint16_t rc) {
    const SharedItem* item = Combo_GetForeignPlacementForOoTCheck(rc);
    if (item == nullptr) {
        return false;
    }

    if (!Combo_ForeignPairingActive()) {
        fprintf(stderr,
                "[OoT] foreign pickup REFUSED: OoT check %u holds a foreign placement, but this session has no live "
                "cross-game pairing (sourceIsRando=%d settingsHash=%08X). No durable shared-item record is authored — "
                "there is no paired world it could belong to (#610/#493)\n",
                (unsigned)rc, gComboCtx.sourceIsRando ? 1 : 0, gComboCtx.sharedRandoSettingsHash);
        fflush(stderr);
        return false;
    }

    // ONCE PER HOST. The drain gates on `!loc->HasObtained()` already; the same
    // gate is here too, so the recording function cannot be the thing that
    // double-delivers when a future caller forgets it.
    auto ctx = Rando::Context::GetInstance();
    Rando::ItemLocation* il = (ctx != nullptr) ? ctx->GetItemLocation((RandomizerCheck)rc) : nullptr;
    if (il != nullptr && il->HasObtained()) {
        return false;
    }

    // Durable immediately (the serialized array, so an OoT save+quit before the
    // next switch cannot lose the pickup — the stage/commit outbox is RAM-only,
    // see shared_items.h). ONE COPY PER PICKUP (ADR 0010 increment 3): under the
    // single bag two OoT hosts may hold two copies of one MM id, and both must
    // reach MM, so the record is never content-merged (RSBS_SHARED_ITEM_CROSSING).
    return Combo_RecordSharedItemCrossing((GameId)item->originGame, item->id) >= 0;
}

/**
 * The reverse direction's give-path entry point, called by the RC-queue drain
 * (hook_handlers.cpp) and driven directly by the ForeignItemGiveReverse lock.
 *
 * The exact twin of MM_Rando_Foreign_RecordPickup, and named to match: one
 * production function, two callers, so the lock covers the real recording path
 * rather than a copy of it.
 *
 * @return 1 if a durable crossing was authored for this OoT check, 0 otherwise
 *         (no foreign placement here, or no live pairing to author it for).
 */
extern "C" int OoT_Rando_Foreign_RecordPickup(uint16_t rc) {
    return OoT_Foreign_RecordPickupImpl(rc) ? 1 : 0;
}

/** TEST BRIDGE: mark an OoT check collected (or not), as the drain does after a
 *  pickup, so a lock can drive the once-per-host gate. Returns 1 when applied, 0
 *  when this process has no OoT location table to apply it to (a ROM-free row). */
extern "C" int OoT_Rando_Foreign_TestSetObtained(uint16_t rc, int obtained) {
    auto ctx = Rando::Context::GetInstance();
    Rando::ItemLocation* il = (ctx != nullptr && rc < RC_MAX) ? ctx->GetItemLocation((RandomizerCheck)rc) : nullptr;
    if (il == nullptr) {
        return 0;
    }
    il->SetCheckStatus(obtained != 0 ? RCSHOW_COLLECTED : RCSHOW_UNCHECKED);
    return 1;
}

// ============================================================================
// THE MERGED CREATION EVENT (ADR 0010 increment 2; #564's creation-event
// contract; epic #644)
// ============================================================================
//
// WHERE IT RUNS. games/oot/src/code/z_sram.c, inside Save_InitFile, after
// Context_InvalidateSessionOnNewGame retires the previous cross-game session
// and after Randomizer_InitSaveFile authors OoT's half, and BEFORE
// Save_SaveFile writes the slot. That ordering is the whole design: the
// .redsave this file's very first save produces already carries a complete,
// armed MM half, so arrival has nothing left to author.
//
// WHY THE ORCHESTRATOR IS HERE AND NOT IN z_sram.c. games/oot/src/**.c has no
// src/common on its include path (the same reason
// Context_InvalidateSessionOnNewGame is declared locally there), and the seam
// needs gComboCtx, the MM bridges and a 136KB scratch buffer. z_sram.c declares
// one function and calls it; everything structural lives in this C++ TU, which
// already owns the reverse crossing pass.
//
// THE SNAPSHOT BRACKET IS NOT OPTIONAL. gSaveContext is ONE buffer shared by
// both games (src/common/unified_save.c) reinterpreted through two layouts, so
// MM's generation writes over the OoT file this seam is in the middle of
// creating. docs/solver-inventory.md §6.1 amendment (1) names this bracket as
// the first thing any coordinator must do around an MM call, and this is its
// first production instance. The buffer is snapshotted whole
// (OOT_SAVE_CONTEXT_SIZE, the larger of the two layouts) rather than at either
// game's sizeof, because a partial restore would leave MM's bytes visible past
// OoT's struct end.
//
// FAILURE IS TOTAL (ADR 0010 increment 2). "Generation failure fails the
// creation, at file select, wholly — no partial identity, no vanilla Termina."
// On any nonzero return from the MM half this function rolls the identity back
// to nothing, clears both placement tables, and returns nonzero; z_sram.c then
// abandons the file. The caller sees one boolean and a reason string.

// The MM half of the creation event (games/mm/2s2h/GameExports_SingleExe.cpp).
extern "C" int MM_Rando_GenerateAtCreation(int slot, const char* ootSpoilerPath);
// The #533 refusal surface (src/common/save.h) and this file's own
// file-select failure toast, both raised from the failure branch below so
// that ONE callable carries the whole terminal-failure contract — z_sram.c
// only has to not write the file, and the lock can drive the surface without
// standing up OoT's file select.
extern "C" void RsbsSave_RefuseSlotGeneration(int slot);
extern "C" void OoT_Creation_ReportFailureAtFileSelect(int slot, int reason);
// The ON-SCREEN progress surface's presentation half (#582),
// games/oot/soh/SohGui/CreationProgressOverlay.cpp. Installed from the seam
// below rather than at boot so the thread it latches as "the renderer's" is
// provably this one.
extern "C" void OoT_CreationProgressOverlay_Install(void);

// The unified buffer's true capacity. context.h (already included above) pulls
// game.h, so OOT_SAVE_CONTEXT_SIZE is in scope; this assertion is what makes
// "snapshot the whole buffer" mean what it says.
static_assert(sizeof(SaveContext) <= OOT_SAVE_CONTEXT_SIZE,
              "OoT's runtime SaveContext outgrew the unified gSaveContext storage "
              "(src/common/unified_save.c); raise OOT_SAVE_CONTEXT_SIZE in src/common/game.h");
// ...and the fact that makes the bracket below SAFE at OoT's sizeof rather than
// at the unified capacity: MM's whole SaveContext fits INSIDE OoT's, so MM's
// generation cannot write past OoT's struct end and nothing beyond it needs
// restoring. Compile-time rather than a comment, because if MM's capacity ever
// overtook OoT's struct the bracket would start silently under-restoring.
static_assert(MM_SAVE_CONTEXT_SIZE <= sizeof(SaveContext),
              "MM's SaveContext capacity outgrew OoT's struct, so MM's generation can now write past the region "
              "the creation event's snapshot bracket restores (ForeignItemsSingleExe.cpp)");

// ----------------------------------------------------------------------------
// THE BRACKET'S STORAGE, AND THE ON-SCREEN OVERLAY'S WINDOW INTO IT (#582)
// ----------------------------------------------------------------------------
//
// The snapshot buffer was a static local inside OoT_RunPairedCreationEvent; it
// is at file scope now because a SECOND caller needs it. The overlay paints
// FROM INSIDE the bracketed call (that is the whole point of #582: the thread
// that would draw is the thread MM's fill is running on), and
// `Gui::StartDraw()` -> `Gui::DrawMenu()` draws every registered GuiWindow —
// including SoH's item and check trackers (SohGui.cpp's AddGuiWindow calls),
// which read gSaveContext every frame. Inside the bracket that buffer holds
// MM's world reinterpreted through OoT's layout, so an open tracker would read
// MM bytes as OoT inventory: nonsense at best, an out-of-range table index at
// worst, in the middle of creating the player's file.
//
// NAMING THE DRAW SITE CORRECTLY IS LOAD-BEARING, and the first cut of #582 got
// it wrong in a way that misplaced the fix. It said `Gui::EndDraw` /
// `DrawFloatingWindows` and handed this bracket only the overlay's own ImGui
// draw — so the trackers, which draw one statement EARLIER inside
// `Gui::StartDraw()`, were outside it and still read MM's bytes. In this tree
// `Gui::DrawFloatingWindows()` draws no SoH window at all; it is
// ImGui::UpdatePlatformWindows / RenderPlatformWindowsDefault under
// ImGuiConfigFlags_ViewportsEnable. The painter now hands this bracket the WHOLE
// StartDraw -> EndFrame sequence (CreationProgressOverlay.cpp), which is what
// makes the guarantee below true of every widget on a pumped frame rather than
// only of the progress window.
//
// So a painted frame runs under OoT's bytes and MM's are put back afterwards.
// The swap is WHOLE-BUFFER in both directions, which is what makes it invisible
// to the fill: MM's generation cannot observe a buffer that is byte-identical
// before and after every call it did not make. Two statics rather than one
// because both worlds have to be live at once for the duration of the paint;
// sizeof(SaveContext) each, for the reason the bracket itself is that size (the
// static_assert above).
//
// OUTSIDE a bracket this is a plain call-through, which is what makes the
// headless rows and the menu-side generation path see no new behaviour at all.
static char sOoTSaveSnapshot[sizeof(SaveContext)];
static char sMmInFlightSave[sizeof(SaveContext)];
static bool sCreationBracketActive = false;
// MM's finished half, kept past the bracket for the ONE spoiler's join (lane K11):
// the join reads MM's world out of gSaveContext, and under one bag it runs AFTER
// OoT's own remainder has been placed and its spoiler written, which needs OoT's
// bytes live. So the join gets a second, short bracket over these bytes.
static char sMmFinishedSave[sizeof(SaveContext)];

extern "C" void OoT_Creation_PaintWithOoTSaveVisible(void (*paint)(void)) {
    if (paint == nullptr) {
        return;
    }
    if (!sCreationBracketActive) {
        paint();
        return;
    }
    memcpy(sMmInFlightSave, &gSaveContext, sizeof(SaveContext));
    memcpy(&gSaveContext, sOoTSaveSnapshot, sizeof(SaveContext));
    // RAII rather than a trailing memcpy: this runs inside MM's fill, and an
    // escaping exception (the fill itself throws GenerationTimeout and
    // std::runtime_error) that skipped the restore would hand the rest of the
    // fill OoT's bytes to work on.
    struct MmSaveRestore {
        ~MmSaveRestore() {
            memcpy(&gSaveContext, sMmInFlightSave, sizeof(SaveContext));
        }
    } restore;
    paint();
}

/**
 * An FNV-1a signature of the WHOLE live gSaveContext (#582's review).
 *
 * TEST SEAM WITH A PURPOSE THE POST-HOC COMPARISON COULD NOT SERVE. The
 * combo-creation-event row already compares gSaveContext byte for byte AFTER the
 * creation returns, and that comparison is green whether or not the paint bracket
 * exists at all — the unconditional restore above puts OoT's bytes back either
 * way. What it cannot see is which world's bytes a widget drawn on a PUMPED FRAME
 * read while the creation was still in flight, which is exactly the property the
 * bracket is for and exactly where the first cut of #582 got it wrong.
 *
 * So the bytes get a cheap identity a test-registered GuiWindow can record from
 * inside `Gui::DrawMenu()`'s own loop — the same loop the item and check trackers
 * draw from — and compare against the post-creation value. A signature rather
 * than a copy because the observer runs inside a frame and must not spend 136 KB
 * of memcpy per draw, and because equality is the whole question.
 */
extern "C" uint32_t OoT_Creation_SaveSignature(void) {
    const unsigned char* bytes = (const unsigned char*)&gSaveContext;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < sizeof(SaveContext); i++) {
        hash ^= (uint32_t)bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

/**
 * IS THE LIVE gSaveContext OoT's SNAPSHOT RIGHT NOW? Asked from inside a pumped
 * frame, while the creation is still in flight (#582's review).
 *
 * @return 0 when no creation bracket is active, so there is nothing to check;
 *         1 when a bracket IS active and the live buffer is byte-identical to
 *         OoT's snapshot — i.e. this frame's widgets are looking at the same world
 *         the surrounding file-select frames look at;
 *        -1 when a bracket is active and the live buffer is MM's in-flight world.
 *
 * WHY THE COMPARISON HAPPENS HERE AND NOT IN THE TEST. The question is only
 * meaningful DURING the bracket, and a row cannot see inside a frame. Recording a
 * signature and comparing it afterwards is not equivalent and was tried first: the
 * LAST frame of a creation is the terminal paint, which happens after the bracket
 * closed, so a remembered-last-value observer reads OoT's world no matter how
 * wrong the bracket is, and the check passes vacuously. The verdict has to be
 * formed frame by frame, while the answer can still be "no".
 *
 * memcmp rather than the signature above because exactness is free here: this runs
 * at most at the overlay's 10 Hz repaint interval, and only in a process that
 * registered the observer.
 */
extern "C" int OoT_Creation_LiveSaveIsOoTSnapshot(void) {
    if (!sCreationBracketActive) {
        return 0;
    }
    return memcmp(&gSaveContext, sOoTSaveSnapshot, sizeof(SaveContext)) == 0 ? 1 : -1;
}

/**
 * Run the MM half of the creation event over a snapshot-bracketed
 * gSaveContext, and publish or retract the pairing identity accordingly.
 *
 * @param slot the slot being created (gSaveContext.fileNum at the seam).
 * @return 1 when the whole creation succeeded (including "this is not a paired
 *         file", which succeeds by authoring nothing); 0 when the paired
 *         creation FAILED and the file must not be written.
 */
// ComboLogicEngineOoT.cpp: OoT's per-game remainder after the single-bag fill.
extern "C" int OoT_ComboLogic_FinishGeneralPass(int writeSpoiler);

/**
 * OoT'S SIDE OF A PAIRED CREATION, after the MM half has run the single-bag fill
 * (ADR 0010 increment 3; lane K11). Two steps, in this order:
 *
 *   1. THE CROSSINGS, captured from the coordinator's tables into the crossing
 *      store (ADR 0010 O7): the one durable record of which host of either game
 *      yields an item of the other, frozen with the world and persisted in the
 *      .redsave's Tier-4 by the file's first Save_SaveFile().
 *   2. OoT's REMAINDER (OoT_ComboLogic_FinishGeneralPass): its junk, renewables
 *      and traps onto its leftover hosts, its overrides, its hints and, when
 *      `writeSpoiler`, its spoiler document.
 *
 * A named function rather than lines in OoT_RunPairedCreationEvent because the
 * headless harnesses that drive MM's half directly (the golden digests, the
 * ladder and switch-entry rows) must complete the SAME world the creation event
 * completes, not a copy of the steps.
 *
 * @return the number of crossings stored (>= 0); negative when the creation must
 *         fail (-1 the store refused the crossings, -2 OoT's remainder could not
 *         run). On failure the store is left empty.
 */
extern "C" int OoT_Creation_FinishPairedHalf(int writeSpoiler) {
    const int crossings = Combo_Crossings_CaptureFromCoordinator();
    if (crossings < 0) {
        fprintf(stderr, "[OoT] creation: the crossing store refused the single bag's crossings (%s)\n",
                Combo_Crossings_StatusName(crossings));
        return -1;
    }
    if (OoT_ComboLogic_FinishGeneralPass(writeSpoiler) != 0) {
        fprintf(stderr, "[OoT] creation: OoT's remainder after the single-bag fill could not run\n");
        Combo_Crossings_Clear();
        return -2;
    }
    return crossings;
}

extern "C" int OoT_RunPairedCreationEvent(int slot) {
    if (!Combo_ForeignPairingActive()) {
        // A vanilla file, or a rando file whose stamp the KEEP identity check
        // discarded (#597). Nothing to author; not a failure — UNLESS OoT's world
        // is a paired generation's, waiting at its general pass for a single-bag
        // fill that can now never run (the pairing it was generated for is gone:
        // a creation that failed and retracted it, or a stamp discarded since).
        // That world has no general pass at all, so a file created from it would
        // be missing most of OoT's items. Refused, the same way a failed creation
        // is (ADR 0010 increment 3, lane K11): the player generates again.
        if (OoT_ComboLogic_GeneralPassDeferred() != 0) {
            fprintf(stderr,
                    "[OoT] creation event: slot %d REFUSED — OoT's world was generated for a paired creation that no "
                    "longer has an identity, and its general pass was never placed; generate the seed again\n",
                    slot);
            fflush(stderr);
            RsbsSave_RefuseSlotGeneration(slot);
            OoT_Creation_ReportFailureAtFileSelect(slot, 0);
            return 0;
        }
        return 1;
    }

    // The pre-Fill gate's answer, read HERE FROM THE FROZEN RECORD rather than
    // re-resolved from the CVars (#657/#667). That is the whole point of the
    // gate's second act: the seam must be unable to reach a different conclusion
    // than the creation did, and the only way to guarantee that is to read the
    // record the freeze wrote. Combo_ComboDirectionArms serves the frozen
    // direction once Combo_ComboSettingsFrozen() is true, so this is the same
    // fact both placement passes gate on.
    const bool crossingsAuthored =
        Combo_ComboDirectionArms((uint8_t)GAME_OOT) || Combo_ComboDirectionArms((uint8_t)GAME_MM);
    fprintf(stderr,
            "[OoT] creation event: slot %d, masterSeed=%u settingsHash=%08X mmProfileDigest=%08X "
            "comboFingerprint=%08X crossingsAuthored=%d\n",
            slot, gComboCtx.sharedRandoSeed, gComboCtx.sharedRandoSettingsHash, gComboCtx.mmProfileDigest,
            gComboCtx.comboSettingsHash, crossingsAuthored ? 1 : 0);
    // A LOG, NOT A REFUSAL, and the asymmetry is deliberate. The sanctioned
    // writers refuse while frozen, so a live CVar that disagrees with the record
    // by now can only have come from a raw store write — worth naming loudly
    // because it means something is authoring behind the freeze. It changes
    // nothing: the frozen record is the authority for a created world, which is
    // precisely why a player toggling a setting between Generate and file select
    // must NOT fail their creation.
    if (Combo_ForeignCrossingsRequested() != crossingsAuthored) {
        fprintf(stderr,
                "[OoT] creation event: WARNING the live combo store now resolves crossings=%d against a frozen "
                "record that authored %d — a raw write reached a frozen world; the RECORD wins (#657)\n",
                Combo_ForeignCrossingsRequested() ? 1 : 0, crossingsAuthored ? 1 : 0);
    }

    // THE OoT SPOILER IS NOT WRITTEN YET (ADR 0010 increment 3, lane K11). A
    // paired world's OoT half stopped at its general pass at Generate, and its
    // spoiler is written below, once the single-bag fill inside the MM half has
    // placed the bag and OoT's own remainder is down. So the MM half is handed
    // no path and does not join; the join runs at the end of this function.

    // The creation seam's own progress session (#582). Separate from the one
    // Playthrough_Init opened around OoT's staged generation, because the two
    // are separated by however long the player spent in the menu. THIS is the
    // one whose elapsed time a player actually waits through at file select, and
    // therefore the one the ~30 s floor is about (P12).
    //
    // The on-screen leg is installed FIRST so that Begin's own report already
    // paints: the player must see the overlay before MM's first fill attempt
    // starts, not after it ends. Installing here (rather than at boot) also
    // latches the render thread to the thread the creation actually blocks,
    // which is what keeps OoT's menu-side worker-thread generation from
    // reaching a renderer (CreationProgressOverlay.cpp).
    OoT_CreationProgressOverlay_Install();
    Combo_GenProgress_Begin();

    // THE BRACKET. Its buffer is a file static (declared above, beside the
    // overlay's window into it) rather than a stack array: OoT's SaveContext is
    // ~136 KB, far past any sane frame budget; this seam is game-thread-only,
    // and MM's own attempt ladder uses the same shape for the same reason. The
    // `active` flag is what lets a frame painted from INSIDE the call below show
    // OoT's bytes instead of MM's (#582).
    //
    // SIZED AT OoT's sizeof, NOT at the unified capacity, and that is a fix
    // rather than a nicety. Copying OOT_SAVE_CONTEXT_SIZE bytes THROUGH a
    // SaveContext* reads and writes past the declared object even though the
    // real storage behind it is a larger char array, which glibc's
    // _FORTIFY_SOURCE catches as "*** buffer overflow detected ***" and aborts —
    // green on MSVC, SIGABRT on the Linux CI leg. The static_assert above is what
    // makes the smaller copy sufficient: MM's whole SaveContext fits inside
    // OoT's, so there is nothing past OoT's struct end for MM to have written.
    memcpy(sOoTSaveSnapshot, &gSaveContext, sizeof(SaveContext));
    sCreationBracketActive = true;

    const int mmRc = MM_Rando_GenerateAtCreation(slot, "");

    sCreationBracketActive = false;
    // MM's finished half, kept for the spoiler join at the end (see its buffer).
    memcpy(sMmFinishedSave, &gSaveContext, sizeof(SaveContext));
    memcpy(&gSaveContext, sOoTSaveSnapshot, sizeof(SaveContext));
    // The OoT-side tail, measured for the same reason MM measures its two
    // stretches (#582's review asked for numbers rather than the assertion that
    // "everything else reports a phase and moves on within milliseconds"). This one
    // is the shortfall stats read and a toast, and the number below is what says so.
    const uint32_t ootTailStartMs = Combo_GenProgress_ElapsedMs();

    if (mmRc != 0) {
        // TERMINAL. Retract everything the freeze published so no artifact of a
        // half-created world survives: no identity for a later arrival to
        // compare against, no crossing tables for either direction, no armed MM
        // shadow (the MM half never armed one — it returned before that, or
        // arming itself failed).
        fprintf(stderr,
                "[OoT] creation event: FAILED (MM half rc=%d) — retracting the pairing identity; slot %d must not be "
                "written\n",
                mmRc, slot);
        fflush(stderr);
        Combo_GenProgress_End(false);
        // The player-visible half, raised HERE rather than by the caller: a
        // creation that failed must surface identically from every route into
        // it, and the only way to guarantee that is for the surface to live
        // with the verdict.
        RsbsSave_RefuseSlotGeneration(slot);
        OoT_Creation_ReportFailureAtFileSelect(slot, 0);
        Context_ClearFrozenState(GAME_MM);
        Combo_ClearForeignPlacements();
        Combo_ClearForeignPlacementsOoT();
        Combo_ClearForeignGiveCaps();
        // The single bag's own artifacts (lane K11): no crossings for a world that
        // was not created, and no engine record that a later fill could "restore".
        Combo_Crossings_Clear();
        Combo_SingleBag_Forget();
        // formatVersion 0 is the record's ABSENT tag (ADR 0011 decision 4.2) —
        // the occupancy byte that makes the other eleven usable. Zeroing it is
        // how a record is retracted; there is deliberately no "unfreeze" API,
        // because the only legitimate retraction is this one.
        memset(&gComboCtx.comboSettings, 0, sizeof(gComboCtx.comboSettings));
        gComboCtx.comboSettingsHash = 0;
        gComboCtx.sourceIsRando = false;
        gComboCtx.sharedRandoSeed = 0;
        gComboCtx.sharedRandoSettingsHash = 0;
        gComboCtx.mmProfileDigest = 0;
        gComboCtx.mmPairedAttempt = 0;
        return 0;
    }

    // ------------------------------------------------------------------------
    // THE SINGLE BAG'S OoT-SIDE TAIL (ADR 0010 increment 3, D3; lane K11).
    //
    // The MM half ran the single-bag fill over both worlds and MM's own pass over
    // MM's leftovers. What is left is OoT's, and it runs HERE because it needs
    // OoT's bytes live (its spoiler writer reads gSaveContext.language):
    //
    //   1. THE CROSSINGS, captured from the coordinator's tables into the store
    //      (ADR 0010 O7) — the one durable record of which host of either game
    //      yields an item of the other, frozen with the world, persisted in the
    //      .redsave's Tier-4 by the Save_SaveFile() this seam's caller runs next.
    //   2. OoT's REMAINDER: its junk, renewables and traps onto its leftover
    //      hosts, overrides, hints, and its spoiler document.
    //   3. THE ONE SPOILER: MM's half joined into OoT's document as "combo", over
    //      a second short bracket that puts MM's finished bytes back in view.
    //   4. COMMIT: the coordinator's and both engines' roll-back records are
    //      dropped, so no later fill can "restore" this world.
    //
    // A failure in any of them fails the creation the same way a failed MM half
    // does: nothing is written, nothing of the identity survives.
    // ------------------------------------------------------------------------
    Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_CROSSINGS, 0, "Filling Hyrule's remaining checks");
    const int crossings = OoT_Creation_FinishPairedHalf(1);
    bool tailOk = crossings >= 0;
    if (tailOk) {
        Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_SPOILER, 0, "writing the paired spoiler");
        const std::string cvarPath = CVarGetString(CVAR_GENERAL("SpoilerLog"), "");
        if (cvarPath.empty()) {
            fprintf(stderr, "[OoT] creation event: no OoT spoiler on record - the paired half has nothing to join\n");
        } else {
            std::string relative = cvarPath;
            if (relative.rfind("./", 0) == 0) {
                relative = relative.substr(2);
            }
            const std::string ootSpoilerAbsolute = Ship::Context::GetPathRelativeToAppDirectory(relative.c_str());
            memcpy(sOoTSaveSnapshot, &gSaveContext, sizeof(SaveContext));
            memcpy(&gSaveContext, sMmFinishedSave, sizeof(SaveContext));
            sCreationBracketActive = true;
            MM_Rando_AugmentSpoilerWithPairedHalf(ootSpoilerAbsolute.c_str());
            sCreationBracketActive = false;
            memcpy(&gSaveContext, sOoTSaveSnapshot, sizeof(SaveContext));
        }
    }
    const ComboSingleBagReport* bag = Combo_SingleBag_LastReport();
    fprintf(stderr,
            "[OoT] creation event: single bag — %d crossings stored (%d OoT items in Termina, %d MM items in Hyrule), "
            "%d rows placed, %d surplus dropped, %d rounds, fill %ums\n",
            crossings, bag->crossingsIntoMM, bag->crossingsIntoOoT, bag->fill.placed, bag->fill.surplusDropped,
            bag->fill.rounds, bag->wallMs);
    Combo_SingleBag_Forget();
    if (!tailOk) {
        Combo_GenProgress_End(false);
        RsbsSave_RefuseSlotGeneration(slot);
        OoT_Creation_ReportFailureAtFileSelect(slot, 0);
        Context_ClearFrozenState(GAME_MM);
        Combo_ClearForeignPlacements();
        Combo_ClearForeignPlacementsOoT();
        Combo_ClearForeignGiveCaps();
        Combo_Crossings_Clear();
        memset(&gComboCtx.comboSettings, 0, sizeof(gComboCtx.comboSettings));
        gComboCtx.comboSettingsHash = 0;
        gComboCtx.sourceIsRando = false;
        gComboCtx.sharedRandoSeed = 0;
        gComboCtx.sharedRandoSettingsHash = 0;
        gComboCtx.mmProfileDigest = 0;
        gComboCtx.mmPairedAttempt = 0;
        return 0;
    }

    fprintf(stderr,
            "[OoT] creation event: the OoT-side tail after MM's half returned (crossings, OoT's remainder, the spoiler "
            "join) took %ums (#582)\n",
            Combo_GenProgress_ElapsedMs() - ootTailStartMs);
    fflush(stderr);
    Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_PUBLISH, 0, nullptr);
    Combo_GenProgress_End(true);
    fprintf(stderr, "[OoT] creation event: slot %d complete — both halves authored under one frozen identity\n", slot);
    fflush(stderr);
    return 1;
}

/**
 * THE FILE-SELECT FAILURE SURFACE (ADR 0010 increment 2; #533/#568's machinery,
 * new leg).
 *
 * Before this increment a paired generation that could not converge was
 * discovered in Termina, hours later: OnFileCreate's catch reverted the MM save
 * to vanilla and the arrival gate raised the refusal. The whole point of moving
 * generation to the creation seam is that the failure now lands WHERE THE
 * DECISION WAS MADE — at file select, on the same shared overlay the arrival
 * refusals use, while the player still has the settings that caused it in front
 * of them.
 *
 * MUTED, like every other cross-game refusal toast, and the reasoning that said
 * otherwise was wrong in an instructive way. "This fires inside OoT's own file
 * select, where the audio session certainly exists" is true of PRODUCTION and
 * false of this function's other callers: the creation event is driven directly
 * by the ROM-free and display-free locks, where Notification::Emit's unmuted arm
 * reaches Audio_PlaySoundGeneral with no archives mounted. That is the same
 * hazard the arrival refusals mute for, and the seam does not get an exemption
 * just because its production caller is better equipped. The toast is the
 * surface; the sound is not load-bearing.
 *
 * VISUAL, THEREFORE UNVERIFIABLE BY THE TIERS. The locks assert the CREATION's
 * verdict (no file written, no identity left, slot refused); that the toast
 * renders is a playtest observation. Stated rather than implied.
 *
 * @param slot   the slot whose creation failed.
 * @param reason reserved for a future failure taxonomy; 0 today ("generation
 *               did not converge"), which is the only way to get here.
 */
extern "C" void OoT_Creation_ReportFailureAtFileSelect(int slot, int reason) {
    (void)reason;
    fprintf(stderr,
            "[OoT] creation event: slot %d REFUSED at file select — the paired Majora's Mask world could not be "
            "generated; no file was written and no pairing identity survives\n",
            slot);
    fflush(stderr);

    ComboNotification failureToast;
    memset(&failureToast, 0, sizeof(failureToast));
    failureToast.prefix = "File NOT created:";
    failureToast.prefixColor[0] = 0.9f;
    failureToast.prefixColor[1] = 0.35f;
    failureToast.prefixColor[2] = 0.3f;
    failureToast.prefixColor[3] = 1.0f;
    failureToast.message = "The paired Majora's Mask world could not be generated for this seed and these "
                           "settings. Nothing was saved. Try a different seed, or relax the Majora's Mask "
                           "options, and create the file again.";
    failureToast.messageColor[0] = 1.0f;
    failureToast.messageColor[1] = 1.0f;
    failureToast.messageColor[2] = 1.0f;
    failureToast.messageColor[3] = 1.0f;
    failureToast.remainingTime = 20.0f;
    failureToast.mute = 1; // see the header: this seam is driven by display-free locks too
    OoT_Notification_Emit(&failureToast);
}

#endif // RSBS_SINGLE_EXECUTABLE
