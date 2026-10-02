/**
 * @file test_crossing_commit.c
 * @brief Headless lock for #837 (decision 16, ruled 2026-10-01 option (b)):
 *        every cross-game crossing is a whole-file save commit, as OoTMM's
 *        comboGameSwitch saves at every game switch.
 *
 * WHAT WAS MISSING. A crossing froze the departing half into its shadow and
 * signalled the launcher; nothing reached disk. The .redsave carried the last
 * SAVE, so a process kill after a crossing reloaded a world in which the
 * crossing never happened: the departing half rolled back to its last save, a
 * shared item staged during the visit was gone, the pool moved back. And a moon
 * crash restored the last commit, which rolled OoT's half back with it (#803).
 *
 * THE FIX UNDER TEST. Switch_CommitCrossing (src/common/switch.cpp), called by
 * GameRunner_SwitchTo between the departing game's suspend and the target's
 * resume/init (src/common/game_lifecycle.c). At that point the suspend has
 * moved the staged pickups into Tier-1 and harvested the pool, and both shadows
 * are whole: the departing one was just frozen, the target's still holds its own
 * last departure (or the half a load or the creation armed). The commit is
 * RsbsSave_Save on the calling thread, with sourceGame = the target.
 *
 * THE LEGS. Each drives the REAL GameRunner_SwitchTo with mock GameOps (the
 * test_game_lifecycle.c pattern) and the real SaveManager against a scratch
 * save directory.
 *
 *   1. OoT -> MM is durable across a kill. A last OoT save at generation G;
 *      OoT plays on and departs (Switch_PrepareHotSwap freezes the later half);
 *      the mock OoT suspend stages an MM-bound item and moves the pool, as OoT's
 *      Game_Suspend does; GameRunner_SwitchTo(MM). Then everything in memory is
 *      dropped and the slot is loaded as OoT's file select would with the .sav
 *      still at G: the load must find generation G+1, take the .redsave's OoT
 *      half as the authority, and deliver the departure freeze, the item
 *      (unredeemed) and the pool; sourceGame names MM.
 *   2. MM -> OoT, the reverse: Tier-3 is MM's departure freeze and sourceGame
 *      names OoT.
 *   3. Writes nothing (file bytes and commitGeneration unchanged) when the slot
 *      is latched (never armed this session, and refused for identity), when
 *      there is no active slot, when the departing game has no freeze for this
 *      trip, and when the departing half is not a live file (the flush's note,
 *      Context_NoteDepartureLiveFile). These legs are green without the fix and
 *      go red if a guard is removed. A control shows the not-live note is spent
 *      by its own freeze: the next departure commits.
 *   4. (test_game_lifecycle.c) the lifecycle rows run with no active slot and
 *      no frozen state, so in an AllTests process their mock switches never
 *      write an earlier row's armed slot.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++)
 * like test_whole_file_commit.c, so it can use rsbs::SaveManager directly.
 */

#include "../context.h"
#include "../crossing_store.h"
#include "../game.h"
#include "../game_lifecycle.h"
#include "../save.h"
#include "../shared_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);
extern "C" int Switch_PrepareHotSwap(GameId departing, const void* saveContext, size_t size);

#define XC_ASSERT(cond, msg)                                                   \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("[TEST] FAIL (crossing-commit): %s  [%s]\n", (msg), #cond); \
            return TEST_FAIL;                                                  \
        }                                                                      \
    } while (0)

namespace {

const char* const kCrossingTestDir = "rsbs_test_crossing_commit";

using XcBlob = std::vector<uint8_t>;

// An MM-bound item the mock OoT suspend stages (an RI_* id; the value only has
// to be nonzero and recognisable), and an OoT-bound one for the reverse leg.
constexpr uint16_t kMMBoundItem = 0x0123;
constexpr uint16_t kOoTBoundItem = 0x0045;
// The wallet tier the mock suspends harvest: a MONOTONIC kind, so the pool
// value is exactly the harvested one whatever the watermarks say.
constexpr uint16_t kOoTWalletTier = 2;
constexpr uint16_t kMMWalletTier = 3;

bool XcUniform(const void* blob, size_t size, uint8_t value) {
    if (blob == nullptr) {
        return false;
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(blob);
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != value) {
            return false;
        }
    }
    return true;
}

