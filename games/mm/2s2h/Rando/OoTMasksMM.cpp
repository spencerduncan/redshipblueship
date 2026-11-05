/**
 * @file OoTMasksMM.cpp
 * @brief Ocarina of Time's Gerudo, Skull and Spooky Masks as Majora's Mask items
 *        (#980, epic #975 P5; OoTMM gerudoMaskMm, skullMaskMm, spookyMaskMm and
 *        their shared variants sharedMaskGerudo, sharedMaskSkull,
 *        sharedMaskSpooky). The MM direction of P4's pattern (BlastMaskOoT.cpp,
 *        StoneMaskOoT.cpp).
 *
 * WHERE IT LIVES. In 2ship_rando (WHOLE_ARCHIVE in single-exe), beside the P3
 * groundwork (ForeignMaskMM.cpp): MM's Enhancements/Masks/*.cpp is link-elided.
 *
 * THE ITEMS. Under the frozen Gerudo / Skull / Spooky Mask (MM) rule
 * (RSBS_XITEM_GERUDO_MASK_MM ..., a Cross-Game Rules row each, off by default)
 * MM's pool gains RI_MASK_GERUDO / RI_MASK_SKULL / RI_MASK_SPOOKY
 * (Logic/GeneratePools.cpp), ids APPENDED before RI_MAX_TRAP (Types.h). Each is
 * named as OoTMM names it ("Gerudo Mask (MM)", StaticData/Items.cpp). Its give
 * sets an OWNED BIT (GiveItem.cpp), and a second copy converts to junk
 * (ConvertItem.cpp).
 *
 * WHERE OWNERSHIP IS STORED. In the high nibble of the shared pause slot's state
 * byte, `rando.foreignMaskSlotSel[slot - 1]` (foreign_masks.h,
 * Combo_ForeignMask_SlotOwnedBit): the Gerudo and Skull Masks share the Stone
 * Mask's slot, the Spooky Mask the Gibdo Mask's (OoTMM's kMmStoneGerudoSkull and
 * kMmGibdoSpooky). Not a RANDO_INF flag: MM's randoInf array holds 78 flags in 80
 * bits, and a third flag would grow it by one u16, shifting every later field of
 * RandoSaveInfo (randoSaveChecks included) in every frozen world. Not a new field
 * either: RandoSaveInfo has no tail padding left, so one more byte grows Save and
 * shifts the raw MM tier after it. The state byte's high nibble is zero in every
 * existing world, because nothing could own or choose these masks before now.
 *
 * WEARING THEM is P3's (#977, ForeignMaskMM.cpp): the worn field, the head draw,
 * the shared pause slots, the C button. This file registers MM's ownership
 * source (Combo_ForeignMask_SetOwnedSource), which turns all of it on for a mask
 * the frozen world added and the player holds. A shared slot whose own mask the
 * player does not hold now shows the OoT mask instead of an empty cell
 * (MM_ForeignMask_PageCellItem).
 *
 * WHAT THEY DO (OoTMM's MM runtime, packages/generator/src/mm/actors). Human form
 * only, as every worn OoT mask in MM:
 *  - Gerudo Mask: the Pirates' Fortress purple guards (En_Ge2) treat Link as
 *    wearing the Stone Mask (En_Ge2.c: Player_IsWearingCustomGerudoMask beside
 *    PLAYER_MASK_STONE). Aveil (En_Ge3) and the pirate fighters (En_Kaizoku) are
 *    not fooled, as in OoTMM.
 *  - Skull Mask: Keese, Guays and Bad Bats (skullmask_enemies.c) and the Deku
 *    Palace garden guards (En_Look_Nuts.c) treat Link as Stone-masked; the
 *    palace's entrance guards (En_Guard_Nuts.c) and throne-room guards
 *    (En_Dns.c) treat him as a Deku.
 *  - Spooky Mask: the Gibdo Mask, at night only (Player.c,
 *    Player_GetCustomEffectiveMask). OoTMM spoofs currentMask for the whole frame;
 *    here only the actors whose behaviour the Gibdo Mask changes and the logic
 *    reads see it: the Gibdos (En_Railgibud, En_Talk_Gibud, En_Rd) and the Poe
 *    collector (En_Gb2).
 * Each actor's update runs with Link's currentMask (or transformation) replaced
 * by what it reads, put back as soon as the update returns: replaced in the
 * actor's own ShouldActorUpdate, put back in its OnActorUpdate. A hook that keeps
 * the update from running leaves no OnActorUpdate, so the player's own
 * ShouldActorUpdate and ShouldActorDraw put it back too: nothing outside the
 * listed update ever reads a foreign value, and Link's draw never does.
 * currentMask only ever holds MM's own Stone or Gibdo Mask, never a foreign id
 * (the epic's design rule 4).
 *
 * THE LOGIC (Logic.h, MM_OoTMask_Usable): OoTMM's can_enter_deku_palace
 * (Regions/South.cpp), can_evade_gerudo where MM's logic credits the Stone Mask
 * against the purple guards (Regions/PiratesFortress.cpp), and can_pass_gibdo's
 * night term (Regions/BeneathTheWell.cpp, Regions/East.cpp). The masks do not
 * count toward the Moon's mask total (MoonMaskCount): the Moon children's
 * runtime counts MM's own mask slots.
 *
 * THE SHARED VARIANTS are the bag's (combo_logic.h, THE SHARED PAIR): MM's copy
 * registers here and OoT's (RG_GERUDO_MASK ...) in OoT's GameExports_SingleExe.cpp,
 * both at the shared rule's family bit.
 *
 * WHAT XW5 (the six MM warp-song items) CAN REUSE: the appended-id rows
 * (Types.h, Items.cpp, GiveItem.cpp, ConvertItem.cpp, GeneratePools.cpp), the
 * get-item draw of the other game's own model (MM_ForeignModel_DrawForeignItem,
 * ForeignModelSingleExe.cpp), the icon fallback (MM_OoTMask_IconPath) and the
 * runtime registration shape (MM_OoTMasks_RegisterRuntime). NOT the ownership
 * storage: the state byte has one free bit, so six more owned flags need their own
 * home (randoInf has two free bits before it grows; see above).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstring>
#include <string>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "Rando/Rando.h"
#include "Rando/ForeignMaskMM.h"
#include "Rando/OoTMasksMM.h"

#include "context.h"          // src/common: Context_GetCurrentGame
#include "foreign_items.h"    // src/common: the rule bits, Combo_ComboCrossItemRule, Combo_GetForeignItemByNameFor
#include "foreign_masks.h"    // src/common: the worn-mask table, the slot state byte
#include "shared_resources.h" // src/common: the shared-family registry

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
// ForeignModelSingleExe.cpp (#980): the other game's item model, MM as the host.
int MM_ForeignModel_DrawForeignItem(PlayState* play, uint8_t originGame, uint16_t id);
}

namespace {

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

/** The shared slot state byte that holds `riId`'s owned bit, or nullptr. */
uint8_t* SlotState(uint16_t riId) {
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(MM_OoTMask_ForeignId(riId));
    if (info == nullptr || info->mmSharedSlot == COMBO_FOREIGN_MASK_SLOT_NONE ||
        info->mmSharedSlot >= COMBO_FOREIGN_MASK_SLOT_COUNT) {
        return nullptr;
    }
    return &gSaveContext.save.shipSaveInfo.rando.foreignMaskSlotSel[info->mmSharedSlot - 1];
}

