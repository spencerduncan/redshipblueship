/**
 * @file mm_oot_masks_test.cpp
 * MM's half of the OoT-masks-in-MM locks (#980, epic #975 P5; the items are
 * 2s2h/Rando/OoTMasksMM.cpp, the rules RSBS_XITEM_GERUDO_MASK_MM ... in
 * src/common/foreign_items.h).
 *
 * THE HELPERS the redship rows `oot-masks-mm-rule` and `oot-masks-mm-items`
 * (src/common/tests/test_oot_masks_mm.c) drive MM's REAL give, conversion,
 * classification, ownership and actor seam through. They only name MM's ids and
 * put a heap Player in a known state; every decision is the production code's.
 * They live here because only an MM TU has MM's enums, `gSaveContext` and
 * `Player` in scope.
 *
 * THE LOGIC LEG (`oot-masks-mm-logic`, rando tier, MM_OoTMasksLogic_RunHeadless):
 * MM's REAL region lambdas, evaluated under a hand-picked time mask after
 * MM_Rando_InitCore populates the region graph (mm_logic_gates_test.cpp's
 * bring-up): OoTMM's can_enter_deku_palace, can_evade_gerudo and
 * can_pass_gibdo's night term, each with a control that opens the same edge the
 * native way, so a probe that evaluates nothing fails.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "global.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Logic/Logic.h"
#include "Rando/ForeignMaskMM.h"
#include "Rando/OoTMasksMM.h"

#include "context.h"       // src/common: ComboContext_Init
#include "foreign_items.h" // src/common: the rule bits, Combo_FreezeComboSettingsEx
#include "foreign_masks.h" // src/common: the worn-mask ids

extern "C" {
#include "variables.h"
}

extern "C" void MM_Rando_InitCore(void);

namespace {

std::unique_ptr<Player> sOmPlayer;

Player* OmPlayer() {
    if (!sOmPlayer) {
        sOmPlayer = std::make_unique<Player>();
        std::memset(sOmPlayer.get(), 0, sizeof(Player));
        sOmPlayer->transformation = PLAYER_FORM_HUMAN;
    }
    return sOmPlayer.get();
}

} // namespace

/** MM ids by index: 0..2 the three masks, 3 RI_WOODFALL_STRAY_FAIRY (the last
 *  id before them), 4 RI_MAX_TRAP, 5 RI_MAX, 6 RI_MASK_STONE, 7 RI_ABILITY_SWIM,
 *  8 RI_JUNK; -1 past the table. */
extern "C" int MM_OoTMasksTest_Id(int which) {
    static const int kIds[] = { RI_MASK_GERUDO,          RI_MASK_SKULL,   RI_MASK_SPOOKY,
                                RI_WOODFALL_STRAY_FAIRY, RI_MAX_TRAP,     RI_MAX,
                                RI_MASK_STONE,           RI_ABILITY_SWIM, RI_JUNK };
    return (which >= 0 && which < (int)(sizeof(kIds) / sizeof(kIds[0]))) ? kIds[which] : -1;
}

/** MM actor ids by index: the eleven listed actors in MM_OoTMasks_SpoofFor's order
 *  (EnGe2, EnLookNuts, EnFirefly, EnCrow, EnBat, EnGuardNuts, EnDns, EnRailgibud,
 *  EnTalkGibud, EnRd, EnGb2), then two that are NOT listed: 11 EnGe3 (Aveil) and
 *  12 EnKaizoku; -1 past the table. */
extern "C" int MM_OoTMasksTest_ActorId(int which) {
    static const int kIds[] = { ACTOR_EN_GE2,        ACTOR_EN_LOOK_NUTS,  ACTOR_EN_FIREFLY, ACTOR_EN_CROW,
                                ACTOR_EN_BAT,        ACTOR_EN_GUARD_NUTS, ACTOR_EN_DNS,     ACTOR_EN_RAILGIBUD,
                                ACTOR_EN_TALK_GIBUD, ACTOR_EN_RD,         ACTOR_EN_GB2,     ACTOR_EN_GE3,
                                ACTOR_EN_KAIZOKU };
    return (which >= 0 && which < (int)(sizeof(kIds) / sizeof(kIds[0]))) ? kIds[which] : -1;
}

