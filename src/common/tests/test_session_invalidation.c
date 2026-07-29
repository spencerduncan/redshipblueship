/**
 * @file test_session_invalidation.c
 * @brief Cross-game session invalidation on reset / new game — issue #440
 *
 * The operator's repro, headlessly: play a rando on OoT slot 3, SOFT RESET
 * (no process restart), start a NEW rando on slot 1, walk into the Happy Mask
 * Shop — and MM came up carrying the PREVIOUS session's state, in-game clock
 * where the old session left it and the Laundry Pool stray fairy still
 * collected. "The gamestate didn't reset at all."
 *
 * Root cause: cross-game session state is process-global and had no
 * invalidation inverse. The frozen blobs, both shadow copies (the SAME storage
 * inside FrozenStateManager) and every session field of gComboCtx survived a
 * soft reset untouched. PR #400 made Combo_ConsumeFrozenState retire the blob
 * it hands over, which stops a blob being consumed TWICE — but a dead session's
 * blob was still sitting there for the FIRST consume of the next session, which
 * is exactly what the operator hit.
 *
 * It is also cross-seed contamination, and that is why the netplay spike
 * (#460) is blocked on it: a stale sharedItemsTagged carries another player's
 * grants from a dead room into a fresh seed.
 *
 * What this file locks, in the order a session actually ends:
 *
 *   1. Session A really is resident — the precondition the whole bug needs.
 *   2. A cross-game ARRIVAL must NOT invalidate. The negative control, and the
 *      most dangerous way to "fix" this bug: an arrival walks the same title
 *      chain as a reset, so an unguarded hook would eat the blob the arrival is
 *      on its way to consume and turn every return leg into a lost session.
 *   3. Soft reset to title retires the blobs, the shadows and gComboCtx.
 *   4. Entering MM after the reset finds NOTHING to consume — the operator's
 *      symptom, asserted directly.
 *  4b. The COMPOSED regression, and post-#447 the headline one: session A
 *      frozen -> reset -> new game on a different slot -> first MM entry must
 *      PAIR, not merely be fresh. #447 (d075aeaf) made MM's arrival guard key
 *      off Combo_ConsumeFrozenState's return value to answer "does this MM save
 *      already exist?" — so a dead session's blob tells a brand-new seed that
 *      it does, and the paired world is never generated at all. The dependency
 *      is one-directional: invalidating the blob restores correct pairing with
 *      no change to the guard.
 *   5. New game keeps a generated seed stamp — and (#534) the reverse
 *      placement table generation authored WITH that stamp — and drops
 *      everything else. Generation runs before the file is created and
 *      nothing re-places the reverse table afterwards, so a wipe here is a
 *      permanent loss: #524's MM-items-in-OoT-checks never deliver and the
 *      zeroed table is persisted by the new slot's first .redsave.
 *   6. A NON-rando new file drops the seed stamp too, or
 *      Combo_ForeignPairingActive() would keep reporting a paired world.
 *   7. A legitimate existing-slot load STILL restores that slot's .redsave.
 *      The do-not-break case: fixing the leak by breaking restore would be a
 *      worse bug than the leak.
 *   8. Loading a slot with NO .redsave yields no cross-game state rather than
 *      inheriting the previous session's.
 *
 * Included at FILE SCOPE (compiled as C++, NOT inside an extern "C" block) so
 * it can drive the C++-linkage rsbs::SaveManager for cases 7 and 8, the same
 * way test_save_roundtrip.c does. Everything else it touches is C-linkage and
 * declared through the headers below.
 */

#include "../context.h"
#include "../entrance.h"
#include "../foreign_items.h"
#include "../game.h"
#include "../save.h"
#include "../shared_items.h"
#include "../test_runner.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

// The consume side of the freeze machinery (src/common/switch.cpp). Declared
// locally for the same reason test_hotswap_freeze.c does: the switch policy has
// no header of its own.
extern "C" int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);

#define SESSION_ASSERT(cond)                                                                                           \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                  \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

