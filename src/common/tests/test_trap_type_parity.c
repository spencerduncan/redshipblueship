/**
 * @file test_trap_type_parity.c
 * @brief TrapTypeParity: every OoTMM trap type works in both games (#1424).
 *
 * OoTMM has seven trap types (packages/generator/include/combo/traps.h: Ice,
 * Fire, Shock, Drain, Anti-Magic, Knockback, Rupoor), each with one runtime
 * effect (packages/generator/src/common/traps.c). SoH's Additional Traps lacked
 * Drain, Anti-Magic and Rupoor; MM's randomizer lacked Fire, Drain, Anti-Magic,
 * Knockback and Rupoor. ROM-free, redship tier:
 *
 *   1. THE TABLE. Each OoTMM trap names a type in each game, and that type's
 *      key is the one the game reads. Both lists are APPEND-ONLY: SoH's ten and
 *      MM's seven come first, at the places they always had (a type's number is
 *      its place in the roll's list, and in MM its bit in every frozen set), and
 *      MM's Fire and Knockback are 7 and 8, upstream 2Ship's numbers (#1452).
 *   2. OoT's ROLL IS STABLE. With only SoH's own types on, the list the roll
 *      draws from is exactly the list it always was, so a config that does not
 *      turn a new type on picks the same trap from the same seed; a new type
 *      alone is the only type drawn; with every type on the new ones are drawn.
 *   3. OoT's SAVE HALVES THROUGH THE REAL HARVEST. Drain leaves 1 health unit,
 *      Anti-Magic an empty meter, a Rupoor 10 rupees fewer (through
 *      Rupees_ChangeBy's accumulator), and OoT_HarvestSharedResources carries
 *      each into the shared pool; a Rupoor never takes more than the wallet has.
 *   4. MM's SAVE HALVES THROUGH THE REAL HARVEST, likewise through
 *      MM_HarvestSharedResources; Anti-Magic also ends the Chateau Romani's
 *      endless magic, as OoTMM's does, and a Rupoor never takes more than the
 *      wallet has. Fire and Knockback are Link's reaction only and have no save
 *      half.
 *   5. MM's FIRE SPARES GORON LINK, and only him (OoTMM's
 *      Player_IsGoronOrGoronTunic).
 *   6. MM's ROLL READS THE HIGH BYTE. A type of 8 or more (randoSaveTrapTypesHi)
 *      is drawn when it is in the frozen set, and bits no type owns change no roll.
 *
 * Linkage note: #included into test_runner.cpp at file scope (compiled as C++);
 * Test_TrapTypeParity there brings up the display-free Ship::Context first,
 * because the keys are CVars.
 */

#include "../context.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
// ExtraTraps.cpp (#1424)
int OoT_ExtraTraps_TestTypeCount(void);
const char* OoT_ExtraTraps_TestCVarName(int type);
int OoT_ExtraTraps_TestEnabledList(int* out, int cap);
int OoT_ExtraTraps_TestPick(uint32_t finalSeed);
int OoT_ExtraTraps_TestApplySaveEffect(int type);
// oot_trap_parity_test.cpp
void OoT_TrapParityTest_ArmLive(int health, int healthCapacity, int magic, int rupees);
void OoT_TrapParityTest_Vitals(int* health, int* magic, int* rupees);
// Traps.cpp (#1047, #1424)
int MM_Rando_Traps_TestRoll(uint32_t finalSeed, uint16_t mask, uint16_t randoCheckId, uint64_t streamSeed);
const char* MM_Rando_Traps_TestCVarName(int type);
int MM_Rando_Traps_TestTypeCount(void);
const char* MM_Rando_Traps_TestLabel(int type);
int MM_Rando_Traps_TestApplySaveEffect(int type);
int MM_Rando_Traps_TestFireSpares(int transformation);
// mm_trap_parity_test.cpp
void MM_TrapParityTest_ArmLive(int health, int healthCapacity, int magic, int rupees, int chateau);
void MM_TrapParityTest_Vitals(int* health, int* magic, int* rupees, int* chateau);
int MM_TrapParityTest_Form(int which);
// GameExports_SingleExe.cpp (both games)
void OoT_HarvestSharedResources(void);
void MM_HarvestSharedResources(void);
}

