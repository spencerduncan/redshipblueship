#include "Traps.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "MiscBehavior.h"
#include "Rando/ActorBehavior/ActorBehavior.h"
#include "2s2h/DeveloperTools/SaveEditor.h"
#include "2s2h/ShipUtils.h"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "Rando/Foreign.h" // ResolvePairedProfile, for the creation bridge (#1047)
#include <cstddef>         // offsetof: the high byte's layout lock (#1424)
#include <memory>
#endif

extern "C" {
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_En_Time_Tag/z_en_time_tag.h"
void func_80833B18(PlayState* play, Player* thisx, s32 arg2, f32 speed, f32 velocityY, s16 arg5,
                   s32 invincibilityTimer);
void EnTimeTag_KickOut_Transition(EnTimeTag* enTimeTag, PlayState* play);
}

extern void UpdateGameTime(u16 gameTime);

int roll = TRAP_FREEZE;
const u16 timeSkipInterval = 4000;

std::map<TrapTypes, const char*> trapToCvarMap = {
    { TRAP_FREEZE, "gRando.Traps.Freeze" }, { TRAP_BLAST, "gRando.Traps.Blast" },
    { TRAP_SHOCK, "gRando.Traps.Shock" },   { TRAP_JINX, "gRando.Traps.Jinx" },
    { TRAP_WALLET, "gRando.Traps.Wallet" }, { TRAP_ENEMY, "gRando.Traps.Enemy" },
    { TRAP_TIME, "gRando.Traps.Time" },
    // RedShipBlueShip (#1424): OoTMM's trap types MM lacked (Fire and Knockback
    // under upstream 2Ship's own keys, 254cc60e1).
    { TRAP_FIRE, "gRando.Traps.Fire" },
    { TRAP_KNOCKBACK, "gRando.Traps.Knockback" },
    { TRAP_DRAIN, "gRando.Traps.Drain" },
    { TRAP_ANTI_MAGIC, "gRando.Traps.AntiMagic" },
    { TRAP_RUPOOR, "gRando.Traps.Rupoor" },
};

// RedShipBlueShip (#1424): the SAVE half of OoTMM's Drain, Anti-Magic and Rupoor
// traps (OoTMM traps.c Play_UpdateTrapsDrain / AntiMagic / Rupoor), everything
// but Link's reaction, so a ROM-free row can spring it with no play state. Each
// writes the live field the shared-pool harvest reads: health and magic
// directly, rupees through Rupees_ChangeBy's accumulator, which
// MM_HarvestSharedRupees folds in. True when the type has a save half.
static bool ApplyTrapSaveEffect(int type) {
    switch (type) {
        case TRAP_DRAIN:
            gSaveContext.save.saveInfo.playerData.health = 1;
            return true;
        case TRAP_ANTI_MAGIC:
            gSaveContext.save.saveInfo.playerData.magic = 0;
            // OoTMM ends the Chateau Romani's endless magic too.
            if (CHECK_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI)) {
                CLEAR_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI);
            }
            return true;
        case TRAP_RUPOOR:
            MM_Rupees_ChangeBy(-10);
            return true;
        default:
            return false;
    }
}

// #1424: OoTMM's Fire Trap leaves Goron Link alone (Player_IsGoronOrGoronTunic;
// MM has no Goron Tunic, so that is the Goron form).
static bool TrapFireSpares(u8 transformation) {
    return transformation == PLAYER_FORM_GORON;
}

// #1424: Link's reaction to the traps OoTMM added: the damage response OoTMM
// gives each (Player_ApplyDamage is func_80833B18 here), with no damage of its
// own (colChkInfo.damage = 0) and OoTMM's 20 invincibility frames.
static void TrapPlayerReaction(s32 response, f32 speed, f32 velocityY, s16 yaw) {
    Player* player = GET_PLAYER(MM_gPlayState);
    player->actor.colChkInfo.damage = 0;
    func_80833B18(MM_gPlayState, player, response, speed, velocityY, yaw, 20);
}