/** The native mask whose pause slot `riId` shares: MM's Stone or Gibdo Mask. */
ItemId SlotNativeItem(uint16_t riId) {
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(MM_OoTMask_ForeignId(riId));
    return (info != nullptr && info->mmSharedSlot == COMBO_FOREIGN_MASK_SLOT_MM_GIBDO) ? ITEM_MASK_GIBDO
                                                                                       : ITEM_MASK_STONE;
}

/** Combo_ForeignMask_Owned's source for MM: the owned bit of the mask's item. */
int OwnedSource(uint8_t id) {
    return MM_OoTMask_Owned(MM_OoTMask_ItemId(id)) ? 1 : 0;
}

// The shared families' MM members: held and given through the owned bit, pooled
// exactly when the extension rule put the item in MM's pool.
bool SharedHeld(uint16_t nativeId) {
    return MM_OoTMask_Owned(nativeId);
}
void SharedGive(uint16_t nativeId) {
    MM_OoTMask_Give(nativeId);
}
bool SharedPooled(uint16_t nativeId) {
    return MM_OoTMask_RuleOn(nativeId);
}

/** What MM_OoTMasks_BeforeActorUpdate replaced, and the true values. */
struct Held {
    Player* player;
    bool maskSet;
    bool formSet;
    uint8_t mask;
    uint8_t form;
};
Held sHeld = {};