namespace {

int sTtpFailures;

#define TTP_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sTtpFailures++;                           \
        }                                             \
    } while (0)

/** SoH's ten Additional Traps keys, in AltTrapType order, then the three #1424
 *  appended. Spelled here, not read from the file under test. */
const char* const kTtpOoTKeys[] = {
    "gEnhancements.ExtraTraps.Ice",       "gEnhancements.ExtraTraps.Burn",  "gEnhancements.ExtraTraps.Shock",
    "gEnhancements.ExtraTraps.Knockback", "gEnhancements.ExtraTraps.Speed", "gEnhancements.ExtraTraps.Bomb",
    "gEnhancements.ExtraTraps.Void",      "gEnhancements.ExtraTraps.Ammo",  "gEnhancements.ExtraTraps.Kill",
    "gEnhancements.ExtraTraps.Teleport",  "gEnhancements.ExtraTraps.Drain", "gEnhancements.ExtraTraps.AntiMagic",
    "gEnhancements.ExtraTraps.Rupoor",
};
const int kTtpOoTSohTypes = 10; // SoH's own, before #1424
const int kTtpOoTTypes = (int)(sizeof(kTtpOoTKeys) / sizeof(kTtpOoTKeys[0]));
const int kTtpOoTDrain = 10;
const int kTtpOoTAntiMagic = 11;
const int kTtpOoTRupoor = 12;

/** MM's seven trap keys and names, in TrapTypes order, then the five #1424
 *  appended: Fire and Knockback first, at upstream 2Ship's numbers and keys
 *  (254cc60e1, #1452: TRAP_FIRE = 7, TRAP_KNOCKBACK = 8), so a port of that
 *  commit moves no frozen bit. */
struct TtpMMType {
    const char* key;
    const char* label;
};
const TtpMMType kTtpMMTypes[] = {
    { "gRando.Traps.Freeze", "Freeze Traps" },
    { "gRando.Traps.Blast", "Blast Traps" },
    { "gRando.Traps.Shock", "Shock Traps" },
    { "gRando.Traps.Jinx", "Jinx Traps" },
    { "gRando.Traps.Wallet", "Wallet Traps" },
    { "gRando.Traps.Enemy", "Like Like Traps" },
    { "gRando.Traps.Time", "Time Traps" },
    { "gRando.Traps.Fire", "Fire Traps" },
    { "gRando.Traps.Knockback", "Knockback Traps" },
    { "gRando.Traps.Drain", "Drain Traps" },
    { "gRando.Traps.AntiMagic", "Anti-Magic Traps" },
    { "gRando.Traps.Rupoor", "Rupoor Traps" },
};
const int kTtpMMOwnTypes = 7; // MM's own, before #1424
const int kTtpMMTypeCount = (int)(sizeof(kTtpMMTypes) / sizeof(kTtpMMTypes[0]));
const int kTtpMMFreeze = 0;
const int kTtpMMFire = 7;
const int kTtpMMKnockback = 8;
const int kTtpMMDrain = 9;
const int kTtpMMAntiMagic = 10;
const int kTtpMMRupoor = 11;

/** OoTMM's trap list (traps.h, in its order) and the type that is each one in
 *  each game. OoT's Fire is SoH's own Burn Traps, which #1424 leaves as SoH
 *  has it: OoTMM's Fire Trap skips Link in the Goron Tunic, while SoH's still
 *  makes him flinch and burn briefly (vanilla's tunic rule only shortens the
 *  flames). OoT's Knockback is SoH's, which throws Link backwards. */