namespace {

const char* const kSessionTestDir = "rsbs_test_session";

// A slice of a SaveContext is enough: Context_FreezeState/RestoreState clamp to
// min(size, per-game capacity), so a short buffer round-trips exactly. Same
// trick test_hotswap_freeze.c uses.
const size_t kSessionBuf = 512;

// Byte fills standing in for "session A" and "session B" MM saves. In the
// operator's terms, kSessionAByte is the old seed's clock and stray-fairy bits.
const uint8_t kSessionAByte = 0xA5;
const uint8_t kSessionBByte = 0x5B;

// Reverse-table rows (OoT checks hosting MM items, #524): one belonging to the
// dead session A, one authored by the mirrored generation pass below. Distinct
// on purpose — case 5 must be able to tell "generation's table was kept" from
// "the dead session's table leaked through the keep-set" (#534).
const uint16_t kSessionAReverseCheck = 501;
const uint16_t kGenReverseCheck = 777;
const uint16_t kGenReverseItemId = 91;

// Put a recognizable, fully-populated session into every global the bug leaks
// through: both frozen blobs, both shadows, the seed stamp, the crossing
// tables, and a staged-but-uncommitted pickup in the RAM outbox.
void SeedSessionA(uint32_t seed, uint32_t settingsHash) {
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    ComboContext_Init();

    std::vector<uint8_t> blob(kSessionBuf, kSessionAByte);
    Context_FreezeState(GAME_MM, MM_ENTR_SOUTH_CLOCK_TOWN_0, blob.data(), blob.size());
    Context_FreezeState(GAME_OOT, OOT_ENTR_MARKET_FROM_MASK_SHOP, blob.data(), blob.size());

    gComboCtx.sourceGame = GAME_OOT;
    gComboCtx.sourceEntrance = OOT_ENTR_HAPPY_MASK_SHOP;
    gComboCtx.sharedFlags[3] = 0xDEADBEEFu;
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = seed;
    gComboCtx.sharedRandoSettingsHash = settingsHash;

    // The two fields the netplay grant model composes with (#460).
    Combo_RecordSharedItem(GAME_OOT, 42);
    SharedItem foreign;
    foreign.originGame = (uint8_t)GAME_OOT;
    foreign.flags = 0;
    foreign.id = 77;
    Combo_SetForeignPlacement(/*mmCheckId=*/9, foreign);

    // The dead session's REVERSE row. Present so every DROP path below is
    // proven to wipe it — the keep-set (#534) is generation's table, never a
    // dead session's.
    SharedItem foreignRev;
    foreignRev.originGame = (uint8_t)GAME_MM;
    foreignRev.flags = 0;
    foreignRev.id = 88;
    Combo_SetForeignPlacementOoT(kSessionAReverseCheck, foreignRev);

    // Staged but never committed — the session died mid-pickup.
    Combo_StageSharedItem(GAME_OOT, 43);
}

// What MM's cross-game arrival decides once #447 (d075aeaf) merged.
//
// The authoritative copy is MM_Rando_PairOnCrossGameArrival in
// games/mm/2s2h/GameExports_SingleExe.cpp; this mirrors its decision because
// BOTH of its inputs are src/common state — Combo_ForeignPairingActive() and
// Combo_ConsumeFrozenState()'s return value. That is exactly why #440 was able
// to change the guard's answer without a line of pairing logic being wrong:
// the guard asks "does this MM save already exist?", and a dead session's blob
// answers "yes" for a save that does not exist. So post-#447 this bug does not
// merely leak the old clock and stray fairies into the new seed — it makes the
// new seed DECLINE TO PAIR AT ALL, suppressing the headline feature.
//
// Mirrored rather than called: the real guard needs gSaveContext and MM's
// SAVETYPE_RANDO, which a headless src/common test has no business booting.
// MMPairSwitchEntry (games/mm/2s2h/mm_rando_gen_test.cpp) covers the dispatch
// itself; what belongs HERE is that the inputs the guard reads are correct.
enum PairDecision {
    PAIR_DECLINE_NO_WORLD,       // Combo_ForeignPairingActive() == false
    PAIR_DECLINE_EXISTING_SAVE,  // hadFrozenState != 0 — the #440 failure
    PAIR_PROCEED,                // dispatches OnSaveInit; the paired world activates
};

PairDecision ArrivalPairDecision(int hadFrozenState) {
    if (!Combo_ForeignPairingActive()) {
        return PAIR_DECLINE_NO_WORLD;
    }
    if (hadFrozenState) {
        return PAIR_DECLINE_EXISTING_SAVE;
    }
    return PAIR_PROCEED;
}

// Author the generation-time gComboCtx state the way Playthrough_Init does —
// which for a new file happens BEFORE the file is created: the seed stamp,
// then immediately the reverse placement table derived from it
// (OoT_PlaceForeignItems clears a predecessor's rows and re-places; #510).
// Mirrored rather than called for the usual reason: the real pass needs a
// finished fill, which a headless src/common test has no business booting.
void StampGeneratedSeed(uint32_t seed, uint32_t settingsHash) {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = seed;
    gComboCtx.sharedRandoSettingsHash = settingsHash;

    Combo_ClearForeignPlacementsOoT();
    SharedItem mmItem;
    mmItem.originGame = (uint8_t)GAME_MM;
    mmItem.flags = 0;
    mmItem.id = kGenReverseItemId;
    Combo_SetForeignPlacementOoT(kGenReverseCheck, mmItem);
}

// Every session field must read as freshly initialized.
bool SessionIsClear(bool expectSeedStamp) {
    if (Context_HasFrozenState(GAME_OOT) || Context_HasFrozenState(GAME_MM)) {
        return false;
    }
    if (gComboCtx.switchRequested || gComboCtx.sourceGame != GAME_NONE || gComboCtx.sourceEntrance != 0) {
        return false;
    }
    if (gComboCtx.sharedFlags[3] != 0) {
        return false;
    }
    if (Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true) != 0) {
        return false;
    }
    if (Combo_CountForeignPlacements() != 0) {
        return false;
    }
    // The reverse table follows the seed stamp's policy (#534): generation
    // authors both together, so it may outlive invalidation ONLY when the
    // stamp does. On every DROP path it must read empty; the KEEP case's exact
    // expected content is asserted at the call site.
    if (!expectSeedStamp && Combo_CountForeignPlacementsOoT() != 0) {
        return false;
    }
    if (expectSeedStamp != gComboCtx.sourceIsRando) {
        return false;
    }
    // The unified save's active slot is session state and must not outlive the
    // session. It used to: SaveManager owned it, three sites set it, and no
    // invalidation path cleared it — so "which .redsave am I part of" survived
    // a quit to title, and an F10 into a fresh bootstrap MM session would
    // owl-save straight over the previously-loaded slot, destroying that slot's
    // MM progress and its rando pairing identity. Checked here rather than at
    // one call site so EVERY invalidation entry point is covered by the
    // assertions that already exist.
    if (RsbsSave_GetActiveSlot() != -1) {
        return false;
    }
    return true;
}

}  // namespace