#ifdef RSBS_SINGLE_EXECUTABLE
// #1047: a trap's type is part of the world (one-game semantics; OoTMM decides a
// trap's kind at generation; SoH's ExtraTraps seeds its roll from the world's
// seed). Upstream rolled it at pickup from the LIVE gRando.Traps.* CVars and
// wherever MM's global stream happened to be, so a reload or a config change
// could spring a different trap from the same check. The set now freezes into
// the save at creation (ResolveTrapTypesFromCVars, written by
// Rando::Foreign::ResolvePairedProfile) and the roll is seeded by the world's
// seed plus the check.
uint16_t ResolveTrapTypesFromCVars() {
    uint16_t mask = 0;
    for (auto& trap : trapToCvarMap) {
        if (CVarGetInteger(trap.second, 0)) {
            mask |= (uint16_t)(1u << trap.first);
        }
    }
    return mask;
}

// #1424: the frozen set is 16 bits over two bytes. The high byte sits in what
// was RandoSaveInfo's padding before foundTriforcePieces (z64save.h), so the
// layout every existing .redsave has is unchanged: that u16 must still be at
// foundDungeonKeys + 10, the place the 9-byte array's padding always put it.
static_assert(TRAP_MAX <= 16, "the frozen trap-type set holds one bit per type in two bytes");
static_assert(offsetof(RandoSaveInfo, foundDungeonKeys) % 2 == 0,
              "foundDungeonKeys must start even for the byte after it to have been padding");
static_assert(offsetof(RandoSaveInfo, randoSaveTrapTypesHi) == offsetof(RandoSaveInfo, foundDungeonKeys) + 9,
              "randoSaveTrapTypesHi must fill the padding byte after foundDungeonKeys");
static_assert(offsetof(RandoSaveInfo, foundTriforcePieces) == offsetof(RandoSaveInfo, foundDungeonKeys) + 10,
              "randoSaveTrapTypesHi moved foundTriforcePieces, so every later Save field and the raw MM tier moved");

uint16_t FrozenTrapTypesOf(const RandoSaveInfo& info) {
    // Only the bits a type exists for: a byte no build wrote reads as no type.
    const uint16_t known = (uint16_t)((1u << TRAP_MAX) - 1u);
    return (uint16_t)((info.randoSaveTrapTypes | ((uint16_t)info.randoSaveTrapTypesHi << 8)) & known);
}

void StoreFrozenTrapTypes(RandoSaveInfo& info, uint16_t mask) {
    info.randoSaveTrapTypes = (uint8_t)(mask & 0xFFu);
    info.randoSaveTrapTypesHi = (uint8_t)(mask >> 8);
}

const char* TrapTypeCVar(int type) {
    auto it = trapToCvarMap.find((TrapTypes)type);
    return it == trapToCvarMap.end() ? nullptr : it->second;
}

const char* TrapTypeLabel(int type) {
    // Rando/Menu.cpp's names for the toggles (not built into the single exe).
    static const char* const kLabels[TRAP_MAX] = {
        "Freeze Traps", "Blast Traps", "Shock Traps", "Jinx Traps", "Wallet Traps", "Like Like Traps", "Time Traps",
        // #1424: OoTMM's names for its trap settings, in the rows' "... Traps" form.
        "Fire Traps", "Knockback Traps", "Drain Traps", "Anti-Magic Traps", "Rupoor Traps",
    };
    return type >= 0 && type < TRAP_MAX ? kLabels[type] : nullptr;
}

std::vector<TrapTypes> getEnabledTrapTypes() {
    // In TrapTypes order, so a set without the appended types (#1424) gives the
    // list, and the roll, it always gave.
    const uint16_t mask = FrozenTrapTypesOf(gSaveContext.save.shipSaveInfo.rando);
    std::vector<TrapTypes> enabledTrapTypes;
    for (auto& trap : trapToCvarMap) {
        if (mask & (1u << trap.first)) {
            enabledTrapTypes.push_back(trap.first);
        }
    }
    if (enabledTrapTypes.size() == 0) {
        enabledTrapTypes.push_back(TRAP_FREEZE);
    }
    return enabledTrapTypes;
};