struct TtpOoTMMTrap {
    const char* name;
    int ootType;
    int mmType;
};
const TtpOoTMMTrap kTtpOoTMM[] = {
    { "Ice", 0, kTtpMMFreeze },
    { "Fire", 1, kTtpMMFire },
    { "Shock", 2, 2 },
    { "Drain", kTtpOoTDrain, kTtpMMDrain },
    { "Anti-Magic", kTtpOoTAntiMagic, kTtpMMAntiMagic },
    { "Knockback", 3, kTtpMMKnockback },
    { "Rupoor", kTtpOoTRupoor, kTtpMMRupoor },
};

const uint32_t kTtpSeeds[] = { 0x74C32F12u, 0xE74E56F7u, 0x00000001u, 0x1424F00Du };

void TtpClearOoTKeys() {
    for (int t = 0; t < kTtpOoTTypes; t++) {
        CVarClear(kTtpOoTKeys[t]);
    }
}

std::vector<int> TtpOoTList() {
    int buf[32] = {};
    const int n = OoT_ExtraTraps_TestEnabledList(buf, 32);
    return std::vector<int>(buf, buf + (n < 32 ? n : 32));
}

std::vector<int> TtpRange(int from, int to) {
    std::vector<int> v;
    for (int i = from; i < to; i++) {
        v.push_back(i);
    }
    return v;
}

/** The pool's value for @p kind, or -1 when it has never been shared. */
int TtpPool(uint8_t kind) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource(kind, &value)) {
        return -1;
    }
    return (int)value;
}

void TtpFreshPool() {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
}

// ---- 1. The table ------------------------------------------------------------
void TtpLegTable() {
    const int ootCount = OoT_ExtraTraps_TestTypeCount();
    const int mmCount = MM_Rando_Traps_TestTypeCount();
    printf("[TEST] (1) OoT has %d trap types (expected %d), MM %d (expected %d)\n", ootCount, kTtpOoTTypes, mmCount,
           kTtpMMTypeCount);
    TTP_CHECK(ootCount == kTtpOoTTypes, "OoT has %d trap types, expected %d", ootCount, kTtpOoTTypes);
    TTP_CHECK(mmCount == kTtpMMTypeCount, "MM has %d trap types, expected %d", mmCount, kTtpMMTypeCount);
    for (int t = 0; t < kTtpOoTTypes && t < ootCount; t++) {
        const char* key = OoT_ExtraTraps_TestCVarName(t);
        TTP_CHECK(key != nullptr && strcmp(key, kTtpOoTKeys[t]) == 0, "OoT trap type %d reads '%s', expected '%s'%s", t,
                  key != nullptr ? key : "(none)", kTtpOoTKeys[t],
                  t < kTtpOoTSohTypes ? " (SoH's own types must keep their places)" : "");
    }
    for (int t = 0; t < kTtpMMTypeCount && t < mmCount; t++) {
        const char* key = MM_Rando_Traps_TestCVarName(t);
        const char* label = MM_Rando_Traps_TestLabel(t);
        TTP_CHECK(key != nullptr && strcmp(key, kTtpMMTypes[t].key) == 0 && label != nullptr &&
                      strcmp(label, kTtpMMTypes[t].label) == 0,
                  "MM trap type %d is '%s' over '%s', expected '%s' over '%s'%s", t,
                  label != nullptr ? label : "(none)", key != nullptr ? key : "(none)", kTtpMMTypes[t].label,
                  kTtpMMTypes[t].key, t < kTtpMMOwnTypes ? " (MM's own types must keep their bits)" : "");
    }
    for (const TtpOoTMMTrap& trap : kTtpOoTMM) {
        const bool inOoT = trap.ootType >= 0 && trap.ootType < ootCount;
        const bool inMM = trap.mmType >= 0 && trap.mmType < mmCount;
        printf("[TEST]   OoTMM %-10s -> OoT %s, MM %s\n", trap.name,
               inOoT ? OoT_ExtraTraps_TestCVarName(trap.ootType) : "(missing)",
               inMM ? MM_Rando_Traps_TestCVarName(trap.mmType) : "(missing)");
        TTP_CHECK(inOoT && inMM, "OoTMM's %s trap is missing from %s", trap.name, !inOoT ? "OoT" : "MM");
    }
}