/** PLAYER_MASK_STONE (0), PLAYER_MASK_GIBDO (1), PLAYER_FORM_DEKU (2),
 *  PLAYER_FORM_HUMAN (3), PLAYER_MASK_NONE (4). */
extern "C" int MM_OoTMasksTest_Const(int which) {
    switch (which) {
        case 0:
            return PLAYER_MASK_STONE;
        case 1:
            return PLAYER_MASK_GIBDO;
        case 2:
            return PLAYER_FORM_DEKU;
        case 3:
            return PLAYER_FORM_HUMAN;
        default:
            return PLAYER_MASK_NONE;
    }
}

/** MM's live save as a cross-game MM session runs it: human, every mask slot
 *  empty, every RandoSaveInfo byte zero. */
extern "C" void MM_OoTMasksTest_ArmLive(void) {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    std::memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE,
                sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    Player* player = OmPlayer();
    std::memset(player, 0, sizeof(Player));
    player->transformation = PLAYER_FORM_HUMAN;
    player->currentMask = PLAYER_MASK_NONE;
}

/** MM's REAL give (Rando::GiveItem) of item `ri`. */
extern "C" void MM_OoTMasksTest_Give(uint16_t ri) {
    Rando::GiveItem((RandoItemId)ri);
}

/** MM's REAL conversion (Rando::ConvertItem) of item `ri`, no check. */
extern "C" int MM_OoTMasksTest_Convert(uint16_t ri) {
    return (int)Rando::ConvertItem((RandoItemId)ri);
}

/** The Items row's type: 1 RITYPE_MASK, 0 another type, -1 no row. */
extern "C" int MM_OoTMasksTest_IsMaskType(uint16_t ri) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)ri);
    if (it == Rando::StaticData::Items.end()) {
        return -1;
    }
    return it->second.randoItemType == RITYPE_MASK ? 1 : 0;
}

/** The Items row's display name, or NULL. */
extern "C" const char* MM_OoTMasksTest_Name(uint16_t ri) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)ri);
    return it == Rando::StaticData::Items.end() ? nullptr : it->second.name;
}

/** Is the Items row's junk conversion target junk (RITYPE_JUNK)? */
extern "C" int MM_OoTMasksTest_IsJunk(int ri) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)ri);
    return (it != Rando::StaticData::Items.end() && it->second.randoItemType == RITYPE_JUNK) ? 1 : 0;
}

/** Own (or not) MM's Stone (slot 1) or Gibdo (slot 2) Mask. */
extern "C" void MM_OoTMasksTest_SetNativeOwned(uint8_t slot, int owned) {
    const int item = slot == COMBO_FOREIGN_MASK_SLOT_MM_STONE ? ITEM_MASK_STONE : ITEM_MASK_GIBDO;
    INV_CONTENT(item) = owned ? (u8)item : (u8)ITEM_NONE;
}

/** The mask-page cell (0-based) of shared slot `slot`'s own mask. */
extern "C" int MM_OoTMasksTest_PageCell(uint8_t slot) {
    const int item = slot == COMBO_FOREIGN_MASK_SLOT_MM_STONE ? ITEM_MASK_STONE : ITEM_MASK_GIBDO;
    return SLOT(item) - ITEM_NUM_SLOTS;
}

/** The inventory byte of mask-page cell `cell`. */
extern "C" uint8_t MM_OoTMasksTest_CellStored(int cell) {
    return gSaveContext.save.saveInfo.inventory.items[cell + ITEM_NUM_SLOTS];
}

/** ITEM_MASK_STONE (0), ITEM_MASK_GIBDO (1), ITEM_NONE (2). */
extern "C" uint8_t MM_OoTMasksTest_ItemConst(int which) {
    return which == 0 ? (uint8_t)ITEM_MASK_STONE : which == 1 ? (uint8_t)ITEM_MASK_GIBDO : (uint8_t)ITEM_NONE;
}

extern "C" void MM_OoTMasksTest_SetPlayer(int human, uint8_t mask) {
    Player* player = OmPlayer();
    player->transformation = human ? PLAYER_FORM_HUMAN : PLAYER_FORM_DEKU;
    player->currentMask = mask;
}

extern "C" int MM_OoTMasksTest_PlayerMask(void) {
    return (int)OmPlayer()->currentMask;
}

extern "C" int MM_OoTMasksTest_PlayerForm(void) {
    return (int)OmPlayer()->transformation;
}

