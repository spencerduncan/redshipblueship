/**
 * @file StoneMaskOoT.cpp
 * @brief Majora's Mask's Stone Mask as an Ocarina of Time item (#979, epic #975
 *        P4's second mask; OoTMM stoneMaskOot and sharedMaskStone), built on the
 *        Blast Mask's pattern (BlastMaskOoT.cpp).
 *
 * THE ITEM. Under the frozen Stone Mask (OoT) rule (RSBS_XITEM_STONE_MASK_OOT, a
 * Cross-Game Rules row, off by default) OoT's pool gains RG_STONE_MASK
 * (3drando/item_pool.cpp). Its get-item sets RAND_INF_OBTAINED_STONE_MASK and
 * fills an empty child trade slot with the custom ItemID ITEM_MASK_STONE
 * (randomizer.cpp). ITEM_MASK_STONE comes after ITEM_MASK_BLAST and, like it, is
 * never SLOT()-indexed: every give and read goes through ITEM_TRADE_CHILD's slot.
 * Its icon is Majora's Mask's own (OoT_gItemIcons' custom row, curated), and the
 * pause menu names it through VB_DRAW_CUSTOM_ITEM_NAME from MM's own name
 * texture once the curated half carries it.
 *
 * WEARING IT. The P3 groundwork (#977, ForeignMaskOoT.cpp) owns the worn field
 * and draws object_mask_stone's list on Link's head; this file adds what the
 * item needs, as the Blast Mask's file does:
 *  - OWNERSHIP: OoT's one ownership source (BlastMaskOoT.cpp, ForeignMasksOwned)
 *    answers the RAND_INF flag, and Combo_ForeignMask_Owned adds the frozen rule.
 *  - THE CHILD TRADE CYCLE: the ItemID joins it while owned under the rule,
 *    after the Blast Mask, OoTMM's order (kOotTradeChild: the Mask of Truth, the
 *    Blast Mask, the Stone Mask, Kamaro's Mask). The cycle visits its extras in
 *    registration order, so this registration runs the Blast Mask's first.
 *  - THE C BUTTON: VB_CHANGE_HELD_ITEM_AND_USE_ITEM puts it on or takes it off
 *    (OoT_ForeignMask_Use), as OoTMM's Player_UseItem toggles MASK_STONE.
 *  - OOT'S BUTTON RULE: the worn mask comes off when no C button (or D-pad slot)
 *    holds it, unless SoH's Persistent Masks is on.
 *
 * THE EFFECT (OoTMM's shouldActorIgnorePlayer and Actor_RunUpdate, generator
 * src/oot/actors.c): while Link wears it, the enemies and guards OoTMM lists
 * (OoT_StoneMask_IgnoresWearer) do not see him. Such an actor's update runs with
 * the player-relative fields Actor_UpdateAll has just computed replaced by "far
 * away, in a random direction": yawTowardsPlayer random, xzDistToPlayer,
 * yDistToPlayer and xyzDistToPlayerSq 10000, OoTMM's values. The real ones are
 * put back as soon as the update returns, so targeting, the draw and every other
 * actor read the true ones. They are replaced in ShouldActorUpdate (the last
 * hook before the update) and put back in OnActorUpdate (the first after it).
 * When another ShouldActorUpdate handler keeps the update from running,
 * OnActorUpdate does not fire, so the next actor's ShouldActorUpdate puts them
 * back, in the same pass: Actor_UpdateAll never deletes an actor it has already
 * passed. A held actor from an earlier frame or play state is forgotten without
 * being touched (its fields were recomputed, or it is gone).
 *
 * THE LOGIC is logic.cpp's (CanUse(RG_STONE_MASK): a child under the frozen rule,
 * OoTMM's can_use_mask_stone, counted where OoTMM's get_past_skultullas and
 * evade_gerudo count it), and the shared extension's pair is the bag's (both
 * games register their copy at RSBS_XITEM_SHARED_STONE, GameExports_SingleExe.cpp).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstring>
#include <string>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/draw.h"
#include "ForeignMaskOoT.h"
#include "BlastMaskOoT.h"
#include "StoneMaskOoT.h"

#include "context.h"       // src/common: Context_GetCurrentGame
#include "foreign_items.h" // src/common: the rule bits, the frozen accessors, Combo_GetForeignItemByNameFor
#include "foreign_masks.h" // src/common: the worn-mask table, ownership

// At file scope first: z64.h includes <memory> under C++, which must not get C
// linkage. Its include guard makes the nested include below a no-op.
#include <z64.h>
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
extern PlayState* OoT_gPlayState;
// ForeignModelHostOoT.cpp: an MM item's model, drawn by OoT as the host.
int OoT_ForeignModel_DrawForeignItem(PlayState* play, uint8_t originGame, uint16_t id);
}

namespace {

// Majora's Mask's own pause name texture, in the curated half
// (assets/crossgame/manifest.txt). Copied into the pause context by path.
const char kStoneMaskNameTex[] = "__OTR__item_name_static/gItemNameStoneMaskENGTex";

// OoTMM's "far away" (Actor_RunUpdate): every player-relative distance an
// update reads while the Stone Mask hides Link from the actor.
constexpr f32 kFarAway = 10000.0f;

/** The listed actor whose fields are replaced, and its true values. */
struct HiddenFrom {
    Actor* actor;
    uint32_t frame;
    s16 yawTowardsPlayer;
    f32 xyzDistToPlayerSq;
    f32 xzDistToPlayer;
    f32 yDistToPlayer;
};
HiddenFrom sHidden = {};