// ---- 2. OoT's roll is stable ---------------------------------------------------
void TtpLegOoTRoll() {
    OoT_TrapParityTest_ArmLive(0x30, 0x30, 0, 0); // no fishing pole on B: the list is not filtered
    TtpClearOoTKeys();
    for (int t = 0; t < kTtpOoTSohTypes; t++) {
        CVarSetInteger(kTtpOoTKeys[t], 1);
    }
    const std::vector<int> sohList = TtpOoTList();
    TTP_CHECK(sohList == TtpRange(0, kTtpOoTSohTypes),
              "with SoH's ten types on the roll draws from %zu types, not SoH's ten in order", sohList.size());
    int newPicked = 0;
    for (uint32_t s = 0; s < 64; s++) {
        newPicked += OoT_ExtraTraps_TestPick(0x1424u + s * 0x9E3779B9u) >= kTtpOoTSohTypes ? 1 : 0;
    }
    TTP_CHECK(newPicked == 0, "with only SoH's types on, %d of 64 draws picked a type SoH does not have", newPicked);

    for (int t = kTtpOoTSohTypes; t < kTtpOoTTypes; t++) {
        TtpClearOoTKeys();
        CVarSetInteger(kTtpOoTKeys[t], 1);
        const std::vector<int> alone = TtpOoTList();
        int other = 0;
        for (const uint32_t seed : kTtpSeeds) {
            other += OoT_ExtraTraps_TestPick(seed) != t ? 1 : 0;
        }
        TTP_CHECK(alone == std::vector<int>(1, t) && other == 0,
                  "with only '%s' on the roll draws from %zu types and %d of %zu draws were another type",
                  kTtpOoTKeys[t], alone.size(), other, sizeof(kTtpSeeds) / sizeof(kTtpSeeds[0]));
    }

    TtpClearOoTKeys();
    for (int t = 0; t < kTtpOoTTypes; t++) {
        CVarSetInteger(kTtpOoTKeys[t], 1);
    }
    const std::vector<int> allList = TtpOoTList();
    TTP_CHECK(allList == TtpRange(0, kTtpOoTTypes),
              "with every type on the roll draws from %zu types, not all %d in order", allList.size(), kTtpOoTTypes);
    std::set<int> drawn;
    for (uint32_t s = 0; s < 256; s++) {
        drawn.insert(OoT_ExtraTraps_TestPick(0xBEEFu + s * 0x9E3779B9u));
    }
    int newDrawn = 0;
    for (int t = kTtpOoTSohTypes; t < kTtpOoTTypes; t++) {
        newDrawn += drawn.count(t) != 0 ? 1 : 0;
    }
    printf("[TEST] (2) OoT: SoH's list %zu types, every type %zu, %zu distinct draws of 256 (%d of the new three)\n",
           sohList.size(), allList.size(), drawn.size(), newDrawn);
    TTP_CHECK(newDrawn == kTtpOoTTypes - kTtpOoTSohTypes,
              "with every type on, only %d of the three new types were drawn in 256 seeds", newDrawn);
    TtpClearOoTKeys();
}