XcBlob XcReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return XcBlob();
    }
    return XcBlob((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

uint16_t XcPoolWalletTier() {
    uint16_t v = 0;
    Combo_GetSharedResource(RSBS_SHARED_RES_WALLET_TIER, &v);
    return v;
}

// ---- Mock games: what each real Game_Suspend does to cross-game state -------
// OoT_Game_Suspend / MM_Game_Suspend (games/*/GameExports_SingleExe.cpp) move
// the staged pickups into Tier-1 (Combo_CommitStagedSharedItems) and then
// harvest the shared pool. The mocks do exactly those two things.
int sXcSuspendOoT = 0;
int sXcSuspendMM = 0;

int XcInit(int, char**) {
    return 0;
}
void XcRun(void) {
}
void XcResume(void) {
}
void XcShutdown(void) {
}
void XcSuspendOoT(void) {
    sXcSuspendOoT++;
    Combo_StageSharedItem(GAME_MM, kMMBoundItem);
    Combo_CommitStagedSharedItems();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_WALLET_TIER, kOoTWalletTier);
}
void XcSuspendMM(void) {
    sXcSuspendMM++;
    Combo_StageSharedItem(GAME_OOT, kOoTBoundItem);
    Combo_CommitStagedSharedItems();
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_WALLET_TIER, kMMWalletTier);
}
void XcSuspendQuiet(void) {
}

GameOps sXcOoT = { "oot", "Mock OoT", XcInit, XcRun, XcSuspendOoT, XcResume, XcShutdown };
GameOps sXcMM = { "mm", "Mock MM", XcInit, XcRun, XcSuspendMM, XcResume, XcShutdown };
GameOps sXcOoTQuiet = { "oot", "Mock OoT", XcInit, XcRun, XcSuspendQuiet, XcResume, XcShutdown };
GameOps sXcMMQuiet = { "mm", "Mock MM", XcInit, XcRun, XcSuspendQuiet, XcResume, XcShutdown };

void XcRunner(GameRunner* r, GameId start, bool quiet) {
    GameRunner_Init(r);
    GameRunner_RegisterGame(r, GAME_OOT, quiet ? &sXcOoTQuiet : &sXcOoT);
    GameRunner_RegisterGame(r, GAME_MM, quiet ? &sXcMMQuiet : &sXcMM);
    GameRunner_StartGame(r, start, 0, nullptr);
}

// The process dies: every RAM-only cross-game store goes. The latch is process
// state too (ResetSlotSessionState). Context_InitFrozenStates alone would not
// clear the shadows (it is once-only), so they are cleared explicitly.
void XcKill() {
    ComboContext_Init();
    Combo_Crossings_Clear();
    Context_ClearFrozenState(GAME_OOT);
    Context_ClearFrozenState(GAME_MM);
    Combo_ClearSharedItemOutbox();
    Combo_ResetSharedResourceWatermarks();
    rsbs::SaveManager::Instance().ResetSlotSessionState();
}

// A fresh session with slot 0 created, armed and active, and nothing resident.
void XcFreshSlot() {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kCrossingTestDir);
    Context_InitFrozenStates();
    XcKill();
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.ArmSlotOnCreate(0);
    mgr.SetActiveSlot(0);
}

void XcCleanup() {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetActiveSlot(-1);
    mgr.SetSaveDirectory("Save");
    XcKill();
}

// One skip case: the file and the generation must not move across a crossing.
// `arrange` sets up the case after the departing half has been frozen.
TestResult XcExpectNoWrite(const char* what, GameId departing, void (*arrange)(void)) {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    const XcBlob before = XcReadFile(mgr.SlotPath(0));
    if (before.empty()) {
        printf("[TEST] FAIL (crossing-commit): %s: setup: the slot has no file to compare\n", what);
        return TEST_FAIL;
    }
    GameRunner r;
    XcRunner(&r, departing, /*quiet=*/true);
    XcBlob half(departing == GAME_OOT ? OOT_SAVE_CONTEXT_SIZE : MM_SAVE_CONTEXT_SIZE, 0x5C);
    if (!Switch_PrepareHotSwap(departing, half.data(), half.size())) {
        printf("[TEST] FAIL (crossing-commit): %s: setup: the departure freeze did not take\n", what);
        return TEST_FAIL;
    }
    arrange();
    const uint32_t genBefore = gComboCtx.commitGeneration;
    if (GameRunner_SwitchTo(&r, departing == GAME_OOT ? GAME_MM : GAME_OOT, 0, nullptr) != 0) {
        printf("[TEST] FAIL (crossing-commit): %s: the switch itself failed\n", what);
        return TEST_FAIL;
    }
    const XcBlob after = XcReadFile(mgr.SlotPath(0));
    printf("[TEST] crossing-commit: %s: generation %u -> %u, file %s\n", what, (unsigned)genBefore,
           (unsigned)gComboCtx.commitGeneration, after == before ? "unchanged" : "CHANGED");
    if (gComboCtx.commitGeneration != genBefore || after != before) {
        printf("[TEST] FAIL (crossing-commit): %s: a crossing that must write nothing moved the file or the "
               "generation\n",
               what);
        return TEST_FAIL;
    }
    return TEST_PASS;
}

