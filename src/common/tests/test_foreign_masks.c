/**
 * @file test_foreign_masks.c
 * @brief Locks for wearing the other game's masks (#977, epic #975 phase P3).
 *
 * Four rows, all ROM-free and display-free (label "redship"):
 *
 *   ForeignMaskTable (foreign-mask-table): the shared id space and rules
 *     (src/common/foreign_masks.h). Every mask names the other game as its
 *     host; every worn list and icon is curated in its HOST's half by the
 *     manifest compiled into this binary (never the other game's base path,
 *     #981); every mask names its extension and shared-extension rule bits
 *     (#976's item rules record) and is wearable only while the frozen world
 *     holds its extension bit; every byte sanitizes to a mask the host wears
 *     or to none; the toggle, ownership, shared-slot and list-cycle rules.
 *   ForeignMaskRules (foreign-mask-rules): both games' REAL rule functions
 *     (ForeignMaskOoT.cpp, ForeignMaskMM.cpp) on a heap Player. The worn field
 *     never reaches currentMask: for every byte value, using it and applying
 *     the rules leaves currentMask a value the engine's own tables cover, and
 *     only a mask the host wears selects a worn list. OoT: one mask at a time,
 *     child only, off at a scene load unless Persistent Masks. MM: human form
 *     only, one mask at a time, the shared pause slots (cycle, equip, icon).
 *     Both: a mask whose rule the world did not freeze on is neither put on
 *     nor kept. OoT's head draw selects the worn list only for a child with no
 *     native mask on and the head list drawn (OoT_ForeignMask_WornDl). The two
 *     live saves are one storage, so each game's steps arm that game's save.
 *   ForeignMaskPersistence (foreign-mask-persistence): the fields are appended
 *     at the end of their structs, and each game's worn mask
 *     survives a crossing (the production flush, freeze and restore/consume)
 *     and a .redsave save and load.
 *   OoTChildTradeCycle (oot-child-trade-cycle): the list-based child trade
 *     cycle answers exactly as the contiguous cycle it replaced, for every slot
 *     value and every ownership set, and visits a registered custom ItemID.
 *
 * Every check reports and the row goes on, so one run names every broken rule.
 * This file is #included into test_runner.cpp at FILE SCOPE (compiled as C++).
 */

#include "../foreign_masks.h"
#include "../foreign_items.h"
#include "../curated_archives.h"
#include "../context.h"
#include "../save.h"
#include "../test_runner.h"
#include "../test_scratch.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif
// games/oot/soh/oot_foreign_mask_test.cpp
void OoT_ForeignMaskTest_ArmLive(void);
void OoT_ForeignMaskTest_SetStored(uint8_t value);
uint8_t OoT_ForeignMaskTest_Stored(void);
void OoT_ForeignMaskTest_SetAdult(int adult);
void OoT_ForeignMaskTest_SetMaskMemory(uint8_t mask);
uint8_t OoT_ForeignMaskTest_MaskMemory(void);
void OoT_ForeignMaskTest_SetTradeSlot(uint8_t item);
void OoT_ForeignMaskTest_SetPlayerMask(uint8_t mask);
int OoT_ForeignMaskTest_PlayerMask(void);
int OoT_ForeignMaskTest_PlayerMaskMax(void);
int OoT_ForeignMaskTest_Use(uint8_t mask);
void OoT_ForeignMaskTest_ApplyRules(void);
void OoT_ForeignMaskTest_PlayerInit(int persistentMasks);
size_t OoT_ForeignMaskTest_FieldOffset(void);
size_t OoT_ForeignMaskTest_PrevLastOffset(void);
size_t OoT_ForeignMaskTest_SaveSize(void);
void OoT_ForeignMaskTest_Freeze(void);
int OoT_ForeignMaskTest_Restore(void);
void OoT_TradeCycleTest_Set(uint8_t item, uint16_t ownedBits);
uint8_t OoT_TradeCycleTest_Next(void);
uint8_t OoT_TradeCycleTest_Prev(void);
uint8_t OoT_TradeCycleTest_LegacyNext(void);
uint8_t OoT_TradeCycleTest_LegacyPrev(void);
int OoT_TradeCycleTest_ChildTradeCount(void);
uint8_t OoT_TradeCycleTest_FirstItem(void);
uint8_t OoT_TradeCycleTest_ItemNone(void);
uint8_t OoT_TradeCycleTest_FirstFreeCustomItem(void);
int OoT_TradeCycleTest_RegisterExtra(uint8_t itemId, int (*owned)(void));
void OoT_TradeCycleTest_ClearExtras(void);
uint8_t OoT_ForeignMaskTest_Worn(void);
const char* OoT_ForeignMaskTest_WornDl(int headDrawn);
// games/mm/2s2h/mm_foreign_mask_test.cpp
void MM_ForeignMaskTest_ArmLive(void);
void MM_ForeignMaskTest_SetStored(uint8_t value);
uint8_t MM_ForeignMaskTest_Stored(void);
void MM_ForeignMaskTest_SetSlotSel(uint8_t slot, uint8_t value);
uint8_t MM_ForeignMaskTest_SlotSelStored(uint8_t slot);
uint8_t MM_ForeignMaskTest_SlotSelection(uint8_t slot);
void MM_ForeignMaskTest_SetNativeOwned(uint8_t slot, int owned);
int MM_ForeignMaskTest_PageCell(uint8_t slot);
const char* MM_ForeignMaskTest_PageIcon(int cell, const char* nativeIcon);
void MM_ForeignMaskTest_SetForm(int human);
void MM_ForeignMaskTest_SetPlayerMask(uint8_t mask);
int MM_ForeignMaskTest_PlayerMask(void);
void MM_ForeignMaskTest_SetEquippedMask(uint8_t mask);
int MM_ForeignMaskTest_EquippedMask(void);
int MM_ForeignMaskTest_PlayerMaskMax(void);
int MM_ForeignMaskTest_MaskStone(void);
int MM_ForeignMaskTest_MaskGibdo(void);
int MM_ForeignMaskTest_MaskKeaton(void);
int MM_ForeignMaskTest_Use(uint8_t mask);
void MM_ForeignMaskTest_ApplyRules(void);
int MM_ForeignMaskTest_CycleSlot(uint8_t slot, int dir);
int MM_ForeignMaskTest_EquipNative(int maskId);
const char* MM_ForeignMaskTest_WornDl(void);
size_t MM_ForeignMaskTest_FieldOffset(void);
size_t MM_ForeignMaskTest_PrevLastOffset(void);
size_t MM_ForeignMaskTest_SaveSize(void);
void MM_ForeignMaskTest_Freeze(void);
int MM_ForeignMaskTest_Consume(void);
uint8_t MM_ForeignMaskTest_Worn(void);
#ifdef __cplusplus
}
#endif