// ---- 3. OoT's save halves through the real harvest ------------------------------
void TtpLegOoTHarvest() {
    TtpFreshPool();
    OoT_TrapParityTest_ArmLive(0x50, 0x50, 0x30, 50);
    OoT_HarvestSharedResources();
    TTP_CHECK(TtpPool(RSBS_SHARED_RES_HEALTH_CURRENT) == 0x50 && TtpPool(RSBS_SHARED_RES_MAGIC_CURRENT) == 0x30 &&
                  TtpPool(RSBS_SHARED_RES_RUPEES) == 50,
              "setup: OoT's first harvest pooled health %d, magic %d, rupees %d (expected 80, 48, 50)",
              TtpPool(RSBS_SHARED_RES_HEALTH_CURRENT), TtpPool(RSBS_SHARED_RES_MAGIC_CURRENT),
              TtpPool(RSBS_SHARED_RES_RUPEES));

    int health = 0;
    int magic = 0;
    int rupees = 0;
    const int drain = OoT_ExtraTraps_TestApplySaveEffect(kTtpOoTDrain);
    OoT_TrapParityTest_Vitals(&health, &magic, &rupees);
    OoT_HarvestSharedResources();
    printf("[TEST] (3) OoT Drain: live health %d, pool %d (expected 1, 1)\n", health,
           TtpPool(RSBS_SHARED_RES_HEALTH_CURRENT));
    TTP_CHECK(drain == 1 && health == 1 && TtpPool(RSBS_SHARED_RES_HEALTH_CURRENT) == 1,
              "OoT's Drain Trap left health %d and the pool %d, expected 1 and 1", health,
              TtpPool(RSBS_SHARED_RES_HEALTH_CURRENT));

    const int anti = OoT_ExtraTraps_TestApplySaveEffect(kTtpOoTAntiMagic);
    OoT_TrapParityTest_Vitals(&health, &magic, &rupees);
    OoT_HarvestSharedResources();
    printf("[TEST] (3) OoT Anti-Magic: live magic %d, pool %d (expected 0, 0)\n", magic,
           TtpPool(RSBS_SHARED_RES_MAGIC_CURRENT));
    TTP_CHECK(anti == 1 && magic == 0 && TtpPool(RSBS_SHARED_RES_MAGIC_CURRENT) == 0,
              "OoT's Anti-Magic Trap left magic %d and the pool %d, expected 0 and 0", magic,
              TtpPool(RSBS_SHARED_RES_MAGIC_CURRENT));

    const int rupoor = OoT_ExtraTraps_TestApplySaveEffect(kTtpOoTRupoor);
    OoT_TrapParityTest_Vitals(&health, &magic, &rupees);
    OoT_HarvestSharedResources();
    printf("[TEST] (3) OoT Rupoor: live rupees %d, pool %d (expected 40, 40)\n", rupees,
           TtpPool(RSBS_SHARED_RES_RUPEES));
    TTP_CHECK(rupoor == 1 && rupees == 40 && TtpPool(RSBS_SHARED_RES_RUPEES) == 40,
              "OoT's Rupoor left %d rupees and the pool %d, expected 40 and 40", rupees,
              TtpPool(RSBS_SHARED_RES_RUPEES));

    // A Rupoor never takes more than the wallet holds.
    TtpFreshPool();
    OoT_TrapParityTest_ArmLive(0x50, 0x50, 0x30, 5);
    OoT_HarvestSharedResources();
    OoT_ExtraTraps_TestApplySaveEffect(kTtpOoTRupoor);
    OoT_HarvestSharedResources();
    TTP_CHECK(TtpPool(RSBS_SHARED_RES_RUPEES) == 0, "a Rupoor at 5 rupees left the pool at %d, expected 0",
              TtpPool(RSBS_SHARED_RES_RUPEES));

    // SoH's own types are Link's reaction only: no save half.
    for (int t = 0; t < kTtpOoTSohTypes; t++) {
        TTP_CHECK(OoT_ExtraTraps_TestApplySaveEffect(t) == 0, "SoH's trap type %d (%s) gained a save half", t,
                  kTtpOoTKeys[t]);
    }
}