void XcArrangeUnarmed() {
    rsbs::SaveManager::Instance().ResetSlotSessionState();
    rsbs::SaveManager::Instance().SetActiveSlot(0);
}
void XcArrangeIdentityRefused() {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.ResetSlotSessionState();
    mgr.ArmSlotOnCreate(0);
    mgr.SetActiveSlot(0);
    mgr.RefuseSlotIdentity(0);
}
void XcArrangeNoActiveSlot() {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.ResetSlotSessionState();
    mgr.ArmSlotOnCreate(0);
    mgr.SetActiveSlot(-1);
}
void XcArrangeNoFreezeOoT() {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.ResetSlotSessionState();
    mgr.ArmSlotOnCreate(0);
    mgr.SetActiveSlot(0);
    Context_ClearFrozenState(GAME_OOT);
}
// MM's title-screen bootstrap departing: the flush notes "not a live file"
// (what MM_Combo_DepartureIsLiveFile answers under GAMEMODE_TITLE_SCREEN) and
// the freeze that follows takes the note.
void XcArrangeNotLiveMM() {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.ResetSlotSessionState();
    mgr.ArmSlotOnCreate(0);
    mgr.SetActiveSlot(0);
    XcBlob bootstrap(MM_SAVE_CONTEXT_SIZE, 0x5D);
    Context_NoteDepartureLiveFile(GAME_MM, 0);
    Switch_PrepareHotSwap(GAME_MM, bootstrap.data(), bootstrap.size());
}

}  // namespace