bool PathMounted(const char* path) {
    static const char kOtr[] = "__OTR__";
    if (path == nullptr || std::strncmp(path, kOtr, sizeof(kOtr) - 1) != 0) {
        return false;
    }
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr) {
        return false;
    }
    auto archives = ctx->GetResourceManager()->GetArchiveManager();
    return archives != nullptr && archives->HasFile(std::string(path + sizeof(kOtr) - 1));
}

/** Put the held actor's true fields back and let go of it. */
void PutBack() {
    if (sHidden.actor != nullptr) {
        sHidden.actor->yawTowardsPlayer = sHidden.yawTowardsPlayer;
        sHidden.actor->xyzDistToPlayerSq = sHidden.xyzDistToPlayerSq;
        sHidden.actor->xzDistToPlayer = sHidden.xzDistToPlayer;
        sHidden.actor->yDistToPlayer = sHidden.yDistToPlayer;
    }
    sHidden = {};
}

/** The child trade cycle's question: owned AND added by the frozen world. */
int StoneMaskInTradeCycle(void) {
    return Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_STONE);
}

/** Is ITEM_MASK_STONE equipped on a C button (or, with SoH's D-pad equips, a
 *  D-pad slot)? The equips themselves, as the Blast Mask's rule reads them. */
bool StoneMaskOnButtons() {
    for (int i = 1; i <= 3; i++) {
        if (gSaveContext.equips.buttonItems[i] == ITEM_MASK_STONE) {
            return true;
        }
    }
    if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) != 0) {
        for (int i = 4; i <= 7; i++) {
            if (gSaveContext.equips.buttonItems[i] == ITEM_MASK_STONE) {
                return true;
            }
        }
    }
    return false;
}

/** Link's ShouldActorUpdate: OoT's button rule for the worn Stone Mask. */
void StoneMaskBeforePlayerUpdate(void* actorRef, bool* should) {
    (void)should;
    if (Context_GetCurrentGame() != GAME_OOT || OoT_gPlayState == nullptr || actorRef == nullptr) {
        return;
    }
    Player* player = static_cast<Player*>(actorRef);
    if (player != GET_PLAYER(OoT_gPlayState)) {
        return;
    }
    if (OoT_ForeignMask_Worn() == COMBO_FOREIGN_MASK_MM_STONE &&
        CVarGetInteger(CVAR_ENHANCEMENT("PersistentMasks"), 0) == 0 && !StoneMaskOnButtons()) {
        OoT_ForeignMask_Use(player, COMBO_FOREIGN_MASK_MM_STONE); // the toggle takes the worn mask off
    }
}

/** Every actor's ShouldActorUpdate: hide Link from a listed one. */
void StoneMaskBeforeActorUpdate(void* actorRef, bool* should) {
    if (Context_GetCurrentGame() != GAME_OOT || OoT_gPlayState == nullptr || actorRef == nullptr) {
        return;
    }
    OoT_StoneMask_BeforeActorUpdate(static_cast<Actor*>(actorRef), OoT_gPlayState->state.frames,
                                    (should == nullptr || *should) ? 1 : 0);
}