// ---- 4. MM's save halves through the real harvest -------------------------------
void TtpLegMMHarvest() {
    TtpFreshPool();
    MM_TrapParityTest_ArmLive(0x50, 0x50, 0x30, 50, 1);
    MM_HarvestSharedResources();
    TTP_CHECK(TtpPool(RSBS_SHARED_RES_HEALTH_CURRENT) == 0x50 && TtpPool(RSBS_SHARED_RES_MAGIC_CURRENT) == 0x30 &&
                  TtpPool(RSBS_SHARED_RES_RUPEES) == 50,
              "setup: MM's first harvest pooled health %d, magic %d, rupees %d (expected 80, 48, 50)",
              TtpPool(RSBS_SHARED_RES_HEALTH_CURRENT), TtpPool(RSBS_SHARED_RES_MAGIC_CURRENT),
              TtpPool(RSBS_SHARED_RES_RUPEES));

    int health = 0;
    int magic = 0;
    int rupees = 0;
    int chateau = 0;
    const int drain = MM_Rando_Traps_TestApplySaveEffect(kTtpMMDrain);
    MM_TrapParityTest_Vitals(&health, &magic, &rupees, &chateau);
    MM_HarvestSharedResources();
    printf("[TEST] (4) MM Drain: live health %d, pool %d (expected 1, 1)\n", health,
           TtpPool(RSBS_SHARED_RES_HEALTH_CURRENT));
    TTP_CHECK(drain == 1 && health == 1 && TtpPool(RSBS_SHARED_RES_HEALTH_CURRENT) == 1,
              "MM's Drain Trap left health %d and the pool %d, expected 1 and 1", health,
              TtpPool(RSBS_SHARED_RES_HEALTH_CURRENT));

    const int anti = MM_Rando_Traps_TestApplySaveEffect(kTtpMMAntiMagic);
    MM_TrapParityTest_Vitals(&health, &magic, &rupees, &chateau);
    MM_HarvestSharedResources();
    printf("[TEST] (4) MM Anti-Magic: live magic %d, Chateau Romani %d, pool %d (expected 0, 0, 0)\n", magic, chateau,
           TtpPool(RSBS_SHARED_RES_MAGIC_CURRENT));
    TTP_CHECK(anti == 1 && magic == 0 && chateau == 0 && TtpPool(RSBS_SHARED_RES_MAGIC_CURRENT) == 0,
              "MM's Anti-Magic Trap left magic %d, the Chateau Romani %d and the pool %d, expected 0, 0 and 0", magic,
              chateau, TtpPool(RSBS_SHARED_RES_MAGIC_CURRENT));

    const int rupoor = MM_Rando_Traps_TestApplySaveEffect(kTtpMMRupoor);
    MM_TrapParityTest_Vitals(&health, &magic, &rupees, &chateau);
    MM_HarvestSharedResources();
    printf("[TEST] (4) MM Rupoor: live rupees %d, pool %d (expected 40, 40)\n", rupees,
           TtpPool(RSBS_SHARED_RES_RUPEES));
    TTP_CHECK(rupoor == 1 && rupees == 40 && TtpPool(RSBS_SHARED_RES_RUPEES) == 40,
              "MM's Rupoor left %d rupees and the pool %d, expected 40 and 40", rupees,
              TtpPool(RSBS_SHARED_RES_RUPEES));

    // A Rupoor never takes more than the wallet holds (MM_HarvestSharedRupees
    // clamps the count plus the accumulator at zero).
    TtpFreshPool();
    MM_TrapParityTest_ArmLive(0x50, 0x50, 0x30, 5, 0);
    MM_HarvestSharedResources();
    MM_Rando_Traps_TestApplySaveEffect(kTtpMMRupoor);
    MM_HarvestSharedResources();
    TTP_CHECK(TtpPool(RSBS_SHARED_RES_RUPEES) == 0, "an MM Rupoor at 5 rupees left the pool at %d, expected 0",
              TtpPool(RSBS_SHARED_RES_RUPEES));

    // Fire and Knockback are Link's reaction only, as are MM's own types.
    for (int t = 0; t < kTtpMMTypeCount; t++) {
        const bool hasSaveHalf = t == kTtpMMDrain || t == kTtpMMAntiMagic || t == kTtpMMRupoor;
        if (!hasSaveHalf) {
            TTP_CHECK(MM_Rando_Traps_TestApplySaveEffect(t) == 0, "MM's trap type %d (%s) has a save half", t,
                      kTtpMMTypes[t].label);
        }
    }
    TtpFreshPool();
}