int RollTrapType(RandoCheckId randoCheckId) {
    auto enabledTraps = getEnabledTrapTypes();
    Ship_Random_Seed((uint32_t)(gSaveContext.save.shipSaveInfo.rando.finalSeed + (uint32_t)randoCheckId));
    roll = enabledTraps[Ship_Random(0, enabledTraps.size())];
    return roll;
}

// ROM-free test bridges (redship tier; src/common/tests/test_playtime_frozen.c, #1047).

/** The type the trap on `randoCheckId` springs in a world whose seed is
 *  `finalSeed` and whose frozen set is `mask`, with MM's global stream first put
 *  at `streamSeed` (standing for wherever play left it). Leaves the save's seed
 *  and set as it found them. */
extern "C" int MM_Rando_Traps_TestRoll(uint32_t finalSeed, uint16_t mask, uint16_t randoCheckId, uint64_t streamSeed) {
    RandoSaveInfo& rando = gSaveContext.save.shipSaveInfo.rando;
    const uint32_t priorSeed = rando.finalSeed;
    const uint8_t priorLo = rando.randoSaveTrapTypes;
    const uint8_t priorHi = rando.randoSaveTrapTypesHi;
    rando.finalSeed = finalSeed;
    StoreFrozenTrapTypes(rando, mask);
    Ship_Random_Seed(streamSeed);
    const int type = RollTrapType((RandoCheckId)randoCheckId);
    rando.finalSeed = priorSeed;
    rando.randoSaveTrapTypes = priorLo;
    rando.randoSaveTrapTypesHi = priorHi;
    return type;
}

/** The CVar that enables trap type `type`, or nullptr; and TRAP_MAX for -1. */
extern "C" const char* MM_Rando_Traps_TestCVarName(int type) {
    return TrapTypeCVar(type);
}

extern "C" int MM_Rando_Traps_TestTypeCount(void) {
    return (int)TRAP_MAX;
}

/** What creation freezes: runs the real Rando::Foreign::ResolvePairedProfile
 *  (the solo leg, which publishes no identity) over a copy of the save's rando
 *  block and returns the set it wrote, both bytes (#1424); the block is put back.
 *  Both bytes start as 0xFF, so a byte creation forgot reads as every type. */
extern "C" int MM_Rando_Traps_TestCreationFreezes(void) {
    auto saved = std::make_unique<RandoSaveInfo>(gSaveContext.save.shipSaveInfo.rando);
    gSaveContext.save.shipSaveInfo.rando.randoSaveTrapTypes = 0xFF;
    gSaveContext.save.shipSaveInfo.rando.randoSaveTrapTypesHi = 0xFF;
    Rando::Foreign::ResolvePairedProfile(false);
    const int mask = gSaveContext.save.shipSaveInfo.rando.randoSaveTrapTypes |
                     ((int)gSaveContext.save.shipSaveInfo.rando.randoSaveTrapTypesHi << 8);
    gSaveContext.save.shipSaveInfo.rando = *saved;
    return mask;
}

/** #1424: spring the save half of trap @p type (ApplyTrapSaveEffect) on MM's
 *  live save, as OfferTrapItem's event does. 1 when the type has one. */
extern "C" int MM_Rando_Traps_TestApplySaveEffect(int type) {
    return ApplyTrapSaveEffect(type) ? 1 : 0;
}

/** #1424: 1 when a Fire Trap leaves Link alone in @p transformation
 *  (a PlayerTransformation), as OoTMM's spares Goron Link. */
extern "C" int MM_Rando_Traps_TestFireSpares(int transformation) {
    return TrapFireSpares((u8)transformation) ? 1 : 0;
}

