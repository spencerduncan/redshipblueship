/**
 * @file test_oot_masks_mm.c
 * @brief Locks for Ocarina of Time's Gerudo, Skull and Spooky Masks in Majora's
 *        Mask (#980, epic #975 P5; OoTMM gerudoMaskMm, skullMaskMm, spookyMaskMm
 *        and sharedMaskGerudo, sharedMaskSkull, sharedMaskSpooky), the MM
 *        direction of the Blast and Stone Masks' rows (test_blast_mask_oot.c,
 *        test_stone_mask_oot.c).
 *
 * Two rows, split by what each one needs:
 *
 *   oot-masks-mm-rule (CTest OoTMasksMMRule, redship tier) -- THE RULES. The six
 *   tier-4 keys, labelled with OoTMM's names, off by default, exactly 0 or 1;
 *   each reaches its own bit of the resolved item rules and nothing else; each
 *   shared one is dropped while its mask in MM is off (OoTMM's `cond`); and a
 *   session that walked away from a file holding all six restores them by name
 *   at load, in OoTMM's page order beside the masks added to OoT (the record's
 *   round trip, frozen wins at load, #781).
 *
 *   oot-masks-mm-items (CTest OoTMasksMMItems, redship tier) -- THE ITEMS. The
 *   three RI ids appended before RI_MAX_TRAP with every existing id unchanged
 *   (pinned), their rows (RITYPE_MASK, OoTMM's names, progression), MM's REAL give
 *   and conversion through the slot state byte's owned bits (the chosen variant
 *   untouched), ownership gated by the frozen rule, both games' shared members
 *   (MM's already registered at static init, before the row re-registers them)
 *   and THE SHARED PAIR (one bag row per mask only under its shared rule), the
 *   wear path (a cell whose own mask is not owned shows the OoT mask, and its C
 *   button wears it), and the actor seam: OoTMM's list of the actors that read
 *   each mask, the Spooky Mask's night, the value put back after the update and
 *   after a vetoed one, and currentMask never holding a foreign id.
 *
 * The logic (the Deku Palace, the purple guard, the Gibdos at night) is the rando
 * row oot-masks-mm-logic (games/mm/2s2h/mm_oot_masks_test.cpp): it needs MM's
 * region graph.
 *
 * Not lockable headless: a real Gerudo, Deku guard or Gibdo reacting in a scene
 * (it needs a PlayState and the actors' overlays); the playtest covers it.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * after test_stone_mask_oot.c; every symbol under test is C-linkage, and every
 * name here carries the Omm prefix so it cannot meet a neighbour's.
 */

#include "../combo_logic.h"
#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../context.h"
#include "../cvar_shared_keys.h"
#include "../foreign_items.h"
#include "../foreign_masks.h"
#include "../shared_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif
// games/mm/2s2h/mm_oot_masks_test.cpp
int MM_OoTMasksTest_Id(int which);
int MM_OoTMasksTest_ActorId(int which);
int MM_OoTMasksTest_Const(int which);
void MM_OoTMasksTest_ArmLive(void);
void MM_OoTMasksTest_Give(uint16_t ri);
int MM_OoTMasksTest_Convert(uint16_t ri);
int MM_OoTMasksTest_IsMaskType(uint16_t ri);
const char* MM_OoTMasksTest_Name(uint16_t ri);
int MM_OoTMasksTest_IsJunk(int ri);
void MM_OoTMasksTest_SetNativeOwned(uint8_t slot, int owned);
int MM_OoTMasksTest_PageCell(uint8_t slot);
uint8_t MM_OoTMasksTest_CellStored(int cell);
uint8_t MM_OoTMasksTest_ItemConst(int which);
void MM_OoTMasksTest_SetPlayer(int human, uint8_t mask);
int MM_OoTMasksTest_PlayerMask(void);
int MM_OoTMasksTest_PlayerForm(void);
int MM_OoTMasksTest_Use(uint8_t mask);
int MM_OoTMasksTest_EquipNative(int maskId);
int MM_OoTMasksTest_Before(int16_t actorId, int updates, int isNight);
void MM_OoTMasksTest_PutBack(void);
// games/mm/2s2h/Rando/OoTMasksMM.cpp
uint8_t MM_OoTMask_ForeignId(uint16_t riId);
uint16_t MM_OoTMask_ItemId(uint8_t foreignMask);
bool MM_OoTMask_Owned(uint16_t riId);
bool MM_OoTMask_Usable(uint16_t riId);
int MM_OoTMasks_SpoofFor(uint8_t worn, int16_t actorId, int isNight, uint8_t* maskOut, uint8_t* formOut);
void MM_OoTMasks_RegisterRuntime(void);
void MM_OoTMasks_Forget(void);
int MM_OoTMasks_Holding(void);
// games/mm/2s2h/Rando/ForeignMaskMM.cpp
uint8_t MM_ForeignMask_Worn(void);
uint8_t MM_ForeignMask_SlotSelection(uint8_t slot);
uint8_t MM_ForeignMask_SlotSelRaw(uint8_t slot);
void MM_ForeignMask_SetSlotSelRaw(uint8_t slot, uint8_t value);
void MM_ForeignMask_SetStoredRaw(uint8_t value);
uint8_t MM_ForeignMask_PageCellItem(int maskSlot, uint8_t stored);
// games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp
int MM_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
#ifdef __cplusplus
}
#endif