TestResult Test_CrossingCommit(void) {
    printf("[TEST] crossing-commit: every cross-game crossing is a whole-file commit (#837)\n");
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();

    // ======================================================================
    // Leg 1: OoT -> MM is durable across a kill.
    // ======================================================================
    XcFreshSlot();
    {
        XcBlob o1(OOT_SAVE_CONTEXT_SIZE, 0x11);
        XcBlob m1(MM_SAVE_CONTEXT_SIZE, 0x22);
        Context_UpdateShadowCopy(GAME_OOT, o1.data(), o1.size());
        Context_UpdateShadowCopy(GAME_MM, m1.data(), m1.size());
    }
    gComboCtx.sourceGame = GAME_OOT;
    XC_ASSERT(mgr.Save(0), "setup: the last OoT save commits");
    const uint32_t g = gComboCtx.commitGeneration;
    printf("[TEST] crossing-commit: leg 1: the last OoT save is generation %u\n", (unsigned)g);

    GameRunner r;
    XcRunner(&r, GAME_OOT, /*quiet=*/false);
    sXcSuspendOoT = 0;
    {
        // OoT plays on past its save and walks into the door: the departure
        // freezes the later half.
        XcBlob o2(OOT_SAVE_CONTEXT_SIZE, 0x33);
        XC_ASSERT(Switch_PrepareHotSwap(GAME_OOT, o2.data(), o2.size()) == 1, "setup: OoT's departure freezes");
    }
    XC_ASSERT(GameRunner_SwitchTo(&r, GAME_MM, 0, nullptr) == 0, "the switch to MM succeeds");
    XC_ASSERT(sXcSuspendOoT == 1, "setup: OoT's suspend ran (staged pickup, harvest)");
    printf("[TEST] crossing-commit: leg 1: after the crossing the session is at generation %u\n",
           (unsigned)gComboCtx.commitGeneration);

    // The process is killed in Termina without saving, then relaunched; OoT's
    // file select loads the slot with the .sav still mirroring G.
    XcKill();
    mgr.SetActiveSlot(0);
    XC_ASSERT(RsbsSave_LoadSlotChecked(0, g) == RSBS_LOAD_OK, "the slot loads after the kill");
    printf("[TEST] crossing-commit: leg 1: reloaded generation %u (last save %u), skew %d, sourceGame %d\n",
           (unsigned)gComboCtx.commitGeneration, (unsigned)g, RsbsSave_GetSlotCommitSkew(0),
           (int)gComboCtx.sourceGame);
    XC_ASSERT(gComboCtx.commitGeneration == g + 1,
              "the crossing is durable: the file holds a commit taken AT the crossing (generation G+1), not the "
              "last save before it");
    XC_ASSERT(RsbsSave_GetSlotCommitSkew(0) == 1, "the .redsave is one commit ahead of OoT's .sav (skew +1)");
    XC_ASSERT(RsbsSave_TakeOoTHalfAuthority() == 1, "the .redsave's OoT half is the authority for this load");
    {
        XcBlob live(OOT_SAVE_CONTEXT_SIZE, 0x00);
        XC_ASSERT(Combo_ConsumeFrozenState("oot", live.data(), live.size()) == 1, "the OoT half is delivered");
        XC_ASSERT(XcUniform(live.data(), live.size(), 0x33),
                  "the delivered OoT half is the departure freeze, byte for byte (nothing stamped into it)");
    }
    XC_ASSERT(XcUniform(Context_GetMMSaveContext(), MM_SAVE_CONTEXT_SIZE, 0x22),
              "the MM half is the target's own last half, untouched by the crossing");
    XC_ASSERT(Combo_CountSharedItems(GAME_MM, false) == 1,
              "the MM-bound item the suspend staged is in the file, unredeemed");
    XC_ASSERT(XcPoolWalletTier() == kOoTWalletTier, "the pool is the suspend's harvest");
    XC_ASSERT(gComboCtx.sourceGame == GAME_MM, "sourceGame names the game the player crossed into");

    // ======================================================================
    // Leg 2: MM -> OoT, the reverse direction.
    // ======================================================================
    {
        // Session state as it stands while MM runs: OoT's shadow holds OoT's
        // last departure, MM is live.
        XcBlob o2(OOT_SAVE_CONTEXT_SIZE, 0x33);
        Context_FreezeState(GAME_OOT, OOT_ENTR_MARKET_FROM_MASK_SHOP, o2.data(), o2.size());
    }
    const uint32_t g2 = gComboCtx.commitGeneration;
    GameRunner r2;
    XcRunner(&r2, GAME_MM, /*quiet=*/false);
    sXcSuspendMM = 0;
    {
        XcBlob m2(MM_SAVE_CONTEXT_SIZE, 0x44);
        XC_ASSERT(Switch_PrepareHotSwap(GAME_MM, m2.data(), m2.size()) == 1, "setup: MM's departure freezes");
    }
    XC_ASSERT(GameRunner_SwitchTo(&r2, GAME_OOT, 0, nullptr) == 0, "the switch to OoT succeeds");
    XC_ASSERT(sXcSuspendMM == 1, "setup: MM's suspend ran");
    XcKill();
    mgr.SetActiveSlot(0);
    XC_ASSERT(RsbsSave_LoadSlotChecked(0, g) == RSBS_LOAD_OK, "the slot loads after the second kill");
    printf("[TEST] crossing-commit: leg 2: reloaded generation %u (before the crossing %u), sourceGame %d\n",
           (unsigned)gComboCtx.commitGeneration, (unsigned)g2, (int)gComboCtx.sourceGame);
    XC_ASSERT(gComboCtx.commitGeneration == g2 + 1, "the MM -> OoT crossing is durable too (one more generation)");
    XC_ASSERT(XcUniform(Context_GetMMSaveContext(), MM_SAVE_CONTEXT_SIZE, 0x44), "Tier-3 is MM's departure freeze");
    XC_ASSERT(gComboCtx.sourceGame == GAME_OOT, "sourceGame names OoT, the game the player crossed into");
    XC_ASSERT(Combo_CountSharedItems(GAME_OOT, false) == 1, "the OoT-bound item MM's suspend staged is in the file");
    XC_ASSERT(XcPoolWalletTier() == kMMWalletTier, "the pool is MM's suspend harvest");
    {
        XcBlob live(OOT_SAVE_CONTEXT_SIZE, 0x00);
        XC_ASSERT(RsbsSave_TakeOoTHalfAuthority() == 1, "the reverse crossing's OoT half is the authority too");
        XC_ASSERT(Combo_ConsumeFrozenState("oot", live.data(), live.size()) == 1 &&
                      XcUniform(live.data(), live.size(), 0x33),
                  "Tier-2 is OoT's last departure freeze");
    }

    // ======================================================================
    // Leg 3: the crossings that must write nothing.
    // ======================================================================
    XcFreshSlot();
    {
        XcBlob o(OOT_SAVE_CONTEXT_SIZE, 0x66);
        XcBlob m(MM_SAVE_CONTEXT_SIZE, 0x77);
        Context_UpdateShadowCopy(GAME_OOT, o.data(), o.size());
        Context_UpdateShadowCopy(GAME_MM, m.data(), m.size());
    }
    gComboCtx.sourceGame = GAME_OOT;
    XC_ASSERT(mgr.Save(0), "setup: the skip legs' baseline commit");
    if (XcExpectNoWrite("a slot not armed this session", GAME_OOT, XcArrangeUnarmed) != TEST_PASS) {
        return TEST_FAIL;
    }
    if (XcExpectNoWrite("a slot refused for identity", GAME_MM, XcArrangeIdentityRefused) != TEST_PASS) {
        return TEST_FAIL;
    }
    if (XcExpectNoWrite("no active slot", GAME_OOT, XcArrangeNoActiveSlot) != TEST_PASS) {
        return TEST_FAIL;
    }
    if (XcExpectNoWrite("no freeze for this trip", GAME_OOT, XcArrangeNoFreezeOoT) != TEST_PASS) {
        return TEST_FAIL;
    }
    if (XcExpectNoWrite("a departing half that is not a live file", GAME_MM, XcArrangeNotLiveMM) != TEST_PASS) {
        return TEST_FAIL;
    }

    // Control: the not-live note belongs to ONE freeze. A later freeze of the
    // same game without a note (the next departure from gameplay) is live, so
    // the crossing commits; a sticky note would silently stop every later save.
    {
        mgr.ResetSlotSessionState();
        mgr.ArmSlotOnCreate(0);
        mgr.SetActiveSlot(0);
        XcBlob bootstrap(MM_SAVE_CONTEXT_SIZE, 0x5D);
        Context_NoteDepartureLiveFile(GAME_MM, 0);
        XC_ASSERT(Switch_PrepareHotSwap(GAME_MM, bootstrap.data(), bootstrap.size()) == 1, "control: freeze");
        XC_ASSERT(Context_FrozenStateIsLiveFile(GAME_MM) == 0, "control: the noted freeze reads not live");
        XcBlob played(MM_SAVE_CONTEXT_SIZE, 0x5E);
        XC_ASSERT(Switch_PrepareHotSwap(GAME_MM, played.data(), played.size()) == 1, "control: the next freeze");
        XC_ASSERT(Context_FrozenStateIsLiveFile(GAME_MM) == 1, "control: a freeze without a note is live");
        GameRunner rc;
        GameRunner_Init(&rc);
        GameRunner_RegisterGame(&rc, GAME_OOT, &sXcOoTQuiet);
        GameRunner_RegisterGame(&rc, GAME_MM, &sXcMMQuiet);
        GameRunner_StartGame(&rc, GAME_MM, 0, nullptr);
        const uint32_t genBefore = gComboCtx.commitGeneration;
        XC_ASSERT(GameRunner_SwitchTo(&rc, GAME_OOT, 0, nullptr) == 0, "control: the switch");
        XC_ASSERT(gComboCtx.commitGeneration == genBefore + 1,
                  "control: the not-live note is spent by its freeze; the next live departure commits");
        XC_ASSERT(Switch_CommitCrossing(GAME_MM, GAME_MM) == 0 && Switch_CommitCrossing(GAME_NONE, GAME_OOT) == 0,
                  "a crossing needs two different real games");
    }

    XcCleanup();
    printf("[TEST] PASS: every crossing commits the whole file, and the cases that must write nothing do not\n");
    return TEST_PASS;
}