/** #1424: the name of trap type @p type in the options, for the parity row. */
extern "C" const char* MM_Rando_Traps_TestLabel(int type) {
    return TrapTypeLabel(type);
}
#else
std::vector<TrapTypes> getEnabledTrapTypes() {
    std::vector<TrapTypes> enabledTrapTypes;
    for (auto& trap : trapToCvarMap) {
        if (CVarGetInteger(trap.second, 0)) {
            enabledTrapTypes.push_back(trap.first);
        }
    }
    if (enabledTrapTypes.size() == 0) {
        enabledTrapTypes.push_back(TRAP_FREEZE);
    }
    return enabledTrapTypes;
};

int RollTrapType() {
    auto enabledTraps = getEnabledTrapTypes();
    roll = enabledTraps[Ship_Random(0, enabledTraps.size())];
    return roll;
}
#endif // RSBS_SINGLE_EXECUTABLE

// clang-format off
std::vector<std::string> defaultTrapMessages = { 
    "This item is available in the %bRando DLC%w.",
    "This is what happens when %gCaladius%w is left unsupervised.",
    "Oh no!",
    "Uh oh!",
    "KEKW",
    "We've been trying to reach you about your Horse's %gextended warranty%w.",
    "Admit it, you wish this was %gGreg%w.",
    "Error 404: Item Not Found",
    "Get dunked on!",
    "This %rTrap%w is brought to you by today's sponsor...\n%gRaid Shadow Legends%w!",
};

std::vector<std::string> freezeTrapMessages = {
    "%rOcarina of Time%w called, they want their %bIce Trap%w back.",
    "Greetings from %bSnowhead%w! Wish you were here.",
    "This item was too %bcool%w for you anyway.",
    "Let me see your best %bKing Zora%w impersonation.",
    "There's no business like %bSnow%w business!",
    "How much does a polar bear weigh? Enough to break the %bice%w.",
    "You found the %yTrifo%w... Wait, nevermind...",
    "Quick Time Event! Don't die.",
};

std::vector<std::string> blastTrapMessages = {
    "Coming to you live from the %yThunderdome%w!",
    "There was supposed to be an Earth shattering %yKaboom%w!",
    "Pardon me while I %yburst%w.",
    "This is not the greatest %yblast%w in the world, this is just a tribute.",
    "Hey look, this %yitem%w is ticking!",
    "This is sure to make %gPamela%w leave the house.",
    "You found a %yDodongo Snack%w!",
};

std::vector<std::string> shockTrapMessages = {
    "We're losing him!\n%gCLEAR%w",
    "It's got what %gplants%w need.",
    "On todays episode of Grey's Anatomy...",
    "I'm giving it all I've got, captain!",
    "Now simulating a CD in a Microwave.",
    "Hang in there, for %gMeryl%w."
};

std::vector<std::string> jinxTrapMessages = {
    "Looks like someone's got a case of the %bMondays%w. :( ",
    "Your wrist hurts! The doctor says no more swords.",
};

std::vector<std::string> walletTrapMessages = {
    "Spare some %rchange%w?",
    "%rBreaking News%w: the Moon hasn't crashed, but Termina's economy sure has."
};

std::vector<std::string> enemyTrapMessages = {
    "You made a new friend!",
    // Like Like Specific, will need to adjust if new enemies are added.
    "Someone likes you! They %rLike Like%w you!",
    "You don't need a shield anyway.",
    "This item sucks.",
};

std::vector<std::string> timeTrapMessages = {
    "%yTime%w flashes before your eyes!",
    "You have played the %ySun's Song%w!",
    "The %rGoddess of Time%w smites you!",
    "Spent an hour and a half admiring this %bfake item%w.",
    "You found a great place to take a nap!",
    "Break time! Not like the world is ending right?",
    "I just need you to tell me how to get to the %ytime machine%w.",
    "Mweep",
};

// RedShipBlueShip (#1424): the trap types OoTMM added.
std::vector<std::string> fireTrapMessages = {
    "Stop, drop and %rroll%w!",
    "This item was %rtoo hot%w to handle.",
    "A %rGoron%w would have shrugged that off.",
};

std::vector<std::string> drainTrapMessages = {
    "You feel %rdrained%w...",
    "A %gFairy%w would be nice right about now.",
    "That one took a lot out of you.",
};

