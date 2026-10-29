// Headless regression test for Issue #1308:
// MM crash in the Southern Swamp when a room swap (func_800BA798 ->
// MM_Actor_Delete) deletes a lily-pad-sized actor (ACTOR_BG_LOTUS) whose
// actor->overlayEntry pointer had its low 32 bits overwritten by the float
// 3822.27f (0x456EE46C), faulting on `overlayEntry->numLoaded--`.
//
// Exercises the real MM translation units (games/mm/src/code/z_actor.c,
// z_actor_dlftbls.c, z_malloc.c):
//   Case 1: MM_Actor_OverlayEntryIsValid accepts a canonical
//           &gActorOverlayTable[actor->id] pointer.
//   Case 2: MM_Actor_OverlayEntryIsValid rejects a pointer whose low 32 bits
//           were overwritten by 0x456EE46C (3822.27f), NULL, out-of-bounds,
//           misaligned, or belonging to a different actor ID; and
//           MM_Actor_ResolveValidOverlayEntry repairs actor->overlayEntry to
//           &gActorOverlayTable[actor->id] and records the corruption.
//   Case 3: MM_Actor_Delete on a ZeldaArena-allocated actor whose overlayEntry
//           was corrupted (both a decoy ActorOverlay and the exact 0x456EE46C
//           low-dword float corruption) does not touch the bogus address,
//           removes and frees the actor cleanly, and decrements the real
//           gActorOverlayTable[actor->id].numLoaded.
//   Case 4: During MM_Actor_UpdateAll, a culprit actor's update callback that
//           corrupts a neighbor actor's overlayEntry with 0x456EE46C is caught
//           immediately by the per-actor canary, which logs/records the culprit
//           actor ID and victim actor ID and repairs victim->overlayEntry
//           in-place before any room swap runs.

#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
#include "z64.h"
#include "z64malloc.h"
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Okuta/z_en_okuta.h"

extern MtxF* MM_sCurrentMatrix;

void AudioSfx_Reset(void);
void MM_Actor_AddToCategory(ActorContext* actorCtx, Actor* actor, u8 actorCategory);
Actor* MM_Actor_Delete(ActorContext* actorCtx, Actor* actor, PlayState* play);
void EnOkuta_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx);

bool MM_Actor_OverlayEntryIsValid(const Actor* actor);
ActorOverlay* MM_Actor_ResolveValidOverlayEntry(Actor* actor, PlayState* play, const char* site);
s32 MM_Actor_ScanListsForOverlayEntryCorruption(PlayState* play, ActorContext* actorCtx, const Actor* culprit,
                                                const char* phase);
void MM_ActorOverlayGuard_ResetForTest(void);
s32 MM_ActorOverlayGuard_GetHitCountForTest(void);
s16 MM_ActorOverlayGuard_GetLastCulpritIdForTest(void);
s16 MM_ActorOverlayGuard_GetLastVictimIdForTest(void);
u32 MM_ActorOverlayGuard_GetLastBadLowDwordForTest(void);
}

namespace {

constexpr u32 kSwampCrashLowDword = 0x456EE46Cu; // 3822.27f from #1308 trace.txt

alignas(16) u8 sTestArenaBuffer[64 * 1024];

Actor* sCanaryVictimActor = nullptr;
int sCulpritUpdateCallCount = 0;

ActorOverlay* MakeFloatCorruptedOverlayPtr(const ActorOverlay* basePtr, u32 lowDword) {
    const uintptr_t highBits = reinterpret_cast<uintptr_t>(basePtr) & ~static_cast<uintptr_t>(0xFFFFFFFFu);
    return reinterpret_cast<ActorOverlay*>(highBits | static_cast<uintptr_t>(lowDword));
}

void CulpritActor_CorruptVictimOverlayEntry(Actor* /*thisx*/, PlayState* /*play*/) {
    sCulpritUpdateCallCount++;
    if (sCanaryVictimActor != nullptr) {
        sCanaryVictimActor->overlayEntry =
            MakeFloatCorruptedOverlayPtr(&gActorOverlayTable[ACTOR_BG_LOTUS], kSwampCrashLowDword);
    }
}

void NoOpActorUpdate(Actor* /*thisx*/, PlayState* /*play*/) {
}

} // namespace

