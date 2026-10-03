/**
 * @file oot_check_state_load_test.cpp
 * ROM-free, display-free lock for #849 and the load leg of #803 (the same
 * direction), plus the reverse direction: after a load,
 * OoT's check state follows the save half that was actually loaded. CTest
 * label "redship", row OoTCheckStateLoad in CMake/SingleExecutable.cmake,
 * dispatch "oot-check-state-load" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. Whether an OoT check is obtained lives on the heap
 * Rando::Context and is saved in the .sav's tracker section, not in OoT's
 * SaveContext. A load whose .redsave holds a newer whole commit than the .sav
 * (every reload after a crossing, since #846) applies the .redsave's OoT half
 * over the .sav's base section, and the statuses stay as the .sav has them:
 *
 *   Leg A (#849, #803's load leg): a heart piece collected before the crossing is in the loaded
 *   half (its collectible flag is set) but reads SCUMMED, the status a
 *   section-only tracker save writes. VB_ITEM00_DESPAWN despawns it on
 *   HasObtained() alone (hook_handlers.cpp), so it is back in the world. The
 *   same leg carries a shuffled freestanding rupee, whose flag is a
 *   RandomizerInf and whose despawn test is ShuffleFreestanding.cpp's.
 *   Leg B (reverse): a chest the .sav saved as SAVED is closed again in the
 *   loaded half (a half that went back). Opening it fires the real scene-flag
 *   hook, which queues nothing for an obtained check: the item is never given.
 *   No production load is known to produce this state (a .sav-newer pair is
 *   refused, and the half is only applied when it is newer); the leg locks
 *   the rule's other direction.
 *
 * THE SEAM UNDER TEST. OoT_Combo_OnLoadFileSeam (games/oot/soh/SaveManager.cpp),
 * the body of OoT's OnLoadFile hook, driven exactly as LoadFile drives it: the
 * .sav's base section and tracker statuses already in memory, the .sav's
 * mirrored generation passed in, a real .redsave on disk.
 *
 * CONTROLS. Checks whose flag is the same in both halves keep their status: a
 * SCUMMED chest still closed, a SAVED chest still open, and the Master Sword a
 * file creation marks SAVED without its flag (StartingItemGive on an adult
 * start). A rule that set every status from the flag alone would turn that last
 * one not-found, and pulling the sword would give it a second time.
 */

#include <z64.h>

#include "context.h"
#include "game.h"
#include "save.h"

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

extern "C" SaveContext gSaveContext;
// The seam (games/oot/soh/SaveManager.cpp).
extern "C" void OoT_Combo_OnLoadFileSeam(int32_t fileNum, uint32_t savGeneration);
// hook_handlers.cpp: the real hook a chest's treasure flag fires, and the test
// bridge that empties the queue it feeds.
void RandomizerOnSceneFlagSetHandler(int16_t sceneNum, int16_t flagType, int16_t flag);
extern "C" int OoT_Rando_TestTakeQueuedChecks(uint16_t* out, int cap);
// games/mm/2s2h/Rando/Foreign.cpp (declared in src/common/combo_mm_options_view.h):
// an MM half that is the given pair's world.
extern "C" int MM_Rando_AuthorPairHalfForTest(void* mmHalf, size_t mmHalfSize, uint32_t masterSeed);

