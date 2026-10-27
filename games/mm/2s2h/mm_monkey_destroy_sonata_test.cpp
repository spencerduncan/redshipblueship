/**
 * @file mm_monkey_destroy_sonata_test.cpp
 * ROM-free, display-free lock for #1203: leaving the Deku King's chamber after
 * the monkey check must not teach the vanilla Sonata of Awakening in a rando
 * save. CTest label "redship", row MMMonkeyDestroySonata in
 * CMake/SingleExecutable.cmake, dispatch "mm-monkey-destroy-sonata" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN (#1203). The tied-up monkey arms MONKEY_FLAGS_2000 when the
 * player answers its choice (z_en_mnk.c, the TEXT_STATE_CHOICE case 1), and
 * EnMnk_Destroy gives ITEM_SONG_SONATA whenever a tied-up monkey with that
 * flag is unloaded. Under rando the lesson skip
 * (Enhancements/Cutscenes/StoryCutscenes/SkipLearningSonataOfAwakening.cpp)
 * hands the check off through VB_GIVE_ITEM_FROM_MNK and warps Link out, and
 * the warp-out unloads the monkey: the destroy-time give then taught the
 * vanilla Sonata on every rando file, wherever the seed placed the real one.
 *
 * THE GATE. The give alone dispatches VB_GIVE_ITEM_FROM_MNK_ON_DESTROY, which
 * Rando/ActorBehavior/EnMnk.cpp answers false under IS_RANDO. The same block's
 * CLEAR_EVENTINF(EVENTINF_24) (stop forcing the Deku Pipes) stays ungated. The
 * VB is NOT VB_GIVE_ITEM_FROM_MNK: that registrant marks the monkey check
 * eligible, and a second dispatch at unload would re-arm a check the queue may
 * already have given (the #1220 class).
 *
 * LEGS.
 *   1. Vanilla save, rando behaviour disarmed (the control): the destroy still
 *      teaches the Sonata and clears EVENTINF_24.
 *   2. Rando save, armed through Rando::ActorBehavior::OnFileLoad (the
 *      production path a rando save load runs): the VB's registry rises by
 *      exactly one, the destroy leaves QUEST_SONG_SONATA clear, still clears
 *      EVENTINF_24, and does not make RC_DEKU_KINGS_CHAMBER_MONKEY eligible.
 *   3. A rando monkey that never had its choice answered gives nothing either
 *      way (the flag, not the type, is what arms the give).
 * Cleanup restores the SaveContext image and MM_gPlayState and re-runs
 * OnFileLoad under the restored save type, so the row is order-free.
 *
 * RED on origin/main: leg 2 fails at the registry delta (no registrant) and,
 * past it, at the quest bit (the destroy teaches the Sonata).
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
extern "C" int MM_MonkeyDestroySonata_RunHeadless(void) {
    printf("[TEST] mm-monkey-destroy-sonata: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdlib>
#include <cstring>
#include <memory>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"
#include "mm_game_hooks.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_En_Mnk/z_en_mnk.h"

// The actor's destroy (ovl_En_Mnk/z_en_mnk.c); no header declares it.
void EnMnk_Destroy(Actor* thisx, PlayState* play);
}

namespace {

int Fail(int code, const char* msg) {
    printf("[TEST] FAIL(%d): mm-monkey-destroy-sonata: %s\n", code, msg);
    return code;
}

size_t DestroyGiveRegistrants() {
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::ShouldVanillaBehavior>();
    auto& buckets = S2H::GameHooks::Registry<GameInteractor::ShouldVanillaBehavior>::functionsForID;
    auto it = buckets.find(VB_GIVE_ITEM_FROM_MNK_ON_DESTROY);
    return it == buckets.end() ? 0u : it->second.size();
}

void DriveRandoArmState(bool rando) {
    gSaveContext.save.shipSaveInfo.saveType = rando ? SAVETYPE_RANDO : SAVETYPE_VANILLA;
    Rando::ActorBehavior::OnFileLoad();
}

/** One unload of the tied-up monkey, from a known starting state. */
void DestroyTiedUpMonkey(EnMnk* monkey, PlayState* play, bool choiceAnswered) {
    memset(monkey, 0, sizeof(*monkey));
    monkey->picto.actor.id = ACTOR_EN_MNK;
    monkey->picto.actor.params = (s16)(MONKEY_TIED_UP << 7);
    monkey->flags = choiceAnswered ? MONKEY_FLAGS_2000 : 0;
    REMOVE_QUEST_ITEM(QUEST_SONG_SONATA);
    // Set directly, not through SET_EVENTINF: MM_Flags_SetEventInf dispatches
    // OnFlagSet, and this row is not about that hook's registrants.
    gSaveContext.eventInf[EVENTINF_24 >> 4] |= (u8)(1 << (EVENTINF_24 & 0xF));
    RANDO_SAVE_CHECKS[RC_DEKU_KINGS_CHAMBER_MONKEY].eligible = false;
    EnMnk_Destroy(&monkey->picto.actor, play);
}