// ---- 5. Fire spares Goron Link -------------------------------------------------
void TtpLegFire() {
    static const char* const kForms[] = { "human", "Goron", "Zora", "Deku", "Fierce Deity" };
    for (int which = 0; which < 5; which++) {
        const int spared = MM_Rando_Traps_TestFireSpares(MM_TrapParityTest_Form(which));
        TTP_CHECK(spared == (which == 1 ? 1 : 0), "a Fire Trap %s %s Link", spared ? "spares" : "burns", kForms[which]);
    }
}

// ---- 6. MM's roll reads the high byte --------------------------------------------
void TtpLegMMRoll() {
    const uint16_t rupoorOnly = (uint16_t)(1u << kTtpMMRupoor);
    const uint16_t fireRupoor = (uint16_t)((1u << kTtpMMFire) | (1u << kTtpMMRupoor));
    int notRupoor = 0;
    int outside = 0;
    int moved = 0;
    std::set<int> pair;
    for (const uint32_t seed : kTtpSeeds) {
        for (uint16_t id = 1; id < 200; id++) {
            notRupoor += MM_Rando_Traps_TestRoll(seed, rupoorOnly, id, 0x55ull) != kTtpMMRupoor ? 1 : 0;
            const int r = MM_Rando_Traps_TestRoll(seed, fireRupoor, id, 0x66ull);
            outside += (r != kTtpMMFire && r != kTtpMMRupoor) ? 1 : 0;
            pair.insert(r);
            // Bits no type owns (12-15) are ignored: MM's own seven roll as they did.
            moved += MM_Rando_Traps_TestRoll(seed, 0x7Fu, id, 0x77ull) !=
                             MM_Rando_Traps_TestRoll(seed, (uint16_t)(0x7Fu | 0xF000u), id, 0x77ull)
                         ? 1
                         : 0;
        }
    }
    printf("[TEST] (6) MM: %d non-Rupoor rolls under Rupoor alone, %d outside Fire+Rupoor, %zu types drawn, %d rolls "
           "moved by unowned bits\n",
           notRupoor, outside, pair.size(), moved);
    TTP_CHECK(notRupoor == 0, "%d rolls under a Rupoor-only frozen set were another type: the high byte is not read",
              notRupoor);
    TTP_CHECK(outside == 0 && pair.size() == 2, "Fire+Rupoor rolled %d types outside the set and drew %zu types",
              outside, pair.size());
    TTP_CHECK(moved == 0, "%d rolls moved when bits no trap type owns were set in the frozen set", moved);
}

} // namespace

// The row's body. Test_TrapTypeParity (test_runner.cpp) brings up the
// display-free Ship::Context first: the keys are CVars.
static TestResult TrapTypeParity_Run(void) {
    printf("[TEST] trap-type-parity: every OoTMM trap type works in both games, and adding them moved no existing "
           "roll (#1424)\n");
    sTtpFailures = 0;
    TtpLegTable();
    TtpLegOoTRoll();
    TtpLegOoTHarvest();
    TtpLegMMHarvest();
    TtpLegFire();
    TtpLegMMRoll();
    TtpFreshPool();
    printf("[TEST] trap-type-parity: %s\n", sTtpFailures == 0 ? "PASS" : "FAIL");
    return sTtpFailures == 0 ? TEST_PASS : TEST_FAIL;
}
