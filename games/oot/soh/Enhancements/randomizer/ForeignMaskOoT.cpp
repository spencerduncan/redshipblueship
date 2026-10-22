/**
 * @file ForeignMaskOoT.cpp
 * @brief OoT wears and draws a Majora's Mask mask (#977, epic #975 P3).
 *
 * THE FIELD. `gSaveContext.ship.foreignMask` holds the worn mask as a
 * ComboForeignMask (src/common/foreign_masks.h), never `currentMask`: OoT
 * indexes sMaskDlists, the face reactions and the auto-unequip arithmetic by
 * currentMask with no bound (ovl_player_actor/z_player.c), and a tenth value
 * would map onto PLAYER_IA_LENS_OF_TRUTH and past PLAYER_IA_MAX. Every reader
 * here goes through Combo_ForeignMask_Sanitize, so no stored byte, however
 * corrupt, selects anything but a row of the shared table. The field is the
 * last byte of SaveContext, so it crosses with the frozen OoT half and rides
 * the .redsave's Tier 2; SoH's own .sav carries it beside maskMemory.
 * A mask is worn only while the frozen world's item rules hold its extension
 * bit (Combo_ForeignMask_Wearable): every reader here applies it.
 *
 * OOT'S MASK RULES, applied to it as OoT applies them to its own masks:
 *  - a mask comes off at every scene load unless SoH's Persistent Masks is on
 *    (Player_Init's "keep masks thru loading zones"), and then also when the
 *    child trade slot is sold out (OoT_ForeignMask_OnPlayerInit);
 *  - the masks are child items: an adult Link wears none (the age change);
 *  - one mask at a time: putting a native mask on takes the MM one off
 *    (OoT_ForeignMask_ApplyRules), and putting the MM one on takes the native
 *    one off, maskMemory included (OoT_ForeignMask_Use).
 * The items that put one on are P4's (#979): their VB_CHANGE_HELD_ITEM_AND_USE_ITEM
 * handler calls OoT_ForeignMask_Use, and their custom ItemIDs join the child
 * trade cycle through OoT_ChildTradeCycle_RegisterExtra. Until then nothing in
 * a normal session writes the field.
 *
 * THE DRAW. One guarded call in the head limb's post-limb draw
 * (games/oot/src/code/z_player_lib.c, OoT_Player_PostLimbDrawGameplay) lands in
 * OoT_ForeignMask_DrawOnHead, for the real player only (Dark Link draws through
 * the same function), and draws what OoT_ForeignMask_WornDl selects. MM draws
 * its masks from its own head limb's post-limb draw in that limb's space (MM
 * z_player_lib.c, D_801C0B20), so the same list drawn under OoT's head matrix
 * sits where MM puts it. The matrix is loaded explicitly from the CPU stack
 * (the head limb's list may leave another limb's matrix loaded). The list is
 * named by its path in redship-oot.o2r, the curated MM half OoT mounts, never
 * through mm.o2r's (#981). A path no mounted archive holds draws nothing.
 *
 * The Blast Mask's list calls segment 0x09, where MM's Player_DrawBlastMask
 * binds a one-command list setting the env colour to opaque black while the
 * cooldown timer is 0 (MM's glow pass, a TextureAnimation and segment 0x08, is
 * not drawn: P4 owns the cooldown). OoT's Player binds segment 0x09 to the
 * mouth texture before its skeleton (OoT_Player_DrawImpl); the head limb, its
 * only reader, is drawn before this point, and every later draw that reads
 * segment 0x09 binds its own.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <array>
#include <cstddef>
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
#include "ForeignMaskOoT.h"

#include "context.h"       // src/common: Context_GetCurrentGame
#include "foreign_masks.h" // src/common: the table and the equip rules

// At file scope first: z64.h includes <memory> under C++, which must not get C
// linkage. Its include guard makes the nested include below a no-op.
#include <z64.h>
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
extern PlayState* OoT_gPlayState;
}

namespace {

// The child trade cycle's registered extras (P4's masks), in cycle order.
constexpr int kMaxTradeExtras = 8;
struct TradeExtra {
    uint8_t itemId;
    OoTChildTradeOwnedFn owned;
};
std::array<TradeExtra, kMaxTradeExtras> sTradeExtras{};
int sTradeExtraCount = 0;

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

bool IsChild() {
    return gSaveContext.linkAge == LINK_AGE_CHILD;
}

/** OoT's eleven contiguous child trade items, then the registered extras. */
int BuildTradeList(uint8_t* ids, uint8_t* owned, int cap) {
    int n = 0;
    for (int item = ITEM_WEIRD_EGG; item <= ITEM_MASK_TRUTH && n < cap; item++, n++) {
        ids[n] = (uint8_t)item;
        owned[n] =
            Flags_GetRandomizerInf((RandomizerInf)(RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG + (item - ITEM_WEIRD_EGG))) ? 1
                                                                                                                   : 0;
    }
    for (int i = 0; i < sTradeExtraCount && n < cap; i++, n++) {
        ids[n] = sTradeExtras[i].itemId;
        owned[n] = (sTradeExtras[i].owned != nullptr && sTradeExtras[i].owned()) ? 1 : 0;
    }
    return n;
}

