/**
 * @file ForeignMaskMM.cpp
 * @brief MM wears and draws an Ocarina of Time mask (#977, epic #975 P3).
 *
 * WHERE IT LIVES. In 2ship_rando, which single-exe links WHOLE_ARCHIVE, not
 * beside 2S2H's Enhancements/Masks/PersistentMasks.cpp (its near-complete
 * template): 2ship_enh is a plain static archive and none of
 * Enhancements/Masks/*.cpp reaches redship.map (games/mm/CMakeLists.txt).
 *
 * THE FIELD. `gSaveContext.save.shipSaveInfo.rando.foreignMask` holds the worn
 * mask as a ComboForeignMask (src/common/foreign_masks.h), never currentMask or
 * equippedMask: MM indexes sMaskObjectIds, sMaskItemIds and D_801C0B20 by both
 * with no bound (z_player_lib.c). Every reader goes through
 * Combo_ForeignMask_Sanitize. It is appended to RandoSaveInfo, inside Save, so
 * it crosses with the frozen MM half, rides the .redsave's Tier 3, and stays
 * on through scene changes the way equippedMask does.
 *
 * MM'S MASK RULES, applied to it as MM applies them to its own:
 *  - human form only: transforming takes it off (MM_ForeignMask_ApplyRules),
 *    and it cannot be put on in another form (MM_ForeignMask_Use);
 *  - one mask at a time: a native mask put on takes it off, and putting it on
 *    takes the native mask off (currentMask and equippedMask to NONE).
 *
 * THE PAUSE SLOTS (OoTMM's sharing: Stone/Gerudo/Skull share Stone's slot,
 * Gibdo/Spooky share Gibdo's). MM's inventory keeps the slot's own item, so
 * the C button, the item tables and the save all keep an MM ItemId; the slot's
 * chosen variant is `rando.foreignMaskSlotSel[slot - 1]` (NONE: the slot's own
 * mask). On the mask page, A on a shared slot with more than one variant
 * cycles it, as KaleidoItemPage.cpp's short-circuit does for two-item trade
 * slots, and the page draws the chosen variant's icon (one guarded call in
 * z_kaleido_mask.c). Using the slot's C button while a foreign variant is
 * chosen puts that mask on or takes it off (VB_USE_ITEM_EQUIP_MASK) instead of
 * the slot's own. Ownership is P5's (#980): until it registers a source,
 * nothing is owned, every slot has one variant, and the page is unchanged.
 * Still P5's: an owned OoT mask whose slot's own mask is not owned has no cell
 * to select (the cell is empty), the C button shows the slot's own icon, and
 * the item description names the slot's own mask.
 *
 * THE DRAW. OnPlayerPostLimbDraw on PLAYER_LIMB_HEAD, where MM draws its own
 * masks. Each OoT list loads the head matrix itself from segment 0x0D + 0x1C0
 * (OoT's sMaskDlists); MM's flex skeleton draw binds segment 0x0D to its limb
 * matrices the same way (z_skelanime.c), and both child skeletons put the head
 * at that index. The list is named by its path in redship-mm.o2r, the curated
 * OoT half MM mounts, never through oot.o2r's (#981).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "Rando/Rando.h"

#include "context.h"                // src/common: Context_GetCurrentGame
#include "integration_test_hooks.h" // src/common: the playtest drive's capture and fail
#include "foreign_masks.h"          // src/common: the table and the equip rules

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
#include "z64skin_matrix.h"
void func_8082E1F0(Player* player, u16 sfxId);
// soh/Enhancements/randomizer/ForeignMaskOoT.cpp: the worn mask in OoT's frozen half.
uint8_t OoT_ForeignMask_FrozenWorn(void);
}

static_assert(sizeof(((RandoSaveInfo*)0)->foreignMaskSlotSel) == COMBO_FOREIGN_MASK_SLOT_COUNT - 1,
              "one persisted variant per shared pause slot");

namespace {

// Draws per mask since boot, read by the integration leg (#977). Never reset.
std::array<uint32_t, COMBO_FOREIGN_MASK_COUNT> sDrawCounts{};

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

RandoSaveInfo& Rando() {
    return gSaveContext.save.shipSaveInfo.rando;
}

/** The shared slot an MM item heads, or SLOT_NONE. */
uint8_t SlotForItem(int item) {
    switch (item) {
        case ITEM_MASK_STONE:
            return COMBO_FOREIGN_MASK_SLOT_MM_STONE;
        case ITEM_MASK_GIBDO:
            return COMBO_FOREIGN_MASK_SLOT_MM_GIBDO;
        default:
            return COMBO_FOREIGN_MASK_SLOT_NONE;
    }
}