namespace {

int sFmFailures = 0;

#define FM_CHECK(cond, label, ...)                                    \
    do {                                                              \
        if (!(cond)) {                                                \
            printf("  FAIL [%s] %s:%d: ", label, __FILE__, __LINE__); \
            printf(__VA_ARGS__);                                      \
            printf("\n");                                             \
            sFmFailures++;                                            \
        }                                                             \
    } while (0)

TestResult FmVerdict(const char* row) {
    if (sFmFailures != 0) {
        printf("[TEST] FAIL: %s: %d check(s) failed\n", row, sFmFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: %s\n", row);
    return TEST_PASS;
}

// The ownership source the rows register for a host: a bitmask over mask ids.
uint32_t sFmOwnedBits = 0;
int FmOwned(uint8_t id) {
    return (sFmOwnedBits >> id) & 1u;
}

void FmOwn(uint32_t bits) {
    sFmOwnedBits = bits;
}

/** The manifest's entries for one direction ("mm->oot" or "oot->mm"). */
std::vector<std::string> FmManifestEntries(const char* direction) {
    std::vector<std::string> out;
    std::istringstream in{ std::string(rsbs::curated::EmbeddedManifest()) };
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const size_t space = line.find(' ');
        if (space == std::string::npos || line.compare(0, space, direction) != 0) {
            continue;
        }
        out.push_back(line.substr(space + 1));
    }
    return out;
}

/** True when `otrPath` ("__OTR__<path>") is carried by one of `entries`. */
bool FmCurated(const std::vector<std::string>& entries, const char* otrPath) {
    static const char kOtr[] = "__OTR__";
    if (otrPath == nullptr || strncmp(otrPath, kOtr, sizeof(kOtr) - 1) != 0) {
        return false;
    }
    const std::string path(otrPath + sizeof(kOtr) - 1);
    for (const std::string& e : entries) {
        const bool dir = !e.empty() && e.back() == '/';
        if (dir ? path.compare(0, e.size(), e) == 0 : path == e) {
            return true;
        }
    }
    return false;
}

const char* FmGameName(uint8_t game) {
    return game == GAME_OOT ? "oot" : (game == GAME_MM ? "mm" : "none");
}

// The six extension rules (#976): each adds one mask to the other game.
constexpr uint32_t kFmAllRules = RSBS_XITEM_BLAST_MASK_OOT | RSBS_XITEM_STONE_MASK_OOT | RSBS_XITEM_KAMARO_MASK_OOT |
                                 RSBS_XITEM_GERUDO_MASK_MM | RSBS_XITEM_SKULL_MASK_MM | RSBS_XITEM_SPOOKY_MASK_MM;

/** Freeze a world whose cross-game item rules are exactly @p flags, the way
 *  Playthrough_Init stamps one (test_shared_masks.c's SmFreezeWorld). */
void FmFreezeRules(uint32_t flags) {
    ComboContext_Init();
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = flags;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

/** Every row starts from the six extension rules frozen on, nothing owned. */
void FmResetAll(void) {
    Combo_ForeignMask_SetOwnedSource(GAME_OOT, NULL);
    Combo_ForeignMask_SetOwnedSource(GAME_MM, NULL);
    FmOwn(0);
    OoT_TradeCycleTest_ClearExtras();
    OoT_ForeignMaskTest_ArmLive();
    MM_ForeignMaskTest_ArmLive();
    FmFreezeRules(kFmAllRules);
}

/** Every row ends with nothing frozen and no source registered. */
void FmLeaveClean(void) {
    Combo_ForeignMask_SetOwnedSource(GAME_OOT, NULL);
    Combo_ForeignMask_SetOwnedSource(GAME_MM, NULL);
    FmOwn(0);
    OoT_TradeCycleTest_ClearExtras();
    ComboContext_Init();
}

} // namespace

TestResult Test_ForeignMaskTable(void) {
    printf("[TEST] foreign-mask-table: the worn-mask id space, its curated models and the shared rules (#977)\n");
    sFmFailures = 0;
    FmResetAll();

    // ---- T1. the table ------------------------------------------------------
    FM_CHECK(Combo_ForeignMask_Info(COMBO_FOREIGN_MASK_NONE) == NULL, "T1", "NONE has a row");
    FM_CHECK(Combo_ForeignMask_Info(COMBO_FOREIGN_MASK_COUNT) == NULL, "T1", "the id past the end has a row");
    FM_CHECK(COMBO_FOREIGN_MASK_MM_BLAST == 1 && COMBO_FOREIGN_MASK_MM_STONE == 2 &&
                 COMBO_FOREIGN_MASK_MM_KAMARO == 3 && COMBO_FOREIGN_MASK_OOT_GERUDO == 4 &&
                 COMBO_FOREIGN_MASK_OOT_SKULL == 5 && COMBO_FOREIGN_MASK_OOT_SPOOKY == 6 &&
                 COMBO_FOREIGN_MASK_SLOT_MM_STONE == 1 && COMBO_FOREIGN_MASK_SLOT_MM_GIBDO == 2,
             "T1", "a persisted id moved (the values are saved: append-only)");
    int perHost[3] = { 0, 0, 0 };
    const std::vector<std::string> intoOoT = FmManifestEntries("mm->oot");
    const std::vector<std::string> intoMM = FmManifestEntries("oot->mm");
    FM_CHECK(!intoOoT.empty() && !intoMM.empty(), "T2", "the embedded manifest has no entries in a direction");
    for (uint8_t id = 1; id < COMBO_FOREIGN_MASK_COUNT; id++) {
        const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(id);
        FM_CHECK(info != NULL && info->id == id, "T1", "mask %u has no row of its own", (unsigned)id);
        if (info == NULL) {
            continue;
        }
        FM_CHECK((info->origin == GAME_OOT && info->host == GAME_MM) ||
                     (info->origin == GAME_MM && info->host == GAME_OOT),
                 "T1", "%s: origin %u host %u is not the other game", info->name, (unsigned)info->origin,
                 (unsigned)info->host);
        perHost[info->host]++;
        FM_CHECK((info->host == GAME_MM) == (info->mmSharedSlot != COMBO_FOREIGN_MASK_SLOT_NONE), "T1",
                 "%s: shares MM slot %u", info->name, (unsigned)info->mmSharedSlot);
        FM_CHECK(info->bindsSegment9 == (id == COMBO_FOREIGN_MASK_MM_BLAST), "T1",
                 "%s: the segment 0x09 binding is for MM's Blast Mask list only", info->name);
        // The record's bits (foreign_items.h): ids 1..6 map onto the six
        // extension bits 0x20..0x400 and the six shared extensions 0x800..0x10000
        // in the same order, which is the order OoTMM lists them in.
        FM_CHECK(info->ruleBit == (RSBS_XITEM_BLAST_MASK_OOT << (id - 1)), "T1",
                 "%s: extension rule bit 0x%08X is not its record bit", info->name, (unsigned)info->ruleBit);
        FM_CHECK(info->sharedRuleBit == (RSBS_XITEM_SHARED_BLAST << (id - 1)) &&
                     (info->sharedRuleBit & RSBS_XITEM_FAMILY_MASK) == info->sharedRuleBit,
                 "T1", "%s: shared rule bit 0x%08X is not its family bit", info->name, (unsigned)info->sharedRuleBit);
        // ---- T2. every model is curated in its HOST's half (#981) ----------
        const std::vector<std::string>& hostHalf = info->host == GAME_OOT ? intoOoT : intoMM;
        const std::vector<std::string>& otherHalf = info->host == GAME_OOT ? intoMM : intoOoT;
        printf("[TEST] T2: %s (%s->%s): worn %s, icon %s\n", info->name, FmGameName(info->origin),
               FmGameName(info->host), info->wornDl, info->icon);
        FM_CHECK(FmCurated(hostHalf, info->wornDl), "T2", "%s: the worn list %s is not curated in %s's half",
                 info->name, info->wornDl, FmGameName(info->host));
        FM_CHECK(FmCurated(hostHalf, info->icon), "T2", "%s: the icon %s is not curated in %s's half", info->name,
                 info->icon, FmGameName(info->host));
        FM_CHECK(!FmCurated(otherHalf, info->wornDl), "T2", "%s: the worn list is curated in the wrong half",
                 info->name);
    }
    FM_CHECK(perHost[GAME_OOT] == 3 && perHost[GAME_MM] == 3, "T1", "OoT wears %d masks and MM %d, not three each",
             perHost[GAME_OOT], perHost[GAME_MM]);

    // ---- T3. every byte sanitizes to a mask the host wears, or to none --------
    for (int v = 0; v < 256; v++) {
        for (int host = GAME_OOT; host <= GAME_MM; host++) {
            const uint8_t s = Combo_ForeignMask_Sanitize((uint8_t)v, (GameId)host);
            const ComboForeignMaskInfo* info = Combo_ForeignMask_Info((uint8_t)v);
            const bool wearable = info != NULL && info->host == host;
            FM_CHECK(s == (wearable ? (uint8_t)v : (uint8_t)COMBO_FOREIGN_MASK_NONE), "T3",
                     "byte %d in %s sanitized to %u", v, FmGameName((uint8_t)host), (unsigned)s);
        }
    }
    FM_CHECK(Combo_ForeignMask_Sanitize(COMBO_FOREIGN_MASK_MM_STONE, GAME_NONE) == COMBO_FOREIGN_MASK_NONE, "T3",
             "a mask was worn by no game");

    // ---- T4. the toggle ------------------------------------------------------
    FM_CHECK(Combo_ForeignMask_Toggle(COMBO_FOREIGN_MASK_NONE, COMBO_FOREIGN_MASK_MM_BLAST, GAME_OOT) ==
                 COMBO_FOREIGN_MASK_MM_BLAST,
             "T4", "putting on a mask OoT wears");
    FM_CHECK(Combo_ForeignMask_Toggle(COMBO_FOREIGN_MASK_MM_BLAST, COMBO_FOREIGN_MASK_MM_BLAST, GAME_OOT) ==
                 COMBO_FOREIGN_MASK_NONE,
             "T4", "the worn mask again takes it off");
    FM_CHECK(Combo_ForeignMask_Toggle(COMBO_FOREIGN_MASK_MM_BLAST, COMBO_FOREIGN_MASK_MM_STONE, GAME_OOT) ==
                 COMBO_FOREIGN_MASK_MM_STONE,
             "T4", "another mask replaces the worn one");
    FM_CHECK(Combo_ForeignMask_Toggle(COMBO_FOREIGN_MASK_MM_BLAST, COMBO_FOREIGN_MASK_OOT_SKULL, GAME_OOT) ==
                 COMBO_FOREIGN_MASK_MM_BLAST,
             "T4", "OoT put on an OoT mask through the foreign field");
    FM_CHECK(Combo_ForeignMask_Toggle(COMBO_FOREIGN_MASK_OOT_SKULL, 200, GAME_MM) == COMBO_FOREIGN_MASK_OOT_SKULL, "T4",
             "an out-of-range request changed the worn mask");
    FM_CHECK(Combo_ForeignMask_Toggle(COMBO_FOREIGN_MASK_OOT_SKULL, COMBO_FOREIGN_MASK_NONE, GAME_MM) ==
                 COMBO_FOREIGN_MASK_OOT_SKULL,
             "T4", "a NONE request changed the worn mask");

    // ---- T5. ownership and the shared slots ----------------------------------
    FM_CHECK(Combo_ForeignMask_Owned(GAME_MM, COMBO_FOREIGN_MASK_OOT_SKULL) == 0, "T5",
             "a mask is owned with no source registered");
    Combo_ForeignMask_SetOwnedSource(GAME_MM, FmOwned);
    FmOwn((1u << COMBO_FOREIGN_MASK_OOT_SKULL) | (1u << COMBO_FOREIGN_MASK_MM_STONE));
    FM_CHECK(Combo_ForeignMask_Owned(GAME_MM, COMBO_FOREIGN_MASK_OOT_SKULL) == 1, "T5", "the source's mask");
    FM_CHECK(Combo_ForeignMask_Owned(GAME_MM, COMBO_FOREIGN_MASK_MM_STONE) == 0, "T5",
             "MM owns a mask it does not wear");
    uint8_t v[COMBO_FOREIGN_MASK_SLOT_MAX_VARIANTS];
    int n = Combo_ForeignMask_SlotVariants(COMBO_FOREIGN_MASK_SLOT_MM_STONE, 1, v, 3);
    FM_CHECK(n == 2 && v[0] == COMBO_FOREIGN_MASK_NONE && v[1] == COMBO_FOREIGN_MASK_OOT_SKULL, "T5",
             "Stone's slot with Stone and Skull owned: %d variant(s)", n);
    FmOwn((1u << COMBO_FOREIGN_MASK_OOT_SKULL) | (1u << COMBO_FOREIGN_MASK_OOT_GERUDO) |
          (1u << COMBO_FOREIGN_MASK_OOT_SPOOKY));
    n = Combo_ForeignMask_SlotVariants(COMBO_FOREIGN_MASK_SLOT_MM_STONE, 1, v, 3);
    FM_CHECK(n == 3 && v[0] == COMBO_FOREIGN_MASK_NONE && v[1] == COMBO_FOREIGN_MASK_OOT_GERUDO &&
                 v[2] == COMBO_FOREIGN_MASK_OOT_SKULL,
             "T5", "Stone's slot cycles Stone, Gerudo, Skull (got %d)", n);
    n = Combo_ForeignMask_SlotVariants(COMBO_FOREIGN_MASK_SLOT_MM_GIBDO, 0, v, 3);
    FM_CHECK(n == 1 && v[0] == COMBO_FOREIGN_MASK_OOT_SPOOKY, "T5", "Gibdo's slot without Gibdo holds Spooky only");
    FM_CHECK(Combo_ForeignMask_SlotCycle(COMBO_FOREIGN_MASK_SLOT_MM_STONE, COMBO_FOREIGN_MASK_NONE, 1, 1) ==
                     COMBO_FOREIGN_MASK_OOT_GERUDO &&
                 Combo_ForeignMask_SlotCycle(COMBO_FOREIGN_MASK_SLOT_MM_STONE, COMBO_FOREIGN_MASK_OOT_SKULL, 1, 1) ==
                     COMBO_FOREIGN_MASK_NONE &&
                 Combo_ForeignMask_SlotCycle(COMBO_FOREIGN_MASK_SLOT_MM_STONE, COMBO_FOREIGN_MASK_NONE, 1, -1) ==
                     COMBO_FOREIGN_MASK_OOT_SKULL,
             "T5", "the shared slot's cycle order or wrap");
    FmOwn(1u << COMBO_FOREIGN_MASK_OOT_SKULL);
    FM_CHECK(Combo_ForeignMask_SlotCycle(COMBO_FOREIGN_MASK_SLOT_MM_STONE, COMBO_FOREIGN_MASK_OOT_GERUDO, 1, 1) ==
                 COMBO_FOREIGN_MASK_NONE,
             "T5", "a variant no longer owned did not fall back to the list's first");
    FM_CHECK(Combo_ForeignMask_SlotVariants(COMBO_FOREIGN_MASK_SLOT_NONE, 1, v, 3) == 0 &&
                 Combo_ForeignMask_SlotVariants(COMBO_FOREIGN_MASK_SLOT_COUNT, 1, v, 3) == 0,
             "T5", "an unknown slot has variants");

    // ---- T6. the list cycle --------------------------------------------------
    const uint8_t owned[5] = { 1, 0, 1, 0, 1 };
    FM_CHECK(Combo_ListCycle_Step(owned, 5, 0, 1) == 2 && Combo_ListCycle_Step(owned, 5, 4, 1) == 0 &&
                 Combo_ListCycle_Step(owned, 5, 0, -1) == 4 && Combo_ListCycle_Step(owned, 5, 2, -1) == 0 &&
                 Combo_ListCycle_Step(owned, 5, 1, 1) == 2 && Combo_ListCycle_Step(owned, 5, 7, 1) == 4,
             "T6", "the list cycle's step or wrap");
    const uint8_t none[3] = { 0, 0, 0 };
    const uint8_t self[3] = { 0, 1, 0 };
    FM_CHECK(Combo_ListCycle_Step(none, 3, 0, 1) == -1 && Combo_ListCycle_Step(self, 3, 1, 1) == 1 &&
                 Combo_ListCycle_Step(self, 3, 1, -1) == 1,
             "T6", "an empty list answers none; a lone owned entry answers itself");

    // ---- T7. the rule bits gate wearing and owning ----------------------------
    Combo_ForeignMask_SetOwnedSource(GAME_OOT, FmOwned);
    Combo_ForeignMask_SetOwnedSource(GAME_MM, FmOwned);
    FmOwn(0x7Eu);
    ComboContext_Init(); // nothing frozen
    for (uint8_t id = 1; id < COMBO_FOREIGN_MASK_COUNT; id++) {
        const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(id);
        const GameId host = info != NULL ? (GameId)info->host : GAME_NONE;
        FM_CHECK(!Combo_ForeignMask_Wearable(id, host) && !Combo_ForeignMask_Owned(host, id), "T7",
                 "mask %u is wearable or owned with no world frozen", (unsigned)id);
    }
    FmFreezeRules(RSBS_XITEM_BLAST_MASK_OOT | RSBS_XITEM_SHARED_GORON);
    for (uint8_t id = 1; id < COMBO_FOREIGN_MASK_COUNT; id++) {
        const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(id);
        const GameId host = info != NULL ? (GameId)info->host : GAME_NONE;
        const bool on = id == COMBO_FOREIGN_MASK_MM_BLAST;
        FM_CHECK((Combo_ForeignMask_Wearable(id, host) != 0) == on && (Combo_ForeignMask_Owned(host, id) != 0) == on,
                 "T7", "with only the Blast Mask's rule frozen on, mask %u wearable %d owned %d", (unsigned)id,
                 Combo_ForeignMask_Wearable(id, host), Combo_ForeignMask_Owned(host, id));
    }
    FM_CHECK(!Combo_ForeignMask_Wearable(COMBO_FOREIGN_MASK_MM_BLAST, GAME_MM), "T7",
             "MM wears the Blast Mask through the foreign field");
    FmFreezeRules(kFmAllRules);
    for (uint8_t id = 1; id < COMBO_FOREIGN_MASK_COUNT; id++) {
        const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(id);
        const GameId host = info != NULL ? (GameId)info->host : GAME_NONE;
        FM_CHECK(Combo_ForeignMask_Wearable(id, host) && Combo_ForeignMask_Owned(host, id), "T7",
                 "with every extension rule on, mask %u is not wearable and owned", (unsigned)id);
    }

    FmLeaveClean();
    return FmVerdict("foreign-mask-table");
}

TestResult Test_ForeignMaskRules(void) {
    printf("[TEST] foreign-mask-rules: both games' worn-mask rules and the never-currentMask lock (#977)\n");
    sFmFailures = 0;
    FmResetAll();

    // ---- R1. OoT: the field never reaches currentMask -------------------------
    const int ootMax = OoT_ForeignMaskTest_PlayerMaskMax();
    for (int v = 0; v < 256; v++) {
        OoT_ForeignMaskTest_ArmLive();
        OoT_ForeignMaskTest_SetPlayerMask(0);
        OoT_ForeignMaskTest_Use((uint8_t)v);
        OoT_ForeignMaskTest_ApplyRules();
        const bool wearable = Combo_ForeignMask_Wearable((uint8_t)v, GAME_OOT) != 0;
        FM_CHECK(OoT_ForeignMaskTest_PlayerMask() == 0, "R1", "OoT: using byte %d moved currentMask to %d", v,
                 OoT_ForeignMaskTest_PlayerMask());
        FM_CHECK(OoT_ForeignMaskTest_Stored() == (wearable ? (uint8_t)v : 0), "R1", "OoT: using byte %d stored %u", v,
                 (unsigned)OoT_ForeignMaskTest_Stored());
        // A corrupt stored byte is cleared, never acted on.
        OoT_ForeignMaskTest_SetStored((uint8_t)v);
        OoT_ForeignMaskTest_ApplyRules();
        FM_CHECK(OoT_ForeignMaskTest_Worn() == (wearable ? (uint8_t)v : 0) &&
                     OoT_ForeignMaskTest_Stored() == (wearable ? (uint8_t)v : 0),
                 "R1", "OoT: stored byte %d survived the rules as %u", v, (unsigned)OoT_ForeignMaskTest_Stored());
        FM_CHECK(OoT_ForeignMaskTest_PlayerMask() >= 0 && OoT_ForeignMaskTest_PlayerMask() < ootMax, "R1",
                 "OoT: currentMask %d is past the engine's tables", OoT_ForeignMaskTest_PlayerMask());
    }

    // ---- R2. OoT: one mask at a time, child only --------------------------------
    OoT_ForeignMaskTest_ArmLive();
    OoT_ForeignMaskTest_SetPlayerMask(3); // a native mask on (PLAYER_MASK_SPOOKY)
    OoT_ForeignMaskTest_SetMaskMemory(3);
    FM_CHECK(OoT_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_MM_STONE) == 1, "R2", "OoT refused the Stone Mask");
    FM_CHECK(OoT_ForeignMaskTest_Worn() == COMBO_FOREIGN_MASK_MM_STONE && OoT_ForeignMaskTest_PlayerMask() == 0 &&
                 OoT_ForeignMaskTest_MaskMemory() == 0,
             "R2", "putting the Stone Mask on left native mask %d (memory %u) on", OoT_ForeignMaskTest_PlayerMask(),
             (unsigned)OoT_ForeignMaskTest_MaskMemory());
    FM_CHECK(OoT_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_MM_STONE) == 1 && OoT_ForeignMaskTest_Worn() == 0, "R2",
             "using the worn Stone Mask again did not take it off");
    OoT_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_MM_BLAST);
    OoT_ForeignMaskTest_SetPlayerMask(1); // the player puts Keaton on
    OoT_ForeignMaskTest_ApplyRules();
    FM_CHECK(OoT_ForeignMaskTest_Worn() == 0 && OoT_ForeignMaskTest_PlayerMask() == 1, "R2",
             "a native mask put on left the Blast Mask on (worn %u)", (unsigned)OoT_ForeignMaskTest_Worn());
    OoT_ForeignMaskTest_SetPlayerMask(0);
    OoT_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_MM_KAMARO);
    OoT_ForeignMaskTest_SetAdult(1);
    OoT_ForeignMaskTest_ApplyRules();
    FM_CHECK(OoT_ForeignMaskTest_Worn() == 0, "R2", "an adult Link kept wearing Kamaro's Mask");
    FM_CHECK(OoT_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_MM_KAMARO) == 0 && OoT_ForeignMaskTest_Worn() == 0, "R2",
             "an adult Link put Kamaro's Mask on");
    OoT_ForeignMaskTest_SetAdult(0);

    // ---- R3. OoT: off at a scene load, unless Persistent Masks ----------------
    OoT_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_MM_STONE);
    OoT_ForeignMaskTest_PlayerInit(0);
    FM_CHECK(OoT_ForeignMaskTest_Worn() == 0, "R3",
             "the Stone Mask stayed on through a scene load without Persistent Masks");
    OoT_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_MM_STONE);
    OoT_ForeignMaskTest_SetTradeSlot(0x24); // ITEM_MASK_KEATON
    OoT_ForeignMaskTest_PlayerInit(1);
    FM_CHECK(OoT_ForeignMaskTest_Worn() == COMBO_FOREIGN_MASK_MM_STONE, "R3",
             "Persistent Masks did not keep the Stone Mask through a scene load");
    OoT_ForeignMaskTest_SetTradeSlot(0x2C); // ITEM_SOLD_OUT
    OoT_ForeignMaskTest_PlayerInit(1);
    FM_CHECK(OoT_ForeignMaskTest_Worn() == 0, "R3", "a sold-out trade slot kept the Stone Mask (maskMemory's rule)");

    // ---- R4. MM: the field never reaches currentMask or equippedMask ----------
    const int mmMax = MM_ForeignMaskTest_PlayerMaskMax();
    for (int b = 0; b < 256; b++) {
        MM_ForeignMaskTest_ArmLive();
        MM_ForeignMaskTest_SetForm(1);
        MM_ForeignMaskTest_SetPlayerMask(0);
        MM_ForeignMaskTest_Use((uint8_t)b);
        MM_ForeignMaskTest_ApplyRules();
        const bool wearable = Combo_ForeignMask_Wearable((uint8_t)b, GAME_MM) != 0;
        FM_CHECK(MM_ForeignMaskTest_PlayerMask() == 0 && MM_ForeignMaskTest_EquippedMask() == 0, "R4",
                 "MM: using byte %d moved currentMask %d / equippedMask %d", b, MM_ForeignMaskTest_PlayerMask(),
                 MM_ForeignMaskTest_EquippedMask());
        FM_CHECK(MM_ForeignMaskTest_Stored() == (wearable ? (uint8_t)b : 0), "R4", "MM: using byte %d stored %u", b,
                 (unsigned)MM_ForeignMaskTest_Stored());
        MM_ForeignMaskTest_SetStored((uint8_t)b);
        MM_ForeignMaskTest_ApplyRules();
        const char* dl = MM_ForeignMaskTest_WornDl();
        const ComboForeignMaskInfo* info = Combo_ForeignMask_Info((uint8_t)b);
        FM_CHECK(wearable ? (dl != NULL && info != NULL && dl == info->wornDl) : dl == NULL, "R4",
                 "MM: stored byte %d selects worn list %s", b, dl != NULL ? dl : "(none)");
        FM_CHECK(MM_ForeignMaskTest_PlayerMask() >= 0 && MM_ForeignMaskTest_PlayerMask() < mmMax, "R4",
                 "MM: currentMask %d is past the engine's tables", MM_ForeignMaskTest_PlayerMask());
    }

    // ---- R5. MM: human form only, one mask at a time ---------------------------
    MM_ForeignMaskTest_ArmLive();
    MM_ForeignMaskTest_SetForm(0);
    FM_CHECK(MM_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_OOT_SKULL) == 0 && MM_ForeignMaskTest_Worn() == 0, "R5",
             "Deku Link put the Skull Mask on");
    MM_ForeignMaskTest_SetForm(1);
    MM_ForeignMaskTest_SetPlayerMask((uint8_t)MM_ForeignMaskTest_MaskKeaton());
    MM_ForeignMaskTest_SetEquippedMask((uint8_t)MM_ForeignMaskTest_MaskKeaton());
    FM_CHECK(MM_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_OOT_SKULL) == 1 &&
                 MM_ForeignMaskTest_Worn() == COMBO_FOREIGN_MASK_OOT_SKULL,
             "R5", "human Link could not put the Skull Mask on");
    FM_CHECK(MM_ForeignMaskTest_PlayerMask() == 0 && MM_ForeignMaskTest_EquippedMask() == 0, "R5",
             "the Skull Mask went on over Keaton's (currentMask %d, equippedMask %d)", MM_ForeignMaskTest_PlayerMask(),
             MM_ForeignMaskTest_EquippedMask());
    MM_ForeignMaskTest_SetForm(0);
    MM_ForeignMaskTest_ApplyRules();
    FM_CHECK(MM_ForeignMaskTest_Worn() == 0, "R5", "transforming kept the Skull Mask on");
    FM_CHECK(MM_ForeignMaskTest_WornDl() == NULL, "R5", "a transformed Link selects a worn list");
    MM_ForeignMaskTest_SetForm(1);
    MM_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_OOT_SPOOKY);
    MM_ForeignMaskTest_SetPlayerMask((uint8_t)MM_ForeignMaskTest_MaskGibdo());
    MM_ForeignMaskTest_ApplyRules();
    FM_CHECK(MM_ForeignMaskTest_Worn() == 0, "R5", "putting the Gibdo Mask on kept the Spooky Mask on");

    // ---- R6. MM: the shared pause slots ---------------------------------------
    MM_ForeignMaskTest_ArmLive();
    MM_ForeignMaskTest_SetForm(1);
    MM_ForeignMaskTest_SetPlayerMask(0);
    MM_ForeignMaskTest_SetNativeOwned(COMBO_FOREIGN_MASK_SLOT_MM_STONE, 1);
    MM_ForeignMaskTest_SetSlotSel(COMBO_FOREIGN_MASK_SLOT_MM_STONE, COMBO_FOREIGN_MASK_OOT_SKULL);
    FM_CHECK(MM_ForeignMaskTest_SlotSelection(COMBO_FOREIGN_MASK_SLOT_MM_STONE) == 0, "R6",
             "an unowned Skull Mask was the Stone slot's choice");
    FM_CHECK(MM_ForeignMaskTest_EquipNative(MM_ForeignMaskTest_MaskStone()) == 0 && MM_ForeignMaskTest_Worn() == 0,
             "R6", "with nothing owned, Stone's C button did not stay the Stone Mask");
    FM_CHECK(MM_ForeignMaskTest_CycleSlot(COMBO_FOREIGN_MASK_SLOT_MM_STONE, 1) == 0, "R6",
             "a slot with one variant cycled");
    const int cell = MM_ForeignMaskTest_PageCell(COMBO_FOREIGN_MASK_SLOT_MM_STONE);
    static const char kNativeIcon[] = "__OTR__native";
    FM_CHECK(MM_ForeignMaskTest_PageIcon(cell, kNativeIcon) == kNativeIcon, "R6",
             "the Stone slot showed another icon with nothing owned");
    Combo_ForeignMask_SetOwnedSource(GAME_MM, FmOwned);
    FmOwn((1u << COMBO_FOREIGN_MASK_OOT_GERUDO) | (1u << COMBO_FOREIGN_MASK_OOT_SKULL));
    FM_CHECK(MM_ForeignMaskTest_CycleSlot(COMBO_FOREIGN_MASK_SLOT_MM_STONE, 1) == 1 &&
                 MM_ForeignMaskTest_SlotSelection(COMBO_FOREIGN_MASK_SLOT_MM_STONE) == COMBO_FOREIGN_MASK_OOT_GERUDO,
             "R6", "A on Stone's slot did not move to the Gerudo Mask");
    FM_CHECK(MM_ForeignMaskTest_EquipNative(MM_ForeignMaskTest_MaskStone()) == 1 &&
                 MM_ForeignMaskTest_Worn() == COMBO_FOREIGN_MASK_OOT_GERUDO && MM_ForeignMaskTest_PlayerMask() == 0,
             "R6", "Stone's C button with Gerudo chosen did not put the Gerudo Mask on");
    FM_CHECK(MM_ForeignMaskTest_EquipNative(MM_ForeignMaskTest_MaskStone()) == 1 && MM_ForeignMaskTest_Worn() == 0,
             "R6", "Stone's C button again did not take the Gerudo Mask off");
    FM_CHECK(MM_ForeignMaskTest_EquipNative(MM_ForeignMaskTest_MaskGibdo()) == 0 &&
                 MM_ForeignMaskTest_EquipNative(MM_ForeignMaskTest_MaskKeaton()) == 0,
             "R6", "another slot's C button was taken over");
    const ComboForeignMaskInfo* gerudo = Combo_ForeignMask_Info(COMBO_FOREIGN_MASK_OOT_GERUDO);
    const char* icon = MM_ForeignMaskTest_PageIcon(cell, kNativeIcon);
    // The icon is the chosen variant's only when redship-mm.o2r holds it; a
    // ROM-free run has no archive, so the answer is the native icon OR exactly
    // the table's icon, never anything else.
    FM_CHECK(icon == kNativeIcon || icon == gerudo->icon, "R6", "the Stone slot showed icon %s", icon);
    FM_CHECK(MM_ForeignMaskTest_PageIcon(cell + 1, kNativeIcon) == kNativeIcon &&
                 MM_ForeignMaskTest_PageIcon(-1, kNativeIcon) == kNativeIcon &&
                 MM_ForeignMaskTest_PageIcon(1000, kNativeIcon) == kNativeIcon,
             "R6", "a cell that shares nothing showed another icon");
    MM_ForeignMaskTest_CycleSlot(COMBO_FOREIGN_MASK_SLOT_MM_STONE, 1);
    MM_ForeignMaskTest_CycleSlot(COMBO_FOREIGN_MASK_SLOT_MM_STONE, 1);
    FM_CHECK(MM_ForeignMaskTest_SlotSelection(COMBO_FOREIGN_MASK_SLOT_MM_STONE) == 0, "R6",
             "three presses did not bring Stone's slot back to the Stone Mask");
    MM_ForeignMaskTest_SetSlotSel(COMBO_FOREIGN_MASK_SLOT_MM_STONE, COMBO_FOREIGN_MASK_OOT_SPOOKY);
    FM_CHECK(MM_ForeignMaskTest_SlotSelection(COMBO_FOREIGN_MASK_SLOT_MM_STONE) == 0, "R6",
             "Gibdo's Spooky Mask was chosen in Stone's slot");

    // ---- R7. both: a mask whose rule is off is neither put on nor kept --------
    // The two ports' live saves are one storage (src/common/unified_save.c):
    // arming MM's zeroes OoT's linkAge (adult) and MM's equippedMask is OoT's
    // linkAge low byte. Each half arms its own game and freezes its own record.
    FmResetAll();
    OoT_ForeignMaskTest_ArmLive();
    OoT_ForeignMaskTest_SetPlayerMask(0);
    FM_CHECK(OoT_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_MM_STONE) == 1, "R7",
             "OoT: with the rules on, the Stone Mask could not be put on");
    FmFreezeRules(kFmAllRules & ~RSBS_XITEM_STONE_MASK_OOT);
    FM_CHECK(OoT_ForeignMaskTest_Worn() == 0, "R7", "OoT: the Stone Mask reads as worn with its rule off (%u)",
             (unsigned)OoT_ForeignMaskTest_Worn());
    OoT_ForeignMaskTest_ApplyRules();
    FM_CHECK(OoT_ForeignMaskTest_Stored() == 0, "R7", "OoT: the rules kept the Stone Mask with its rule off (%u)",
             (unsigned)OoT_ForeignMaskTest_Stored());
    FM_CHECK(OoT_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_MM_STONE) == 0 && OoT_ForeignMaskTest_Stored() == 0, "R7",
             "OoT: the Stone Mask was put on with its rule off");
    FM_CHECK(OoT_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_MM_BLAST) == 1, "R7",
             "OoT: the Blast Mask, whose rule is on, was refused beside the Stone Mask's");
    FM_CHECK(OoT_ForeignMaskTest_WornDl(1) != NULL, "R7", "OoT's worn Blast Mask selects no list");
    ComboContext_Init(); // nothing frozen: every rule off
    FM_CHECK(OoT_ForeignMaskTest_Worn() == 0 && OoT_ForeignMaskTest_WornDl(1) == NULL, "R7",
             "OoT: with no world frozen a mask reads as worn");

    FmFreezeRules(kFmAllRules);
    MM_ForeignMaskTest_ArmLive();
    MM_ForeignMaskTest_SetForm(1);
    MM_ForeignMaskTest_SetPlayerMask(0);
    FM_CHECK(MM_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_OOT_SKULL) == 1, "R7",
             "MM: with the rules on, the Skull Mask could not be put on");
    FmFreezeRules(kFmAllRules & ~RSBS_XITEM_SKULL_MASK_MM);
    FM_CHECK(MM_ForeignMaskTest_Worn() == 0, "R7", "MM: the Skull Mask reads as worn with its rule off (%u)",
             (unsigned)MM_ForeignMaskTest_Worn());
    MM_ForeignMaskTest_ApplyRules();
    FM_CHECK(MM_ForeignMaskTest_Stored() == 0, "R7", "MM: the rules kept the Skull Mask with its rule off (%u)",
             (unsigned)MM_ForeignMaskTest_Stored());
    FM_CHECK(MM_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_OOT_SKULL) == 0 && MM_ForeignMaskTest_Stored() == 0, "R7",
             "MM: the Skull Mask was put on with its rule off");
    FM_CHECK(MM_ForeignMaskTest_Use(COMBO_FOREIGN_MASK_OOT_GERUDO) == 1, "R7",
             "MM: the Gerudo Mask, whose rule is on, was refused beside the Skull Mask's");
    FM_CHECK(MM_ForeignMaskTest_WornDl() != NULL, "R7", "MM's worn Gerudo Mask selects no list");
    ComboContext_Init(); // nothing frozen: every rule off
    FM_CHECK(MM_ForeignMaskTest_Worn() == 0 && MM_ForeignMaskTest_WornDl() == NULL, "R7",
             "MM: with no world frozen a mask reads as worn");

    // ---- R8. OoT: the head draw's selection (OoT_ForeignMask_WornDl) ----------
    // What OoT_ForeignMask_DrawOnHead draws: the worn mask's table list for a
    // child with no native mask on and the head list drawn, else nothing.
    FmFreezeRules(kFmAllRules);
    OoT_ForeignMaskTest_ArmLive();
    OoT_ForeignMaskTest_SetPlayerMask(0);
    FM_CHECK(OoT_ForeignMaskTest_WornDl(1) == NULL, "R8", "OoT: nothing worn selects a worn list");
    for (int v = 0; v < 256; v++) {
        OoT_ForeignMaskTest_SetStored((uint8_t)v);
        const bool wearable = Combo_ForeignMask_Wearable((uint8_t)v, GAME_OOT) != 0;
        const ComboForeignMaskInfo* info = Combo_ForeignMask_Info((uint8_t)v);
        const char* dl = OoT_ForeignMaskTest_WornDl(1);
        FM_CHECK(wearable ? (dl != NULL && info != NULL && dl == info->wornDl) : dl == NULL, "R8",
                 "OoT: stored byte %d selects worn list %s", v, dl != NULL ? dl : "(none)");
    }
    OoT_ForeignMaskTest_SetStored(COMBO_FOREIGN_MASK_MM_KAMARO);
    const ComboForeignMaskInfo* kamaro = Combo_ForeignMask_Info(COMBO_FOREIGN_MASK_MM_KAMARO);
    FM_CHECK(kamaro != NULL && kamaro->wornDl != NULL && OoT_ForeignMaskTest_WornDl(1) == kamaro->wornDl, "R8",
             "OoT: a child wearing Kamaro's Mask with the head drawn selects no list");
    FM_CHECK(OoT_ForeignMaskTest_WornDl(0) == NULL, "R8", "OoT: the head list not drawn (first person) selects a list");
    OoT_ForeignMaskTest_SetPlayerMask(1); // Keaton on, the rules not yet applied
    FM_CHECK(OoT_ForeignMaskTest_WornDl(1) == NULL, "R8", "OoT: a native mask on selects the worn list too");
    OoT_ForeignMaskTest_SetPlayerMask(0);
    OoT_ForeignMaskTest_SetAdult(1);
    FM_CHECK(OoT_ForeignMaskTest_WornDl(1) == NULL, "R8", "OoT: an adult Link selects a worn list");
    OoT_ForeignMaskTest_SetAdult(0);
    FmFreezeRules(kFmAllRules & ~RSBS_XITEM_KAMARO_MASK_OOT);
    FM_CHECK(OoT_ForeignMaskTest_WornDl(1) == NULL, "R8", "OoT: Kamaro's Mask with its rule off selects a list");
    FmFreezeRules(kFmAllRules);
    FM_CHECK(OoT_ForeignMaskTest_Stored() == COMBO_FOREIGN_MASK_MM_KAMARO &&
                 OoT_ForeignMaskTest_WornDl(1) == kamaro->wornDl,
             "R8", "OoT: the selection changed the worn field");

    FmLeaveClean();
    return FmVerdict("foreign-mask-rules");
}