std::vector<std::string> antiMagicTrapMessages = {
    "Your %gmagic%w fizzles away.",
    "Out of %gmagic%w!",
    "The %gGreat Fairy%w is not amused.",
};

std::vector<std::string> knockbackTrapMessages = {
    "Up, up and %baway%w!",
    "Mind your %bfooting%w.",
};

std::vector<std::string> rupoorTrapMessages = {
    "You found a %rRupoor%w!",
    "That will be %r10 Rupees%w, please.",
};

std::map<TrapTypes, std::vector<std::string>> trapMessageList = {
    { TRAP_FREEZE, freezeTrapMessages },
    { TRAP_BLAST, blastTrapMessages },
    { TRAP_SHOCK, shockTrapMessages },
    { TRAP_JINX, jinxTrapMessages },
    { TRAP_WALLET, walletTrapMessages },
    { TRAP_ENEMY, enemyTrapMessages },
    { TRAP_TIME, timeTrapMessages },
    { TRAP_FIRE, fireTrapMessages },
    { TRAP_KNOCKBACK, knockbackTrapMessages },
    { TRAP_DRAIN, drainTrapMessages },
    { TRAP_ANTI_MAGIC, antiMagicTrapMessages },
    { TRAP_RUPOOR, rupoorTrapMessages },
};
// clang-format on