uint8_t TradeCycleStep(int dir) {
    uint8_t ids[ITEM_MASK_TRUTH - ITEM_WEIRD_EGG + 1 + kMaxTradeExtras];
    uint8_t owned[sizeof(ids)];
    const int n = BuildTradeList(ids, owned, (int)sizeof(ids));
    const uint8_t current = (uint8_t)INV_CONTENT(ITEM_TRADE_CHILD);
    // A slot value outside the list starts where the contiguous cycle started
    // it: its u8 distance from the Weird Egg (ShuffleTradeItems.c).
    int currentIndex = (uint8_t)(current - ITEM_WEIRD_EGG);
    for (int i = 0; i < n; i++) {
        if (ids[i] == current) {
            currentIndex = i;
            break;
        }
    }
    const int next = Combo_ListCycle_Step(owned, n, currentIndex, dir);
    return next < 0 ? (uint8_t)ITEM_NONE : ids[next];
}

void ForeignMaskOnPlayerUpdate() {
    if (Context_GetCurrentGame() != GAME_OOT || OoT_gPlayState == nullptr) {
        return;
    }
    OoT_ForeignMask_ApplyRules(GET_PLAYER(OoT_gPlayState));
}

void ForeignMaskOnPlayerInit(void* actorRef) {
    if (Context_GetCurrentGame() != GAME_OOT) {
        return;
    }
    OoT_ForeignMask_OnPlayerInit(static_cast<Player*>(actorRef),
                                 CVarGetInteger(CVAR_ENHANCEMENT("PersistentMasks"), 0) != 0);
}

} // namespace

extern "C" uint8_t OoT_ForeignMask_Worn(void) {
    const uint8_t stored = gSaveContext.ship.foreignMask;
    return Combo_ForeignMask_Wearable(stored, GAME_OOT) ? stored : (uint8_t)COMBO_FOREIGN_MASK_NONE;
}

extern "C" int OoT_ForeignMask_Use(Player* player, uint8_t mask) {
    if (!IsChild() || !Combo_ForeignMask_Wearable(mask, GAME_OOT)) {
        return 0;
    }
    const uint8_t before = OoT_ForeignMask_Worn();
    const uint8_t after = Combo_ForeignMask_Toggle(before, mask, GAME_OOT);
    if (after == before) {
        return 0;
    }
    gSaveContext.ship.foreignMask = after;
    if (after != COMBO_FOREIGN_MASK_NONE) {
        if (player != nullptr) {
            player->currentMask = PLAYER_MASK_NONE;
        }
        gSaveContext.ship.maskMemory = PLAYER_MASK_NONE;
    }
    return 1;
}

