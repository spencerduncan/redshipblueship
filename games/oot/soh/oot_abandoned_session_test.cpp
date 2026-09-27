/**
 * @file oot_abandoned_session_test.cpp
 * ROM-free, display-free lock for #750, the OoT leg of #666. CTest label
 * "redship", row OoTAbandonedSessionStatics in CMake/SingleExecutable.cmake,
 * dispatch "oot-abandoned-session-statics" in src/common/test_runner.cpp. The
 * MM twin is mm-abandoned-session-statics (games/mm/2s2h/mm_resume_state_test.cpp).
 *
 * WHAT WAS BROKEN. Every departure from OoT, through the Happy Mask Shop door
 * or with F10, retires OoT's Play gamestate without Play_Destroy
 * (OoT_Graph_ResetRunFrameContext in OoT_Game_Suspend), and the next entry
 * re-runs Main(), which re-initializes the arena the actors lived in. So
 * OoT_Actor_Delete never runs for the actors live at that instant, and its
 * epilogue never runs for them either:
 *   - numLoaded-- and OoT_Actor_FreeOverlay, which runs the entry's `reset`
 *     once the overlay has no clients. OoT, unlike MM, never zeroes numLoaded
 *     again after the ActorDB entry is created, so every overlay with a live
 *     actor at departure kept a phantom client for the rest of the process,
 *     its reset never ran again (not even on ordinary in-OoT scene changes),
 *     and each further departure added another phantom;
 *   - ObjectExtension_Free(actor), so every per-actor extension (rando check
 *     identities, the actor-list index, enemy maximum health) survived into a
 *     session whose arena hands the same addresses to other actors.
 * And because Destroy never runs, a static that only Destroy put back (the
 * one-instance latches of the Spirit Temple lifts and mirror, Dampe's race
 * ghost, the Zora diving game, the Lon Lon milk-crate index, ...) kept the
 * abandoned session's value.
 *
 * THE FIX UNDER TEST. OoT_Game_Suspend calls OoT_RetireAbandonedSession
 * (games/oot/soh/GameExports_SingleExe.cpp) right after the graph is retired:
 * every ActorDB entry with clients is marked client-free and freed through
 * OoT_Actor_FreeOverlay (so its reset runs once), entries at zero are left
 * alone, and every ObjectExtension entry is dropped. Each overlay whose
 * Destroy was the only thing restoring a static has a reset that restores it.
 *
 * THE INVARIANTS, observed through the REAL departure (OoT's registered
 * GameOps suspend, the call GameRunner makes on both switch paths):
 *   (a) every entry that had clients has none, and its reset ran exactly once,
 *       only after the graph (and with it OoT_gPlayState) was retired;
 *   (b) an entry with NO clients is not reset again;
 *   (c) each overlay whose Destroy was the only thing restoring a static has
 *       that static back at its initial value (seeded to what a live client
 *       leaves, read back through the TU's accessors);
 *   (d) no per-actor ObjectExtension entry survives;
 *   (e) a SECOND abandoned session, whose actors spawn on top of whatever the
 *       first left (numLoaded++ as OoT_Actor_Spawn does), is retired the same
 *       way: no count stays above zero, and each reset ran once per departure.
 *
 * Headless: the suspend's shared-resource harvest is gated off by a
 * non-gameplay gameMode, the audio calls only latch resetTimer while OoT audio
 * is uninitialized, and gComboCtx is snapshotted around the staged-item commit.
 * A scope guard puts back everything the row seeds on every exit path (AllTests
 * runs every row in one process).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <z64.h>
// ARRAY_COUNT. Must come after z64.h (it declares `extern GraphicsContext*`).
#include "macros.h"

#include "soh/ActorDB.h"
#include "soh/ObjectExtension/ActorListIndex.h"
#include "soh/ObjectExtension/ObjectExtension.h"

#include "context.h"        // gComboCtx, snapshotted around the real suspend
#include "game_lifecycle.h" // GameOps

#include <cstdio>
#include <cstring>

extern "C" {
extern SaveContext gSaveContext;
extern AudioContext gAudioContext;
extern s32 gAudioContextInitalized;
// z_play.c / graph.c: set by Play_Init, nulled by Play_Destroy and by
// OoT_Graph_ResetRunFrameContext, which the suspend under test calls.
extern PlayState* OoT_gPlayState;
extern GameState* OoT_gGameState;
// GameExports_SingleExe.cpp: OoT's registered lifecycle. The row drives its
// suspend, the call GameRunner makes on every departure from OoT.
GameOps* OoT_GetGameOps(void);
// The overlays whose Destroy was the only thing restoring a static (#750):
// seed (dirty != 0) or clear their statics, and read whether they are stale.
#define OOT_ABANDONED_ACCESSORS(stem)                      \
    void OoT_##stem##_SetDestroyStaticsForTest(s32 dirty); \
    s32 OoT_##stem##_DestroyStaticsDirtyForTest(void);
OOT_ABANDONED_ACCESSORS(BgHakaGate)
OOT_ABANDONED_ACCESSORS(BgJya1flift)
OOT_ABANDONED_ACCESSORS(BgJyaBigmirror)
OOT_ABANDONED_ACCESSORS(BgJyaLift)
OOT_ABANDONED_ACCESSORS(BgJyaZurerukabe)
OOT_ABANDONED_ACCESSORS(BgMizuMovebg)
OOT_ABANDONED_ACCESSORS(BgMoriElevator)
OOT_ABANDONED_ACCESSORS(BgMoriIdomizu)
OOT_ABANDONED_ACCESSORS(BgSpot06Objects)
OOT_ABANDONED_ACCESSORS(BgSpot15Rrbox)
OOT_ABANDONED_ACCESSORS(EnDivingGame)
OOT_ABANDONED_ACCESSORS(EnEg)
OOT_ABANDONED_ACCESSORS(EnNb)
OOT_ABANDONED_ACCESSORS(EnPoh)
OOT_ABANDONED_ACCESSORS(EnPoRelay)
OOT_ABANDONED_ACCESSORS(EnRl)
OOT_ABANDONED_ACCESSORS(EnRu1)
OOT_ABANDONED_ACCESSORS(EnRu2)
OOT_ABANDONED_ACCESSORS(ObjBean)
#undef OOT_ABANDONED_ACCESSORS
}

namespace {

#define OAS_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

int sProbeResetCalls = 0;
int sIdleProbeResetCalls = 0;
// Whether OoT_gPlayState was already NULL every time the probe's reset ran:
// the ordering half of the wiring lock (the retire must run after the graph,
// and with it the Play gamestate, is retired).
bool sProbeSawRetiredGraph = true;

void ProbeReset(void) {
    sProbeResetCalls++;
    sProbeSawRetiredGraph = sProbeSawRetiredGraph && (OoT_gPlayState == NULL);
}

void IdleProbeReset(void) {
    sIdleProbeResetCalls++;
}

// The overlays whose Destroy was the only thing restoring a file-scope static,
// each with the seed/read accessors its TU exports for this row.
struct DestroyMaintainedOverlay {
    s16 actorId;
    const char* name;
    void (*setDirty)(s32 dirty);
    s32 (*isDirty)(void);
};

#define OAS_ROW(id, label, stem) \
    { id, label, OoT_##stem##_SetDestroyStaticsForTest, OoT_##stem##_DestroyStaticsDirtyForTest }
const DestroyMaintainedOverlay kDestroyMaintained[] = {
    OAS_ROW(ACTOR_BG_HAKA_GATE, "Bg_Haka_Gate sSkullOfTruthRotY/sBgPoEventPuzzleState", BgHakaGate),
    OAS_ROW(ACTOR_BG_JYA_1FLIFT, "Bg_Jya_1flift sKankyoIsSpawned", BgJya1flift),
    OAS_ROW(ACTOR_BG_JYA_BIGMIRROR, "Bg_Jya_Bigmirror sKankyoIsSpawned", BgJyaBigmirror),
    OAS_ROW(ACTOR_BG_JYA_LIFT, "Bg_Jya_Lift sKankyoIsSpawned", BgJyaLift),
    OAS_ROW(ACTOR_BG_JYA_ZURERUKABE, "Bg_Jya_Zurerukabe D_8089B9C0", BgJyaZurerukabe),
    OAS_ROW(ACTOR_BG_MIZU_MOVEBG, "Bg_Mizu_Movebg D_8089EE40", BgMizuMovebg),
    OAS_ROW(ACTOR_BG_MORI_ELEVATOR, "Bg_Mori_Elevator sKankyoIsSpawned", BgMoriElevator),
    OAS_ROW(ACTOR_BG_MORI_IDOMIZU, "Bg_Mori_Idomizu sKankyoIsSpawned", BgMoriIdomizu),
    OAS_ROW(ACTOR_BG_SPOT06_OBJECTS, "Bg_Spot06_Objects water-control state", BgSpot06Objects),
    OAS_ROW(ACTOR_BG_SPOT15_RRBOX, "Bg_Spot15_Rrbox D_808B4590", BgSpot15Rrbox),
    OAS_ROW(ACTOR_EN_DIVING_GAME, "En_Diving_Game sHasSpawned", EnDivingGame),
    OAS_ROW(ACTOR_EN_EG, "En_Eg voided", EnEg),
    OAS_ROW(ACTOR_EN_NB, "En_Nb D_80AB4318", EnNb),
    OAS_ROW(ACTOR_EN_POH, "En_Poh D_80AE1A50", EnPoh),
    OAS_ROW(ACTOR_EN_PO_RELAY, "En_Po_Relay D_80AD8D24", EnPoRelay),
    OAS_ROW(ACTOR_EN_RL, "En_Rl D_80AE81AC", EnRl),
    OAS_ROW(ACTOR_EN_RU1, "En_Ru1 D_80AF1938", EnRu1),
    OAS_ROW(ACTOR_EN_RU2, "En_Ru2 D_80AF4118", EnRu2),
    OAS_ROW(ACTOR_OBJ_BEAN, "Obj_Bean D_80B90E30", ObjBean),
};
#undef OAS_ROW
constexpr size_t kDestroyMaintainedCount = ARRAY_COUNT(kDestroyMaintained);

ActorDBEntry* Entry(s32 id) {
    return &ActorDB::Instance->RetrieveEntry(id).entry;
}

// Everything the row seeds, put back on EVERY exit path, including a failed
// assertion's early return.
struct AbandonedSessionRowGuard {
    bool ownsActorDB = false;
    bool seeded = false;
    s32 probeSlot = -1;
    s32 idleSlot = -1;
    Actor* fakeActor = nullptr;
    PlayState* playState = nullptr;
    GameState* gameState = nullptr;
    s32 gameMode = 0;
    u32 resetTimer = 0;
    s32 audioInitialized = 0;
    const ComboContext* comboSnapshot = nullptr;

    ~AbandonedSessionRowGuard() {
        if (seeded) {
            Entry(probeSlot)->reset = NULL;
            Entry(probeSlot)->numLoaded = 0;
            Entry(idleSlot)->reset = NULL;
            Entry(idleSlot)->numLoaded = 0;
            for (size_t k = 0; k < kDestroyMaintainedCount; k++) {
                Entry(kDestroyMaintained[k].actorId)->numLoaded = 0;
                kDestroyMaintained[k].setDirty(false);
            }
            ObjectExtension::GetInstance().Free(fakeActor);
            OoT_gPlayState = playState;
            OoT_gGameState = gameState;
            gSaveContext.gameMode = gameMode;
            gAudioContext.resetTimer = resetTimer;
            gAudioContextInitalized = audioInitialized;
            memcpy(&gComboCtx, comboSnapshot, sizeof(ComboContext));
        }
        if (ownsActorDB) {
            delete ActorDB::Instance;
            ActorDB::Instance = nullptr;
        }
    }
};

// What one abandoned session looks like when OoT_Game_Suspend finds it: a live
// Play gamestate; each Destroy-maintained overlay and the probe have live
// clients spawned on top of whatever the counts already were (OoT_Actor_Spawn's
// numLoaded++), with each static where a live client leaves it; an actor carries
// an ObjectExtension entry.
void StageAbandonedSession(AbandonedSessionRowGuard& guard, u8* fakePlayState, s32 probeClients) {
    OoT_gPlayState = (PlayState*)fakePlayState;
    for (size_t k = 0; k < kDestroyMaintainedCount; k++) {
        Entry(kDestroyMaintained[k].actorId)->numLoaded++;
        kDestroyMaintained[k].setDirty(true);
    }
    Entry(guard.probeSlot)->numLoaded += probeClients;
    SetActorListIndex(guard.fakeActor, 7);
}

struct SessionAfter {
    s32 probeClients;
    s32 idleClients;
    s32 destroyMaintainedClients;
    s32 staleStatics;
    const char* firstStale;
    s16 listIndex;
    size_t extensions;
};

SessionAfter ReadAfter(const AbandonedSessionRowGuard& guard) {
    SessionAfter after = {};
    after.probeClients = Entry(guard.probeSlot)->numLoaded;
    after.idleClients = Entry(guard.idleSlot)->numLoaded;
    for (size_t k = 0; k < kDestroyMaintainedCount; k++) {
        after.destroyMaintainedClients += Entry(kDestroyMaintained[k].actorId)->numLoaded;
        if (kDestroyMaintained[k].isDirty()) {
            after.staleStatics++;
            if (after.firstStale == NULL) {
                after.firstStale = kDestroyMaintained[k].name;
            }
        }
    }
    after.listIndex = GetActorListIndex(guard.fakeActor);
    after.extensions = ObjectExtension::GetInstance().Count();
    return after;
}

void PrintAfter(const char* label, const SessionAfter& a) {
    printf("[TEST] oot-abandoned-session-statics: after %s: probe clients %d, probe resets %d (graph retired first: "
           "%d), idle clients %d, idle resets %d, Destroy-maintained clients %d, stale Destroy-maintained statics "
           "%d/%zu%s%s, list index %d, extension entries %zu\n",
           label, (int)a.probeClients, sProbeResetCalls, (int)sProbeSawRetiredGraph, (int)a.idleClients,
           sIdleProbeResetCalls, (int)a.destroyMaintainedClients, (int)a.staleStatics, kDestroyMaintainedCount,
           a.firstStale != NULL ? ", first " : "", a.firstStale != NULL ? a.firstStale : "", (int)a.listIndex,
           a.extensions);
}

} // namespace

extern "C" int OoT_AbandonedSessionStatics_RunHeadless(void) {
    printf("[TEST] oot-abandoned-session-statics: an abandoned OoT session's overlay statics and client counts are "
           "reset (#750)\n");

    AbandonedSessionRowGuard guard;
    if (ActorDB::Instance == nullptr) {
        // The headless tier never runs InitOTR; the real table is pure data.
        ActorDB::Instance = new ActorDB();
        guard.ownsActorDB = true;
    }

    // Two unused ActorDB slots (gaps in the actor table) carry probe resets, so
    // (a), (b) and (e) are observed on a reset this row owns.
    const s32 count = ActorDB::Instance->GetEntryCount();
    for (s32 i = 0; i < count; i++) {
        const ActorDBEntry* e = Entry(i);
        if (!e->valid && e->reset == NULL && e->numLoaded == 0) {
            if (guard.probeSlot < 0) {
                guard.probeSlot = i;
            } else {
                guard.idleSlot = i;
                break;
            }
        }
    }
    // Preconditions: nothing is seeded yet, so these may return directly.
    OAS_ASSERT(guard.probeSlot >= 0 && guard.idleSlot >= 0, "no two unused ActorDB slots for the probes");
    OAS_ASSERT(!gAudioContextInitalized, "OoT audio is initialized: the real suspend would reset a live audio heap");
    OAS_ASSERT(OoT_GetGameOps() != NULL && OoT_GetGameOps()->suspend != NULL, "OoT registers no suspend");
    for (size_t k = 0; k < kDestroyMaintainedCount; k++) {
        const ActorDBEntry* e = Entry(kDestroyMaintained[k].actorId);
        OAS_ASSERT(e->valid, "Destroy-maintained overlay has no ActorDB entry");
        OAS_ASSERT(e->reset != NULL, "Destroy-maintained overlay has no reset");
        OAS_ASSERT(e->numLoaded == 0, "Destroy-maintained overlay already counts clients");
        OAS_ASSERT(!kDestroyMaintained[k].isDirty(), "a Destroy-maintained static is not at its initial value");
    }

    static Actor fakeActor;
    static u8 fakePlayState[sizeof(PlayState)];
    static ComboContext comboSnapshot;
    fakeActor = {};
    sProbeResetCalls = 0;
    sIdleProbeResetCalls = 0;
    sProbeSawRetiredGraph = true;

    guard.fakeActor = &fakeActor;
    guard.playState = OoT_gPlayState;
    guard.gameState = OoT_gGameState;
    guard.gameMode = gSaveContext.gameMode;
    guard.resetTimer = gAudioContext.resetTimer;
    guard.audioInitialized = gAudioContextInitalized;
    memcpy(&comboSnapshot, &gComboCtx, sizeof(ComboContext));
    guard.comboSnapshot = &comboSnapshot;
    guard.seeded = true;

    // gameMode is the title screen so the suspend's harvest stays gated off (it
    // must not fold this row's save into the shared pool).
    gSaveContext.gameMode = GAMEMODE_TITLE_SCREEN;
    Entry(guard.probeSlot)->reset = ProbeReset;
    Entry(guard.idleSlot)->reset = IdleProbeReset;

    // ---- First abandoned session. ----
    StageAbandonedSession(guard, fakePlayState, 2);
    s32 dirtyBefore = 0;
    for (size_t k = 0; k < kDestroyMaintainedCount; k++) {
        dirtyBefore += kDestroyMaintained[k].isDirty() ? 1 : 0;
    }
    const s16 listIndexBefore = GetActorListIndex(&fakeActor);
    const size_t extensionsBefore = ObjectExtension::GetInstance().Count();
    printf("[TEST] oot-abandoned-session-statics: before first suspend: probe clients %d, stale Destroy-maintained "
           "statics %d/%zu, list index %d, extension entries %zu\n",
           (int)Entry(guard.probeSlot)->numLoaded, (int)dirtyBefore, kDestroyMaintainedCount, (int)listIndexBefore,
           extensionsBefore);

    OoT_GetGameOps()->suspend();

    const SessionAfter first = ReadAfter(guard);
    const int probeCallsFirst = sProbeResetCalls;
    PrintAfter("first suspend", first);

    // ---- Second abandoned session, spawned on top of what the first left. ----
    StageAbandonedSession(guard, fakePlayState, 1);
    const s32 probeClientsSecondBefore = Entry(guard.probeSlot)->numLoaded;
    OoT_GetGameOps()->suspend();

    const SessionAfter second = ReadAfter(guard);
    PrintAfter("second suspend", second);

    // The seeding took (read before the suspends; asserted here, with the guard
    // armed, so every exit path restores the state).
    OAS_ASSERT(dirtyBefore == (s32)kDestroyMaintainedCount, "a Destroy-maintained static did not take its seed");
    OAS_ASSERT(listIndexBefore == 7, "ObjectExtension did not take the seeded entry");
    OAS_ASSERT(extensionsBefore > 0, "ObjectExtension count does not see the seeded entry");

    // (a)-(d), first departure.
    OAS_ASSERT(first.probeClients == 0, "the probe overlay still counts clients from the abandoned session");
    OAS_ASSERT(probeCallsFirst == 1, "an overlay with abandoned clients was not reset exactly once");
    OAS_ASSERT(sProbeSawRetiredGraph, "the abandoned session was retired before the graph (OoT_gPlayState still set)");
    OAS_ASSERT(first.idleClients == 0 && sIdleProbeResetCalls == 0,
               "an overlay with no clients was reset again (resets are not all idempotent)");
    OAS_ASSERT(first.destroyMaintainedClients == 0, "a Destroy-maintained overlay still counts abandoned clients");
    OAS_ASSERT(first.staleStatics == 0, "a static only Destroy restored survived the abandoned session");
    OAS_ASSERT(first.listIndex == -1, "a per-actor ObjectExtension entry survived the abandoned session");
    OAS_ASSERT(first.extensions == 0, "ObjectExtension entries survived the abandoned session");

    // (e), second departure: the counts do not accumulate across abandonments.
    OAS_ASSERT(probeClientsSecondBefore == 1, "the second session's probe did not start from a zero count");
    OAS_ASSERT(second.probeClients == 0 && second.destroyMaintainedClients == 0,
               "a second abandoned session left client counts above zero");
    OAS_ASSERT(sProbeResetCalls == 2, "the probe was not reset exactly once per abandoned session");
    OAS_ASSERT(sIdleProbeResetCalls == 0, "an overlay with no clients was reset by the second departure");
    OAS_ASSERT(second.staleStatics == 0, "a static only Destroy restored survived the second abandoned session");
    OAS_ASSERT(second.extensions == 0, "ObjectExtension entries survived the second abandoned session");

    printf("[TEST] PASS: oot-abandoned-session-statics - the abandoned session's overlay and actor state is retired, "
           "twice\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
