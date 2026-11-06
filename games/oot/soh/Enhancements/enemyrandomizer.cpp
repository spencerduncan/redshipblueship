#include "enemyrandomizer.h"
#include "functions.h"
#include "macros.h"
#include "soh/ShipUtils.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "variables.h"
#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/ResourceManagerHelpers.h"

extern "C" {
#include <z64.h>
#include "src/overlays/actors/ovl_En_Rr/z_en_rr.h"
#include "src/overlays/actors/ovl_En_GeldB/z_en_geldb.h"
#include "src/overlays/actors/ovl_En_Peehat/z_en_peehat.h"
#include "src/overlays/actors/ovl_En_Skb/z_en_skb.h"
#include "src/overlays/actors/ovl_En_Tp/z_en_tp.h"
#include "src/overlays/actors/ovl_En_Crow/z_en_crow.h"
#include "src/overlays/actors/ovl_En_Firefly/z_en_firefly.h"

extern PlayState* OoT_gPlayState;
extern void OoT_EnCrow_TurnAway(EnCrow*, PlayState*);
extern void OoT_EnFirefly_SetupFlyAway(EnFirefly*);
extern void OoT_EnFirefly_Fall(EnFirefly*, PlayState*);
}

#define CVAR_ENEMY_RANDOMIZER_NAME CVAR_ENHANCEMENT("RandomizedEnemies")
#define CVAR_ENEMY_RANDOMIZER_DEFAULT ENEMY_RANDOMIZER_OFF
#define CVAR_ENEMY_RANDOMIZER_VALUE CVarGetInteger(CVAR_ENEMY_RANDOMIZER_NAME, CVAR_ENEMY_RANDOMIZER_DEFAULT)

#define CVAR_RANDO_ENEMY_SIZE_NAME CVAR_ENHANCEMENT("RandomizedEnemySizes")
#define CVAR_RANDO_ENEMY_SIZE_DEFAULT 0
#define CVAR_RANDO_ENEMY_SIZE_VALUE CVarGetInteger(CVAR_RANDO_ENEMY_SIZE_NAME, CVAR_RANDO_ENEMY_SIZE_DEFAULT)

#define CVAR_ENEMY_SCALE_HEALTH_NAME CVAR_ENHANCEMENT("EnemySizeScalesHealth")
#define CVAR_ENEMY_SCALE_HEALTH_DEFAULT 0
#define CVAR_ENEMY_SCALE_HEALTH_VALUE CVarGetInteger(CVAR_ENEMY_SCALE_HEALTH_NAME, CVAR_ENEMY_SCALE_HEALTH_DEFAULT)

typedef struct EnemyEntry {
    int16_t id;
    int16_t params;
} EnemyEntry;

bool IsEnemyFoundToRandomize(int16_t sceneNum, int8_t roomNum, int16_t actorId, int16_t params, float posX);
bool IsEnemyAllowedToSpawn(int16_t sceneNum, int8_t roomNum, EnemyEntry enemy);
EnemyEntry GetRandomizedEnemyEntry(uint32_t seed, PlayState* play);