/** Every actor's OnActorUpdate: show Link again. */
void StoneMaskAfterActorUpdate(void* actorRef) {
    OoT_StoneMask_AfterActorUpdate(static_cast<Actor*>(actorRef));
}

/** The C-button press (VB_CHANGE_HELD_ITEM_AND_USE_ITEM): true when `item` is the
 *  Stone Mask, which never reaches OoT_Player_UseItem. */
bool StoneMaskUseItem(int32_t item) {
    if (item != (int32_t)ITEM_MASK_STONE) {
        return false;
    }
    if (Context_GetCurrentGame() != GAME_OOT || OoT_gPlayState == nullptr) {
        return true;
    }
    Player* player = GET_PLAYER(OoT_gPlayState);
    if (Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_STONE) &&
        OoT_ForeignMask_Use(player, COMBO_FOREIGN_MASK_MM_STONE) != 0) {
        OoT_Player_PlaySfx(&player->actor, NA_SE_PL_CHANGE_ARMS);
    }
    return true;
}

/** The pause menu's name (VB_DRAW_CUSTOM_ITEM_NAME): MM's own texture, when the
 *  curated half carries it; otherwise OoT's table names it as before. */
bool StoneMaskDrawName(uint32_t namedItem) {
    if (namedItem != (uint32_t)ITEM_MASK_STONE || OoT_gPlayState == nullptr || !PathMounted(kStoneMaskNameTex)) {
        return false;
    }
    std::memcpy(OoT_gPlayState->pauseCtx.nameSegment, kStoneMaskNameTex, sizeof(kStoneMaskNameTex));
    return true;
}

void StoneMaskOnLoadGame(int32_t fileNum) {
    (void)fileNum;
    OoT_StoneMask_Forget();
    OoT_StoneMask_RegisterRuntime();
}

void StoneMaskOnPlayDestroy() {
    OoT_StoneMask_Forget();
}

} // namespace

extern "C" bool OoT_StoneMaskOoTOn(void) {
    return Combo_ComboCrossItemRule(RSBS_XITEM_STONE_MASK_OOT);
}

extern "C" int OoT_StoneMask_IgnoresWearer(int16_t actorId, int16_t params) {
    // OoTMM's shouldActorIgnorePlayer, actor for actor.
    switch (actorId) {
        case ACTOR_EN_AM:
            return params != 0 ? 1 : 0; // the Armos that fights (ARMOS_ENEMY), not the statue
        case ACTOR_EN_NY:
        case ACTOR_EN_SB:
        case ACTOR_EN_RR:
        case ACTOR_EN_TORCH2:
        case ACTOR_EN_FZ:
        case ACTOR_EN_WEIYER:
        case ACTOR_EN_EIYER:
        case ACTOR_EN_BB:
        case ACTOR_EN_ANUBICE:
        case ACTOR_EN_TP:
        case ACTOR_EN_BA:
        case ACTOR_EN_GOMA:
        case ACTOR_EN_DODOJR:
        case ACTOR_EN_SKJ:
        case ACTOR_EN_CROW:
        case ACTOR_EN_HINTNUTS:
        case ACTOR_EN_WALLMAS:
        case ACTOR_EN_REEBA:
        case ACTOR_EN_FLOORMAS:
        case ACTOR_EN_DEKUNUTS:
        case ACTOR_EN_DNS:
        case ACTOR_EN_MB:
        case ACTOR_EN_TITE:
        case ACTOR_EN_PEEHAT:
        case ACTOR_EN_FIREFLY:
        case ACTOR_EN_DODONGO:
        case ACTOR_EN_SW:
        case ACTOR_EN_VM:
        case ACTOR_EN_ST:
        case ACTOR_EN_GE1:
        case ACTOR_EN_GE2:
        case ACTOR_EN_BILI:
        case ACTOR_EN_VALI:
        case ACTOR_EN_HEISHI2:
        case ACTOR_EN_HEISHI3:
        case ACTOR_EN_HEISHI4:
            return 1;
        default:
            return 0;
    }
}