TestResult Test_ForeignMaskPersistence(void) {
    printf("[TEST] foreign-mask-persistence: both worn fields cross with their half and ride the .redsave (#977)\n");
    sFmFailures = 0;
    FmResetAll();
    // The fields' bytes cross and persist whatever the rules say, so this row
    // freezes no world: a .redsave whose record holds a rule no row of this
    // build authors yet is refused at load (RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS).
    ComboContext_Init();

    // ---- P1. the fields are appended -------------------------------------------
    printf("[TEST] P1: OoT field at %zu of %zu (randomizerInf at %zu); MM field at %zu, Save is %zu (excluded checks "
           "at %zu)\n",
           OoT_ForeignMaskTest_FieldOffset(), OoT_ForeignMaskTest_SaveSize(), OoT_ForeignMaskTest_PrevLastOffset(),
           MM_ForeignMaskTest_FieldOffset(), MM_ForeignMaskTest_SaveSize(), MM_ForeignMaskTest_PrevLastOffset());
    FM_CHECK(OoT_ForeignMaskTest_FieldOffset() > OoT_ForeignMaskTest_PrevLastOffset() &&
                 OoT_ForeignMaskTest_FieldOffset() < OoT_ForeignMaskTest_SaveSize() &&
                 OoT_ForeignMaskTest_SaveSize() <= OOT_SAVE_CONTEXT_SIZE,
             "P1", "OoT's field is not appended inside the Tier-2 blob");
    FM_CHECK(MM_ForeignMaskTest_FieldOffset() > MM_ForeignMaskTest_PrevLastOffset() &&
                 MM_ForeignMaskTest_FieldOffset() < MM_ForeignMaskTest_SaveSize() &&
                 MM_ForeignMaskTest_SaveSize() <= MM_SAVE_CONTEXT_SIZE,
             "P1", "MM's field is not appended inside Save");

    // ---- P2. a crossing: flush, freeze, the other game, restore / consume -------
    Context_ClearAllFrozenStates();
    OoT_ForeignMaskTest_ArmLive();
    OoT_ForeignMaskTest_SetStored(COMBO_FOREIGN_MASK_MM_KAMARO);
    Combo_FlushLiveStateForFreeze(GAME_OOT);
    OoT_ForeignMaskTest_Freeze();
    MM_ForeignMaskTest_ArmLive(); // the two ports overlay one live storage
    MM_ForeignMaskTest_SetStored(COMBO_FOREIGN_MASK_OOT_SPOOKY);
    MM_ForeignMaskTest_SetSlotSel(COMBO_FOREIGN_MASK_SLOT_MM_STONE, COMBO_FOREIGN_MASK_OOT_SKULL);
    MM_ForeignMaskTest_SetSlotSel(COMBO_FOREIGN_MASK_SLOT_MM_GIBDO, COMBO_FOREIGN_MASK_OOT_SPOOKY);
    Combo_FlushLiveStateForFreeze(GAME_MM);
    MM_ForeignMaskTest_Freeze();
    OoT_ForeignMaskTest_ArmLive();
    FM_CHECK(OoT_ForeignMaskTest_Restore() == 1, "P2", "OoT's frozen half did not restore");
    FM_CHECK(OoT_ForeignMaskTest_Stored() == COMBO_FOREIGN_MASK_MM_KAMARO, "P2",
             "OoT's worn Kamaro's Mask was dropped at the crossing (field %u)", (unsigned)OoT_ForeignMaskTest_Stored());
    Combo_FlushLiveStateForFreeze(GAME_OOT);
    OoT_ForeignMaskTest_Freeze();
    MM_ForeignMaskTest_ArmLive();
    FM_CHECK(MM_ForeignMaskTest_Consume() == 1, "P2", "MM's frozen half was not consumed");
    FM_CHECK(MM_ForeignMaskTest_Stored() == COMBO_FOREIGN_MASK_OOT_SPOOKY, "P2",
             "MM's worn Spooky Mask was dropped at the crossing (field %u)", (unsigned)MM_ForeignMaskTest_Stored());
    FM_CHECK(MM_ForeignMaskTest_SlotSelStored(COMBO_FOREIGN_MASK_SLOT_MM_STONE) == COMBO_FOREIGN_MASK_OOT_SKULL &&
                 MM_ForeignMaskTest_SlotSelStored(COMBO_FOREIGN_MASK_SLOT_MM_GIBDO) == COMBO_FOREIGN_MASK_OOT_SPOOKY,
             "P2", "MM's shared-slot choices were dropped at the crossing");
    Combo_FlushLiveStateForFreeze(GAME_MM);
    MM_ForeignMaskTest_Freeze();

    // ---- P3. a .redsave save and load --------------------------------------------
    // A per-process scratch directory (#926), and the previous save directory
    // put back afterwards, so no other row sees this one's files.
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    const std::string priorSaveDir = std::filesystem::path(mgr.SlotPath(0)).parent_path().string();
    mgr.SetSaveDirectory(rsbs::test::ScratchDir("rsbs_test_foreign_masks").string());
    RsbsSave_ResetSlotSessionState();
    mgr.DeleteSave(0);
    FM_CHECK(mgr.Save(0), "P3", "Save(0) failed");
    ComboContext_Init();
    Context_ClearAllFrozenStates();
    OoT_ForeignMaskTest_ArmLive();
    FM_CHECK(mgr.Load(0), "P3", "Load(0) failed");
    FM_CHECK(Context_ArmShadowAsFrozen(GAME_OOT, 0x01D1) == 1 && Context_ArmShadowAsFrozen(GAME_MM, 0xD800) == 1, "P3",
             "a loaded half did not arm");
    OoT_ForeignMaskTest_ArmLive();
    FM_CHECK(OoT_ForeignMaskTest_Restore() == 1 && OoT_ForeignMaskTest_Stored() == COMBO_FOREIGN_MASK_MM_KAMARO, "P3",
             "OoT's worn Kamaro's Mask did not survive the .redsave (field %u)",
             (unsigned)OoT_ForeignMaskTest_Stored());
    MM_ForeignMaskTest_ArmLive();
    FM_CHECK(MM_ForeignMaskTest_Consume() == 1 && MM_ForeignMaskTest_Stored() == COMBO_FOREIGN_MASK_OOT_SPOOKY &&
                 MM_ForeignMaskTest_SlotSelStored(COMBO_FOREIGN_MASK_SLOT_MM_STONE) == COMBO_FOREIGN_MASK_OOT_SKULL,
             "P3", "MM's worn Spooky Mask or slot choice did not survive the .redsave (field %u)",
             (unsigned)MM_ForeignMaskTest_Stored());
    mgr.DeleteSave(0);
    mgr.SetSaveDirectory(priorSaveDir);

    Context_ClearAllFrozenStates();
    FmLeaveClean();
    return FmVerdict("foreign-mask-persistence");
}