namespace {

const char* const kSaveDir = "rsbs_test_oot_check_state_load";
constexpr int kSlot = 0;
constexpr uint32_t kMasterSeed = 0x0849C0DEu;

// Flags of the checks this row uses (location_list.cpp).
constexpr uint32_t kSwordChestBit = 1u << 0x00; // RC_KF_KOKIRI_SWORD_CHEST, SCENE_KOKIRI_FOREST
constexpr uint32_t kHeartPieceBit = 1u << 0x1E; // RC_LH_FREESTANDING_POH, SCENE_LAKE_HYLIA
constexpr uint32_t kMidoRightBit = 1u << 0x01;  // RC_KF_MIDOS_TOP_RIGHT_CHEST, SCENE_MIDOS_HOUSE
// RC_KF_BRIDGE_RUPEE, a shuffled freestanding item: its flag is a RandomizerInf
// (ShuffleFreestanding.cpp), set in the half's ship.randomizerInf.
void SetRandoInf(SaveContext* s, RandomizerInf flag) {
    s->ship.randomizerInf[flag >> 4] |= (uint16_t)(1u << (flag & 0xF));
}
bool HasRandoInf(const SaveContext* s, RandomizerInf flag) {
    return (s->ship.randomizerInf[flag >> 4] & (1u << (flag & 0xF))) != 0;
}

// File-static: SoH's runtime SaveContext is over 100 KB.
SaveContext sSavBase;
SaveContext sCommitted;

std::shared_ptr<Rando::Context> sWorld;

int sFailures = 0;

#define OCSL_CHECK(cond, msg)                                                       \
    do {                                                                            \
        if (!(cond)) {                                                              \
            printf("[TEST] FAIL (oot-check-state-load): %s  [%s]\n", (msg), #cond); \
            sFailures++;                                                            \
        }                                                                           \
    } while (0)

Rando::ItemLocation* Loc(RandomizerCheck rc) {
    return sWorld->GetItemLocation(rc);
}

const char* StatusName(RandomizerCheckStatus s) {
    switch (s) {
        case RCSHOW_UNCHECKED:
            return "UNCHECKED";
        case RCSHOW_SEEN:
            return "SEEN";
        case RCSHOW_IDENTIFIED:
            return "IDENTIFIED";
        case RCSHOW_SCUMMED:
            return "SCUMMED";
        case RCSHOW_COLLECTED:
            return "COLLECTED";
        case RCSHOW_SAVED:
            return "SAVED";
    }
    return "?";
}

bool AuthorWorld(void) {
    // SetCheckStatus dispatches a GameInteractor hook (TrackerAdapterSingleExe.cpp's pattern).
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    sWorld = Rando::Context::CreateInstance();
    if (sWorld == nullptr) {
        return false;
    }
    sWorld->SetSeed(0x849);
    if (Rando::StaticData::GetLocation(RC_KF_KOKIRI_SWORD_CHEST)->GetName().empty()) {
        Rando::StaticData::InitLocationTable();
    }
    if (Rando::StaticData::RetrieveItem(RG_KOKIRI_SWORD).GetName().GetEnglish().empty()) {
        Rando::StaticData::InitItemTable();
    }
    for (RandomizerCheck rc : { RC_KF_KOKIRI_SWORD_CHEST, RC_LH_FREESTANDING_POH, RC_KF_MIDOS_TOP_LEFT_CHEST,
                                RC_KF_MIDOS_TOP_RIGHT_CHEST, RC_TOT_MASTER_SWORD, RC_KF_BRIDGE_RUPEE }) {
        Loc(rc)->SetPlacedItem(RG_PIECE_OF_HEART);
        Loc(rc)->SetCheckStatus(RCSHOW_UNCHECKED);
    }
    return true;
}

void ReleaseWorld(void) {
    // Context and its Logic hold each other by shared_ptr; cut the back edge
    // first (TrackerAdapterSingleExe.cpp, OoT_TrackerAdapter_TestReleaseWorld).
    if (sWorld != nullptr) {
        sWorld->GetLogic()->SetContext(nullptr);
    }
    sWorld.reset();
}

// A rando OoT half with nothing collected.
void BlankRandoHalf(SaveContext* s) {
    memset(s, 0, sizeof(SaveContext));
    s->ship.quest.id = QUEST_RANDOMIZER;
    s->fileNum = kSlot;
}

// The controls every leg carries, set the same way in both halves.
void ArmControls(SaveContext* s) {
    s->sceneFlags[SCENE_MIDOS_HOUSE].chest |= kMidoRightBit; // RC_KF_MIDOS_TOP_RIGHT_CHEST: opened and saved
    // RC_KF_MIDOS_TOP_LEFT_CHEST: collected after the last save, never committed (closed).
    // RC_TOT_MASTER_SWORD: given at creation, EVENTCHKINF_PULLED_MASTER_SWORD_FROM_PEDESTAL never set.
}

void SetTrackerStatuses(RandomizerCheckStatus sword, RandomizerCheckStatus heartPiece,
                        RandomizerCheckStatus bridgeRupee) {
    Loc(RC_KF_KOKIRI_SWORD_CHEST)->SetCheckStatus(sword);
    Loc(RC_LH_FREESTANDING_POH)->SetCheckStatus(heartPiece);
    Loc(RC_KF_BRIDGE_RUPEE)->SetCheckStatus(bridgeRupee);
    Loc(RC_KF_MIDOS_TOP_LEFT_CHEST)->SetCheckStatus(RCSHOW_SCUMMED);
    Loc(RC_KF_MIDOS_TOP_RIGHT_CHEST)->SetCheckStatus(RCSHOW_SAVED);
    Loc(RC_TOT_MASTER_SWORD)->SetCheckStatus(RCSHOW_SAVED);
}

// Two commits on disk: the one the .sav's last save matches (its OoT half is
// `savBase`), then a newer one whose OoT half is `committed` (the crossing
// commit, or a commit written after the half went back). Returns the .sav's
// mirrored generation, or 0 on failure.
uint32_t WriteCommits(const SaveContext& savBase, const SaveContext& committed) {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveDir);
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    mgr.DeleteSave(kSlot);
    mgr.ResetSlotSessionState();
    mgr.ArmSlotOnCreate(kSlot);
    mgr.SetActiveSlot(kSlot);

    // A paired file, as the open path requires of a randomizer file (#836): a
    // record with a pairing identity, and a Majora's Mask half that is this
    // pair's world (#836 PR 2 refuses a vanilla or another pair's half).
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kMasterSeed;
    gComboCtx.sharedRandoSettingsHash = 0x849u;
    static std::vector<uint8_t> sMMHalf(MM_SAVE_CONTEXT_SIZE, 0);
    if (MM_Rando_AuthorPairHalfForTest(sMMHalf.data(), sMMHalf.size(), kMasterSeed) != 1) {
        return 0;
    }
    Context_UpdateShadowCopy(GAME_MM, sMMHalf.data(), sMMHalf.size());

    Context_UpdateShadowCopy(GAME_OOT, &savBase, sizeof(SaveContext));
    gComboCtx.sourceGame = GAME_OOT;
    if (RsbsSave_Save(kSlot) != 1) {
        return 0;
    }
    const uint32_t savGeneration = gComboCtx.commitGeneration;
    Context_UpdateShadowCopy(GAME_OOT, &committed, sizeof(SaveContext));
    gComboCtx.sourceGame = GAME_MM;
    if (RsbsSave_Save(kSlot) != 1 || gComboCtx.commitGeneration != savGeneration + 1) {
        return 0;
    }
    // The process ends; nothing cross-game survives in memory.
    ComboContext_Init();
    Context_ClearAllFrozenStates();
    mgr.ResetSlotSessionState();
    mgr.SetActiveSlot(-1);
    return savGeneration;
}

// LoadFile as OoT's file select runs it: the .sav's sections (base, tracker)
// are in memory, then the OnLoadFile seam runs.
void LoadAsFileSelect(const SaveContext& savBase, uint32_t savGeneration, RandomizerCheckStatus sword,
                      RandomizerCheckStatus heartPiece, RandomizerCheckStatus bridgeRupee) {
    memcpy(&gSaveContext, &savBase, sizeof(SaveContext));
    SetTrackerStatuses(sword, heartPiece, bridgeRupee);
    OoT_Combo_OnLoadFileSeam(kSlot, savGeneration);
}

void CheckControls(const char* leg) {
    const RandomizerCheckStatus left = Loc(RC_KF_MIDOS_TOP_LEFT_CHEST)->GetCheckStatus();
    const RandomizerCheckStatus right = Loc(RC_KF_MIDOS_TOP_RIGHT_CHEST)->GetCheckStatus();
    const RandomizerCheckStatus sword = Loc(RC_TOT_MASTER_SWORD)->GetCheckStatus();
    printf("[TEST] oot-check-state-load: %s controls: Mido left %s, Mido right %s, Master Sword %s\n", leg,
           StatusName(left), StatusName(right), StatusName(sword));
    OCSL_CHECK(left == RCSHOW_SCUMMED, "a check whose flag is unset in both halves keeps SoH's status (SCUMMED)");
    OCSL_CHECK(right == RCSHOW_SAVED, "a check whose flag is set in both halves keeps SoH's status (SAVED)");
    OCSL_CHECK(sword == RCSHOW_SAVED,
               "the Master Sword a creation marked SAVED without its flag stays found: only a flag that differs "
               "between the halves moves a status");
}

int RunLegs(void) {
    // ======================================================================
    // Leg A (#849): collected, crossed, reloaded.
    // ======================================================================
    // The last OoT save: the heart piece not yet taken. Then the heart piece
    // is taken (a section-only tracker save writes it SCUMMED) and the player
    // crosses into Termina: the crossing commit's OoT half holds it.
    BlankRandoHalf(&sSavBase);
    ArmControls(&sSavBase);
    memcpy(&sCommitted, &sSavBase, sizeof(SaveContext));
    sCommitted.sceneFlags[SCENE_LAKE_HYLIA].collect |= kHeartPieceBit;
    SetRandoInf(&sCommitted, RAND_INF_KF_BRIDGE_RUPEE); // a shuffled freestanding item, taken too
    uint32_t savGeneration = WriteCommits(sSavBase, sCommitted);
    OCSL_CHECK(savGeneration != 0, "leg A setup: the two commits are written");
    if (savGeneration == 0) {
        return 1;
    }
    LoadAsFileSelect(sSavBase, savGeneration, RCSHOW_UNCHECKED, RCSHOW_SCUMMED, RCSHOW_SCUMMED);
    {
        const bool rupeeApplied = HasRandoInf(&gSaveContext, RAND_INF_KF_BRIDGE_RUPEE);
        printf("[TEST] oot-check-state-load: leg A: the loaded half %s the shuffled bridge rupee; its check reads %s\n",
               rupeeApplied ? "holds" : "does NOT hold", StatusName(Loc(RC_KF_BRIDGE_RUPEE)->GetCheckStatus()));
        OCSL_CHECK(rupeeApplied, "leg A setup: the loaded half holds the bridge rupee's RandomizerInf");
        OCSL_CHECK(Loc(RC_KF_BRIDGE_RUPEE)->HasObtained(),
                   "#849: a shuffled freestanding item the loaded half holds reads obtained, so ShuffleFreestanding's "
                   "VB_ITEM00_DESPAWN keeps it out of the world");
    }
    {
        const bool halfApplied = (gSaveContext.sceneFlags[SCENE_LAKE_HYLIA].collect & kHeartPieceBit) != 0;
        const RandomizerCheckStatus s = Loc(RC_LH_FREESTANDING_POH)->GetCheckStatus();
        printf("[TEST] oot-check-state-load: leg A: the loaded half %s the heart piece; its check reads %s "
               "(HasObtained %d)\n",
               halfApplied ? "holds" : "does NOT hold", StatusName(s), (int)Loc(RC_LH_FREESTANDING_POH)->HasObtained());
        OCSL_CHECK(halfApplied, "leg A setup: the .redsave's newer OoT half is the one that loaded");
        OCSL_CHECK(Loc(RC_LH_FREESTANDING_POH)->HasObtained(),
                   "#849: a heart piece the loaded half holds reads obtained, so VB_ITEM00_DESPAWN keeps it out of "
                   "the world (it read SCUMMED from the .sav's tracker section and came back)");
        OCSL_CHECK(Loc(RC_KF_KOKIRI_SWORD_CHEST)->GetCheckStatus() == RCSHOW_UNCHECKED,
                   "leg A: a check neither half has collected stays as the .sav has it");
    }
    CheckControls("leg A");

    // ======================================================================
    // Leg B (reverse): rolled back, reopened.
    // ======================================================================
    // The last OoT save holds the Kokiri Sword chest open and the .sav saved
    // its check SAVED. A newer commit carries an OoT half from before it was
    // opened (the half went back). Reopening the chest must give the item.
    BlankRandoHalf(&sSavBase);
    ArmControls(&sSavBase);
    sSavBase.sceneFlags[SCENE_KOKIRI_FOREST].chest |= kSwordChestBit;
    BlankRandoHalf(&sCommitted);
    ArmControls(&sCommitted);
    savGeneration = WriteCommits(sSavBase, sCommitted);
    OCSL_CHECK(savGeneration != 0, "leg B setup: the two commits are written");
    if (savGeneration == 0) {
        return 1;
    }
    (void)OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    LoadAsFileSelect(sSavBase, savGeneration, RCSHOW_SAVED, RCSHOW_UNCHECKED, RCSHOW_UNCHECKED);
    {
        const bool closed = (gSaveContext.sceneFlags[SCENE_KOKIRI_FOREST].chest & kSwordChestBit) == 0;
        const RandomizerCheckStatus s = Loc(RC_KF_KOKIRI_SWORD_CHEST)->GetCheckStatus();
        // The player opens the chest again: the real hook its treasure flag fires.
        RandomizerOnSceneFlagSetHandler(SCENE_KOKIRI_FOREST, FLAG_SCENE_TREASURE, 0x00);
        uint16_t queued[4] = {};
        const int n = OoT_Rando_TestTakeQueuedChecks(queued, 4);
        printf("[TEST] oot-check-state-load: leg B: the loaded half has the chest %s; its check reads %s; "
               "reopening it queued %d check(s)%s\n",
               closed ? "closed" : "OPEN", StatusName(s), n,
               (n == 1 && queued[0] == (uint16_t)RC_KF_KOKIRI_SWORD_CHEST) ? " (the Kokiri Sword chest)" : "");
        OCSL_CHECK(closed, "leg B setup: the .redsave's newer OoT half, the one without the chest, is the one that "
                           "loaded");
        OCSL_CHECK(n == 1 && queued[0] == (uint16_t)RC_KF_KOKIRI_SWORD_CHEST,
                   "reverse: a chest the loaded half has closed gives its item when opened again (the .sav's SAVED "
                   "status made the scene-flag hook queue nothing)");
        OCSL_CHECK(Loc(RC_LH_FREESTANDING_POH)->GetCheckStatus() == RCSHOW_UNCHECKED,
                   "leg B: a check neither half has collected stays as the .sav has it");
    }
    CheckControls("leg B");
    return 0;
}

} // namespace

extern "C" int OoT_CheckStateLoad_RunHeadless(void) {
    printf("[TEST] oot-check-state-load: OoT's check state follows the save half that was loaded (#849, #803)\n");
    sFailures = 0;
    if (!AuthorWorld()) {
        printf("[TEST] FAIL (oot-check-state-load): could not create a Rando::Context\n");
        return 1;
    }
    const int setup = RunLegs();

    // Leave process state clean for whatever runs next, on pass or fail.
    (void)OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.DeleteSave(kSlot);
    mgr.ResetSlotSessionState();
    mgr.SetActiveSlot(-1);
    mgr.SetSaveDirectory("Save");
    ComboContext_Init();
    Context_ClearAllFrozenStates();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    ReleaseWorld();

    if (setup != 0 || sFailures != 0) {
        printf("[TEST] FAIL (oot-check-state-load): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: OoT's check state follows the save half that was loaded\n");
    return 0;
}