/** The shared slot an MM mask heads, or SLOT_NONE. */
uint8_t SlotForMask(int maskId) {
    switch (maskId) {
        case PLAYER_MASK_STONE:
            return COMBO_FOREIGN_MASK_SLOT_MM_STONE;
        case PLAYER_MASK_GIBDO:
            return COMBO_FOREIGN_MASK_SLOT_MM_GIBDO;
        default:
            return COMBO_FOREIGN_MASK_SLOT_NONE;
    }
}

int NativeItemOfSlot(uint8_t slot) {
    return slot == COMBO_FOREIGN_MASK_SLOT_MM_STONE ? ITEM_MASK_STONE : ITEM_MASK_GIBDO;
}

bool NativeOwned(uint8_t slot) {
    const int item = NativeItemOfSlot(slot);
    return INV_CONTENT(item) == item;
}

} // namespace

extern "C" uint8_t MM_ForeignMask_Worn(void) {
    return Combo_ForeignMask_Sanitize(Rando().foreignMask, GAME_MM);
}

/**
 * Put `mask` on (or take it off when worn): human form only. Putting one on
 * takes the native mask off. 1 when the worn field changed, else 0.
 */
extern "C" int MM_ForeignMask_Use(Player* player, uint8_t mask) {
    if (player == nullptr || player->transformation != PLAYER_FORM_HUMAN) {
        return 0;
    }
    const uint8_t before = MM_ForeignMask_Worn();
    const uint8_t after = Combo_ForeignMask_Toggle(before, mask, GAME_MM);
    if (after == before) {
        return 0;
    }
    Rando().foreignMask = after;
    if (after != COMBO_FOREIGN_MASK_NONE) {
        player->prevMask = player->currentMask;
        player->currentMask = PLAYER_MASK_NONE;
        gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
    }
    return 1;
}

/** MM's mask rules on the worn field, once per player update. */
extern "C" void MM_ForeignMask_ApplyRules(Player* player) {
    const uint8_t stored = Rando().foreignMask;
    uint8_t worn = Combo_ForeignMask_Sanitize(stored, GAME_MM);
    if (worn != COMBO_FOREIGN_MASK_NONE && player != nullptr &&
        (player->transformation != PLAYER_FORM_HUMAN || player->currentMask != PLAYER_MASK_NONE)) {
        worn = COMBO_FOREIGN_MASK_NONE;
    }
    if (worn != stored) {
        Rando().foreignMask = worn;
    }
}

/** A shared slot's chosen variant, sanitized: an owned OoT mask sharing it, or NONE (the slot's own mask). */
extern "C" uint8_t MM_ForeignMask_SlotSelection(uint8_t slot) {
    if (slot == COMBO_FOREIGN_MASK_SLOT_NONE || slot >= COMBO_FOREIGN_MASK_SLOT_COUNT) {
        return COMBO_FOREIGN_MASK_NONE;
    }
    const uint8_t sel = Rando().foreignMaskSlotSel[slot - 1];
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(sel);
    if (info == nullptr || info->host != GAME_MM || info->mmSharedSlot != slot ||
        !Combo_ForeignMask_Owned(GAME_MM, sel)) {
        return COMBO_FOREIGN_MASK_NONE;
    }
    return sel;
}

/** Cycle a shared slot's variant (dir > 0 forward). 1 when it changed. */
extern "C" int MM_ForeignMask_CycleSlot(uint8_t slot, int dir) {
    if (slot == COMBO_FOREIGN_MASK_SLOT_NONE || slot >= COMBO_FOREIGN_MASK_SLOT_COUNT) {
        return 0;
    }
    const uint8_t current = MM_ForeignMask_SlotSelection(slot);
    const uint8_t next = Combo_ForeignMask_SlotCycle(slot, current, NativeOwned(slot) ? 1 : 0, dir);
    Rando().foreignMaskSlotSel[slot - 1] = next;
    return next != current;
}