TestResult Test_SessionInvalidation(void) {
    printf("[TEST] session-invalidation: soft reset / new game retire cross-game session state (#440)\n");

    Entrance_ClearStartupEntrance();

    // ---- 1. Session A is resident ----------------------------------------
    // The precondition. If this ever stops holding, every assertion below
    // passes vacuously and the test stops being a lock at all.
    SeedSessionA(0x1234u, 0xABCDu);
    SESSION_ASSERT(Context_HasFrozenState(GAME_MM) == 1);
    SESSION_ASSERT(Context_HasFrozenState(GAME_OOT) == 1);
    SESSION_ASSERT(Combo_CountSharedItems(GAME_OOT, true) == 1);
    SESSION_ASSERT(Combo_CountForeignPlacements() == 1);
    SESSION_ASSERT(Combo_CountForeignPlacementsOoT() == 1);
    SESSION_ASSERT(Combo_ForeignPairingActive());

    // ---- 2. NEGATIVE CONTROL: an arrival must not invalidate --------------
    // A cross-game arrival passes through the SAME OoT title chain a soft reset
    // does (TitleSetup -> Title fast-forward -> Opening -> Play_Init, where the
    // blob is finally consumed). An unguarded return-to-title hook would clear
    // the blob mid-flight and every return leg would silently lose its session
    // — a strictly worse bug than the one being fixed. main.cpp sets the
    // startup entrance BEFORE GameRunner_SwitchTo, so it is visible here.
    Entrance_SetStartupEntrance(MM_ENTR_SOUTH_CLOCK_TOWN_0, GAME_MM);
    SESSION_ASSERT(Context_InvalidateSessionOnReturnToTitle() == 0);
    SESSION_ASSERT(Context_HasFrozenState(GAME_MM) == 1);
    SESSION_ASSERT(Combo_CountForeignPlacements() == 1);
    SESSION_ASSERT(Combo_CountForeignPlacementsOoT() == 1);
    Entrance_ClearStartupEntrance();

    // ---- 3. Soft reset to title retires the session -----------------------
    SESSION_ASSERT(Context_InvalidateSessionOnReturnToTitle() == 1);
    SESSION_ASSERT(SessionIsClear(/*expectSeedStamp=*/false));
    // The seed stamp goes too: at the title no file is active, so nothing
    // stands behind it, and Combo_ForeignPairingActive() is literally
    // `sourceIsRando && sharedRandoSettingsHash != 0`.
    SESSION_ASSERT(gComboCtx.sharedRandoSeed == 0);
    SESSION_ASSERT(gComboCtx.sharedRandoSettingsHash == 0);
    SESSION_ASSERT(!Combo_ForeignPairingActive());
    // Both placement tables go with the session on a DROP — the reverse one
    // included, because at the title nothing stands behind it any more than
    // behind the stamp it was derived from (#534).
    SESSION_ASSERT(Combo_CountForeignPlacementsOoT() == 0);
    // The shadows are the same storage as the blobs and must be wiped with
    // them, or a tracker (or a .redsave written afterwards) would still be
    // reading the dead session's bytes.
    {
        const uint8_t* mmShadow = static_cast<const uint8_t*>(Context_GetMMSaveContext());
        const uint8_t* ootShadow = static_cast<const uint8_t*>(Context_GetOoTSaveContext());
        SESSION_ASSERT(mmShadow != nullptr && ootShadow != nullptr);
        SESSION_ASSERT(mmShadow[0] == 0x00 && mmShadow[kSessionBuf - 1] == 0x00);
        SESSION_ASSERT(ootShadow[0] == 0x00 && ootShadow[kSessionBuf - 1] == 0x00);
    }

    // ---- 4. THE OPERATOR'S SYMPTOM ----------------------------------------
    // Start the new seed and walk into the Happy Mask Shop. MM's arrival calls
    // Combo_ConsumeFrozenState — which before the fix found session A's blob
    // and restored its clock and its collected stray fairies onto a brand new
    // file. It must now find nothing, leaving MM's bootstrap save untouched.
    {
        std::vector<uint8_t> arriving(kSessionBuf, kSessionBByte);
        SESSION_ASSERT(Combo_ConsumeFrozenState("mm", arriving.data(), arriving.size()) == 0);
        SESSION_ASSERT(arriving[0] == kSessionBByte);                 // not session A's clock
        SESSION_ASSERT(arriving[kSessionBuf - 1] == kSessionBByte);   // not session A's fairy flags
        SESSION_ASSERT(arriving[0] != kSessionAByte);
    }

    // ---- 4b. COMPOSED REGRESSION: the new seed must still PAIR -------------
    // Post-#447 (d075aeaf) this is the headline consequence, and it is the one
    // a player would actually notice. MM's arrival guard decides whether to
    // generate the paired world from two src/common inputs; a stale blob makes
    // it answer "this MM save already exists" for a save that does not, so the
    // brand-new seed declines to pair and MM plays vanilla.
    //
    // First, prove the failure shape is real — that this test would CATCH a
    // regression rather than pass vacuously. Session A's blob resident, new
    // seed stamped: the guard declines.
    SeedSessionA(0x1234u, 0xABCDu);
    StampGeneratedSeed(0x99999999u, 0x5555u);
    {
        std::vector<uint8_t> arriving(kSessionBuf, kSessionBByte);
        const int hadFrozen = Combo_ConsumeFrozenState("mm", arriving.data(), arriving.size());
        SESSION_ASSERT(hadFrozen == 1);
        SESSION_ASSERT(ArrivalPairDecision(hadFrozen) == PAIR_DECLINE_EXISTING_SAVE);
    }

    // Now the real sequence: session A resident, soft reset, generate the new
    // seed, create the new file on a DIFFERENT slot, then first MM entry.
    SeedSessionA(0x1234u, 0xABCDu);
    SESSION_ASSERT(Context_InvalidateSessionOnReturnToTitle() == 1);
    StampGeneratedSeed(0x99999999u, 0x5555u);
    Context_InvalidateSessionOnNewGame(/*isRandoFile=*/1);
    {
        std::vector<uint8_t> arriving(kSessionBuf, kSessionBByte);
        const int hadFrozen = Combo_ConsumeFrozenState("mm", arriving.data(), arriving.size());
        // No stale blob, so the guard is told the truth: this MM save does not
        // exist yet.
        SESSION_ASSERT(hadFrozen == 0);
        // The new seed's stamp survived, so a paired OoT world is still visible.
        SESSION_ASSERT(Combo_ForeignPairingActive());
        // Therefore the arrival dispatches OnSaveInit and the paired world
        // activates. Invalidation restores correct pairing with NO change to
        // #447's guard — it was asking the right question all along.
        SESSION_ASSERT(ArrivalPairDecision(hadFrozen) == PAIR_PROCEED);
    }

    // ---- 5. New RANDO file: keep the seed stamp, drop the session ----------
    // Generation runs BEFORE the file is created (generate a seed in the menu,
    // then name the file), so Playthrough_Init has already stamped these fields
    // FOR this file. Dropping them here would unpair a world that was just
    // generated — which is the same "declines to pair" symptom arriving from
    // the opposite direction, and why the stamp is the ONE field invalidation
    // can be asked to preserve.
    SeedSessionA(0x1234u, 0xABCDu);
    StampGeneratedSeed(0x99999999u, 0x5555u);
    Context_InvalidateSessionOnNewGame(/*isRandoFile=*/1);
    SESSION_ASSERT(SessionIsClear(/*expectSeedStamp=*/true));
    SESSION_ASSERT(gComboCtx.sharedRandoSeed == 0x99999999u);
    SESSION_ASSERT(gComboCtx.sharedRandoSettingsHash == 0x5555u);
    // ---- 5b. #534: generation's REVERSE placement table survives too -------
    // Playthrough_Init authored it seconds after the stamp, for THIS file, and
    // nothing re-places it after file creation (the forward direction is
    // re-authored by MM at arrival; the reverse has no such second chance). A
    // wipe here is what made #524 inert in every real playthrough — with the
    // loss then persisted by the new slot's first .redsave write.
    SESSION_ASSERT(Combo_CountForeignPlacementsOoT() == 1);
    {
        const SharedItem* kept = Combo_GetForeignPlacementForOoTCheck(kGenReverseCheck);
        SESSION_ASSERT(kept != NULL);
        SESSION_ASSERT(kept->originGame == (uint8_t)GAME_MM);
        SESSION_ASSERT(kept->id == kGenReverseItemId);
    }
    // ...and it is GENERATION's table that survived, not the dead session's
    // leaking through the keep-set.
    SESSION_ASSERT(Combo_GetForeignPlacementForOoTCheck(kSessionAReverseCheck) == NULL);
    // The dead session's crossings must not reach the new seed. This is the
    // assertion the netplay agents' grant model composes with (#460): a stale
    // sharedItemsTagged is another player's grants from a dead room.
    SESSION_ASSERT(Combo_CountSharedItems(GAME_OOT, true) == 0);
    SESSION_ASSERT(Combo_CountForeignPlacements() == 0);
    // ...including the staged-but-uncommitted outbox, which would otherwise
    // drain into the NEW seed's array at its first suspend.
    SESSION_ASSERT(Combo_CommitStagedSharedItems() == 0);
    SESSION_ASSERT(Combo_CountSharedItems(GAME_OOT, true) == 0);

    // ---- 6. New VANILLA file: the seed stamp goes too ----------------------
    SeedSessionA(0x1234u, 0xABCDu);
    Context_InvalidateSessionOnNewGame(/*isRandoFile=*/0);
    SESSION_ASSERT(SessionIsClear(/*expectSeedStamp=*/false));
    SESSION_ASSERT(gComboCtx.sharedRandoSeed == 0);
    SESSION_ASSERT(gComboCtx.sharedRandoSettingsHash == 0);
    // No generation stands behind a vanilla file, so the reverse table is the
    // dead session's and goes with it — the #534 keep-set is KEEP-only.
    SESSION_ASSERT(Combo_CountForeignPlacementsOoT() == 0);
    // A surviving stamp would leave MM believing a paired world exists for a
    // seed that no longer does — the arrival must decline, and for the RIGHT
    // reason (no paired world, not "the save already exists").
    SESSION_ASSERT(!Combo_ForeignPairingActive());
    SESSION_ASSERT(ArrivalPairDecision(/*hadFrozenState=*/0) == PAIR_DECLINE_NO_WORLD);

    // ---- 7. DO NOT BREAK RESTORE: an existing slot reloads its .redsave ----
    // Fixing the leak by making loads stop restoring would be a worse bug than
    // the leak. Write slot 1 from session A, invalidate, load slot 1 back, and
    // require that slot's own state to return.
    {
        std::error_code ec;
        std::filesystem::remove_all(kSessionTestDir, ec);
        std::filesystem::create_directories(kSessionTestDir, ec);
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        mgr.SetSaveDirectory(kSessionTestDir);

        SeedSessionA(0x2468u, 0x1357u);
        std::vector<uint8_t> mmLive(MM_SAVE_CONTEXT_SIZE, kSessionAByte);
        Context_UpdateShadowCopy(GAME_MM, mmLive.data(), mmLive.size());
        SESSION_ASSERT(mgr.Save(1));

        // Something else entirely happens (a reset), then the player loads
        // slot 1 through the OnLoadFile path: clear, then reload.
        Context_InvalidateSessionOnReturnToTitle();
        SESSION_ASSERT(SessionIsClear(false));

        Context_InvalidateSessionOnSlotLoad();
        SESSION_ASSERT(mgr.HasSave(1));
        SESSION_ASSERT(mgr.Load(1));

        // Slot 1's own cross-game state is back, byte-exact.
        SESSION_ASSERT(gComboCtx.sharedRandoSeed == 0x2468u);
        SESSION_ASSERT(gComboCtx.sharedRandoSettingsHash == 0x1357u);
        SESSION_ASSERT(gComboCtx.sourceIsRando);
        SESSION_ASSERT(gComboCtx.sharedFlags[3] == 0xDEADBEEFu);
        SESSION_ASSERT(Combo_CountSharedItems(GAME_OOT, true) == 1);
        SESSION_ASSERT(Combo_CountForeignPlacements() == 1);
        SESSION_ASSERT(Combo_GetForeignPlacementForCheck(9) != nullptr);
        // ...and the slot's own reverse table (#534) — the durable complement
        // of case 5b: what the keep-set preserved at creation, the .redsave
        // must keep returning on every subsequent load.
        SESSION_ASSERT(Combo_CountForeignPlacementsOoT() == 1);
        SESSION_ASSERT(Combo_GetForeignPlacementForOoTCheck(kSessionAReverseCheck) != nullptr);
        {
            const uint8_t* mmShadow = static_cast<const uint8_t*>(Context_GetMMSaveContext());
            SESSION_ASSERT(mmShadow != nullptr);
            SESSION_ASSERT(mmShadow[0] == kSessionAByte);
            SESSION_ASSERT(mmShadow[MM_SAVE_CONTEXT_SIZE - 1] == kSessionAByte);
        }
        // A load ARMS the MM half so it is actually reachable (#35 follow-up).
        //
        // RENEGOTIATED, deliberately. This assertion used to require the
        // OPPOSITE — Context_HasFrozenState(GAME_MM) == 0 — citing #419/#420's
        // "a plain .redsave load applies on the next switch only". That
        // contract was never implementable as written: the next cross-game
        // switch freezes the DEPARTING game (Combo_CheckEntranceSwitch resolves
        // its gameId from Context_GetCurrentGame), so it never armed the
        // ARRIVING side, and the loaded MM half was applied on the next switch
        // or on any other occasion. The old assertion was the operator's "MM
        // will be reset after game restart" written down as required behavior.
        SESSION_ASSERT(Context_HasFrozenState(GAME_MM) == 1);
        // The armed blob must carry a SAFE return entrance, not 0. The F10
        // hot-swap path sets the arriving game's startup entrance from
        // Context_GetFrozenReturnEntrance, and entrance presence is a separate
        // flag, so 0 is a real consumable id — and ENTR_SCENE_MAYORS_RESIDENCE
        // is 0, which put Link inside the Mayor's Residence. Must match what
        // the real hot-swap freeze records.
        SESSION_ASSERT(Context_GetFrozenReturnEntrance(GAME_MM) == MM_ENTR_SOUTH_CLOCK_TOWN_0);
        // OoT stays unarmed on purpose: OoT's own file{N+1}.sav is the
        // authority for OoT state and Sram_OpenSave applies it on the normal
        // load path. Arming Tier-2 too would race a second, staler copy of
        // OoT's world against it.
        SESSION_ASSERT(Context_HasFrozenState(GAME_OOT) == 0);

        // ...and the complement, which is what stops the arming from being a
        // blanket "always arm": a slot whose MM half is all zeros must NOT arm.
        // That is precisely a slot saved before the player ever entered MM, and
        // arming it would restore a zeroed SaveContext over the bootstrap file
        // MM's title chain authors — strictly worse than the cold boot it
        // replaces. Without this case the empty-tier branch is untested and a
        // regression to unconditional arming would pass.
        {
            Context_InvalidateSessionOnSlotLoad();
            SeedSessionA(0x1111u, 0x2222u);
            std::vector<uint8_t> mmEmpty(MM_SAVE_CONTEXT_SIZE, 0);
            Context_UpdateShadowCopy(GAME_MM, mmEmpty.data(), mmEmpty.size());
            SESSION_ASSERT(mgr.Save(0));

            Context_InvalidateSessionOnSlotLoad();
            SESSION_ASSERT(mgr.Load(0));
            SESSION_ASSERT(Context_HasFrozenState(GAME_MM) == 0);

            // Restore slot 1 as the resident session so the cases below see the
            // state they expect.
            Context_InvalidateSessionOnSlotLoad();
            SESSION_ASSERT(mgr.Load(1));
        }

        // ---- 8. A slot with NO .redsave inherits nothing ------------------
        // The .redsave is per-OoT-slot but these globals are process
        // singletons. Slot 2 was never written, so loading it must leave no
        // cross-game state — not slot 1's, which is still resident right now.
        SESSION_ASSERT(!mgr.HasSave(2));
        Context_InvalidateSessionOnSlotLoad();
        SESSION_ASSERT(SessionIsClear(false));
        SESSION_ASSERT(gComboCtx.sharedRandoSeed == 0);
        SESSION_ASSERT(!Combo_ForeignPairingActive());

        std::filesystem::remove_all(kSessionTestDir, ec);
    }

    printf("[TEST] PASS: a dead session's frozen blobs, shadows and gComboCtx crossings do not "
           "survive a soft reset or a new game; the generation-authored seed stamp and reverse "
           "placement table survive rando file creation (#534); arrivals and existing-slot loads "
           "still restore\n");

    // Leave global state clean for any subsequent test.
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    Entrance_ClearStartupEntrance();
    return TEST_PASS;
}