static int sOmmFailures;

#define OMM_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sOmmFailures++;                           \
        }                                             \
    } while (0)

namespace {

// The rule bits, spelled as #976 reserved them (literals, so the lock states the
// .redsave format rather than reading it back from the header under test).
const uint32_t kOmmGerudoMM = 0x00000100u;
const uint32_t kOmmSkullMM = 0x00000200u;
const uint32_t kOmmSpookyMM = 0x00000400u;
const uint32_t kOmmSharedGerudo = 0x00004000u;
const uint32_t kOmmSharedSkull = 0x00008000u;
const uint32_t kOmmSharedSpooky = 0x00010000u;
const uint32_t kOmmExtensions = kOmmGerudoMM | kOmmSkullMM | kOmmSpookyMM;
const uint32_t kOmmShared = kOmmSharedGerudo | kOmmSharedSkull | kOmmSharedSpooky;
const uint32_t kOmmSettingsHash = 0x0980D1F5u;
const uint32_t kOmmProfileDigest = 0x5980A11Fu;

const ComboSettingId kOmmIds[] = {
    COMBO_SETTING_GERUDO_MASK_MM,     COMBO_SETTING_SKULL_MASK_MM,     COMBO_SETTING_SPOOKY_MASK_MM,
    COMBO_SETTING_SHARED_MASK_GERUDO, COMBO_SETTING_SHARED_MASK_SKULL, COMBO_SETTING_SHARED_MASK_SPOOKY,
};
const char* const kOmmKeys[] = { "GerudoMaskMM",     "SkullMaskMM",     "SpookyMaskMM",
                                 "SharedMaskGerudo", "SharedMaskSkull", "SharedMaskSpooky" };
const char* const kOmmLabels[] = { "Gerudo Mask (MM)",   "Skull Mask (MM)",   "Spooky Mask (MM)",
                                   "Shared Gerudo Mask", "Shared Skull Mask", "Shared Spooky Mask" };
const uint32_t kOmmBits[] = { kOmmGerudoMM,     kOmmSkullMM,     kOmmSpookyMM,
                              kOmmSharedGerudo, kOmmSharedSkull, kOmmSharedSpooky };

/** A paired world frozen with the shipped combo record and @p flags as its item
 *  rules (test_stone_mask_oot.c's StmFreezeWorld). */
void OmmFreezeWorld(uint32_t flags) {
    ComboContext_Init();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0980570Eu;
    gComboCtx.sharedRandoSettingsHash = kOmmSettingsHash;
    gComboCtx.mmProfileDigest = kOmmProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = flags;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

void OmmClearKeys() {
    for (const ComboSettingId id : kOmmIds) {
        Combo_ComboSettingClear(id);
    }
    Combo_ComboSettingClear(COMBO_SETTING_BLAST_MASK_OOT);
    Combo_ComboSettingClear(COMBO_SETTING_SHARED_MASK_BLAST);
    Combo_ComboSettingClear(COMBO_SETTING_STONE_MASK_OOT);
    Combo_ComboSettingClear(COMBO_SETTING_SHARED_MASK_STONE);
}

} // namespace

// ============================================================================
// oot-masks-mm-rule
// ============================================================================

TestResult OoTMasksMMRule_Run(void) {
    printf("[TEST] oot-masks-mm-rule: the Gerudo, Skull and Spooky Mask (MM) keys and their shared variants author "
           "the item rules, each shared one only with its mask in MM, and a file's values come back at load in "
           "OoTMM's order (#980)\n");
    sOmmFailures = 0;
    OMM_CHECK(Combo_ComboSettingStoreAvailable(), "no CVar store: every key leg below would pass on the defaults");
    ComboContext_Init();
    OmmClearKeys();

    // ---- (1) The keys, their labels, defaults and value spaces -------------------
    {
        const std::string prefix = RSBS::kComboIdentityKeyPrefix;
        for (int i = 0; i < 6; i++) {
            const ComboSettingId id = kOmmIds[i];
            OMM_CHECK(Combo_ComboSettingKey(id) == prefix + kOmmKeys[i], "row %d's key is %s", i,
                      Combo_ComboSettingKey(id));
            OMM_CHECK(strcmp(Combo_ComboSettingLabel(id), kOmmLabels[i]) == 0, "row %d is labelled '%s', not '%s'", i,
                      Combo_ComboSettingLabel(id), kOmmLabels[i]);
            OMM_CHECK(Combo_ComboSettingDefault(id) == 0, "'%s' must ship off (OoTMM's defaults; no world moves)",
                      kOmmLabels[i]);
            OMM_CHECK(Combo_ComboSettingValueValid(id, 0) && Combo_ComboSettingValueValid(id, 1) &&
                          !Combo_ComboSettingValueValid(id, 2) && !Combo_ComboSettingValueValid(id, -1),
                      "'%s' is not exactly 0 or 1", kOmmLabels[i]);
        }
    }

    // ---- (2) The resolver ---------------------------------------------------------
    {
        const uint32_t mine = kOmmExtensions | kOmmShared;
        ComboItemRulesRecord r;
        Combo_ResolveComboItemRules(&r);
        OMM_CHECK((r.crossItemFlags & mine) == 0u, "unset keys resolved flags %08X", (unsigned)r.crossItemFlags);

        // Each key alone authors its own bit, a shared one alone nothing (`cond`).
        for (int i = 0; i < 6; i++) {
            OmmClearKeys();
            OMM_CHECK(Combo_ComboSettingSet(kOmmIds[i], 1) == 1, "the writer refused '%s'", kOmmLabels[i]);
            Combo_ResolveComboItemRules(&r);
            const uint32_t want = i < 3 ? kOmmBits[i] : 0u;
            OMM_CHECK((r.crossItemFlags & mine) == want && Combo_ComboItemRulesValid(&r),
                      "'%s' alone resolved %08X, expected %08X within the six bits", kOmmLabels[i],
                      (unsigned)r.crossItemFlags, (unsigned)want);
        }
        // A shared one with its own extension, and never with another mask's.
        for (int i = 0; i < 3; i++) {
            OmmClearKeys();
            OMM_CHECK(Combo_ComboSettingSet(kOmmIds[(i + 1) % 3], 1) == 1 && Combo_ComboSettingSet(kOmmIds[3 + i], 1),
                      "the writer refused a key");
            Combo_ResolveComboItemRules(&r);
            OMM_CHECK((r.crossItemFlags & kOmmBits[3 + i]) == 0u,
                      "'%s' froze with another mask's extension (%08X); it needs its own", kOmmLabels[3 + i],
                      (unsigned)r.crossItemFlags);
            OMM_CHECK(Combo_ComboSettingSet(kOmmIds[i], 1) == 1, "the writer refused '%s'", kOmmLabels[i]);
            Combo_ResolveComboItemRules(&r);
            OMM_CHECK((r.crossItemFlags & (kOmmBits[i] | kOmmBits[3 + i])) == (kOmmBits[i] | kOmmBits[3 + i]) &&
                          Combo_ComboItemRulesValid(&r),
                      "'%s' with its extension resolved %08X", kOmmLabels[3 + i], (unsigned)r.crossItemFlags);
        }
        OMM_CHECK((Combo_ComboItemRuleKeyedBits() & mine) == mine, "the six rule bits are not keyed (%08X)",
                  (unsigned)Combo_ComboItemRuleKeyedBits());
        OMM_CHECK((Combo_ComboItemRuleKeyedBits() & (uint32_t)RSBS_XITEM_KAMARO_MASK_OOT) == 0u,
                  "Kamaro's Mask is not this lane's: its bit must stay unkeyed");
        OmmClearKeys();
    }

    // ---- (3) Frozen wins at load: the record's round trip ---------------------------
    {
        OmmFreezeWorld(kOmmExtensions | kOmmShared);
        uint32_t bits = Combo_ComboSettingsDivergence();
        OMM_CHECK(bits == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                  "a session with every key unset diverges by %08X from a file holding all six", (unsigned)bits);
        OMM_CHECK(Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits) == 1,
                  "the load cannot restore the file's six rules");
        char names[400];
        ComboSettingsKeyUndo undo;
        int ok = Combo_ComboSettingsRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits, names,
                                                sizeof(names), &undo);
        printf("[TEST] (3) restored: %s\n", names);
        OMM_CHECK(ok == 1 &&
                      strcmp(names, "Gerudo Mask (MM), Skull Mask (MM), Spooky Mask (MM), Shared Gerudo Mask, Shared "
                                    "Skull Mask, Shared Spooky Mask") == 0,
                  "the restore wrote '%s', not the six rows in page order", names);
        OMM_CHECK(Combo_ComboSettingsDivergence() == 0u, "after the restore the pair still diverges by %08X",
                  (unsigned)Combo_ComboSettingsDivergence());
        ComboItemRulesRecord live;
        Combo_ResolveComboItemRules(&live);
        OMM_CHECK((live.crossItemFlags & (kOmmExtensions | kOmmShared)) == (kOmmExtensions | kOmmShared),
                  "the restored keys resolve %08X, not the file's six bits", (unsigned)live.crossItemFlags);
        Combo_ComboSettingsRestoreUndo(&undo);
        for (const ComboSettingId id : kOmmIds) {
            OMM_CHECK(!Combo_ComboSettingIsExplicit(id), "the undo did not put '%s' back to unset",
                      Combo_ComboSettingLabel(id));
        }