/** How many variants a shared slot cycles through (1 or 0: no cycling). */
extern "C" int MM_ForeignMask_SlotVariantCount(uint8_t slot) {
    uint8_t variants[COMBO_FOREIGN_MASK_SLOT_MAX_VARIANTS];
    return Combo_ForeignMask_SlotVariants(slot, NativeOwned(slot) ? 1 : 0, variants,
                                          COMBO_FOREIGN_MASK_SLOT_MAX_VARIANTS);
}

/**
 * VB_USE_ITEM_EQUIP_MASK's question: the player used the C button of native
 * mask `maskId`. When its shared slot has a foreign variant chosen, that mask
 * is used instead and this answers 1 (the vanilla equip must not run); else 0.
 */
extern "C" int MM_ForeignMask_OnEquipNative(Player* player, int maskId) {
    const uint8_t sel = MM_ForeignMask_SlotSelection(SlotForMask(maskId));
    if (sel == COMBO_FOREIGN_MASK_NONE) {
        return 0;
    }
    MM_ForeignMask_Use(player, sel);
    return 1;
}

/**
 * The mask page's icon for cell `maskSlot` (0-based on the page), whose own
 * item's icon is `nativeIcon`: the chosen variant's icon on a shared slot, when
 * redship-mm.o2r holds it; `nativeIcon` otherwise. Called by z_kaleido_mask.c.
 */
extern "C" const char* MM_ForeignMask_PageIcon(int maskSlot, const char* nativeIcon) {
    if (maskSlot < 0 || maskSlot >= MASK_NUM_SLOTS) {
        return nativeIcon;
    }
    const uint8_t slot = SlotForItem(gSaveContext.save.saveInfo.inventory.items[maskSlot + ITEM_NUM_SLOTS]);
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(MM_ForeignMask_SlotSelection(slot));
    if (info == nullptr || !PathMounted(info->icon)) {
        return nativeIcon;
    }
    return info->icon;
}

/** The list MM's head draw would emit for the live worn field, or NULL. */
extern "C" const char* MM_ForeignMask_WornDl(Player* player) {
    const uint8_t worn = MM_ForeignMask_Worn();
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(worn);
    if (info == nullptr || player == nullptr || player->transformation != PLAYER_FORM_HUMAN ||
        player->currentMask != PLAYER_MASK_NONE) {
        return nullptr;
    }
    return info->wornDl;
}

/** Draws of mask `id` since boot (the ForeignMasksWorn integration leg). */
extern "C" uint32_t MM_ForeignMask_DrawCount(uint8_t id) {
    return id < COMBO_FOREIGN_MASK_COUNT ? sDrawCounts[id] : 0;
}

/**
 * The MM half of the IntForeignMasksWorn integration leg (#977), with
 * RSBS_GP_FOREIGN_MASKS=1, called with the MM window's live frame count and the
 * arrival number.
 *
 * ARRIVAL 1. Frame 5 requires OoT's frozen half to carry the Kamaro's Mask the
 * OoT window left on (the crossing froze OoT's field with its half). Link turns
 * to face the camera; frames 20, 45 and 70 put on the Gerudo, Skull and Spooky
 * Mask through the production equip path (MM_ForeignMask_Use); fifteen frames
 * later each is captured as foreign-mask-mm-<mask>.png and must have been
 * DRAWN from redship-mm.o2r. Frame 100 requires the three files. The Spooky
 * Mask stays on through the Clock Tower door.
 * ARRIVAL 2 (RSBS_GP_CYCLES=2). Frame 5 requires the Spooky Mask still worn:
 * MM's field went out with the frozen MM half, through OoT, and back. Frame 20
 * captures it as foreign-mask-mm-persisted.png and requires it drawn.
 * Returns 0 after failing the run.
 */