extern "C" int MM_ActorOverlayEntryGuard_RunHeadless(void) {
    int failures = 0;
    auto check = [&](bool cond, const char* msg) {
        if (!cond) {
            std::fprintf(stderr, "[TEST] FAIL (mm-actor-overlay-entry-guard): %s\n", msg);
            ++failures;
        }
    };

    const s32 savedLotusNumLoaded = gActorOverlayTable[ACTOR_BG_LOTUS].numLoaded;
    const s32 savedEtceteraNumLoaded = gActorOverlayTable[ACTOR_OBJ_ETCETERA].numLoaded;
    ActorResetFunc savedLotusReset = (gActorOverlayTable[ACTOR_BG_LOTUS].profile != nullptr)
                                         ? gActorOverlayTable[ACTOR_BG_LOTUS].profile->reset
                                         : nullptr;
    if (gActorOverlayTable[ACTOR_BG_LOTUS].profile != nullptr) {
        gActorOverlayTable[ACTOR_BG_LOTUS].profile->reset = nullptr;
    }

    // --- Case 1: Canonical &gActorOverlayTable[actor->id] is accepted ---
    {
        MM_ActorOverlayGuard_ResetForTest();
        PlayState play{};
        Actor lotus{};
        lotus.id = ACTOR_BG_LOTUS;
        lotus.category = ACTORCAT_BG;
        lotus.room = 1;
        lotus.overlayEntry = &gActorOverlayTable[ACTOR_BG_LOTUS];

        check(MM_Actor_OverlayEntryIsValid(&lotus),
              "Case 1: canonical &gActorOverlayTable[ACTOR_BG_LOTUS] must be valid");
        check(MM_Actor_ResolveValidOverlayEntry(&lotus, &play, "case1") == &gActorOverlayTable[ACTOR_BG_LOTUS],
              "Case 1: MM_Actor_ResolveValidOverlayEntry must return canonical overlayEntry unchanged");
        check(MM_ActorOverlayGuard_GetHitCountForTest() == 0, "Case 1: valid overlayEntry must not trip guard counter");
    }

    // --- Case 2: Corrupted overlayEntry values (including 0x456EE46C = 3822.27f) are rejected and repaired ---
    {
        MM_ActorOverlayGuard_ResetForTest();
        PlayState play{};
        play.sceneId = SCENE_20SICHITAI2;
        play.roomCtx.curRoom.num = 0;
        play.roomCtx.prevRoom.num = 2;

        gActorOverlayTable[ACTOR_BG_LOTUS].numLoaded = 1;

        Actor lotus{};
        lotus.id = ACTOR_BG_LOTUS;
        lotus.category = ACTORCAT_BG;
        lotus.room = 1;
        lotus.overlayEntry = MakeFloatCorruptedOverlayPtr(&gActorOverlayTable[ACTOR_BG_LOTUS], kSwampCrashLowDword);

        check(!MM_Actor_OverlayEntryIsValid(&lotus),
              "Case 2a: overlayEntry with low dword 0x456EE46C (3822.27f) must be rejected");

        ActorOverlay* resolved = MM_Actor_ResolveValidOverlayEntry(&lotus, &play, "case2_float");
        check(resolved == &gActorOverlayTable[ACTOR_BG_LOTUS],
              "Case 2a: ResolveValidOverlayEntry must recover &gActorOverlayTable[ACTOR_BG_LOTUS]");
        check(lotus.overlayEntry == &gActorOverlayTable[ACTOR_BG_LOTUS],
              "Case 2a: ResolveValidOverlayEntry must repair actor->overlayEntry in-place");
        check(MM_ActorOverlayGuard_GetHitCountForTest() == 1,
              "Case 2a: guard hit count must increment on float-corrupted overlayEntry");
        check(MM_ActorOverlayGuard_GetLastVictimIdForTest() == ACTOR_BG_LOTUS,
              "Case 2a: guard must record victim actor ID (ACTOR_BG_LOTUS)");
        check(MM_ActorOverlayGuard_GetLastBadLowDwordForTest() == kSwampCrashLowDword,
              "Case 2a: guard must record corrupted low dword 0x456EE46C");

        // NULL, out-of-bounds, misaligned, and wrong-actor-id pointers must all be rejected.
        lotus.overlayEntry = nullptr;
        check(!MM_Actor_OverlayEntryIsValid(&lotus), "Case 2b: NULL overlayEntry must be rejected");

        lotus.overlayEntry = &gActorOverlayTable[0] - 1;
        check(!MM_Actor_OverlayEntryIsValid(&lotus), "Case 2c: before-table overlayEntry must be rejected");

        lotus.overlayEntry = &gActorOverlayTable[ACTOR_ID_MAX];
        check(!MM_Actor_OverlayEntryIsValid(&lotus), "Case 2d: past-end overlayEntry must be rejected");

        lotus.overlayEntry =
            reinterpret_cast<ActorOverlay*>(reinterpret_cast<uintptr_t>(&gActorOverlayTable[ACTOR_BG_LOTUS]) + 4);
        check(!MM_Actor_OverlayEntryIsValid(&lotus), "Case 2e: misaligned interior overlayEntry must be rejected");

        lotus.overlayEntry = &gActorOverlayTable[ACTOR_PLAYER];
        check(!MM_Actor_OverlayEntryIsValid(&lotus),
              "Case 2f: overlayEntry pointing to a different actor ID's slot must be rejected");
    }

    // --- Case 3: MM_Actor_Delete guards against corrupted overlayEntry and still frees the actor ---
    {
        AudioSfx_Reset();
        std::memset(sTestArenaBuffer, 0, sizeof(sTestArenaBuffer));
        MM_ZeldaArena_Init(sTestArenaBuffer, sizeof(sTestArenaBuffer));

        PlayState play{};
        play.sceneId = SCENE_20SICHITAI2;
        play.roomCtx.curRoom.num = 0;
        play.roomCtx.prevRoom.num = 2;

        // 3a: Writable decoy overlayEntry proves MM_Actor_Delete does not decrement the bogus target.
        MM_ActorOverlayGuard_ResetForTest();
        ActorOverlay decoyOverlay{};
        decoyOverlay.numLoaded = 7;

        gActorOverlayTable[ACTOR_BG_LOTUS].numLoaded = 1;
        Actor* victimDecoy = static_cast<Actor*>(MM_ZeldaArena_Malloc(sizeof(Actor)));
        check(victimDecoy != nullptr, "Case 3a: MM_ZeldaArena_Malloc must allocate test actor");
        if (victimDecoy != nullptr) {
            std::memset(victimDecoy, 0, sizeof(Actor));
            victimDecoy->id = ACTOR_BG_LOTUS;
            victimDecoy->room = 1;
            victimDecoy->overlayEntry = &decoyOverlay;
            MM_Actor_AddToCategory(&play.actorCtx, victimDecoy, ACTORCAT_BG);

            Actor* next = MM_Actor_Delete(&play.actorCtx, victimDecoy, &play);
            check(next == nullptr, "Case 3a: MM_Actor_Delete must unlink single actor and return NULL head");
            check(play.actorCtx.actorLists[ACTORCAT_BG].first == nullptr &&
                      play.actorCtx.actorLists[ACTORCAT_BG].length == 0,
                  "Case 3a: ACTORCAT_BG list must be empty after MM_Actor_Delete");
            check(decoyOverlay.numLoaded == 7,
                  "Case 3a: MM_Actor_Delete must NOT decrement bogus decoy overlayEntry->numLoaded");
            check(gActorOverlayTable[ACTOR_BG_LOTUS].numLoaded == 0,
                  "Case 3a: MM_Actor_Delete must recover and decrement gActorOverlayTable[ACTOR_BG_LOTUS].numLoaded");
        }

        // 3b: Exact #1308 low-dword float corruption (0x456EE46C) through MM_Actor_Delete.
        MM_ActorOverlayGuard_ResetForTest();
        gActorOverlayTable[ACTOR_BG_LOTUS].numLoaded = 1;
        Actor* victimFloat = static_cast<Actor*>(MM_ZeldaArena_Malloc(sizeof(Actor)));
        check(victimFloat != nullptr, "Case 3b: MM_ZeldaArena_Malloc must allocate second test actor");
        if (victimFloat != nullptr) {
            std::memset(victimFloat, 0, sizeof(Actor));
            victimFloat->id = ACTOR_BG_LOTUS;
            victimFloat->room = 1;
            victimFloat->overlayEntry =
                MakeFloatCorruptedOverlayPtr(&gActorOverlayTable[ACTOR_BG_LOTUS], kSwampCrashLowDword);
            MM_Actor_AddToCategory(&play.actorCtx, victimFloat, ACTORCAT_BG);

            const bool rejectedCorruptPtr = !MM_Actor_OverlayEntryIsValid(victimFloat);
            check(rejectedCorruptPtr,
                  "Case 3b: MM_Actor_OverlayEntryIsValid must reject 0x456EE46C before MM_Actor_Delete");
            if (rejectedCorruptPtr) {
                Actor* next = MM_Actor_Delete(&play.actorCtx, victimFloat, &play);
                check(next == nullptr, "Case 3b: MM_Actor_Delete must complete without crashing on 0x456EE46C");
                check(play.actorCtx.actorLists[ACTORCAT_BG].first == nullptr,
                      "Case 3b: victimFloat must be removed from ACTORCAT_BG");
                check(gActorOverlayTable[ACTOR_BG_LOTUS].numLoaded == 0,
                      "Case 3b: gActorOverlayTable[ACTOR_BG_LOTUS].numLoaded must be decremented to 0");
                check(MM_ActorOverlayGuard_GetLastBadLowDwordForTest() == kSwampCrashLowDword,
                      "Case 3b: MM_Actor_Delete guard must log/record 0x456EE46C");
            } else {
                // Clean up list if running on unguarded [lock] seam so arena teardown is tidy.
                play.actorCtx.actorLists[ACTORCAT_BG].first = nullptr;
                play.actorCtx.actorLists[ACTORCAT_BG].length = 0;
                MM_ZeldaArena_Free(victimFloat);
            }
        }

        MM_ZeldaArena_Cleanup();
    }

    // --- Case 4: Update-loop canary identifies culprit actor ID and repairs victim overlayEntry in-place ---
    {
        MM_ActorOverlayGuard_ResetForTest();
        sCulpritUpdateCallCount = 0;

        PlayState play{};
        play.sceneId = SCENE_20SICHITAI2;
        play.roomCtx.curRoom.num = 0;
        play.roomCtx.prevRoom.num = 2;
        play.objectCtx.slots[0].id = 1; // Mark object slot 0 loaded for headless Actor_UpdateActor

        Player dummyPlayer{};
        dummyPlayer.actor.id = ACTOR_PLAYER;
        dummyPlayer.actor.category = ACTORCAT_PLAYER;
        dummyPlayer.actor.overlayEntry = &gActorOverlayTable[ACTOR_PLAYER];
        dummyPlayer.stateFlags1 = PLAYER_STATE1_2; // Skip Attention_Update in headless test
        play.actorCtx.actorLists[ACTORCAT_PLAYER].first = &dummyPlayer.actor;
        play.actorCtx.actorLists[ACTORCAT_PLAYER].length = 1;

        Actor victimLotus{};
        victimLotus.id = ACTOR_BG_LOTUS;
        victimLotus.room = 1;
        victimLotus.objectSlot = 0;
        victimLotus.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_FREEZE_EXCEPTION;
        victimLotus.overlayEntry = &gActorOverlayTable[ACTOR_BG_LOTUS];
        victimLotus.update = NoOpActorUpdate;
        MM_Actor_AddToCategory(&play.actorCtx, &victimLotus, ACTORCAT_BG);

        Actor culpritActor{};
        culpritActor.id = ACTOR_OBJ_ETCETERA;
        culpritActor.room = 0;
        culpritActor.objectSlot = 0;
        culpritActor.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_FREEZE_EXCEPTION;
        culpritActor.overlayEntry = &gActorOverlayTable[ACTOR_OBJ_ETCETERA];
        culpritActor.update = CulpritActor_CorruptVictimOverlayEntry;
        MM_Actor_AddToCategory(&play.actorCtx, &culpritActor, ACTORCAT_PROP);

        sCanaryVictimActor = &victimLotus;
        MM_Actor_UpdateAll(&play, &play.actorCtx);
        sCanaryVictimActor = nullptr;

        check(sCulpritUpdateCallCount == 1, "Case 4: culprit actor update callback must run during MM_Actor_UpdateAll");
        check(MM_ActorOverlayGuard_GetLastCulpritIdForTest() == ACTOR_OBJ_ETCETERA,
              "Case 4: canary must identify ACTOR_OBJ_ETCETERA as the culprit actor that corrupted overlayEntry");
        check(MM_ActorOverlayGuard_GetLastVictimIdForTest() == ACTOR_BG_LOTUS,
              "Case 4: canary must identify ACTOR_BG_LOTUS as the victim actor");
        check(MM_ActorOverlayGuard_GetLastBadLowDwordForTest() == kSwampCrashLowDword,
              "Case 4: canary must record corrupted low dword 0x456EE46C");
        check(victimLotus.overlayEntry == &gActorOverlayTable[ACTOR_BG_LOTUS],
              "Case 4: canary must repair victimLotus.overlayEntry in-place during MM_Actor_UpdateAll");
    }

    // --- Case 5: EnOkuta_PostLimbDraw bounds guard and MM_SkelAnime_Init limbCount clamp (#1308 root cause) ---
    // When an OoT 38-limb Octorok skeleton (`alt/objects/object_okuta/gOctorokSkel`)
    // was loaded into MM's 16-limb `EnOkuta`:
    //   (a) `EnOkuta_PostLimbDraw` with `limbIndex = 18` read `sLimbToBodyParts[18]`
    //       out of bounds (`0xBB` = `-69` from the adjacent `sEffectsBodyPartOffsets[0].x = 1500.0f`)
    //       and called `Matrix_MultZero(&this->bodyPartsPos[-69])`, writing a `Vec3f`
    //       before `this` and clobbering the preceding heap actor's `overlayEntry`.
    //   (b) `MM_SkelAnime_Init` set `skelAnime->limbCount = 38` against 16-entry
    //       fixed `jointTable`/`morphTable` buffers.
    {
        PlayState play{};
        MtxF testMtx = gIdentityMtxF;
        float swampCoord = 0.0f;
        std::memcpy(&swampCoord, &kSwampCrashLowDword, sizeof(swampCoord));
        testMtx.mf[3][0] = swampCoord;
        testMtx.mf[3][1] = swampCoord;
        testMtx.mf[3][2] = swampCoord;

        MtxF* savedCurrentMatrix = MM_sCurrentMatrix;
        MM_sCurrentMatrix = &testMtx;

        // Place `victim` and `okuta` in one contiguous buffer at the exact byte
        // stride where `&okuta->bodyPartsPos[-69].z` aliases `&victim->overlayEntry`
        // (`sizeof(BgLotus) + sizeof(ArenaNode) == 0x1D8` on x64).
        constexpr std::ptrdiff_t kUnderflowZFromOkuta = static_cast<std::ptrdiff_t>(offsetof(EnOkuta, bodyPartsPos)) -
                                                        69 * static_cast<std::ptrdiff_t>(sizeof(Vec3f)) +
                                                        static_cast<std::ptrdiff_t>(offsetof(Vec3f, z));
        constexpr std::ptrdiff_t kOkutaOffsetFromVictim =
            static_cast<std::ptrdiff_t>(offsetof(Actor, overlayEntry)) - kUnderflowZFromOkuta;
        static_assert(kOkutaOffsetFromVictim > 0, "okuta must follow victim in the contiguous arena buffer");

        alignas(16) u8 contiguousHeap[static_cast<size_t>(kOkutaOffsetFromVictim) + sizeof(EnOkuta)]{};
        Actor* victim = reinterpret_cast<Actor*>(contiguousHeap);
        EnOkuta* okuta = reinterpret_cast<EnOkuta*>(contiguousHeap + kOkutaOffsetFromVictim);

        victim->id = ACTOR_BG_LOTUS;
        victim->overlayEntry = &gActorOverlayTable[ACTOR_BG_LOTUS];
        okuta->actor.id = ACTOR_EN_OKUTA;
        okuta->actor.overlayEntry = &gActorOverlayTable[ACTOR_EN_OKUTA];

        // Sanity-check the layout: `okuta + kUnderflowZFromOkuta` (`&okuta->bodyPartsPos[-69].z`)
        // must point at `&victim->overlayEntry`.
        check(reinterpret_cast<void*>(reinterpret_cast<u8*>(okuta) + kUnderflowZFromOkuta) ==
                  reinterpret_cast<void*>(&victim->overlayEntry),
              "Case 5a precondition: &okuta->bodyPartsPos[-69].z must alias &victim->overlayEntry");

        EnOkuta_PostLimbDraw(&play, 18, nullptr, nullptr, &okuta->actor);
        EnOkuta_PostLimbDraw(&play, -1, nullptr, nullptr, &okuta->actor);
        MM_sCurrentMatrix = savedCurrentMatrix;

        check(victim->overlayEntry == &gActorOverlayTable[ACTOR_BG_LOTUS],
              "Case 5a: EnOkuta_PostLimbDraw with out-of-range limbIndex (18, -1) must not overwrite preceding "
              "actor->overlayEntry");

        // 5b: MM_SkelAnime_Init must clamp skelAnime.limbCount to the caller's fixed jointTable size.
        SkeletonHeader oversizedSkel{};
        oversizedSkel.limbCount = 37; // 37 + 1 = 38 limbs (OoT's 3DS Octorok skeleton)
        SkelAnime skelAnime{};
        Vec3s jointTable[OCTOROK_LIMB_MAX]{};
        Vec3s morphTable[OCTOROK_LIMB_MAX]{};
        MM_SkelAnime_Init(&play, &skelAnime, &oversizedSkel, nullptr, jointTable, morphTable, OCTOROK_LIMB_MAX);
        check(skelAnime.limbCount == OCTOROK_LIMB_MAX,
              "Case 5b: MM_SkelAnime_Init must clamp skelAnime.limbCount (38 -> 16) to match fixed jointTable size");
    }

    gActorOverlayTable[ACTOR_BG_LOTUS].numLoaded = savedLotusNumLoaded;
    gActorOverlayTable[ACTOR_OBJ_ETCETERA].numLoaded = savedEtceteraNumLoaded;
    if (gActorOverlayTable[ACTOR_BG_LOTUS].profile != nullptr) {
        gActorOverlayTable[ACTOR_BG_LOTUS].profile->reset = savedLotusReset;
    }

    if (failures == 0) {
        std::printf("[TEST] PASS: mm-actor-overlay-entry-guard (5 cases)\n");
        return 0;
    }
    std::fprintf(stderr, "[TEST] %d failure(s) in mm-actor-overlay-entry-guard\n", failures);
    return 1;
}
