#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "soh/ShipUtils.h"
#include "soh/Notification/Notification.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
extern PlayState* OoT_gPlayState;
GetItemEntry ItemTable_RetrieveEntry(s16 modIndex, s16 getItemID);
}

#define CVAR_EXTRA_TRAPS_NAME CVAR_ENHANCEMENT("ExtraTraps.Enabled")
#define CVAR_EXTRA_TRAPS_DEFAULT 0
#define CVAR_EXTRA_TRAPS_VALUE CVarGetInteger(CVAR_EXTRA_TRAPS_NAME, CVAR_EXTRA_TRAPS_DEFAULT)

typedef enum {
    ADD_ICE_TRAP,
    ADD_BURN_TRAP,
    ADD_SHOCK_TRAP,
    ADD_KNOCK_TRAP,
    ADD_SPEED_TRAP,
    ADD_BOMB_TRAP,
    ADD_VOID_TRAP,
    ADD_AMMO_TRAP,
    ADD_KILL_TRAP,
    ADD_TELEPORT_TRAP,
    // RedShipBlueShip (#1424): OoTMM's trap types SoH lacked (OoTMM
    // packages/generator/src/common/traps.c). APPEND ONLY: the roll picks from
    // the enabled types in this order, so a config without them rolls as before.
    ADD_DRAIN_TRAP,
    ADD_ANTIMAGIC_TRAP,
    ADD_RUPOOR_TRAP,
    ADD_TRAP_MAX
} AltTrapType;

static AltTrapType roll = ADD_TRAP_MAX;
static int statusTimer = -1;
static int eventTimer = -1;
// [RSBS #1397] The teleport trap's destination, drawn right after the type from
// the roll's own seeded state (the way SoH 40425c8c2 draws teleportRoll), so a
// seeded roll keeps its destination now that it no longer seeds rando_state.
static int teleportRoll = 0;

const char* altTrapTypeCvars[] = {
    CVAR_ENHANCEMENT("ExtraTraps.Ice"),    CVAR_ENHANCEMENT("ExtraTraps.Burn"),
    CVAR_ENHANCEMENT("ExtraTraps.Shock"),  CVAR_ENHANCEMENT("ExtraTraps.Knockback"),
    CVAR_ENHANCEMENT("ExtraTraps.Speed"),  CVAR_ENHANCEMENT("ExtraTraps.Bomb"),
    CVAR_ENHANCEMENT("ExtraTraps.Void"),   CVAR_ENHANCEMENT("ExtraTraps.Ammo"),
    CVAR_ENHANCEMENT("ExtraTraps.Kill"),   CVAR_ENHANCEMENT("ExtraTraps.Teleport"),
    CVAR_ENHANCEMENT("ExtraTraps.Drain"),  CVAR_ENHANCEMENT("ExtraTraps.AntiMagic"),
    CVAR_ENHANCEMENT("ExtraTraps.Rupoor"),
};
static_assert(sizeof(altTrapTypeCvars) / sizeof(altTrapTypeCvars[0]) == ADD_TRAP_MAX,
              "every trap type needs its CVar, in AltTrapType order");

// RedShipBlueShip (#1424): the SAVE half of OoTMM's Drain, Anti-Magic and Rupoor
// traps (OoTMM traps.c Play_UpdateTrapsDrain / AntiMagic / Rupoor), everything
// but Link's reaction, so a ROM-free row can spring it with no play state. Each
// writes the live field the shared-pool harvest reads: health and magic
// directly, rupees through Rupees_ChangeBy's accumulator, which
// OoT_HarvestSharedRupees folds in. True when the type has a save half.
static bool ApplyTrapSaveEffect(AltTrapType type) {
    switch (type) {
        case ADD_DRAIN_TRAP:
            gSaveContext.health = 1;
            return true;
        case ADD_ANTIMAGIC_TRAP:
            gSaveContext.magic = 0;
            return true;
        case ADD_RUPOOR_TRAP:
            OoT_Rupees_ChangeBy(-10);
            return true;
        default:
            return false;
    }
}

std::vector<AltTrapType> getEnabledAddTraps() {
    std::vector<AltTrapType> enabledAddTraps;
    for (int i = 0; i < ADD_TRAP_MAX; i++) {
        if (CVarGetInteger(altTrapTypeCvars[i], 0)) {
            if (gSaveContext.equips.buttonItems[0] == ITEM_FISHING_POLE &&
                (i == ADD_VOID_TRAP || i == ADD_TELEPORT_TRAP)) {
                continue; // don't add void or teleport if you're holding the fishing pole, as this causes issues
            }
            enabledAddTraps.push_back(static_cast<AltTrapType>(i));
        }
    }
    if (enabledAddTraps.size() == 0) {
        enabledAddTraps.push_back(ADD_ICE_TRAP);
    }
    return enabledAddTraps;
};