extern "C" int MM_ForeignMask_PlaytestFrame(int frame, int arrival) {
    static const uint8_t kMasks[3] = { COMBO_FOREIGN_MASK_OOT_GERUDO, COMBO_FOREIGN_MASK_OOT_SKULL,
                                       COMBO_FOREIGN_MASK_OOT_SPOOKY };
    static const char* const kNames[3] = { "foreign-mask-mm-gerudo", "foreign-mask-mm-skull",
                                           "foreign-mask-mm-spooky" };
    static uint32_t sCountAtWear[3];
    static uint32_t sCountAtArrival2;
    if (MM_gPlayState == nullptr) {
        return 1;
    }
    Player* player = GET_PLAYER(MM_gPlayState);
    Camera* cam = GET_ACTIVE_CAM(MM_gPlayState);
    if (frame >= 15 && frame <= 100 && player != nullptr && cam != nullptr) {
        const s16 yaw = MM_Math_Vec3f_Yaw(&player->actor.world.pos, &cam->eye);
        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = yaw;
    }
    char msg[256];
    if (arrival >= 2) {
        const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(COMBO_FOREIGN_MASK_OOT_SPOOKY);
        if (frame == 5) {
            fprintf(stderr, "[FOREIGN-MASK] MM arrival %d: worn field %u (stored %u), expected the %s\n", arrival,
                    (unsigned)MM_ForeignMask_Worn(), (unsigned)Rando().foreignMask, info->name);
            if (MM_ForeignMask_Worn() != COMBO_FOREIGN_MASK_OOT_SPOOKY) {
                snprintf(msg, sizeof(msg), "MM arrival %d lost the worn %s across the round trip (field %u)", arrival,
                         info->name, (unsigned)Rando().foreignMask);
                IntegrationTest_GameplayFail(msg);
                return 0;
            }
            sCountAtArrival2 = MM_ForeignMask_DrawCount(COMBO_FOREIGN_MASK_OOT_SPOOKY);
        } else if (frame == 20) {
            IntegrationTest_CaptureFrame("foreign-mask-mm-persisted");
            const uint32_t drawn = MM_ForeignMask_DrawCount(COMBO_FOREIGN_MASK_OOT_SPOOKY) - sCountAtArrival2;
            fprintf(stderr, "[FOREIGN-MASK] MM PASS: the %s persisted across the round trip and drew %u time(s)\n",
                    info->name, (unsigned)drawn);
            if (drawn == 0) {
                IntegrationTest_GameplayFail("MM kept the worn Spooky Mask across the round trip but never drew it");
                return 0;
            }
        }
        fflush(stderr);
        return 1;
    }
    if (frame == 5) {
        const uint8_t frozen = OoT_ForeignMask_FrozenWorn();
        fprintf(stderr, "[FOREIGN-MASK] MM arrival 1: OoT's frozen half carries worn mask %u (expected %u, Kamaro's)\n",
                (unsigned)frozen, (unsigned)COMBO_FOREIGN_MASK_MM_KAMARO);
        if (frozen != COMBO_FOREIGN_MASK_MM_KAMARO) {
            IntegrationTest_GameplayFail("the crossing did not freeze OoT's worn Kamaro's Mask with OoT's half");
            return 0;
        }
    }
    for (int k = 0; k < 3; k++) {
        const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(kMasks[k]);
        const int wearFrame = 20 + 25 * k;
        if (frame == wearFrame) {
            sCountAtWear[k] = MM_ForeignMask_DrawCount(kMasks[k]);
            if (!MM_ForeignMask_Use(player, kMasks[k]) || MM_ForeignMask_Worn() != kMasks[k]) {
                snprintf(msg, sizeof(msg), "MM refused to put on the %s (worn=%u, form=%d)", info->name,
                         (unsigned)MM_ForeignMask_Worn(), player != nullptr ? (int)player->transformation : -1);
                IntegrationTest_GameplayFail(msg);
                return 0;
            }
            fprintf(stderr, "[FOREIGN-MASK] MM frame %d: put on the %s\n", frame, info->name);
        } else if (frame == wearFrame + 15) {
            IntegrationTest_CaptureFrame(kNames[k]);
            const uint32_t drawn = MM_ForeignMask_DrawCount(kMasks[k]) - sCountAtWear[k];
            fprintf(stderr, "[FOREIGN-MASK] MM frame %d: the %s's worn list %s drew %u time(s) since it went on\n",
                    frame, info->name, info->wornDl, (unsigned)drawn);
            if (drawn == 0) {
                snprintf(msg, sizeof(msg), "MM never drew the worn %s (%s)", info->name, info->wornDl);
                IntegrationTest_GameplayFail(msg);
                return 0;
            }
        }
    }
    if (frame == 100) {
        const char* dir = std::getenv("RSBS_CAPTURE_OUT");
        for (int k = 0; dir != nullptr && k < 3; k++) {
            const std::filesystem::path png = std::filesystem::path(dir) / (std::string(kNames[k]) + ".png");
            std::error_code ec;
            if (!std::filesystem::exists(png, ec) || std::filesystem::file_size(png, ec) == 0) {
                snprintf(msg, sizeof(msg), "no capture of the worn mask at %s", png.string().c_str());
                IntegrationTest_GameplayFail(msg);
                return 0;
            }
        }
        fprintf(stderr, "[FOREIGN-MASK] MM PASS: the Gerudo, Skull and Spooky Mask were worn and drawn%s\n",
                dir != nullptr ? "; captures written" : "");
    }
    fflush(stderr);
    return 1;
}