        // Beside the masks added to OoT: OoTMM's page order, the masks added to OoT,
        // then those added to MM, then their shared variants in the same order.
        OmmFreezeWorld((uint32_t)RSBS_XITEM_BLAST_MASK_OOT | (uint32_t)RSBS_XITEM_SHARED_BLAST | kOmmSkullMM |
                       kOmmSharedSkull);
        bits = Combo_ComboSettingsDivergence();
        ok = Combo_ComboSettingsRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits, names,
                                            sizeof(names), &undo);
        printf("[TEST] (3) restored with the Blast Mask's: %s\n", names);
        OMM_CHECK(ok == 1 &&
                      strcmp(names, "Blast Mask (OoT), Skull Mask (MM), Shared Blast Mask, Shared Skull Mask") == 0,
                  "the restore wrote '%s', not OoTMM's order", names);
        Combo_ComboSettingsRestoreUndo(&undo);
        ComboContext_Init();
        OmmClearKeys();
    }

    printf("[TEST] oot-masks-mm-rule: %s\n", sOmmFailures == 0 ? "PASS" : "FAIL");
    return sOmmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// oot-masks-mm-items
// ============================================================================

TestResult Test_OoTMasksMMItems(void) {
    printf("[TEST] oot-masks-mm-items: the three MM ids are appended, given and owned through the slot state byte "
           "only under the frozen rule, pair into one bag row under the shared rule, are worn from a cell whose own "
           "mask is not owned, and the listed actors read them for exactly their update (#980)\n");
    sOmmFailures = 0;
    const uint16_t gerudo = (uint16_t)MM_OoTMasksTest_Id(0);
    const uint16_t skull = (uint16_t)MM_OoTMasksTest_Id(1);
    const uint16_t spooky = (uint16_t)MM_OoTMasksTest_Id(2);
    const uint16_t masks[3] = { gerudo, skull, spooky };
    const uint8_t wornIds[3] = { COMBO_FOREIGN_MASK_OOT_GERUDO, COMBO_FOREIGN_MASK_OOT_SKULL,
                                 COMBO_FOREIGN_MASK_OOT_SPOOKY };
    const uint32_t ruleBits[3] = { kOmmGerudoMM, kOmmSkullMM, kOmmSpookyMM };
    const uint32_t sharedBits[3] = { kOmmSharedGerudo, kOmmSharedSkull, kOmmSharedSpooky };
    static const char* const kOmmNames[3] = { "Gerudo Mask (MM)", "Skull Mask (MM)", "Spooky Mask (MM)" };

    // ---- (0) The static-init registration, before this row re-registers ----------
    // A paired creation can run before MM's ShipInit, so MM's three shared members
    // must already be registered at static init (OoTMasksMM.cpp's registrar). No
    // row clears these three families, so this holds in a --test all process too.
    // MM's ownership source is not checked here: test_foreign_masks.c resets it.
    for (int i = 0; i < 3; i++) {
        uint16_t mmId = 0;
        OMM_CHECK(Combo_SharedFamilyMemberId(GAME_MM, sharedBits[i], &mmId) && mmId == masks[i],
                  "MM's %s (%u) is not registered at static init (the family's MM member is %u)", kOmmNames[i],
                  (unsigned)masks[i], (unsigned)mmId);
    }
    MM_OoTMasks_RegisterRuntime();

    // ---- (1) The ids: appended, every saved id unchanged --------------------------
    {
        const int woodfallFairy = MM_OoTMasksTest_Id(3);
        const int maxTrap = MM_OoTMasksTest_Id(4);
        const int riMax = MM_OoTMasksTest_Id(5);
        printf("[TEST] (1) RI_WOODFALL_STRAY_FAIRY %d, the masks %u %u %u, RI_MAX_TRAP %d, RI_MAX %d; RI_MASK_STONE "
               "%d, RI_ABILITY_SWIM %d, RI_JUNK %d\n",
               woodfallFairy, (unsigned)gerudo, (unsigned)skull, (unsigned)spooky, maxTrap, riMax,
               MM_OoTMasksTest_Id(6), MM_OoTMasksTest_Id(7), MM_OoTMasksTest_Id(8));
        // RI_MAX_TRAP is not pinned to the last mask: a later lane appends after it
        // (#1169 XW5's warp songs).
        OMM_CHECK(gerudo == woodfallFairy + 1 && skull == gerudo + 1 && spooky == skull + 1 && maxTrap > spooky &&
                      riMax == maxTrap + 1,
                  "the three ids are not appended between RI_WOODFALL_STRAY_FAIRY and RI_MAX_TRAP");
        // Saved ids (randoSaveChecks[].randoItemId, the crossing store): pinned at
        // their pre-#980 values, so no existing world or crossing row renumbers.
        OMM_CHECK(MM_OoTMasksTest_Id(3) == 234 && MM_OoTMasksTest_Id(6) == 84 && MM_OoTMasksTest_Id(7) == 1 &&
                      MM_OoTMasksTest_Id(8) == 56,
                  "a saved RI_* value moved: RI_WOODFALL_STRAY_FAIRY %d (234), RI_MASK_STONE %d (84), "
                  "RI_ABILITY_SWIM %d (1), RI_JUNK %d (56)",
                  MM_OoTMasksTest_Id(3), MM_OoTMasksTest_Id(6), MM_OoTMasksTest_Id(7), MM_OoTMasksTest_Id(8));
        for (int i = 0; i < 3; i++) {
            const char* name = MM_OoTMasksTest_Name(masks[i]);
            OMM_CHECK(MM_OoTMasksTest_IsMaskType(masks[i]) == 1, "%s has no RITYPE_MASK row", kOmmNames[i]);
            OMM_CHECK(name != nullptr && strcmp(name, kOmmNames[i]) == 0, "id %u is named '%s', not OoTMM's '%s'",
                      (unsigned)masks[i], name != nullptr ? name : "(none)", kOmmNames[i]);
            ComboItemClassRow row;
            OMM_CHECK(MM_ComboLogic_ClassifyItem(masks[i], &row) == 1 && row.fillClass == RSBS_FILL_CLASS_PROGRESSION &&
                          row.armedBy == 0u,
                      "%s is not an unarmed progression item to the bag", kOmmNames[i]);
            OMM_CHECK(MM_OoTMask_ForeignId(masks[i]) == wornIds[i] && MM_OoTMask_ItemId(wornIds[i]) == masks[i],
                      "%s and worn mask %u do not map onto each other", kOmmNames[i], (unsigned)wornIds[i]);
        }
        OMM_CHECK(MM_OoTMask_ForeignId((uint16_t)MM_OoTMasksTest_Id(6)) == COMBO_FOREIGN_MASK_NONE &&
                      MM_OoTMask_ItemId(COMBO_FOREIGN_MASK_MM_STONE) == 0u,
                  "MM's own Stone Mask, or OoT's worn Stone Mask, maps onto an OoT mask in MM");
    }

    // ---- (2) The give, the slot state byte, the conversion and ownership ----------
    {
        OmmFreezeWorld(kOmmExtensions);
        MM_OoTMasksTest_ArmLive();
        MM_ForeignMask_SetSlotSelRaw(COMBO_FOREIGN_MASK_SLOT_MM_STONE, COMBO_FOREIGN_MASK_OOT_SKULL);
        for (int i = 0; i < 3; i++) {
            OMM_CHECK(!MM_OoTMask_Owned(masks[i]) && !Combo_ForeignMask_Owned(GAME_MM, wornIds[i]),
                      "a fresh save owns %s", kOmmNames[i]);
            OMM_CHECK(MM_OoTMasksTest_Convert(masks[i]) == (int)masks[i], "an unowned %s converted to %d", kOmmNames[i],
                      MM_OoTMasksTest_Convert(masks[i]));
            MM_OoTMasksTest_Give(masks[i]);
            OMM_CHECK(MM_OoTMask_Owned(masks[i]) && Combo_ForeignMask_Owned(GAME_MM, wornIds[i]) &&
                          MM_OoTMask_Usable(masks[i]),
                      "MM's give left %s unowned", kOmmNames[i]);
            const int second = MM_OoTMasksTest_Convert(masks[i]);
            OMM_CHECK(second != (int)masks[i] && MM_OoTMasksTest_IsJunk(second),
                      "a second %s converted to %d, not junk", kOmmNames[i], second);
        }
        const uint8_t stoneSlot = MM_ForeignMask_SlotSelRaw(COMBO_FOREIGN_MASK_SLOT_MM_STONE);
        const uint8_t gibdoSlot = MM_ForeignMask_SlotSelRaw(COMBO_FOREIGN_MASK_SLOT_MM_GIBDO);
        printf("[TEST] (2) slot state bytes after the three gives: Stone's %02X, Gibdo's %02X\n", (unsigned)stoneSlot,
               (unsigned)gibdoSlot);
        OMM_CHECK(stoneSlot == (uint8_t)(0x30u | COMBO_FOREIGN_MASK_OOT_SKULL) && gibdoSlot == 0x40u,
                  "the owned bits are not the Gerudo and Skull Masks' in Stone's byte and the Spooky Mask's in "
                  "Gibdo's, or the give moved the chosen variant (Stone's %02X, Gibdo's %02X)",
                  (unsigned)stoneSlot, (unsigned)gibdoSlot);
        OMM_CHECK(Combo_ForeignMask_SlotOwnedBit(COMBO_FOREIGN_MASK_OOT_GERUDO) == 0x10u &&
                      Combo_ForeignMask_SlotOwnedBit(COMBO_FOREIGN_MASK_OOT_SKULL) == 0x20u &&
                      Combo_ForeignMask_SlotOwnedBit(COMBO_FOREIGN_MASK_OOT_SPOOKY) == 0x40u &&
                      Combo_ForeignMask_SlotOwnedBit(COMBO_FOREIGN_MASK_MM_STONE) == 0u,
                  "the owned bits are .redsave format and moved");
        OMM_CHECK(MM_ForeignMask_SlotSelection(COMBO_FOREIGN_MASK_SLOT_MM_STONE) == COMBO_FOREIGN_MASK_OOT_SKULL,
                  "the chosen variant no longer reads through the owned bits");
        // The rule gates ownership: the bits stay, the masks are not owned.
        OmmFreezeWorld(kOmmExtensions & ~kOmmSkullMM);
        OMM_CHECK(MM_OoTMask_Owned(skull) && !Combo_ForeignMask_Owned(GAME_MM, COMBO_FOREIGN_MASK_OOT_SKULL) &&
                      !MM_OoTMask_Usable(skull) && MM_OoTMask_Usable(gerudo),
                  "with the Skull Mask's rule off the Skull Mask is still owned or usable (or the Gerudo Mask is not)");
        ComboContext_Init();
        OMM_CHECK(!MM_OoTMask_Usable(gerudo) && !Combo_ForeignMask_Owned(GAME_MM, COMBO_FOREIGN_MASK_OOT_GERUDO),
                  "with no world frozen the Gerudo Mask is usable");
    }

    // ---- (3) The shared members and THE SHARED PAIR ----------------------------------
    {
        const char* const kOotNames[3] = { "Gerudo Mask", "Skull Mask", "Spooky Mask" };
        ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
        for (int i = 0; i < 3; i++) {
            SharedItem oot;
            memset(&oot, 0, sizeof(oot));
            OMM_CHECK(TestNamedItem((uint8_t)GAME_OOT, kOotNames[i], &oot), "OoT has no item named \"%s\"",
                      kOotNames[i]);
            uint16_t ootId = 0;
            uint16_t mmId = 0;
            OMM_CHECK(Combo_SharedFamilyMemberId(GAME_OOT, sharedBits[i], &ootId) && ootId == oot.id,
                      "OoT's %s (%u) is not its shared family's OoT member (%u)", kOotNames[i], (unsigned)oot.id,
                      (unsigned)ootId);
            OMM_CHECK(Combo_SharedFamilyMemberId(GAME_MM, sharedBits[i], &mmId) && mmId == masks[i],
                      "MM's %s (%u) is not its shared family's MM member (%u)", kOmmNames[i], (unsigned)masks[i],
                      (unsigned)mmId);
            // MM's member: held and given through the owned bit, pooled under the rule.
            ComboSharedFamilyMember mm;
            memset(&mm, 0, sizeof(mm));
            if (Combo_SharedFamilyMemberGet(GAME_MM, sharedBits[i], &mm) && mm.held != nullptr && mm.give != nullptr &&
                mm.pooled != nullptr) {
                OmmFreezeWorld(ruleBits[i] | sharedBits[i]);
                MM_OoTMasksTest_ArmLive();
                OMM_CHECK(!mm.held(mm.nativeId) && mm.pooled(mm.nativeId), "%s's MM member: held or not pooled",
                          kOmmNames[i]);
                mm.give(mm.nativeId);
                OMM_CHECK(mm.held(mm.nativeId) && MM_OoTMask_Owned(masks[i]), "%s's MM member's give owned nothing",
                          kOmmNames[i]);
                OmmFreezeWorld(0u);
                OMM_CHECK(!mm.pooled(mm.nativeId), "%s's MM member is pooled with its rule off", kOmmNames[i]);
            } else {
                OMM_CHECK(false, "%s has no complete MM member", kOmmNames[i]);
            }

            OmmFreezeWorld(ruleBits[i]);
            int n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
            OMM_CHECK(n == 0, "the %s rule alone built %d shared pair(s)", kOmmNames[i], n);
            OmmFreezeWorld(ruleBits[i] | sharedBits[i]);
            n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
            printf("[TEST] (3) %s shared: %d pair(s)%s", kOmmNames[i], n, n > 0 ? ":" : "");
            for (int p = 0; p < n; p++) {
                printf(" (%u, %u)", (unsigned)pairs[p].ootId, (unsigned)pairs[p].mmId);
            }
            printf(" (expected 1: (%u, %u))\n", (unsigned)oot.id, (unsigned)masks[i]);
            OMM_CHECK(n == 1 && pairs[0].ootId == oot.id && pairs[0].mmId == masks[i],
                      "the shared rule did not compose exactly one row from OoT's and MM's %s", kOotNames[i]);
            OMM_CHECK((Combo_SharedFamiliesArmed() & sharedBits[i]) == sharedBits[i],
                      "the shared rule did not arm its family");
        }
        ComboContext_Init();
    }

    // ---- (4) Wearing: the cell, its C button, one mask at a time ---------------------
    {
        OmmFreezeWorld(kOmmExtensions);
        MM_OoTMasksTest_ArmLive();
        const int stoneCell = MM_OoTMasksTest_PageCell(COMBO_FOREIGN_MASK_SLOT_MM_STONE);
        const uint8_t none = MM_OoTMasksTest_ItemConst(2);
        const uint8_t stoneItem = MM_OoTMasksTest_ItemConst(0);
        OMM_CHECK(MM_ForeignMask_PageCellItem(stoneCell, MM_OoTMasksTest_CellStored(stoneCell)) == none,
                  "with nothing owned, Stone's cell is not empty");
        MM_OoTMasksTest_Give(gerudo);
        OMM_CHECK(MM_OoTMasksTest_CellStored(stoneCell) == none &&
                      MM_ForeignMask_PageCellItem(stoneCell, MM_OoTMasksTest_CellStored(stoneCell)) == stoneItem,
                  "with the Gerudo Mask owned and the Stone Mask not, Stone's cell does not show the shared slot (or "
                  "the inventory was written)");
        OMM_CHECK(MM_ForeignMask_SlotSelection(COMBO_FOREIGN_MASK_SLOT_MM_STONE) == COMBO_FOREIGN_MASK_OOT_GERUDO,
                  "the slot does not read as the Gerudo Mask while it is the only variant");
        MM_OoTMasksTest_SetPlayer(1, (uint8_t)MM_OoTMasksTest_Const(4));
        OMM_CHECK(MM_OoTMasksTest_EquipNative(MM_OoTMasksTest_Const(0)) == 1 &&
                      MM_ForeignMask_Worn() == COMBO_FOREIGN_MASK_OOT_GERUDO &&
                      MM_OoTMasksTest_PlayerMask() == MM_OoTMasksTest_Const(4),
                  "the cell's C button did not put the Gerudo Mask on through the foreign field");
        OMM_CHECK(MM_OoTMasksTest_EquipNative(MM_OoTMasksTest_Const(0)) == 1 && MM_ForeignMask_Worn() == 0,
                  "the C button again did not take it off");
        // The cell was lent and the Gerudo Mask is no longer usable (its rule off;
        // the owned bit stays): the cell is empty again and the C button wears
        // nothing, never the unowned Stone Mask.
        OmmFreezeWorld(kOmmExtensions & ~kOmmGerudoMM);
        OMM_CHECK(MM_OoTMask_Owned(gerudo) &&
                      MM_ForeignMask_SlotSelection(COMBO_FOREIGN_MASK_SLOT_MM_STONE) == COMBO_FOREIGN_MASK_NONE &&
                      MM_ForeignMask_PageCellItem(stoneCell, MM_OoTMasksTest_CellStored(stoneCell)) == none,
                  "with the Gerudo Mask's rule off, Stone's cell still shows the slot");
        OMM_CHECK(MM_OoTMasksTest_EquipNative(MM_OoTMasksTest_Const(0)) == 1 && MM_ForeignMask_Worn() == 0 &&
                      MM_OoTMasksTest_PlayerMask() == MM_OoTMasksTest_Const(4),
                  "a lent Stone C button with the Stone Mask unowned and the Gerudo Mask unusable put a mask on");
        // A slot whose cell was never lent (nothing held in it) keeps MM's own equip:
        // P5 takes over no C button it did not lend (test_foreign_masks.c R6, R6b).
        OmmFreezeWorld(kOmmExtensions);
        MM_OoTMasksTest_ArmLive();
        OMM_CHECK(MM_OoTMasksTest_EquipNative(MM_OoTMasksTest_Const(0)) == 0 &&
                      MM_OoTMasksTest_EquipNative(MM_OoTMasksTest_Const(1)) == 0,
                  "a Stone or Gibdo C button whose cell was never lent did not stay MM's own equip");
        MM_OoTMasksTest_SetNativeOwned(COMBO_FOREIGN_MASK_SLOT_MM_STONE, 1);
        OMM_CHECK(MM_OoTMasksTest_EquipNative(MM_OoTMasksTest_Const(0)) == 0,
                  "with the Stone Mask owned and nothing else, its C button no longer wears the Stone Mask");
        ComboContext_Init();
    }

    // ---- (5) The actor seam ------------------------------------------------------------
    {
        const int16_t ge2 = (int16_t)MM_OoTMasksTest_ActorId(0);
        const int16_t guardNuts = (int16_t)MM_OoTMasksTest_ActorId(5);
        const int16_t railgibud = (int16_t)MM_OoTMasksTest_ActorId(7);
        const int maskStone = MM_OoTMasksTest_Const(0);
        const int maskGibdo = MM_OoTMasksTest_Const(1);
        const int formDeku = MM_OoTMasksTest_Const(2);
        const int formHuman = MM_OoTMasksTest_Const(3);
        const int maskNone = MM_OoTMasksTest_Const(4);

        // OoTMM's list, mask for mask (packages/generator/src/mm/actors).
        struct Want {
            uint8_t worn;
            int actor;
            int night;
            int mask;
            int form;
        };
        const Want kWant[] = {
            { COMBO_FOREIGN_MASK_OOT_GERUDO, 0, 0, maskStone, -1 }, // En_Ge2.c
            { COMBO_FOREIGN_MASK_OOT_GERUDO, 11, 0, -1, -1 },       // Aveil (En_Ge3) is not fooled
            { COMBO_FOREIGN_MASK_OOT_GERUDO, 12, 0, -1, -1 },       // nor are the pirate fighters
            { COMBO_FOREIGN_MASK_OOT_SKULL, 1, 0, maskStone, -1 },  // En_Look_Nuts.c
            { COMBO_FOREIGN_MASK_OOT_SKULL, 2, 0, maskStone, -1 },  // skullmask_enemies.c: Keese
            { COMBO_FOREIGN_MASK_OOT_SKULL, 3, 0, maskStone, -1 },  // Guay
            { COMBO_FOREIGN_MASK_OOT_SKULL, 4, 0, maskStone, -1 },  // Bad Bat
            { COMBO_FOREIGN_MASK_OOT_SKULL, 5, 0, -1, formDeku },   // En_Guard_Nuts.c
            { COMBO_FOREIGN_MASK_OOT_SKULL, 6, 0, -1, formDeku },   // En_Dns.c
            { COMBO_FOREIGN_MASK_OOT_SKULL, 0, 0, -1, -1 },         // the purple guards see the Skull Mask
            { COMBO_FOREIGN_MASK_OOT_SPOOKY, 7, 1, maskGibdo, -1 }, // Player.c: the Gibdo Mask at night
            { COMBO_FOREIGN_MASK_OOT_SPOOKY, 8, 1, maskGibdo, -1 },
            { COMBO_FOREIGN_MASK_OOT_SPOOKY, 9, 1, maskGibdo, -1 },
            { COMBO_FOREIGN_MASK_OOT_SPOOKY, 10, 1, maskGibdo, -1 },
            { COMBO_FOREIGN_MASK_OOT_SPOOKY, 7, 0, -1, -1 }, // ... and never by day
            { COMBO_FOREIGN_MASK_OOT_SPOOKY, 10, 0, -1, -1 },
            { COMBO_FOREIGN_MASK_NONE, 0, 1, -1, -1 },
            { COMBO_FOREIGN_MASK_MM_STONE, 0, 1, -1, -1 }, // OoT's worn masks are not MM's
        };
        for (const Want& w : kWant) {
            uint8_t mask = 0;
            uint8_t form = 0;
            const int16_t actor = (int16_t)MM_OoTMasksTest_ActorId(w.actor);
            const int hit = MM_OoTMasks_SpoofFor(w.worn, actor, w.night, &mask, &form);
            const int gotMask = mask == 0xFF ? -1 : (int)mask;
            const int gotForm = form == 0xFF ? -1 : (int)form;
            OMM_CHECK(gotMask == w.mask && gotForm == w.form && hit == ((w.mask >= 0 || w.form >= 0) ? 1 : 0),
                      "worn %u, actor %d, night %d: mask %d form %d (OoTMM: %d %d)", (unsigned)w.worn, (int)actor,
                      w.night, gotMask, gotForm, w.mask, w.form);
        }

        // The seam on a real Player: set for the update, put back after it.
        OmmFreezeWorld(kOmmExtensions);
        MM_OoTMasksTest_ArmLive();
        MM_OoTMasks_Forget();
        MM_OoTMasksTest_Give(gerudo);
        MM_OoTMasksTest_Give(skull);
        MM_OoTMasksTest_Give(spooky);
        MM_OoTMasksTest_SetPlayer(1, (uint8_t)maskNone);
        OMM_CHECK(MM_OoTMasksTest_Use(COMBO_FOREIGN_MASK_OOT_GERUDO) == 1, "human Link could not wear the Gerudo Mask");
        OMM_CHECK(MM_OoTMasksTest_Before(ge2, 1, 0) == 1 && MM_OoTMasksTest_PlayerMask() == maskStone,
                  "the purple guard's update does not read the Stone Mask (%d)", MM_OoTMasksTest_PlayerMask());
        MM_OoTMasksTest_PutBack();
        OMM_CHECK(MM_OoTMasksTest_PlayerMask() == maskNone && !MM_OoTMasks_Holding(),
                  "after the update currentMask is %d, not none", MM_OoTMasksTest_PlayerMask());
        // An update another hook already kept from running is never touched.
        OMM_CHECK(MM_OoTMasksTest_Before(ge2, 0, 0) == 0 && MM_OoTMasksTest_PlayerMask() == maskNone,
                  "a vetoed update had currentMask set");
        // A hook that vetoes after the swap: the next put-back (the player's own
        // update or draw, or the next listed actor's) restores it.
        OMM_CHECK(MM_OoTMasksTest_Before(ge2, 1, 0) == 1, "the swap did not arm");
        OMM_CHECK(MM_OoTMasksTest_Before(guardNuts, 1, 0) == 0 && MM_OoTMasksTest_PlayerMask() == maskNone,
                  "the next listed actor did not put the held value back first (%d)", MM_OoTMasksTest_PlayerMask());
        MM_OoTMasksTest_PutBack();

        // The Skull Mask: a Deku to the palace guards, for the update only.
        OMM_CHECK(MM_OoTMasksTest_Use(COMBO_FOREIGN_MASK_OOT_SKULL) == 1, "human Link could not wear the Skull Mask");
        OMM_CHECK(MM_OoTMasksTest_Before(guardNuts, 1, 0) == 1 && MM_OoTMasksTest_PlayerForm() == formDeku &&
                      MM_OoTMasksTest_PlayerMask() == maskNone,
                  "the palace guard does not read a Deku (form %d, mask %d)", MM_OoTMasksTest_PlayerForm(),
                  MM_OoTMasksTest_PlayerMask());
        MM_OoTMasksTest_PutBack();
        OMM_CHECK(MM_OoTMasksTest_PlayerForm() == formHuman, "after the update Link is form %d, not human",
                  MM_OoTMasksTest_PlayerForm());

        // The Spooky Mask: the Gibdo Mask at night only.
        OMM_CHECK(MM_OoTMasksTest_Use(COMBO_FOREIGN_MASK_OOT_SPOOKY) == 1, "human Link could not wear the Spooky Mask");
        OMM_CHECK(MM_OoTMasksTest_Before(railgibud, 1, 0) == 0 && MM_OoTMasksTest_PlayerMask() == maskNone,
                  "a Gibdo read the Spooky Mask by day");
        OMM_CHECK(MM_OoTMasksTest_Before(railgibud, 1, 1) == 1 && MM_OoTMasksTest_PlayerMask() == maskGibdo,
                  "a Gibdo did not read the Gibdo Mask at night");
        MM_OoTMasksTest_PutBack();

        // A transformed Link wears nothing, and a native mask on is never replaced.
        MM_OoTMasksTest_SetPlayer(0, (uint8_t)maskNone);
        OMM_CHECK(MM_OoTMasksTest_Before(railgibud, 1, 1) == 0 && MM_OoTMasksTest_PlayerForm() == formDeku,
                  "a Deku Link's update was spoofed");
        MM_OoTMasksTest_SetPlayer(1, (uint8_t)maskGibdo);
        OMM_CHECK(MM_OoTMasksTest_Before(ge2, 1, 0) == 0 && MM_OoTMasksTest_PlayerMask() == maskGibdo,
                  "a native mask on was replaced");
        // The rule off: nothing is worn, nothing is read.
        MM_OoTMasksTest_SetPlayer(1, (uint8_t)maskNone);
        MM_ForeignMask_SetStoredRaw(COMBO_FOREIGN_MASK_OOT_GERUDO);
        OmmFreezeWorld(kOmmExtensions & ~kOmmGerudoMM);
        OMM_CHECK(MM_OoTMasksTest_Before(ge2, 1, 0) == 0 && MM_OoTMasksTest_PlayerMask() == maskNone,
                  "a Gerudo Mask whose rule is off fooled the purple guard");
        MM_OoTMasks_Forget();
        ComboContext_Init();
    }

    MM_OoTMasks_RegisterRuntime();
    printf("[TEST] oot-masks-mm-items: %s\n", sOmmFailures == 0 ? "PASS" : "FAIL");
    return sOmmFailures == 0 ? TEST_PASS : TEST_FAIL;
}