// RedShipBlueShip (#1424): the draw, apart from springing it, so the parity row
// reads the type the production roll picks.
static AltTrapType PickRandomTrap(uint32_t finalSeed, uint64_t* randomState) {
    ShipUtils::RandInit(finalSeed, randomState);
    auto enabledAddTraps = getEnabledAddTraps();
    return ShipUtils::RandomElement(enabledAddTraps, false, randomState);
}

static void RollRandomTrap(uint32_t seed) {
    uint32_t finalSeed = seed + (IS_RANDO ? Rando::Context::GetInstance()->GetSeed()
                                          : static_cast<uint32_t>(gSaveContext.ship.stats.fileCreatedAt));
    uint64_t randomState = 0;
    roll = PickRandomTrap(finalSeed, &randomState);
    switch (roll) {
        case ADD_ICE_TRAP:
            GameInteractor::RawAction::FreezePlayer();
            break;
        case ADD_BURN_TRAP:
            GameInteractor::RawAction::BurnPlayer();
            break;
        case ADD_SHOCK_TRAP:
            GameInteractor::RawAction::ElectrocutePlayer();
            break;
        case ADD_KNOCK_TRAP:
            eventTimer = 3;
            break;
        case ADD_SPEED_TRAP:
            Audio_PlaySoundGeneral(NA_SE_VO_KZ_MOVE, &OoT_gSfxDefaultPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                                   &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
            GameInteractor::State::MovementSpeedMultiplier = 0.5f;
            statusTimer = 200;
            Notification::Emit({ .message = "Speed Decreased!" });
            break;
        case ADD_BOMB_TRAP:
            eventTimer = 3;
            break;
        case ADD_VOID_TRAP:
            Audio_PlaySoundGeneral(NA_SE_EN_GANON_LAUGH, &OoT_gSfxDefaultPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                                   &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
            eventTimer = 3;
            break;
        case ADD_AMMO_TRAP:
            eventTimer = 3;
            Notification::Emit({ .message = "Ammo Halved!" });
            break;
        case ADD_KILL_TRAP:
            GameInteractor::RawAction::SetPlayerHealth(0);
            break;
        case ADD_TELEPORT_TRAP:
            eventTimer = 3;
            teleportRoll = static_cast<int>(ShipUtils::Random(0, 7, &randomState));
            break;
        // #1424: OoTMM's handlers: the save half, then (Drain, Anti-Magic) its
        // damage response 0 with no damage of its own and 20 invincibility frames.
        case ADD_DRAIN_TRAP:
        case ADD_ANTIMAGIC_TRAP: {
            ApplyTrapSaveEffect(roll);
            Player* player = GET_PLAYER(OoT_gPlayState);
            player->actor.colChkInfo.damage = 0;
            func_80837C0C(OoT_gPlayState, player, 0, 0, 0, 0, 20);
            break;
        }
        case ADD_RUPOOR_TRAP:
            ApplyTrapSaveEffect(roll);
            break;
        default:
            break;
    }
}

static void OnPlayerUpdate() {
    Player* player = GET_PLAYER(OoT_gPlayState);
    if (statusTimer == 0) {
        GameInteractor::State::MovementSpeedMultiplier = 1.0f;
    }
    if (eventTimer == 0) {
        switch (roll) {
            case ADD_KNOCK_TRAP:
                GameInteractor::RawAction::KnockbackPlayer(1);
                break;
            case ADD_BOMB_TRAP:
                GameInteractor::RawAction::SpawnActor(ACTOR_EN_BOM, 1);
                break;
            case ADD_VOID_TRAP:
                Play_TriggerRespawn(OoT_gPlayState);
                break;
            case ADD_AMMO_TRAP:
                AMMO(ITEM_STICK) = static_cast<int8_t>(floor(AMMO(ITEM_STICK) * 0.5f));
                AMMO(ITEM_NUT) = static_cast<int8_t>(floor(AMMO(ITEM_NUT) * 0.5f));
                AMMO(ITEM_SLINGSHOT) = static_cast<int8_t>(floor(AMMO(ITEM_SLINGSHOT) * 0.5f));
                AMMO(ITEM_BOW) = static_cast<int8_t>(floor(AMMO(ITEM_BOW) * 0.5f));
                AMMO(ITEM_BOMB) = static_cast<int8_t>(floor(AMMO(ITEM_BOMB) * 0.5f));
                AMMO(ITEM_BOMBCHU) = static_cast<int8_t>(floor(AMMO(ITEM_BOMBCHU) * 0.5f));
                Audio_PlaySoundGeneral(NA_SE_VO_FR_SMILE_0, &OoT_gSfxDefaultPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                                       &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
                break;
            case ADD_TELEPORT_TRAP: {
                int entrance;
                switch (teleportRoll) {
                    case 0:
                        entrance = GI_TP_DEST_SERENADE;
                        break;
                    case 1:
                        entrance = GI_TP_DEST_REQUIEM;
                        break;
                    case 2:
                        entrance = GI_TP_DEST_BOLERO;
                        break;
                    case 3:
                        entrance = GI_TP_DEST_MINUET;
                        break;
                    case 4:
                        entrance = GI_TP_DEST_NOCTURNE;
                        break;
                    case 5:
                        entrance = GI_TP_DEST_PRELUDE;
                        break;
                    default:
                        entrance = GI_TP_DEST_LINKSHOUSE;
                        break;
                }
                GameInteractor::RawAction::TeleportPlayer(entrance);
                break;
            }
            default:
                break;
        }
    }
    if (statusTimer >= 0) {
        statusTimer--;
    }
    if (eventTimer >= 0) {
        eventTimer--;
    }
}

void RegisterExtraTraps() {
    COND_HOOK(OnPlayerUpdate, CVAR_EXTRA_TRAPS_VALUE, OnPlayerUpdate);

    COND_VB_SHOULD(VB_SHORT_CIRCUIT_GIVE_ITEM_PROCESS, true, {
        if (!gSaveContext.ship.pendingIceTrapCount) {
            return;
        }

        Player* player = GET_PLAYER(OoT_gPlayState);

        *should = true;
        gSaveContext.ship.pendingIceTrapCount--;
        gSaveContext.ship.stats.count[COUNT_ICE_TRAPS]++;
        GameInteractor_ExecuteOnItemReceiveHooks(ItemTable_RetrieveEntry(MOD_RANDOMIZER, RG_ICE_TRAP));
        if (CVAR_EXTRA_TRAPS_VALUE) {
            RollRandomTrap(OoT_gPlayState->sceneNum + player->getItemEntry.drawItemId);
        } else {
            GameInteractor::RawAction::FreezePlayer();
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterExtraTraps, { CVAR_EXTRA_TRAPS_NAME });

#ifdef RSBS_SINGLE_EXECUTABLE
// ROM-free test bridges (redship tier; src/common/tests/test_trap_type_parity.c, #1424).

extern "C" int OoT_ExtraTraps_TestTypeCount(void) {
    return (int)ADD_TRAP_MAX;
}

/** The CVar that enables trap type @p type, or nullptr outside 0..ADD_TRAP_MAX-1. */
extern "C" const char* OoT_ExtraTraps_TestCVarName(int type) {
    return type >= 0 && type < (int)ADD_TRAP_MAX ? altTrapTypeCvars[type] : nullptr;
}

/** The list the roll picks from (getEnabledAddTraps) under the live CVars, in its
 *  order: writes at most @p cap types and returns how many there are. */
extern "C" int OoT_ExtraTraps_TestEnabledList(int* out, int cap) {
    const std::vector<AltTrapType> list = getEnabledAddTraps();
    for (int i = 0; i < (int)list.size() && i < cap; i++) {
        out[i] = (int)list[i];
    }
    return (int)list.size();
}

/** The type the production draw (PickRandomTrap) picks for @p finalSeed under
 *  the live CVars, without springing it. */
extern "C" int OoT_ExtraTraps_TestPick(uint32_t finalSeed) {
    uint64_t randomState = 0;
    return (int)PickRandomTrap(finalSeed, &randomState);
}

/** Spring the save half of trap @p type on OoT's live save. 1 when it has one. */
extern "C" int OoT_ExtraTraps_TestApplySaveEffect(int type) {
    if (type < 0 || type >= (int)ADD_TRAP_MAX) {
        return 0;
    }
    return ApplyTrapSaveEffect((AltTrapType)type) ? 1 : 0;
}
#endif