namespace {

void DrawOnHead(Player* player, s32 limbIndex) {
    (void)limbIndex;
    if (Context_GetCurrentGame() != GAME_MM || MM_gPlayState == nullptr || player == nullptr ||
        player != GET_PLAYER(MM_gPlayState)) {
        return;
    }
    const char* dl = MM_ForeignMask_WornDl(player);
    if (dl == nullptr || !PathMounted(dl)) {
        return;
    }
    // MM's own first-person test for drawing masks (z_player.c, Player_Draw):
    // with the camera inside the head, no mask.
    if (player->stateFlags1 & PLAYER_STATE1_100000) {
        Vec3f projected;
        MM_SkinMatrix_Vec3fMtxFMultXYZ(&MM_gPlayState->viewProjectionMtxF, &player->actor.focus.pos, &projected);
        if (projected.z < -4.0f) {
            return;
        }
    }

    OPEN_DISPS(MM_gPlayState->state.gfxCtx);
    // The head list's geometry mode, as PersistentMasks.cpp restores it before
    // a mask list that does not set its own.
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_FOG | G_LIGHTING | G_SHADING_SMOOTH);
    MM_gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);
    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    sDrawCounts[MM_ForeignMask_Worn()]++;
}

void RegisterForeignMasksMM() {
    COND_ID_HOOK(OnPlayerPostLimbDraw, PLAYER_LIMB_HEAD, true, DrawOnHead);

    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, true, [](Actor* actor) {
        if (Context_GetCurrentGame() == GAME_MM) {
            MM_ForeignMask_ApplyRules((Player*)actor);
        }
    });

    COND_VB_SHOULD(VB_USE_ITEM_EQUIP_MASK, true, {
        PlayerMask* maskId = va_arg(args, PlayerMask*);
        if (MM_gPlayState == nullptr || maskId == nullptr) {
            return;
        }
        Player* player = GET_PLAYER(MM_gPlayState);
        const uint8_t before = MM_ForeignMask_Worn();
        if (MM_ForeignMask_OnEquipNative(player, (int)*maskId)) {
            *should = false;
            func_8082E1F0(player, MM_ForeignMask_Worn() != before && MM_ForeignMask_Worn() != COMBO_FOREIGN_MASK_NONE
                                      ? NA_SE_PL_CHANGE_ARMS
                                      : NA_SE_PL_TAKE_OUT_SHIELD);
        }
    });

    // A on a shared slot with a choice cycles it instead of showing the text.
    COND_VB_SHOULD(VB_KALEIDO_DISPLAY_ITEM_TEXT, true, {
        u16* cursorItem = va_arg(args, u16*);
        if (cursorItem == nullptr) {
            return;
        }
        const uint8_t slot = SlotForItem(*cursorItem);
        if (slot == COMBO_FOREIGN_MASK_SLOT_NONE || MM_ForeignMask_SlotVariantCount(slot) < 2) {
            return;
        }
        *should = false;
        MM_ForeignMask_CycleSlot(slot, 1);
        Audio_PlaySfx(NA_SE_SY_CURSOR);
    });
}

} // namespace

// Always registered: the hooks act only on a worn field or a slot choice,
// which nothing but the items P5 adds (or a test) writes.
static RegisterShipInitFunc initFunc(RegisterForeignMasksMM, {});

#endif // RSBS_SINGLE_EXECUTABLE