TestResult Test_OoTChildTradeCycle(void) {
    printf("[TEST] oot-child-trade-cycle: the list-based child trade cycle (#977)\n");
    sFmFailures = 0;
    FmResetAll();

    // ---- C1. with no extra, exactly the contiguous cycle ----------------------
    const int count = OoT_TradeCycleTest_ChildTradeCount();
    FM_CHECK(count == 11, "C1", "OoT's child trade range has %d items, not 11", count);
    int compared = 0;
    int mismatches = 0;
    for (uint32_t bits = 0; bits < (1u << count); bits++) {
        for (int item = 0; item < 256; item++) {
            OoT_TradeCycleTest_Set((uint8_t)item, (uint16_t)bits);
            const uint8_t next = OoT_TradeCycleTest_Next();
            const uint8_t legacyNext = OoT_TradeCycleTest_LegacyNext();
            const uint8_t prev = OoT_TradeCycleTest_Prev();
            const uint8_t legacyPrev = OoT_TradeCycleTest_LegacyPrev();
            compared += 2;
            if (next != legacyNext || prev != legacyPrev) {
                if (mismatches++ < 5) {
                    printf("  slot 0x%02X owned 0x%03X: next %u (contiguous %u), prev %u (contiguous %u)\n", item, bits,
                           (unsigned)next, (unsigned)legacyNext, (unsigned)prev, (unsigned)legacyPrev);
                }
            }
        }
    }
    printf("[TEST] C1: %d answers compared with the contiguous cycle, %d differ\n", compared, mismatches);
    FM_CHECK(mismatches == 0, "C1", "the list cycle differs from the contiguous one in %d case(s)", mismatches);

    // ---- C2. a registered custom ItemID joins the cycle -------------------------
    const uint8_t first = OoT_TradeCycleTest_FirstItem();
    const uint8_t last = (uint8_t)(first + count - 1); // ITEM_MASK_TRUTH
    const uint8_t custom = OoT_TradeCycleTest_FirstFreeCustomItem();
    static int sExtraOwned = 1;
    FM_CHECK(OoT_TradeCycleTest_RegisterExtra(custom, []() { return sExtraOwned; }) == 1, "C2",
             "the custom id 0x%02X was refused", (unsigned)custom);
    FM_CHECK(OoT_TradeCycleTest_RegisterExtra(custom, []() { return 1; }) == 0 &&
                 OoT_TradeCycleTest_RegisterExtra(last, []() { return 1; }) == 0,
             "C2", "a duplicate or an in-range id was registered");
    const uint16_t truthAndEgg = (uint16_t)((1u << (count - 1)) | 1u);
    OoT_TradeCycleTest_Set(last, truthAndEgg);
    FM_CHECK(OoT_TradeCycleTest_Next() == custom, "C2", "after the Mask of Truth came 0x%02X, not the custom id",
             (unsigned)OoT_TradeCycleTest_Next());
    OoT_TradeCycleTest_Set(custom, truthAndEgg);
    FM_CHECK(OoT_TradeCycleTest_Next() == first && OoT_TradeCycleTest_Prev() == last, "C2",
             "from the custom id: next 0x%02X, prev 0x%02X", (unsigned)OoT_TradeCycleTest_Next(),
             (unsigned)OoT_TradeCycleTest_Prev());
    OoT_TradeCycleTest_Set(first, truthAndEgg);
    FM_CHECK(OoT_TradeCycleTest_Prev() == custom, "C2", "before the Weird Egg came 0x%02X, not the custom id",
             (unsigned)OoT_TradeCycleTest_Prev());
    sExtraOwned = 0;
    OoT_TradeCycleTest_Set(last, truthAndEgg);
    FM_CHECK(OoT_TradeCycleTest_Next() == first, "C2", "an unowned custom id was visited");
    OoT_TradeCycleTest_Set(custom, 0);
    FM_CHECK(OoT_TradeCycleTest_Next() == OoT_TradeCycleTest_ItemNone(), "C2", "nothing owned answered 0x%02X",
             (unsigned)OoT_TradeCycleTest_Next());
    sExtraOwned = 1;

    FmLeaveClean();
    return FmVerdict("oot-child-trade-cycle");
}