const char* enemyCVarList[RANDOMIZED_ENEMY_SPAWN_TABLE_SIZE] = {
    CVAR_ENHANCEMENT("RandomizedEnemyList.Anubis"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Armos"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Arwing"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.BabyDodongo"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Bari"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Beamos"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.BigSkulltula"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.BigStalchild"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Biri"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.BlackKnuckle"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.BlueTektite"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Bubble"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.ClubMoblin"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.DarkLink"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Dinolfos"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Dodongo"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.FireKeese"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.FlareDancer"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.FloorTile"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Floormaster"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.FlyingPeahat"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.FlyingPot"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Freezard"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.GerudoFighter"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Gibdo"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.GohmaLarva"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Guay"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.IceKeese"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.InvisFireKeese"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.InvisFloormaster"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.InvisIceKeese"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.InvisKeese"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.InvisRedead"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.InvisSkulltula"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.InvisStalfos"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Keese"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.LargeBaba"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.LikeLike"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Lizalfos"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.MadScrub"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.NormalWolfos"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.PeahatLarva"),
    // CVAR_ENHANCEMENT("RandomizedEnemyList.Poe"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Redead"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.RedTektite"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Shabom"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.ShellBlade"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Skulltula"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.SkullKid"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.SmallBaba"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.SmallStalchild"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.SpearMoblin"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Spike"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Stalfos"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Stinger"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Tailparasan"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.TorchSlug"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.Wallmaster"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.WhiteKnuckle"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.WhiteWolfos"),
    CVAR_ENHANCEMENT("RandomizedEnemyList.WitheredBaba"),
};

const char* enemyNameList[RANDOMIZED_ENEMY_SPAWN_TABLE_SIZE] = {
    "Anubis",
    "Armos",
    "Arwing",
    "Baby Dodongo",
    "Bari",
    "Beamos",
    "Big Skulltula",
    "Stalchild (Big)",
    "Biri",
    "Iron Knuckle (Black)",
    "Blue Tektite",
    "Bubble",
    "Club Moblin",
    "Dark Link",
    "Dinolfos",
    "Dodongo",
    "Fire Keese",
    "Flare Dancer",
    "Floor Tile",
    "Floormaster",
    "Flying Peahat",
    "Flying Pot",
    "Freezard",
    "Gerudo Fighter",
    "Gibdo",
    "Gohma Larva",
    "Guay",
    "Ice Keese",
    "Invisible Fire Keese",
    "Invisible Floormaster",
    "Invisible Ice Keese",
    "Invisible Keese",
    "Invisible Redead",
    "Invisible Skulltula",
    "Invisible Stalfos",
    "Keese",
    "Large Deku Baba",
    "Like-Like",
    "Lizalfos",
    "Mad Scrub",
    "Wolfos (Normal)",
    "Peahat Larva",
    //"Poe",
    "Redead",
    "Red Tektite",
    "Shabom",
    "Shell Blade",
    "Skulltula",
    "Skull Kid",
    "Small Deku Baba",
    "Stalchild (Small)",
    "Spear Moblin",
    "Spike",
    "Stalfos",
    "Stinger",
    "Tailpasaran",
    "Torch Slug",
    "Wallmaster",
    "Iron Knuckle (White)",
    "Wolfos (White)",
    "Withered Deku Baba",
};

static EnemyEntry randomizedEnemySpawnTable[RANDOMIZED_ENEMY_SPAWN_TABLE_SIZE] = {
    { ACTOR_EN_ANUBICE_TAG, 1 }, // Anubis
    { ACTOR_EN_AM, -1 },         // Armos
    { ACTOR_EN_CLEAR_TAG, 1 },   // Arwing
    { ACTOR_EN_DODOJR, 0 },      // Baby Dodongo
    { ACTOR_EN_VALI, -1 },       // Bari (big jellyfish)
    { ACTOR_EN_VM, 1280 },       // Beamos
    { ACTOR_EN_ST, 1 },          // Skulltula (big)
    { ACTOR_EN_SKB, 20 },        // Stalchild (big)
    { ACTOR_EN_BILI, 0 },        // Biri (jellyfish)
    { ACTOR_EN_IK, 2 },          // Iron Knuckle (black, standing)
    { ACTOR_EN_TITE, -2 },       // Tektite (blue)
    { ACTOR_EN_BB, -1 },         // Bubble (flying skull enemy) (blue)
    { ACTOR_EN_MB, 0 },          // Club Moblin
    { ACTOR_EN_TORCH2, 0 },      // Dark Link
    { ACTOR_EN_ZF, -2 },         // Dinolfos
    { ACTOR_EN_DODONGO, -1 },    // Dodongo
    { ACTOR_EN_FIREFLY, 1 },     // Fire Keese
    { ACTOR_EN_FD, 0 },          // Flare Dancer
    { ACTOR_EN_YUKABYUN, 0 },      // Flying Floor Tile
    { ACTOR_EN_FLOORMAS, 0 },      // Floormaster
    { ACTOR_EN_PEEHAT, -1 },       // Flying Peahat (big grounded, doesn't spawn larva)
    { ACTOR_EN_TUBO_TRAP, 0 },     // Flying pot
    { ACTOR_EN_FZ, 0 },            // Freezard
    { ACTOR_EN_GELDB, 0 },         // Gerudo Fighter
    { ACTOR_EN_RD, 32766 },        // Gibdo (standing)
    { ACTOR_EN_GOMA, 7 },          // Gohma Larva (Non-Gohma rooms)
    { ACTOR_EN_CROW, 0 },          // Guay
    { ACTOR_EN_FIREFLY, 4 },       // Ice Keese
    { ACTOR_EN_FIREFLY, 0x8001 },  // Fire Keese (invisible)
    { ACTOR_EN_FLOORMAS, 0x8000 }, // Floormaster (invisible)
    { ACTOR_EN_FIREFLY, 0x8004 },  // Ice Keese (invisible)
    { ACTOR_EN_FIREFLY, 0x8002 },  // Keese (invisible)
    { ACTOR_EN_RD, 3 },            // Redead (invisible)
    { ACTOR_EN_ST, 2 },            // Skulltula (invisible)
    { ACTOR_EN_TEST, 0 },          // Stalfos (invisible)
    { ACTOR_EN_FIREFLY, 2 },       // Regular Keese
    { ACTOR_EN_DEKUBABA, 1 },      // Deku Baba (large)
    // Doesn't work (reliant on surface, without a spawner it kills itself too quickly)
    // { ACTOR_EN_REEBA, 0 },       // Leever
    { ACTOR_EN_RR, 0 },         // Like-Like
    { ACTOR_EN_ZF, -1 },        // Lizalfos
    { ACTOR_EN_DEKUNUTS, 768 }, // Mad Scrub (triple attack) (projectiles don't work)
    { ACTOR_EN_WF, 0 },         // Wolfos (normal)
    // Doesn't work (actor directly uses water box collision to handle hiding/popping up)
    // { ACTOR_EN_OKUTA, 0 },       // Octorok
    { ACTOR_EN_PEEHAT, 1 }, // Flying Peahat Larva
    // Doesn't work (Seems to rely on other objects?)
    // { ACTOR_EN_POH, 0 },         // Poe
    // Doesn't work (Seems to rely on other objects?)
    // { ACTOR_EN_POH, 2 },         // Poe (composer Sharp)
    // Doesn't work (Seems to rely on other objects?)
    // { ACTOR_EN_POH, 3 },         // Poe (composer Flat)
    { ACTOR_EN_RD, 1 },       // Redead (standing)
    { ACTOR_EN_TITE, -1 },    // Tektite (red)
    { ACTOR_EN_BUBBLE, 0 },   // Shabom (bubble)
    { ACTOR_EN_SB, 0 },       // Shell Blade
    { ACTOR_EN_ST, 0 },       // Skulltula (normal)
    { ACTOR_EN_SKJ, 4159 },   // Skull Kid
    { ACTOR_EN_DEKUBABA, 0 }, // Deku Baba (small)
    { ACTOR_EN_SKB, 1 },      // Stalchild (small)
    { ACTOR_EN_MB, -1 },      // Spear Moblin
    { ACTOR_EN_NY, 0 },       // Spike (rolling enemy)
    { ACTOR_EN_TEST, 2 },     // Stalfos
    { ACTOR_EN_EIYER, 10 },   // Stinger (land) (One in formation, sink under floor and do not activate)
    { ACTOR_EN_TP, -1 },      // Electric Tailpasaran
    { ACTOR_EN_BW, 0 },       // Torch Slug
    { ACTOR_EN_WALLMAS, 1 },  // Wallmaster
    { ACTOR_EN_IK, 3 },       // Iron Knuckle (white, standing)
    { ACTOR_EN_WF, 1 },       // Wolfos (white)
    { ACTOR_EN_KAREBABA, 0 }, // Withered Deku Baba
};

static int enemiesToRandomize[] = {
    ACTOR_EN_ANUBICE_TAG, // Anubis
    ACTOR_EN_FIREFLY,     // Keese (including fire/ice)
    ACTOR_EN_TEST,        // Stalfos
    ACTOR_EN_TITE,        // Tektite
    ACTOR_EN_POH,         // Poe (normal, blue rupee, composers)
    ACTOR_EN_OKUTA,       // Octorok
    ACTOR_EN_WALLMAS,     // Wallmaster
    ACTOR_EN_DODONGO,     // Dodongo
    // ACTOR_EN_REEBA,       // Leever (reliant on spawner (z_en_encount1.c))
    ACTOR_EN_PEEHAT,    // Flying Peahat, big one spawning larva, larva
    ACTOR_EN_ZF,        // Lizalfos, Dinolfos
    ACTOR_EN_GOMA,      // Gohma Larva (normal, eggs, gohma eggs)
    ACTOR_EN_BUBBLE,    // Shabom (bubble)
    ACTOR_EN_DODOJR,    // Baby Dodongo
    ACTOR_EN_TORCH2,    // Dark Link
    ACTOR_EN_BILI,      // Biri (small jellyfish)
    ACTOR_EN_TP,        // Electric Tailpasaran
    ACTOR_EN_ST,        // Skulltula (normal, big, invisible)
    ACTOR_EN_BW,        // Torch Slug
    ACTOR_EN_EIYER,     // Stinger (land)
    ACTOR_EN_MB,        // Moblins (Club, spear)
    ACTOR_EN_DEKUBABA,  // Deku Baba (small, large)
    ACTOR_EN_AM,        // Armos (enemy variant)
    ACTOR_EN_DEKUNUTS,  // Mad Scrub (single attack, triple attack)
    ACTOR_EN_VALI,      // Bari (big jellyfish) (spawns very high up)
    ACTOR_EN_BB,        // Bubble (flying skull enemy) (all colors)
    ACTOR_EN_YUKABYUN,  // Flying Floor Tile
    ACTOR_EN_VM,        // Beamos
    ACTOR_EN_FLOORMAS,  // Floormaster
    ACTOR_EN_RD,        // Redead, Gibdo
    ACTOR_EN_SW,        // Skullwalltula
    ACTOR_EN_FD,        // Flare Dancer
    ACTOR_EN_SB,        // Shell Blade
    ACTOR_EN_KAREBABA,  // Withered Deku Baba
    ACTOR_EN_RR,        // Like-Like
    ACTOR_EN_NY,        // Spike (rolling enemy)
    ACTOR_EN_IK,        // Iron Knuckle
    ACTOR_EN_TUBO_TRAP, // Flying pot
    ACTOR_EN_FZ,        // Freezard
    ACTOR_EN_WEIYER,    // Stinger (Water)
    ACTOR_EN_HINTNUTS,  // Hint Deku Scrubs
    ACTOR_EN_WF,        // Wolfos
    ACTOR_EN_SKB,       // Stalchild
    ACTOR_EN_CROW,      // Guay
    ACTOR_EN_SKJ,       // Skull Kid
};

extern "C" uint8_t GetRandomizedEnemy(PlayState* play, int16_t* actorId, f32* posX, f32* posY, f32* posZ, int16_t* rotX,
                                      int16_t* rotY, int16_t* rotZ, int16_t* params) {

    uint32_t isMQ = ResourceMgr_IsSceneMasterQuest(play->sceneNum);

    // Hack to remove enemies that wrongfully spawn because of bypassing object dependency with enemy randomizer on.
    // This should probably be handled on OTR generation in the future when object dependency is fully removed.
    // Remove bats and Skulltulas from graveyard.
    // Remove Octorok in Lost Woods.
    // Remove signs in Gerudo Fortress as child
    if (((*actorId == ACTOR_EN_FIREFLY || (*actorId == ACTOR_EN_SW && *params == 0)) &&
         play->sceneNum == SCENE_GRAVEYARD) ||
        (*actorId == ACTOR_EN_OKUTA && play->sceneNum == SCENE_LOST_WOODS) ||
        (*actorId == ACTOR_EN_KANBAN && play->sceneNum == SCENE_GERUDOS_FORTRESS && LINK_IS_CHILD)) {
        return 0;
    }

    // Hack to change a pot in Spirit Temple that holds a Deku Shield to not hold anything.
    // This should probably be handled on OTR generation in the future when object dependency is fully removed.
    // This Deku Shield doesn't normally spawn in authentic gameplay because of object dependency.
    if (*actorId == ACTOR_OBJ_TSUBO && *params == 24597) {
        *params = 24067;
    }

    // Lengthen timer in non-MQ Jabu Jabu bubble room.
    if (!isMQ && *actorId == ACTOR_OBJ_ROOMTIMER && *params == 30760 && play->sceneNum == SCENE_JABU_JABU &&
        play->roomCtx.curRoom.num == 12) {
        *params = (*params & ~0x3FF) | 120;
    }

    if (IsEnemyFoundToRandomize(play->sceneNum, play->roomCtx.curRoom.num, *actorId, *params, *posX)) {

        // When replacing Iron Knuckles in Spirit Temple, move them away from the throne because
        // some enemies can get stuck on the throne.
        if (*actorId == ACTOR_EN_IK && play->sceneNum == SCENE_SPIRIT_TEMPLE) {
            if (*params == 6657) {
                *posX = *posX + 150;
            } else if (*params == 6401) {
                *posX = *posX - 150;
            }
        }

        // Move like-likes in MQ Jabu Jabu down into the room as they otherwise get stuck on Song of Time blocks.
        if (*actorId == ACTOR_EN_RR && play->sceneNum == SCENE_JABU_JABU && play->roomCtx.curRoom.num == 11) {
            if (*posX == 1003) {
                *posX = *posX - 75;
            } else {
                *posX = *posX + 75;
            }
            *posY = *posY - 200;
        }

        // One enemy in GTG hammer room doesn't raycast properly, move it slightly
        if (*posY == 117 && play->sceneNum == SCENE_GERUDO_TRAINING_GROUND && play->roomCtx.curRoom.num == 5) {
            *posY = 118;
        }

        // Do a raycast from the original position of the actor to find the ground below it, then try to place
        // the new actor on the ground. This way enemies don't spawn very high in the sky, and gives us control
        // over height offsets per enemy from a proven grounded position.
        CollisionPoly poly;
        Vec3f pos;
        f32 raycastResult;

        pos.x = *posX;
        pos.y = *posY + 50;
        pos.z = *posZ;
        if (isMQ && play->sceneNum == SCENE_SPIRIT_TEMPLE && play->roomCtx.curRoom.num == 27) {
            // Similar to the twisted hallway, the turntable here is an actor used as flooring, so the enemy needs
            // spawning on it
            *posY = 50;
        } else {
            raycastResult = OoT_BgCheck_AnyRaycastFloor1(&play->colCtx, &poly, &pos);

            // If ground is found below actor, move actor to that height.
            if (raycastResult > BGCHECK_Y_MIN) {
                *posY = raycastResult;
            }
        }

        // Get randomized enemy ID and parameter.
        // Dark Link's rotation is inconsistent (yawTowardsPlayer), breaking seeded randomizer.
        int16_t seededRotY = (*actorId == ACTOR_EN_TORCH2) ? 6969 : *rotY;
        uint32_t seed =
            play->sceneNum + *actorId + (int)*posX + (int)*posY + (int)*posZ + *rotX + seededRotY + *rotZ + *params;
        EnemyEntry randomEnemy = GetRandomizedEnemyEntry(seed, play);

        *actorId = randomEnemy.id;
        *params = randomEnemy.params;

        // Straighten out enemies so they aren't flipped on their sides when the original spawn is.
        *rotX = 0;

        switch (*actorId) {
            // When spawning big jellyfish, spawn it up high.
            case ACTOR_EN_VALI:
                *posY = *posY + 300;
                break;
            // Spawn Peahat off the ground, otherwise it kills itself by colliding with the ground.
            case ACTOR_EN_PEEHAT:
                if (*params == 1) {
                    *posY = *posY + 100;
                }
                break;
            // Spawn Skulltulas off the ground.
            case ACTOR_EN_ST:
                *posY = *posY + 200;
                break;
            // Spawn flying enemies off the ground.
            case ACTOR_EN_FIREFLY:
            case ACTOR_EN_BILI:
            case ACTOR_EN_BB:
            case ACTOR_EN_CLEAR_TAG:
            case ACTOR_EN_CROW:
                *posY = *posY + 75;
                break;
            // Remove switch flag setting from Wolfos
            case ACTOR_EN_WF:
                *params |= 0xFF00;
                break;
            default:
                break;
        }
    }

    // Enemy finished randomization process.
    return 1;
}

static std::vector<EnemyEntry> selectedEnemyList;

void GetSelectedEnemies() {
    selectedEnemyList.clear();
    for (int i = 0; i < RANDOMIZED_ENEMY_SPAWN_TABLE_SIZE; i++) {
        if (CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemyList.All"), 0)) {
            selectedEnemyList.push_back(randomizedEnemySpawnTable[i]);
        } else if (CVarGetInteger(enemyCVarList[i], 1)) {
            selectedEnemyList.push_back(randomizedEnemySpawnTable[i]);
        }
    }
    if (selectedEnemyList.size() == 0) {
        selectedEnemyList.push_back(randomizedEnemySpawnTable[0]);
    }
}

EnemyEntry GetRandomizedEnemyEntry(uint32_t seed, PlayState* play) {
    std::vector<EnemyEntry> filteredEnemyList = {};
    if (selectedEnemyList.size() == 0) {
        GetSelectedEnemies();
    }
    for (EnemyEntry enemy : selectedEnemyList) {
        if (IsEnemyAllowedToSpawn(play->sceneNum, play->roomCtx.curRoom.num, enemy)) {
            filteredEnemyList.push_back(enemy);
        }
    }
    if (filteredEnemyList.size() == 0) {
        filteredEnemyList = selectedEnemyList;
    }
    if (CVAR_ENEMY_RANDOMIZER_VALUE == ENEMY_RANDOMIZER_RANDOM_SEEDED) {
        uint64_t randomState = 0;

        ShipUtils::RandInit(
            seed + (IS_RANDO ? Rando::Context::GetInstance()->GetSeed() : gSaveContext.ship.stats.fileCreatedAt),
            &randomState);

        return ShipUtils::RandomElement(filteredEnemyList, false, &randomState);
    }

    return ShipUtils::RandomElement(filteredEnemyList, false);
}

bool IsEnemyFoundToRandomize(int16_t sceneNum, int8_t roomNum, int16_t actorId, int16_t params, float posX) {

    uint32_t isMQ = ResourceMgr_IsSceneMasterQuest(sceneNum);

    for (int i = 0; i < ARRAY_COUNT(enemiesToRandomize); i++) {

        if (actorId == enemiesToRandomize[i]) {

            switch (actorId) {
                // Only randomize the main component of Electric Tailparasans, not the tail segments they spawn.
                case ACTOR_EN_TP:
                    return (params == -1);
                // Only randomize the initial Deku Scrub actor (single and triple attack), not the flower they spawn.
                case ACTOR_EN_DEKUNUTS:
                    return (params == -256 || params == 768);
                // Don't randomize the OoB wallmaster in the Silver Rupee room because it's only there to
                // not trigger unlocking the door after killing the other wallmaster in authentic gameplay.
                case ACTOR_EN_WALLMAS:
                    return (!(!isMQ && sceneNum == SCENE_GERUDO_TRAINING_GROUND && roomNum == 2 && posX == -2345));
                // Only randomize initial Floormaster actor (it can split and does some spawning on init).
                case ACTOR_EN_FLOORMAS:
                    return (params == 0 || params == -32768);
                // Only randomize the initial eggs, not the enemies that spawn from them.
                case ACTOR_EN_GOMA:
                    return (params >= 0 && params <= 9);
                // Only randomize Skullwalltulas, not Golden Skulltulas.
                case ACTOR_EN_SW:
                    return (params == 0);
                // Don't randomize Nabooru because it'll break the cutscene and the door.
                // Don't randomize Iron Knuckle in MQ Spirit Trial because it's needed to
                // break the thrones in the room to access a button.
                case ACTOR_EN_IK:
                    return (params != 1280 && !(isMQ && sceneNum == SCENE_INSIDE_GANONS_CASTLE && roomNum == 17));
                // Only randomize the initial spawn of the huge jellyfish. It spawns another copy when hit with a sword.
                case ACTOR_EN_VALI:
                    return (params == -1);
                // Don't randomize Lizalfos in Dodongo's Cavern because the gates won't work correctly otherwise.
                case ACTOR_EN_ZF:
                    return (params != 1280 && params != 1281 && params != 1536 && params != 1537);
                // Don't randomize the Wolfos in SFM because it's needed to open the gate.
                case ACTOR_EN_WF:
                    return (params != 7936);
                // Don't randomize the Stalfos in Forest Temple because other enemies fall through the hole and don't
                // trigger the platform. Don't randomize the Stalfos spawning on the boat in Shadow Temple, as
                // randomizing them places the new enemies down in the river.
                case ACTOR_EN_TEST:
                    return (params != 1 && !(sceneNum == SCENE_SHADOW_TEMPLE && roomNum == 21));
                // Only randomize the enemy variant of Armos Statue.
                // Leave one Armos unrandomized in the Spirit Temple room where an armos is needed to push down a
                // button.
                case ACTOR_EN_AM:
                    return ((params == -1 || params == 255) && !(sceneNum == SCENE_SPIRIT_TEMPLE && posX == 2141));
                // Don't randomize Shell Blades and Spikes in the underwater portion in Water Temple as it's impossible
                // to kill most other enemies underwater with just hookshot and they're required to be killed for a
                // grate to open.
                case ACTOR_EN_SB:
                case ACTOR_EN_NY:
                    return (!(!isMQ && sceneNum == SCENE_WATER_TEMPLE && roomNum == 2));
                case ACTOR_EN_SKJ:
                    return !(sceneNum == SCENE_LOST_WOODS && LINK_IS_CHILD);
                default:
                    return 1;
            }
        }
    }

    // If no enemy is found, don't randomize the actor.
    return 0;
}

bool IsEnemyAllowedToSpawn(int16_t sceneNum, int8_t roomNum, EnemyEntry enemy) {
    uint32_t isMQ = ResourceMgr_IsSceneMasterQuest(sceneNum);

    // Freezard - Child Link can only kill this with jump slash Deku Sticks or other equipment like bombs.
    // Beamos - Needs bombs.
    // Anubis - Needs fire.
    // Shell Blade & Spike - Child Link can't kill these with sword or Deku Stick.
    // Flare dancer, Arwing & Dark Link - Both go out of bounds way too easily, softlocking the player.
    // Wallmaster - Not easily visible, often makes players think they're softlocked and that there's no enemies left.
    // Club Moblin - Many issues with them falling or placing out of bounds. Maybe fixable in the future?
    // Withered Deku Baba - Stationary, can get stuck in air/on chests, softlock risk.
    bool enemiesToExcludeClearRooms =
        enemy.id == ACTOR_EN_FZ || enemy.id == ACTOR_EN_VM || enemy.id == ACTOR_EN_SB || enemy.id == ACTOR_EN_NY ||
        enemy.id == ACTOR_EN_CLEAR_TAG || enemy.id == ACTOR_EN_WALLMAS || enemy.id == ACTOR_EN_TORCH2 ||
        (enemy.id == ACTOR_EN_MB && enemy.params == 0) || enemy.id == ACTOR_EN_FD || enemy.id == ACTOR_EN_ANUBICE_TAG ||
        enemy.id == ACTOR_EN_KAREBABA;

    // Bari - Spawns 3 more enemies, potentially extremely difficult in timed rooms.
    bool enemiesToExcludeTimedRooms = enemiesToExcludeClearRooms || enemy.id == ACTOR_EN_VALI;

    // Don't allow Lizalfos in the silver rupee bridge room of spirit, as they spawn in the room above
    if (sceneNum == SCENE_SPIRIT_TEMPLE && roomNum == 2 && enemy.id == ACTOR_EN_ZF && enemy.params == -1) {
        return false;
    }

    switch (sceneNum) {
        // Deku Tree
        case SCENE_DEKU_TREE:
            return (!(!isMQ && enemiesToExcludeClearRooms && (roomNum == 1 || roomNum == 9)) &&
                    !(isMQ && enemiesToExcludeClearRooms &&
                      (roomNum == 4 || roomNum == 6 || roomNum == 9 || roomNum == 10)));
        // Dodongo's Cavern
        case SCENE_DODONGOS_CAVERN:
            return (!(!isMQ && enemiesToExcludeClearRooms && roomNum == 15) &&
                    !(isMQ && enemiesToExcludeClearRooms && (roomNum == 5 || roomNum == 13 || roomNum == 14)));
        // Jabu Jabu
        case SCENE_JABU_JABU:
            return (!(!isMQ && enemiesToExcludeClearRooms && (roomNum == 8 || roomNum == 9)) &&
                    !(!isMQ && enemiesToExcludeTimedRooms && roomNum == 12) &&
                    !(isMQ && enemiesToExcludeClearRooms && (roomNum == 11 || roomNum == 14)));
        // Forest Temple
        case SCENE_FOREST_TEMPLE:
            return (!(!isMQ && enemiesToExcludeClearRooms &&
                      (roomNum == 6 || roomNum == 10 || roomNum == 18 || roomNum == 21)) &&
                    !(isMQ && enemiesToExcludeClearRooms &&
                      (roomNum == 5 || roomNum == 6 || roomNum == 18 || roomNum == 21)));
        // Fire Temple
        case SCENE_FIRE_TEMPLE:
            return (!(!isMQ && enemiesToExcludeClearRooms && roomNum == 15) &&
                    !(isMQ && enemiesToExcludeClearRooms && (roomNum == 15 || roomNum == 17 || roomNum == 18)));
        // Water Temple
        case SCENE_WATER_TEMPLE:
            return (!(!isMQ && enemiesToExcludeClearRooms && (roomNum == 13 || roomNum == 18 || roomNum == 19)) &&
                    !(isMQ && enemiesToExcludeClearRooms && (roomNum == 13 || roomNum == 18)));
        // Spirit Temple
        case SCENE_SPIRIT_TEMPLE:
            return (!(!isMQ && enemiesToExcludeClearRooms &&
                      (roomNum == 1 || roomNum == 10 || roomNum == 17 || roomNum == 20)) &&
                    !(isMQ && enemiesToExcludeClearRooms &&
                      (roomNum == 1 || roomNum == 2 || roomNum == 4 || roomNum == 10 || roomNum == 15 ||
                       roomNum == 19 || roomNum == 20)));
        // Shadow Temple
        case SCENE_SHADOW_TEMPLE:
            return (
                !(!isMQ && enemiesToExcludeClearRooms &&
                  (roomNum == 1 || roomNum == 7 || roomNum == 11 || roomNum == 14 || roomNum == 16 || roomNum == 17 ||
                   roomNum == 19 || roomNum == 20)) &&
                !(isMQ && enemiesToExcludeClearRooms &&
                  (roomNum == 1 || roomNum == 6 || roomNum == 7 || roomNum == 11 || roomNum == 14 || roomNum == 20)));
        // Ganon's Castle Trials
        case SCENE_INSIDE_GANONS_CASTLE:
            return (!(!isMQ && enemiesToExcludeClearRooms && (roomNum == 2 || roomNum == 5 || roomNum == 9)) &&
                    !(isMQ && enemiesToExcludeClearRooms &&
                      (roomNum == 0 || roomNum == 2 || roomNum == 5 || roomNum == 9)));
        // Ice Caverns
        case SCENE_ICE_CAVERN:
            return (!(!isMQ && enemiesToExcludeClearRooms && (roomNum == 1 || roomNum == 7)) &&
                    !(isMQ && enemiesToExcludeClearRooms && (roomNum == 3 || roomNum == 7)));
        // Bottom of the Well
        // Exclude Dark Link from room with holes in the floor because it can pull you in a like-like making the player
        // fall down.
        case SCENE_BOTTOM_OF_THE_WELL:
            return (!(!isMQ && enemy.id == ACTOR_EN_TORCH2 && roomNum == 3));
        // Don't allow Dark Link in areas with lava void out zones as it voids out the player as well.
        // Gerudo Training Ground.
        case SCENE_GERUDO_TRAINING_GROUND:
            return (!(enemy.id == ACTOR_EN_TORCH2 && roomNum == 6) &&
                    !(!isMQ && enemiesToExcludeTimedRooms && (roomNum == 1 || roomNum == 7)) &&
                    !(!isMQ && enemiesToExcludeClearRooms && (roomNum == 3 || roomNum == 5 || roomNum == 10)) &&
                    !(isMQ && enemiesToExcludeTimedRooms &&
                      (roomNum == 1 || roomNum == 3 || roomNum == 5 || roomNum == 7)) &&
                    !(isMQ && enemiesToExcludeClearRooms && roomNum == 10));
        // Don't allow certain enemies in Ganon's Tower because they would spawn up on the ceiling,
        // becoming impossible to kill.
        // Ganon's Tower.
        case SCENE_GANONS_TOWER:
            return (!(enemiesToExcludeClearRooms || enemy.id == ACTOR_EN_VALI ||
                      (enemy.id == ACTOR_EN_ZF && enemy.params == -1)));
        // Ganon's Tower Escape.
        case SCENE_GANONS_TOWER_COLLAPSE_INTERIOR:
            return (!((enemiesToExcludeTimedRooms || (enemy.id == ACTOR_EN_ZF && enemy.params == -1)) && roomNum == 1));
        // Don't allow big Stalchildren, big Peahats and the large Bari (jellyfish) during the Gohma fight because they
        // can clip into Gohma and it crashes the game. Likely because Gohma on the ceiling can't handle collision with
        // other enemies.
        case SCENE_DEKU_TREE_BOSS:
            return (!enemiesToExcludeTimedRooms && !(enemy.id == ACTOR_EN_SKB && enemy.params == 20) &&
                    !(enemy.id == ACTOR_EN_PEEHAT && enemy.params == -1));
        // Grottos.
        case SCENE_GROTTOS:
            return (!(enemiesToExcludeClearRooms && (roomNum == 2 || roomNum == 7)));
        // Royal Grave.
        case SCENE_ROYAL_FAMILYS_TOMB:
            return (!(enemiesToExcludeClearRooms && roomNum == 0));
        // Don't allow Dark Link in areas with lava void out zones as it voids out the player as well.
        // Death Mountain Crater.
        case SCENE_DEATH_MOUNTAIN_CRATER:
            return (enemy.id != ACTOR_EN_TORCH2);
        default:
            return 1;
    }
}

void FixClubMoblinScale(void* ptr) {
    Actor* actor = (Actor*)ptr;
    if (actor->params == -1) {
        OoT_Actor_SetScale(actor, 0.014f);
    }
}

// EnTp, EnSkb and EnPeehat update collider position only in their draw functions,
// causes 0,0,0 issues, so set their collider positions once on actor init
void EnemyRando_SetColliderPosTp(void* ptr) {
    EnTp* enTp = (EnTp*)ptr;

    enTp->collider.elements[0].dim.worldSphere.center.x = (s16)enTp->actor.world.pos.x;
    enTp->collider.elements[0].dim.worldSphere.center.y = (s16)enTp->actor.world.pos.y;
    enTp->collider.elements[0].dim.worldSphere.center.z = (s16)enTp->actor.world.pos.z;
    enTp->collider.elements[0].dim.worldSphere.radius =
        (s16)(enTp->collider.elements[0].dim.modelSphere.radius * enTp->collider.elements[0].dim.scale);
}

void EnemyRando_SetColliderPosSkb(void* ptr) {
    EnSkb* enSkb = (EnSkb*)ptr;
    u8 i;

    for (i = 0; i < 2; i++) {
        enSkb->collider.elements[i].dim.worldSphere.center.x = (s16)enSkb->actor.world.pos.x;
        enSkb->collider.elements[i].dim.worldSphere.center.y = (s16)enSkb->actor.world.pos.y;
        enSkb->collider.elements[i].dim.worldSphere.center.z = (s16)enSkb->actor.world.pos.z;
        enSkb->collider.elements[i].dim.worldSphere.radius =
            (s16)(enSkb->collider.elements[i].dim.modelSphere.radius * enSkb->collider.elements[i].dim.scale);
    }
}

void EnemyRando_SetColliderPosPeehat(void* ptr) {
    EnPeehat* enPeehat = (EnPeehat*)ptr;
    u8 i;

    for (i = 0; i < 4; i++) {
        enPeehat->colQuad.dim.quad[i].x = enPeehat->actor.world.pos.x + (f32)i;
        enPeehat->colQuad.dim.quad[i].y = enPeehat->actor.world.pos.y + (f32)i;
        enPeehat->colQuad.dim.quad[i].z = enPeehat->actor.world.pos.z + (f32)i;
    }
    OoT_Collider_SetQuadVertices(&enPeehat->colQuad, &enPeehat->colQuad.dim.quad[0], &enPeehat->colQuad.dim.quad[1],
                                 &enPeehat->colQuad.dim.quad[2], &enPeehat->colQuad.dim.quad[3]);
}

static void OnGerudoFighterDefeat(void* refActor) {
    EnGeldB* enGeldB = reinterpret_cast<EnGeldB*>(refActor);

    if (enGeldB->keyFlag == 0) {
        OoT_Item_DropCollectibleRandom(OoT_gPlayState, &enGeldB->actor, &enGeldB->actor.world.pos, 0xC0);
    }
}

void RegisterEnemyRandomizer() {
    COND_ID_HOOK(OnActorInit, ACTOR_EN_MB, CVAR_ENEMY_RANDOMIZER_VALUE, FixClubMoblinScale);
    COND_ID_HOOK(OnActorInit, ACTOR_EN_TP, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT,
                 EnemyRando_SetColliderPosTp);
    COND_ID_HOOK(OnActorInit, ACTOR_EN_SKB, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT,
                 EnemyRando_SetColliderPosSkb);
    COND_ID_HOOK(OnActorInit, ACTOR_EN_PEEHAT, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT,
                 EnemyRando_SetColliderPosPeehat);

    // prevent dark link from triggering a voidout
    COND_VB_SHOULD(VB_TRIGGER_VOIDOUT, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT, {
        Actor* actor = va_arg(args, Actor*);

        if (*should == true && actor->category != ACTORCAT_PLAYER) {
            // Improvement opportunity: Play sfx
            *should = false;
            OoT_Actor_Kill(actor);
        }
    });

    // prevent dark link dealing fall damage to the player
    COND_VB_SHOULD(VB_RECIEVE_FALL_DAMAGE, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT, {
        Actor* actor = va_arg(args, Actor*);

        if (actor->category != ACTORCAT_PLAYER) {
            *should = false;
        }
    });

    // prevent dark link from interfering with HESS/recoil/etc when at more than 100 away from him
    COND_VB_SHOULD(VB_TORCH2_HANDLE_CLANKING, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT, {
        Actor* darkLink = va_arg(args, Actor*);

        if (darkLink->xzDistToPlayer > 100.0f) {
            *should = false;
        }
    });

    // prevent dark link from interfering with ice floors
    COND_VB_SHOULD(VB_SET_STATIC_PREV_FLOOR_TYPE, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT, {
        Player* playerOrDarkLink = va_arg(args, Player*);

        if (playerOrDarkLink->actor.id != ACTOR_PLAYER) {
            *should = false;
        }
    });

    // prevent dark link from interfering with ice floors
    COND_VB_SHOULD(VB_SET_STATIC_FLOOR_TYPE, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT, {
        Player* playerOrDarkLink = va_arg(args, Player*);

        if (playerOrDarkLink->actor.id != ACTOR_PLAYER) {
            *should = false;
        }
    });

    // prevent dark link from being grabbed by like likes and therefore grabbing the player
    COND_VB_SHOULD(VB_LIKE_LIKE_GRAB_PLAYER, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT, {
        EnRr* likeLike = va_arg(args, EnRr*);

        if (!(likeLike->collider1.base.oc != NULL && likeLike->collider1.base.oc->category == ACTORCAT_PLAYER) &&
            !(likeLike->collider2.base.oc != NULL && likeLike->collider2.base.oc->category == ACTORCAT_PLAYER)) {
            *should = false;
        }
    });

    // Allow Random Gerudo Fighters (contain no keys) to spawn without any switch flags
    COND_VB_SHOULD(VB_GERUDO_FIGHTER_CONTINUE_WAITING, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT, {
        EnGeldB* enGeldB = va_arg(args, EnGeldB*);

        if (enGeldB->keyFlag == 0) {
            if (!enGeldB->invisible || enGeldB->actor.xzDistToPlayer <= 300.0f) {
                *should = false;
            }
        }
    });

    // Don't play Miniboss music for Random Gerudo Fighters
    COND_VB_SHOULD(VB_GERUDO_FIGHTER_PLAY_MINIBOSS_MUSIC, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT,
                   {
                       EnGeldB* enGeldB = va_arg(args, EnGeldB*);

                       if (enGeldB->keyFlag == 0) {
                           *should = false;
                       }
                   });

    // If Random Gerudo Fighters knock Link down, void him out like Wallmasters
    COND_VB_SHOULD(VB_GERUDO_FIGHTER_THROW_LINK_TO_JAIL, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT, {
        EnGeldB* enGeldB = va_arg(args, EnGeldB*);

        if (enGeldB->keyFlag == 0) {
            *should = false;
            Sfx_PlaySfxCentered(NA_SE_OC_ABYSS);
            Play_TriggerRespawn(OoT_gPlayState);
        }
    });

    // If Random Gerudo Fighters are defeated, drop some items
    COND_ID_HOOK(OnEnemyDefeat, ACTOR_EN_GELDB, CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT,
                 OnGerudoFighterDefeat);
}

static RegisterShipInitFunc initFunc(RegisterEnemyRandomizer, { CVAR_ENEMY_RANDOMIZER_NAME });

// If scale > normal, start downscaling already during death fall
void EnemyRandoSize_KeeseFallScale(void* ptr) {
    EnFirefly* enFirefly = (EnFirefly*)ptr;
    if (enFirefly->actionFunc == OoT_EnFirefly_Fall && enFirefly->actor.scale.x > 0.005f) {
        OoT_Math_StepToF(&enFirefly->actor.scale.x, 0.005f, 0.00034f);
        enFirefly->actor.scale.y = enFirefly->actor.scale.z = enFirefly->actor.scale.x;
    }
}

static void RegisterEnemyRandoSizeFixes() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_EN_FIREFLY, CVAR_RANDO_ENEMY_SIZE_VALUE, EnemyRandoSize_KeeseFallScale);

    // Guays try to die on any damage, but EnCrow_Update doesn't let them reach ground if health != 0
    // Makeshift "damaged but not dead" action setup
    COND_VB_SHOULD(VB_GUAY_SETUP_DAMAGED, CVAR_RANDO_ENEMY_SIZE_VALUE && CVAR_ENEMY_SCALE_HEALTH_VALUE, {
        EnCrow* enCrow = va_arg(args, EnCrow*);

        if (enCrow->actor.colChkInfo.damage < enCrow->actor.colChkInfo.health) {
            *should = false;
            OoT_Actor_ApplyDamage(&enCrow->actor);
            enCrow->actor.colorFilterTimer = 40;
            OoT_Actor_SetColorFilter(&enCrow->actor, 0x4000, 255, 0, 40);
            Audio_PlayActorSound2(&enCrow->actor, NA_SE_EN_KAICHO_DEAD);
            enCrow->timer = 50;
            enCrow->actor.speedXZ = 3.5f;
            enCrow->aimRotX = -0x1000;
            enCrow->aimRotY = enCrow->actor.yawTowardsPlayer + 0x8000;
            enCrow->skelAnime.playSpeed = 2.0f;
            enCrow->actionFunc = OoT_EnCrow_TurnAway;
        }
    });

    // Clamp distance from ground due to hard to hit with Kokiri
    COND_VB_SHOULD(VB_GUAY_ALIVE_MOVE_HEIGHT_OFFSET, CVAR_RANDO_ENEMY_SIZE_VALUE && CVAR_ENEMY_SCALE_HEALTH_VALUE, {
        f32* scale = va_arg(args, f32*);

        if (*should && *scale > 2.0f) {
            *scale = 2.0f;
        }
    });

    // Keese try to fall/die on any damage which behaves oddly if health is above normal 1
    // Makeshift "damaged but not dead" action setup
    COND_VB_SHOULD(VB_KEESE_SETUP_FALL, CVAR_RANDO_ENEMY_SIZE_VALUE && CVAR_ENEMY_SCALE_HEALTH_VALUE, {
        EnFirefly* enFirefly = va_arg(args, EnFirefly*);

        if (*should && enFirefly->actor.colChkInfo.health != 0) {
            *should = false;
            Audio_PlayActorSound2(&enFirefly->actor, NA_SE_EN_FFLY_DEAD);
            OoT_Actor_SetColorFilter(&enFirefly->actor, 0x4000, 0xFF, 0, 40);
            OoT_EnFirefly_SetupFlyAway(enFirefly);
        }
    });

    // Ensure Keese just die on Ice Arrows
    COND_VB_SHOULD(VB_KEESE_SETUP_FROZENFALL, CVAR_RANDO_ENEMY_SIZE_VALUE && CVAR_ENEMY_SCALE_HEALTH_VALUE, {
        EnFirefly* enFirefly = va_arg(args, EnFirefly*);
        enFirefly->actor.colChkInfo.health = 0;
    });
}

static RegisterShipInitFunc initSizeFixesFunc(RegisterEnemyRandoSizeFixes,
                                              { CVAR_RANDO_ENEMY_SIZE_NAME, CVAR_ENEMY_SCALE_HEALTH_NAME });