int RunLegs(EnMnk* monkey, PlayState* play) {
    // ---- Leg 1: vanilla save (control) -------------------------------------
    DriveRandoArmState(false);
    const size_t before = DestroyGiveRegistrants();
    DestroyTiedUpMonkey(monkey, play, true);
    if (MONKEY_GET_TYPE(&monkey->picto.actor) != MONKEY_TIED_UP) {
        return Fail(2, "the fixture's params do not decode to MONKEY_TIED_UP");
    }
    if (!CHECK_QUEST_ITEM(QUEST_SONG_SONATA)) {
        return Fail(2, "a vanilla save no longer learns the Sonata when the tied-up monkey is unloaded after its "
                       "choice: the gate reached vanilla play");
    }
    if (CHECK_EVENTINF(EVENTINF_24)) {
        return Fail(3, "a vanilla unload left EVENTINF_24 set (the Deku Pipes stay forced)");
    }

    // ---- Leg 2: rando save, armed through the production path --------------
    DriveRandoArmState(true);
    const size_t after = DestroyGiveRegistrants();
    if (after != before + 1) {
        printf("[TEST] mm-monkey-destroy-sonata: VB_GIVE_ITEM_FROM_MNK_ON_DESTROY registrants %zu -> %zu\n", before,
               after);
        return Fail(10, "arming rando did not register exactly one VB_GIVE_ITEM_FROM_MNK_ON_DESTROY answer "
                        "(Rando/ActorBehavior/EnMnk.cpp, reached from ActorBehavior::OnFileLoad) (#1203)");
    }
    DestroyTiedUpMonkey(monkey, play, true);
    if (CHECK_QUEST_ITEM(QUEST_SONG_SONATA)) {
        return Fail(11, "a rando save learned the vanilla Sonata when the tied-up monkey was unloaded after its "
                        "choice: EnMnk_Destroy's give is ungated, so the player owns a song the seed did not place "
                        "there (#1203)");
    }
    if (CHECK_EVENTINF(EVENTINF_24)) {
        return Fail(12, "the rando unload left EVENTINF_24 set: the gate swallowed the instrument reset along with "
                        "the give");
    }
    if (RANDO_SAVE_CHECKS[RC_DEKU_KINGS_CHAMBER_MONKEY].eligible) {
        return Fail(13, "the unload made RC_DEKU_KINGS_CHAMBER_MONKEY eligible again: the destroy re-arms a check "
                        "the lesson skip already handed off (#1220 class)");
    }

    // ---- Leg 3: no answered choice, no give --------------------------------
    DestroyTiedUpMonkey(monkey, play, false);
    if (CHECK_QUEST_ITEM(QUEST_SONG_SONATA)) {
        return Fail(14, "a tied-up monkey whose choice was never answered taught the Sonata on unload");
    }
    if (!CHECK_EVENTINF(EVENTINF_24)) {
        return Fail(14, "a tied-up monkey whose choice was never answered cleared EVENTINF_24 on unload");
    }
    return 0;
}

} // namespace

extern "C" int MM_MonkeyDestroySonata_RunHeadless(void) {
    printf("[TEST] mm-monkey-destroy-sonata: unloading the tied-up monkey teaches no Sonata in a rando save "
           "(#1203)\n");

    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;

    PlayState* play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    EnMnk* monkey = static_cast<EnMnk*>(calloc(1, sizeof(EnMnk)));
    if (play == nullptr || monkey == nullptr) {
        free(play);
        free(monkey);
        return Fail(1, "could not allocate the fixtures");
    }
    play->sceneId = SCENE_DEKU_KING;
    MM_gPlayState = play;

    const int rc = RunLegs(monkey, play);

    // ---- Cleanup (pass and fail) -------------------------------------------
    MM_gPlayState = savedPlay;
    memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));
    Rando::ActorBehavior::OnFileLoad();
    free(play);
    free(monkey);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-monkey-destroy-sonata: PASS (vanilla still learns the Sonata; rando learns nothing, still "
           "clears EVENTINF_24 and re-arms no check)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