extern "C" int MM_OoTMasksTest_Use(uint8_t mask) {
    return MM_ForeignMask_Use(OmPlayer(), mask);
}

extern "C" int MM_OoTMasksTest_EquipNative(int maskId) {
    return MM_ForeignMask_OnEquipNative(OmPlayer(), maskId);
}

extern "C" int MM_OoTMasksTest_Before(int16_t actorId, int updates, int isNight) {
    return MM_OoTMasks_BeforeActorUpdate(OmPlayer(), actorId, updates, isNight);
}

extern "C" void MM_OoTMasksTest_PutBack(void) {
    MM_OoTMasks_PutBack(OmPlayer());
}

namespace {

int sOmFailures = 0;

void OmFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-OOT-MASKS-LOGIC] FAIL: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    sOmFailures++;
}

void FreezeRules(uint32_t flags) {
    ComboContext_Init();
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    std::memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = flags;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

/** An empty save: every slot ITEM_NONE (a zeroed slot reads as holding item 0),
 *  options and tricks zeroed, human form. */
void ResetSave() {
    std::memset(&gSaveContext, 0, sizeof(gSaveContext));
    std::memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE,
                sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
}

bool Edge(RandoRegionId from, RandoRegionId to, bool* found) {
    auto& region = Rando::Logic::Regions[from];
    const auto it = region.connections.find(to);
    if (it == region.connections.end()) {
        *found = false;
        return false;
    }
    *found = true;
    return it->second.first();
}

void Expect(const char* leg, RandoRegionId from, RandoRegionId to, bool expected, const char* why) {
    bool found = false;
    const bool got = Edge(from, to, &found);
    if (!found) {
        OmFail("%s: the edge is not in the region graph, so this leg probes nothing", leg);
        return;
    }
    if (got != expected) {
        OmFail("%s: the edge is %s: %s", leg, got ? "open" : "closed", why);
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "open" : "closed");
}

} // namespace

/**
 * The logic leg. Returns 0 on success, the failure count otherwise.
 *  D. Deku Palace (South.cpp, OoTMM can_enter_deku_palace): from the outside to
 *     the inside, human, no Deku Mask: closed with the Skull Mask owned and its
 *     rule off; closed with the rule on and nothing owned; open with both; open
 *     with the Deku Mask alone (the control).
 *  G. Pirates' Fortress plaza tower (PiratesFortress.cpp, OoTMM can_evade_gerudo):
 *     closed empty-handed; open with the Gerudo Mask under its rule; open with the
 *     Stone Mask (the control).
 *  S. Beneath the Well (BeneathTheWell.cpp, OoTMM can_pass_gibdo): the Freezard
 *     room's Gibdo door with the Spooky Mask under its rule, open at night and
 *     closed by day; the Gibdo Mask opens it by day (the control).
 */