extern "C" void OoT_ForeignMask_ApplyRules(Player* player) {
    const uint8_t stored = gSaveContext.ship.foreignMask;
    uint8_t worn = OoT_ForeignMask_Worn();
    if (worn != COMBO_FOREIGN_MASK_NONE &&
        (!IsChild() || (player != nullptr && player->currentMask != PLAYER_MASK_NONE))) {
        worn = COMBO_FOREIGN_MASK_NONE;
    }
    if (worn != stored) {
        gSaveContext.ship.foreignMask = worn;
    }
}

extern "C" void OoT_ForeignMask_OnPlayerInit(Player* player, int persistentMasks) {
    if (!persistentMasks || INV_CONTENT(ITEM_TRADE_CHILD) == ITEM_SOLD_OUT) {
        gSaveContext.ship.foreignMask = COMBO_FOREIGN_MASK_NONE;
    }
    OoT_ForeignMask_ApplyRules(player);
}

extern "C" void OoT_ForeignMask_DrawOnHead(PlayState* play, Player* player, int headDrawn) {
    // Dark Link (ovl_En_Torch2) draws through the same post-limb draw with a
    // Player of its own: only the real player wears the worn field.
    if (play == nullptr || player == nullptr || player != GET_PLAYER(play)) {
        return;
    }
    const char* dl = OoT_ForeignMask_WornDl(player, headDrawn);
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(OoT_ForeignMask_Worn());
    if (dl == nullptr || info == nullptr || !PathMounted(dl)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    if (info->bindsSegment9) {
        Gfx* noGlow = (Gfx*)Graph_Alloc(play->state.gfxCtx, 2 * sizeof(Gfx));
        Gfx* g = noGlow;
        gDPSetEnvColor(g++, 0, 0, 0, 255);
        gSPEndDisplayList(g++);
        gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)noGlow);
    }
    // The head list may leave another limb's matrix loaded: load the head
    // limb's own from the CPU stack.
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" uint8_t OoT_ChildTradeCycle_Next(void) {
    return TradeCycleStep(1);
}

extern "C" uint8_t OoT_ChildTradeCycle_Prev(void) {
    return TradeCycleStep(-1);
}

extern "C" int OoT_ChildTradeCycle_RegisterExtra(uint8_t itemId, OoTChildTradeOwnedFn owned) {
    if (sTradeExtraCount >= kMaxTradeExtras || (itemId >= ITEM_WEIRD_EGG && itemId <= ITEM_MASK_TRUTH)) {
        return 0;
    }
    for (int i = 0; i < sTradeExtraCount; i++) {
        if (sTradeExtras[i].itemId == itemId) {
            return 0;
        }
    }
    sTradeExtras[sTradeExtraCount++] = { itemId, owned };
    return 1;
}

/** TEST ONLY (the OoTChildTradeCycle row): forget every registered extra. */
extern "C" void OoT_ChildTradeCycle_ClearExtrasForTest(void) {
    sTradeExtras = {};
    sTradeExtraCount = 0;
}

extern "C" const char* OoT_ForeignMask_WornDl(Player* player, int headDrawn) {
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(OoT_ForeignMask_Worn());
    if (info == nullptr || player == nullptr || !headDrawn || !IsChild() || player->currentMask != PLAYER_MASK_NONE) {
        return nullptr;
    }
    return info->wornDl;
}

extern "C" uint8_t OoT_ForeignMask_StoredRaw(void) {
    return gSaveContext.ship.foreignMask;
}

extern "C" void OoT_ForeignMask_SetStoredRaw(uint8_t value) {
    gSaveContext.ship.foreignMask = value;
}

extern "C" size_t OoT_ForeignMask_FieldOffset(void) {
    return offsetof(SaveContext, ship) + offsetof(ShipSaveContextData, foreignMask);
}

// Always registered: the hooks act only on a worn field, which nothing but the
// items P4 adds (or a test) writes.
static void RegisterForeignMasksOoT() {
    COND_HOOK(OnPlayerUpdate, true, ForeignMaskOnPlayerUpdate);
    COND_ID_HOOK(OnActorInit, ACTOR_PLAYER, true, ForeignMaskOnPlayerInit);
}

static RegisterShipInitFunc initFunc(RegisterForeignMasksOoT);

#endif // RSBS_SINGLE_EXECUTABLE