#ifdef RSBS_SINGLE_EXECUTABLE
std::string GetTrapMessage(RandoCheckId randoCheckId) {
    RollTrapType(randoCheckId);
#else
std::string GetTrapMessage() {
    RollTrapType();
#endif
    auto findIt = trapMessageList.find((TrapTypes)roll);
    if (findIt == trapMessageList.end()) {
        return defaultTrapMessages[Ship_Random(0, defaultTrapMessages.size())];
    } else {
        return findIt->second[Ship_Random(0, findIt->second.size())];
    }
}

void Rando::MiscBehavior::OfferTrapItem() {
    if (!MM_gPlayState) {
        return;
    }

    switch (roll) {
        case TRAP_FREEZE:
            MM_GameEvents_Queue().emplace_back(
                GIEventTrap{ .action = []() { func_80833B18(MM_gPlayState, GET_PLAYER(MM_gPlayState), 3, 0, 0, 0, 0); } });
            break;
        case TRAP_BLAST:
            MM_GameEvents_Queue().emplace_back(GIEventTrap{ .action = []() {
                MM_Actor_Spawn(&MM_gPlayState->actorCtx, MM_gPlayState, ACTOR_EN_BOM, GET_PLAYER(MM_gPlayState)->actor.world.pos.x,
                            GET_PLAYER(MM_gPlayState)->actor.world.pos.y, GET_PLAYER(MM_gPlayState)->actor.world.pos.z, 1, 0,
                            0, 0);
            } });
            break;
        case TRAP_SHOCK:
            MM_GameEvents_Queue().emplace_back(
                GIEventTrap{ .action = []() { func_80833B18(MM_gPlayState, GET_PLAYER(MM_gPlayState), 4, 0, 0, 0, 0); } });
            break;
        case TRAP_JINX:
            MM_GameEvents_Queue().emplace_back(GIEventTrap{ .action = []() {
                Actor_PlaySfx(&GET_PLAYER(MM_gPlayState)->actor, NA_SE_EN_BUBLE_BITE);
                gSaveContext.jinxTimer = 1200;
            } });
            break;
        case TRAP_WALLET:
            MM_GameEvents_Queue().emplace_back(GIEventTrap{ .action = []() {
                int16_t currentRupees = gSaveContext.save.saveInfo.playerData.rupees;
                if (currentRupees != 0) {
                    Vec3f positional = GET_PLAYER(MM_gPlayState)->actor.world.pos;
                    positional.y = GET_PLAYER(MM_gPlayState)->actor.world.pos.y + 100.0f;
                    Item00Type rupee = ITEM00_RUPEE_GREEN;
                    int16_t spawnedRupees = 0;
                    int16_t remainingRupees = currentRupees;
                    for (int i = spawnedRupees; spawnedRupees < remainingRupees;) {
                        if (currentRupees >= 20) {
                            rupee = ITEM00_RUPEE_RED;
                            spawnedRupees += 20;
                            MM_Rupees_ChangeBy(-20);
                            currentRupees -= 20;
                        } else if (currentRupees >= 5) {
                            rupee = ITEM00_RUPEE_BLUE;
                            spawnedRupees += 5;
                            MM_Rupees_ChangeBy(-5);
                            currentRupees -= 5;
                        } else if (currentRupees >= 1) {
                            rupee = ITEM00_RUPEE_GREEN;
                            spawnedRupees += 1;
                            MM_Rupees_ChangeBy(-1);
                            currentRupees -= 1;
                        }
                        EnItem00* rupeeActor = (EnItem00*)MM_Item_DropCollectible(MM_gPlayState, &positional, rupee);
                        // RSBS (#1412): MM No Random Drops can cancel the drop, and then it returns NULL.
                        if (rupeeActor != NULL) {
                            rupeeActor->actor.speed = MM_Rand_CenteredFloat(5.0f);
                            rupeeActor->unk152 = 600; // Extending Time before Despawning
                        }
                    }
                }
            } });
            break;
        case TRAP_ENEMY:
            MM_GameEvents_Queue().emplace_back(GIEventTrap{ .action = []() {
                MM_Actor_Spawn(&MM_gPlayState->actorCtx, MM_gPlayState, ACTOR_EN_RR, GET_PLAYER(MM_gPlayState)->actor.world.pos.x,
                            GET_PLAYER(MM_gPlayState)->actor.world.pos.y, GET_PLAYER(MM_gPlayState)->actor.world.pos.z, 0, 0,
                            0, 1);
            } });
            break;
        case TRAP_TIME:
            MM_GameEvents_Queue().emplace_back(GIEventTrap{ .action = []() {
                u16 previous_time = gSaveContext.save.time;
                u16 new_time = gSaveContext.save.time + timeSkipInterval;
                if (previous_time < MORNING_TIME && new_time >= MORNING_TIME) {
                    // Handles case where Night -> Day
                    if (gSaveContext.save.day != 3) {
                        gSaveContext.save.day++;
                        gSaveContext.save.eventDayCount++;
                        UpdateGameTime(new_time);
                        Interface_NewDay(MM_gPlayState, CURRENT_DAY);
                        // Load environment values for new day
                        Environment_NewDay(&MM_gPlayState->envCtx);
                        // Clear weather from day 2
                        MM_gWeatherMode = WEATHER_MODE_CLEAR;
                        MM_gPlayState->envCtx.lightningState = LIGHTNING_OFF;
                    } else {
                        // Handles Moonfall case, prevents skipping past it by setting time right before Moonfall.
                        UpdateGameTime(MORNING_TIME - (timeSkipInterval / 10));
                    }
                } else {
                    // Every other case
                    UpdateGameTime(new_time);
                }
                MM_TransitionFade_SetColor(&MM_gPlayState->unk_18E48, 0x000000);
                R_TRANS_FADE_FLASH_ALPHA_STEP = -1;
                MM_Player_PlaySfx(GET_PLAYER(MM_gPlayState), NA_SE_SY_TRANSFORM_MASK_FLASH);

                // Handle kickouts, if needed
                EnTimeTag* enTimeTag = (EnTimeTag*)MM_Actor_FindNearby(MM_gPlayState, &GET_PLAYER(MM_gPlayState)->actor,
                                                                    ACTOR_EN_TIME_TAG, ACTORCAT_ITEMACTION, 99999.9f);
                if (enTimeTag != nullptr) {
                    TimeTagType timeTagType = (TimeTagType)TIMETAG_GET_TYPE(&enTimeTag->actor);
                    if (timeTagType == TIMETAG_KICKOUT_DOOR || timeTagType >= TIMETAG_KICKOUT_FINAL_HOURS) {
                        s16 kickoutHour = TIMETAG_KICKOUT_HOUR(&enTimeTag->actor);
                        s16 kickoutMinute = TIMETAG_KICKOUT_MINUTE(&enTimeTag->actor);
                        s32 kickoutTime = CLOCK_TIME(kickoutHour, kickoutMinute);
                        kickoutTime = ZERO_DAY_START(kickoutTime);
                        previous_time = ZERO_DAY_START(previous_time);
                        new_time = ZERO_DAY_START(new_time);
                        // If we were here before the kickout time, and now it's after, then get out of my house
                        if (previous_time <= kickoutTime && new_time >= kickoutTime) {
                            // Unless this is the Stock Pot Inn, and the room key is obtained
                            if (!(MM_gPlayState->sceneId == SCENE_YADOYA &&
                                  Flags_GetRandoInf(RANDO_INF_OBTAINED_ROOM_KEY))) {
                                // This comes from EnTimeTag_KickOut_WaitForTime
                                MM_Player_SetCsActionWithHaltedActors(MM_gPlayState, &enTimeTag->actor, PLAYER_CSACTION_WAIT);
                                MM_Message_StartTextbox(MM_gPlayState, 0x1883 + TIMETAG_KICKOUT_GET_TEXT(&enTimeTag->actor),
                                                     NULL);
                                enTimeTag->actionFunc = EnTimeTag_KickOut_Transition;
                            }
                        }
                    }
                }
            } });
            break;
        // RedShipBlueShip (#1424): OoTMM's trap types (OoTMM traps.c), each
        // its handler there: the save half (ApplyTrapSaveEffect) and Link's
        // damage response.
        case TRAP_FIRE:
            MM_GameEvents_Queue().emplace_back(GIEventTrap{ .action = []() {
                Player* player = GET_PLAYER(MM_gPlayState);
                if (TrapFireSpares(player->transformation)) {
                    return;
                }
                TrapPlayerReaction(0, 0.0f, 0.0f, 0);
                for (int i = 0; i < PLAYER_BODYPART_MAX; i++) {
                    player->bodyFlameTimers[i] = MM_Rand_S16Offset(0, 200);
                }
                player->bodyIsBurning = true;
            } });
            break;
        case TRAP_DRAIN:
            MM_GameEvents_Queue().emplace_back(GIEventTrap{ .action = []() {
                ApplyTrapSaveEffect(TRAP_DRAIN);
                TrapPlayerReaction(0, 0.0f, 0.0f, 0);
            } });
            break;
        case TRAP_ANTI_MAGIC:
            MM_GameEvents_Queue().emplace_back(GIEventTrap{ .action = []() {
                ApplyTrapSaveEffect(TRAP_ANTI_MAGIC);
                TrapPlayerReaction(0, 0.0f, 0.0f, 0);
            } });
            break;
        case TRAP_KNOCKBACK:
            MM_GameEvents_Queue().emplace_back(GIEventTrap{ .action = []() {
                // A random speed, height and direction, as OoTMM throws Link.
                TrapPlayerReaction(1, MM_Rand_ZeroFloat(20.0f), MM_Rand_ZeroFloat(15.0f),
                                   (s16)(s32)MM_Rand_ZeroFloat(65536.0f));
            } });
            break;
        case TRAP_RUPOOR:
            MM_GameEvents_Queue().emplace_back(GIEventTrap{ .action = []() { ApplyTrapSaveEffect(TRAP_RUPOOR); } });
            break;
        default:
            break;
    }
}

void Rando::ActorBehavior::InitTrapsBehavior() {
    // Selectively disable object dependency for actors spawned by traps
    COND_VB_SHOULD(VB_ENABLE_OBJECT_DEPENDENCY, IS_RANDO && RANDO_SAVE_OPTIONS[RO_SHUFFLE_TRAPS], {
        ObjectId objectId = (ObjectId)va_arg(args, int);
        // Allow spawning of like-like & shield so it drops from like-like
        if (objectId == OBJECT_RR || objectId == OBJECT_GI_SHIELD_2) {
            *should = false;
        }
    });
}