extern "C" int MM_OoTMasksLogic_RunHeadless(void) {
    printf("[TEST] oot-masks-mm-logic: the Skull Mask opens the Deku Palace, the Gerudo Mask gets past the purple "
           "guard, and the Spooky Mask passes the Gibdos at night only, each under its frozen rule (#980)\n");
    sOmFailures = 0;
    MM_Rando_InitCore();
    if (gRegEditor == NULL) {
        static RegEditor sProbeRegEditor = {};
        gRegEditor = &sProbeRegEditor;
    }
    auto saved = std::make_unique<SaveContext>();
    std::memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    const uint64_t savedTime = Rando::Logic::gCurrentRegionTime;
    const uint32_t kAll = RSBS_XITEM_GERUDO_MASK_MM | RSBS_XITEM_SKULL_MASK_MM | RSBS_XITEM_SPOOKY_MASK_MM;
    const uint64_t kEveryTime = Rando::Logic::TIME_ALL_SLICES;
    const uint64_t kDay = Rando::Logic::TIME_BIT_ONE << Rando::Logic::TIME_DAY1_AM_06_00;
    const uint64_t kNight = Rando::Logic::TIME_BIT_ONE << Rando::Logic::TIME_NIGHT1_PM_06_00;

    // ---- D. the Deku Palace -------------------------------------------------
    Rando::Logic::gCurrentRegionTime = kEveryTime;
    ResetSave();
    FreezeRules(kAll & ~RSBS_XITEM_SKULL_MASK_MM);
    MM_OoTMask_Give(RI_MASK_SKULL);
    Expect("D1 Skull Mask owned, rule off", RR_DEKU_PALACE_OUTSIDE, RR_DEKU_PALACE_INSIDE_LOWER, false,
           "a mask the world did not add opened the palace");
    ResetSave();
    FreezeRules(kAll);
    Expect("D2 rule on, nothing owned", RR_DEKU_PALACE_OUTSIDE, RR_DEKU_PALACE_INSIDE_LOWER, false,
           "the palace opened for a human with nothing");
    MM_OoTMask_Give(RI_MASK_SKULL);
    Expect("D3 rule on, Skull Mask owned", RR_DEKU_PALACE_OUTSIDE, RR_DEKU_PALACE_INSIDE_LOWER, true,
           "OoTMM's can_enter_deku_palace: has_mask_deku || has_mask_skull");
    ResetSave();
    INV_CONTENT(ITEM_MASK_DEKU) = ITEM_MASK_DEKU;
    Expect("D4 control: the Deku Mask", RR_DEKU_PALACE_OUTSIDE, RR_DEKU_PALACE_INSIDE_LOWER, true,
           "the Deku Mask no longer opens the palace, so D1-D3 probe a broken edge");

    // ---- G. the Pirates' Fortress purple guard -------------------------------
    ResetSave();
    FreezeRules(kAll);
    Expect("G1 empty-handed", RR_PIRATES_FORTRESS_PLAZA, RR_PIRATES_FORTRESS_PLAZA_TOWER, false,
           "the tower opened with no way past the guard");
    MM_OoTMask_Give(RI_MASK_GERUDO);
    Expect("G2 the Gerudo Mask under its rule", RR_PIRATES_FORTRESS_PLAZA, RR_PIRATES_FORTRESS_PLAZA_TOWER, true,
           "OoTMM's can_evade_gerudo counts has_mask_gerudo");
    ResetSave();
    INV_CONTENT(ITEM_MASK_STONE) = ITEM_MASK_STONE;
    Expect("G3 control: the Stone Mask", RR_PIRATES_FORTRESS_PLAZA, RR_PIRATES_FORTRESS_PLAZA_TOWER, true,
           "the Stone Mask no longer gets past the guard");

    // ---- S. the Spooky Mask at night -----------------------------------------
    ResetSave();
    FreezeRules(kAll);
    MM_OoTMask_Give(RI_MASK_SPOOKY);
    Rando::Logic::gCurrentRegionTime = kNight;
    Expect("S1 the Spooky Mask at night", RR_BENEATH_THE_WELL_FREEZARD_ROOM, RR_BENEATH_THE_WELL_RIGHT_FIRE_KEESE, true,
           "OoTMM's can_pass_gibdo: has_mask_spooky && is_night");
    Rando::Logic::gCurrentRegionTime = kDay;
    Expect("S2 the Spooky Mask by day", RR_BENEATH_THE_WELL_FREEZARD_ROOM, RR_BENEATH_THE_WELL_RIGHT_FIRE_KEESE, false,
           "the Spooky Mask works as the Gibdo Mask at night only");
    FreezeRules(kAll & ~RSBS_XITEM_SPOOKY_MASK_MM);
    Rando::Logic::gCurrentRegionTime = kNight;
    Expect("S3 the Spooky Mask at night, rule off", RR_BENEATH_THE_WELL_FREEZARD_ROOM,
           RR_BENEATH_THE_WELL_RIGHT_FIRE_KEESE, false, "a mask the world did not add passed the Gibdos");
    ResetSave();
    INV_CONTENT(ITEM_MASK_GIBDO) = ITEM_MASK_GIBDO;
    Rando::Logic::gCurrentRegionTime = kDay;
    Expect("S4 control: the Gibdo Mask by day", RR_BENEATH_THE_WELL_FREEZARD_ROOM, RR_BENEATH_THE_WELL_RIGHT_FIRE_KEESE,
           true, "the Gibdo Mask no longer passes the Gibdos");

    ComboContext_Init();
    Rando::Logic::gCurrentRegionTime = savedTime;
    std::memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    printf("[TEST] %s: oot-masks-mm-logic (%d failure(s))\n", sOmFailures == 0 ? "PASS" : "FAIL", sOmFailures);
    return sOmFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