/** OoTMM's Player_IsNightTime: 18:00 to 06:00. */
int IsNightNow() {
    const u16 time = gSaveContext.save.time;
    return (time >= CLOCK_TIME(18, 0) || time < CLOCK_TIME(6, 0)) ? 1 : 0;
}

Player* LivePlayer() {
    return (Context_GetCurrentGame() == GAME_MM && MM_gPlayState != nullptr) ? GET_PLAYER(MM_gPlayState) : nullptr;
}

} // namespace

extern "C" uint8_t MM_OoTMask_ForeignId(uint16_t riId) {
    switch (riId) {
        case (uint16_t)RI_MASK_GERUDO:
            return COMBO_FOREIGN_MASK_OOT_GERUDO;
        case (uint16_t)RI_MASK_SKULL:
            return COMBO_FOREIGN_MASK_OOT_SKULL;
        case (uint16_t)RI_MASK_SPOOKY:
            return COMBO_FOREIGN_MASK_OOT_SPOOKY;
        default:
            return COMBO_FOREIGN_MASK_NONE;
    }
}

extern "C" uint16_t MM_OoTMask_ItemId(uint8_t foreignMask) {
    switch (foreignMask) {
        case COMBO_FOREIGN_MASK_OOT_GERUDO:
            return (uint16_t)RI_MASK_GERUDO;
        case COMBO_FOREIGN_MASK_OOT_SKULL:
            return (uint16_t)RI_MASK_SKULL;
        case COMBO_FOREIGN_MASK_OOT_SPOOKY:
            return (uint16_t)RI_MASK_SPOOKY;
        default:
            return (uint16_t)RI_UNKNOWN;
    }
}

extern "C" bool MM_OoTMask_RuleOn(uint16_t riId) {
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(MM_OoTMask_ForeignId(riId));
    return info != nullptr && Combo_ComboCrossItemRule(info->ruleBit);
}

extern "C" bool MM_OoTMask_Owned(uint16_t riId) {
    const uint8_t* state = SlotState(riId);
    const uint8_t bit = Combo_ForeignMask_SlotOwnedBit(MM_OoTMask_ForeignId(riId));
    return state != nullptr && bit != 0u && (*state & bit) != 0u;
}

extern "C" void MM_OoTMask_Give(uint16_t riId) {
    uint8_t* state = SlotState(riId);
    const uint8_t bit = Combo_ForeignMask_SlotOwnedBit(MM_OoTMask_ForeignId(riId));
    if (state != nullptr && bit != 0u) {
        *state = (uint8_t)(*state | bit);
    }
}

extern "C" bool MM_OoTMask_Usable(uint16_t riId) {
    return MM_OoTMask_RuleOn(riId) && MM_OoTMask_Owned(riId);
}