extern "C" int OoT_StoneMask_BeforeActorUpdate(Actor* actor, uint32_t frame, int updates) {
    if (sHidden.actor != nullptr) {
        if (sHidden.frame == frame) {
            PutBack(); // its update did not run: another handler kept it from running
        } else {
            sHidden = {}; // an earlier frame's: recomputed since, or gone; never touched
        }
    }
    if (actor == nullptr || updates == 0 || OoT_StoneMask_IgnoresWearer(actor->id, actor->params) == 0 ||
        OoT_ForeignMask_Worn() != COMBO_FOREIGN_MASK_MM_STONE) {
        return 0;
    }
    sHidden.actor = actor;
    sHidden.frame = frame;
    sHidden.yawTowardsPlayer = actor->yawTowardsPlayer;
    sHidden.xyzDistToPlayerSq = actor->xyzDistToPlayerSq;
    sHidden.xzDistToPlayer = actor->xzDistToPlayer;
    sHidden.yDistToPlayer = actor->yDistToPlayer;
    actor->yawTowardsPlayer = (s16)(u16)(OoT_Rand_ZeroOne() * 0x10000);
    actor->xyzDistToPlayerSq = kFarAway;
    actor->xzDistToPlayer = kFarAway;
    actor->yDistToPlayer = kFarAway;
    return 1;
}

extern "C" void OoT_StoneMask_AfterActorUpdate(Actor* actor) {
    if (actor != nullptr && actor == sHidden.actor) {
        PutBack();
    }
}

extern "C" void OoT_StoneMask_Forget(void) {
    sHidden = {};
}

extern "C" void OoT_StoneMask_RegisterRuntime(void) {
    // The Blast Mask's first: it registers OoT's one ownership source (whose
    // ForeignMasksOwned answers the Stone Mask too), and the cycle visits its
    // extras in registration order, the Blast Mask before the Stone Mask.
    OoT_BlastMask_RegisterRuntime();
    OoT_ChildTradeCycle_RegisterExtra((uint8_t)ITEM_MASK_STONE, StoneMaskInTradeCycle); // 0 once registered
    // AGE_REQ_CHILD (z_kaleido_scope.h): a child trade mask, as the Blast Mask is.
    // The 256-entry table covers the custom ItemID (#983).
    gItemAgeReqs[ITEM_MASK_STONE] = LINK_AGE_CHILD;
}

/**
 * RG_STONE_MASK's get-item model: Majora's Mask's own Stone Mask, drawn by OoT as
 * the host from the curated half (OoT_ForeignModel_DrawForeignItem); the Keaton
 * Mask's row when no archive carries it.
 */
extern "C" void Randomizer_DrawStoneMask(PlayState* play, GetItemEntry* getItemEntry) {
    (void)getItemEntry;
    static SharedItem sMmStone;
    static int sResolved = -1;
    if (play == nullptr) {
        return;
    }
    if (sResolved < 0) {
        sResolved = Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, "Stone Mask", &sMmStone) ? 1 : 0;
    }
    if (sResolved == 1 && OoT_ForeignModel_DrawForeignItem(play, sMmStone.originGame, sMmStone.id) == 1) {
        return;
    }
    OoT_GetItem_Draw(play, GID_MASK_KEATON);
}

// Always registered: every hook acts only on the worn field, the custom ItemID or
// the rule, which a world without the Stone Mask (OoT) rule never holds.
static void RegisterStoneMaskOoT() {
    OoT_StoneMask_RegisterRuntime();
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_PLAYER, true, StoneMaskBeforePlayerUpdate);
    COND_HOOK(ShouldActorUpdate, true, StoneMaskBeforeActorUpdate);
    COND_HOOK(OnActorUpdate, true, StoneMaskAfterActorUpdate);
    COND_HOOK(OnLoadGame, true, StoneMaskOnLoadGame);
    COND_HOOK(OnPlayDestroy, true, StoneMaskOnPlayDestroy);
    COND_VB_SHOULD(VB_CHANGE_HELD_ITEM_AND_USE_ITEM, true, {
        const int32_t usedItem = va_arg(args, int32_t);
        if (StoneMaskUseItem(usedItem)) {
            *should = false;
        }
    });
    COND_VB_SHOULD(VB_DRAW_CUSTOM_ITEM_NAME, true, {
        const uint32_t namedItem = va_arg(args, uint32_t);
        if (StoneMaskDrawName(namedItem)) {
            *should = true;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterStoneMaskOoT);

#endif // RSBS_SINGLE_EXECUTABLE