extern "C" int MM_OoTMasks_SpoofFor(uint8_t worn, int16_t actorId, int isNight, uint8_t* maskOut, uint8_t* formOut) {
    uint8_t mask = 0xFF;
    uint8_t form = 0xFF;
    switch (worn) {
        case COMBO_FOREIGN_MASK_OOT_GERUDO:
            if (actorId == ACTOR_EN_GE2) {
                mask = PLAYER_MASK_STONE;
            }
            break;
        case COMBO_FOREIGN_MASK_OOT_SKULL:
            if (actorId == ACTOR_EN_LOOK_NUTS || actorId == ACTOR_EN_FIREFLY || actorId == ACTOR_EN_CROW ||
                actorId == ACTOR_EN_BAT) {
                mask = PLAYER_MASK_STONE;
            } else if (actorId == ACTOR_EN_GUARD_NUTS || actorId == ACTOR_EN_DNS) {
                form = PLAYER_FORM_DEKU;
            }
            break;
        case COMBO_FOREIGN_MASK_OOT_SPOOKY:
            if (isNight && (actorId == ACTOR_EN_RAILGIBUD || actorId == ACTOR_EN_TALK_GIBUD || actorId == ACTOR_EN_RD ||
                            actorId == ACTOR_EN_GB2)) {
                mask = PLAYER_MASK_GIBDO;
            }
            break;
        default:
            break;
    }
    if (maskOut != nullptr) {
        *maskOut = mask;
    }
    if (formOut != nullptr) {
        *formOut = form;
    }
    return (mask != 0xFF || form != 0xFF) ? 1 : 0;
}

extern "C" int MM_OoTMasks_BeforeActorUpdate(Player* player, int16_t actorId, int updates, int isNight) {
    MM_OoTMasks_PutBack(player);
    if (player == nullptr || updates == 0 || player->transformation != PLAYER_FORM_HUMAN ||
        player->currentMask != PLAYER_MASK_NONE) {
        return 0;
    }
    uint8_t mask = 0xFF;
    uint8_t form = 0xFF;
    if (!MM_OoTMasks_SpoofFor(MM_ForeignMask_Worn(), actorId, isNight, &mask, &form)) {
        return 0;
    }
    sHeld.player = player;
    if (mask != 0xFF) {
        sHeld.maskSet = true;
        sHeld.mask = player->currentMask;
        player->currentMask = mask;
    }
    if (form != 0xFF) {
        sHeld.formSet = true;
        sHeld.form = player->transformation;
        player->transformation = form;
    }
    return 1;
}

extern "C" void MM_OoTMasks_PutBack(Player* player) {
    if (sHeld.player != nullptr && sHeld.player == player) {
        if (sHeld.maskSet) {
            player->currentMask = sHeld.mask;
        }
        if (sHeld.formSet) {
            player->transformation = sHeld.form;
        }
    }
    sHeld = {};
}

extern "C" void MM_OoTMasks_Forget(void) {
    sHeld = {};
}

extern "C" int MM_OoTMasks_Holding(void) {
    return sHeld.player != nullptr ? 1 : 0;
}

extern "C" void MM_OoTMasks_RegisterRuntime(void) {
    Combo_ForeignMask_SetOwnedSource(GAME_MM, OwnedSource);
    static const struct {
        uint32_t family;
        RandoItemId item;
    } kShared[] = {
        { RSBS_XITEM_SHARED_GERUDO, RI_MASK_GERUDO },
        { RSBS_XITEM_SHARED_SKULL, RI_MASK_SKULL },
        { RSBS_XITEM_SHARED_SPOOKY, RI_MASK_SPOOKY },
    };
    for (const auto& s : kShared) {
        const ComboSharedFamilyMember member = { (uint16_t)s.item, SharedHeld, SharedGive, SharedPooled };
        Combo_RegisterSharedFamilyMember(GAME_MM, s.family, &member);
    }
}

extern "C" const char* MM_OoTMask_IconPath(uint16_t riId) {
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(MM_OoTMask_ForeignId(riId));
    if (info == nullptr) {
        return nullptr;
    }
    if (PathMounted(info->icon)) {
        return info->icon;
    }
    return (const char*)MM_gItemIcons[SlotNativeItem(riId)];
}

extern "C" void MM_OoTMask_DrawGetItem(uint16_t riId) {
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(MM_OoTMask_ForeignId(riId));
    if (info == nullptr || MM_gPlayState == nullptr) {
        return;
    }
    // OoT's own get-item model, by the name OoT's item table gives it (the
    // worn-mask table's name), from the co-mounted archives.
    SharedItem oot;
    if (Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, info->name, &oot) &&
        MM_ForeignModel_DrawForeignItem(MM_gPlayState, oot.originGame, oot.id) == 1) {
        return;
    }
    MM_GetItem_Draw(MM_gPlayState, SlotNativeItem(riId) == ITEM_MASK_GIBDO ? GID_MASK_GIBDO : GID_MASK_STONE);
}

namespace {

// The actors that read a worn OoT mask (MM_OoTMasks_SpoofFor).
const int16_t kListedActors[] = {
    ACTOR_EN_GE2, ACTOR_EN_LOOK_NUTS, ACTOR_EN_FIREFLY,    ACTOR_EN_CROW, ACTOR_EN_BAT, ACTOR_EN_GUARD_NUTS,
    ACTOR_EN_DNS, ACTOR_EN_RAILGIBUD, ACTOR_EN_TALK_GIBUD, ACTOR_EN_RD,   ACTOR_EN_GB2,
};
constexpr size_t kListedCount = sizeof(kListedActors) / sizeof(kListedActors[0]);

void BeforeListedUpdate(Actor* actor, bool* should) {
    Player* player = LivePlayer();
    if (player == nullptr || actor == nullptr) {
        return;
    }
    MM_OoTMasks_BeforeActorUpdate(player, actor->id, (should == nullptr || *should) ? 1 : 0, IsNightNow());
}

void AfterListedUpdate(Actor* actor) {
    (void)actor;
    MM_OoTMasks_PutBack(LivePlayer());
}

void RegisterOoTMasksMM() {
    MM_OoTMasks_RegisterRuntime();
    // One registration per listed actor id. Not COND_ID_HOOK in a loop: its
    // hook id is one static per call site, so each pass would unregister the
    // last one's.
    static HOOK_ID sBeforeHooks[kListedCount] = {};
    static HOOK_ID sAfterHooks[kListedCount] = {};
    for (size_t i = 0; i < kListedCount; i++) {
        S2H::GameHooks::UnregisterForID<GameInteractor::ShouldActorUpdate>(sBeforeHooks[i]);
        S2H::GameHooks::UnregisterForID<GameInteractor::OnActorUpdate>(sAfterHooks[i]);
        sBeforeHooks[i] =
            S2H::GameHooks::RegisterForID<GameInteractor::ShouldActorUpdate>(kListedActors[i], BeforeListedUpdate);
        sAfterHooks[i] =
            S2H::GameHooks::RegisterForID<GameInteractor::OnActorUpdate>(kListedActors[i], AfterListedUpdate);
    }
    // A listed update another hook kept from running leaves no OnActorUpdate:
    // put the value back before Link's own update and before his draw.
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_PLAYER, true, [](Actor* actor, bool* should) {
        (void)should;
        MM_OoTMasks_PutBack((Player*)actor);
    });
    COND_ID_HOOK(ShouldActorDraw, ACTOR_PLAYER, true, [](Actor* actor, bool* should) {
        (void)should;
        MM_OoTMasks_PutBack((Player*)actor);
    });
    COND_HOOK(OnPlayDestroy, true, []() { MM_OoTMasks_Forget(); });
    // MM takes its own mask off when the moon falls (z_sram_NES.c); a worn OoT
    // mask comes off with it.
    COND_HOOK(BeforeMoonCrashSaveReset, true, []() { MM_ForeignMask_SetStoredRaw(COMBO_FOREIGN_MASK_NONE); });
    COND_HOOK(OnSaveLoad, true, [](s16 fileNum) {
        (void)fileNum;
        MM_OoTMasks_Forget();
        MM_OoTMasks_RegisterRuntime();
    });
}

// Static-init registration of the ownership source and the shared families (the
// shape of GameExports_SingleExe.cpp's registrars): the bag reads the families at
// a paired creation, which can run before MM's ShipInit registrars ever have. Both
// registries are zero-initialised storage, so the order against other static
// constructors does not matter.
struct OoTMasksMMRegistrar {
    OoTMasksMMRegistrar() {
        MM_OoTMasks_RegisterRuntime();
    }
};
const OoTMasksMMRegistrar gOoTMasksMMRegistrar;

} // namespace

// Always registered: every hook acts only on a worn OoT mask, which nothing but a
// frozen extension rule and an owned item put on.
static RegisterShipInitFunc initFunc(RegisterOoTMasksMM, {});

#endif // RSBS_SINGLE_EXECUTABLE
