/**
 * @file test_stone_mask_oot.c
 * @brief Locks for the Stone Mask in Ocarina of Time (#979, epic #975 P4's
 *        second mask; OoTMM stoneMaskOot and sharedMaskStone), the Blast Mask's
 *        rows (test_blast_mask_oot.c) for the Stone Mask.
 *
 * Two rows, split by what each one needs:
 *
 *   stone-mask-oot-rule (CTest StoneMaskOoTRule, redship tier) -- THE RULES. The
 *   two tier-4 keys (gCombo.Rando.StoneMaskOoT and SharedMaskStone), labelled
 *   with OoTMM's names, off by default, exactly 0 or 1; each reaches its own bit
 *   of the resolved item rules and nothing else; the shared one is dropped while
 *   the extension is off (OoTMM's `cond`), so the resolver never makes a record
 *   the validity rule refuses; and a session that walked away from a file holding
 *   both restores them by name at load (frozen wins at load, #781), in OoTMM's
 *   page order beside the Blast Mask's. Then OoTMM's list of the actors that lose
 *   sight of a Link wearing the Stone Mask, actor id for actor id.
 *
 *   stone-mask-oot-items (CTest StoneMaskOoTItems, redship tier) -- THE ITEM.
 *   The custom ItemID after the Blast Mask and the tables that must cover it
 *   (never SLOT()-indexed, inside gItemAgeReqs and sItemActions, its own curated
 *   icon and name, a child age row). Then ownership gated by the frozen rule, the
 *   child trade cycle visiting it only then and after the Blast Mask (OoTMM's
 *   order), the shared member's give, THE SHARED PAIR (one bag row only under the
 *   shared rule), OoT's REAL logic crediting it where OoTMM's get_past_skultullas
 *   and evade_gerudo do, for a child, only under the rule (and never where
 *   get_past_skultullas_no_stone leaves it out), and THE EFFECT's seam:
 *   a listed actor's update runs with Link far away, its true fields come back
 *   after it, and an update another handler kept from running is put back by the
 *   next actor's, never across a frame.
 *
 * Not lockable headless: a real enemy ignoring Link in a scene (it needs a
 * PlayState and the actors' overlays); the playtest covers it.
 *
 * Every key is spelled from the identity prefix rather than as a literal, so
 * the tier-4 classification scan sees only the manifest's spelling.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * after test_blast_mask_oot.c; every symbol under test is C-linkage, and every
 * name here carries the Stm prefix so it cannot meet a neighbour's.
 */

#include "../combo_logic.h"
#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../context.h"
#include "../curated_archives.h"
#include "../cvar_shared_keys.h"
#include "../foreign_items.h"
#include "../foreign_masks.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#ifdef __cplusplus
extern "C" {
#endif
// games/oot/soh/oot_stone_mask_test.cpp
uint8_t OoT_StoneMaskTest_ItemId(void);
uint8_t OoT_StoneMaskTest_BlastItemId(void);
int OoT_StoneMaskTest_PoolId(void);
int OoT_StoneMaskTest_IsAdvancement(void);
void OoT_StoneMaskTest_SetOwned(int held);
int OoT_StoneMaskTest_Owned(void);
void OoT_StoneMaskTest_SetBlastOwned(int held);
int OoT_StoneMaskTest_IgnoreListMismatches(int* firstBad, int* listed);
void OoT_StoneMaskTest_ArmActor(int slot, int kind);
int OoT_StoneMaskTest_Before(int slot, uint32_t frame, int updates);
void OoT_StoneMaskTest_After(int slot);
int OoT_StoneMaskTest_Seen(int slot);
int OoT_StoneMaskTest_Logic(int held, int child, int which);
// games/oot/soh/oot_blast_mask_test.cpp: the generic table and save readers
uint8_t OoT_BlastMaskTest_RocsFeatherId(void);
int OoT_BlastMaskTest_ItemSlotsCount(void);
uint32_t OoT_BlastMaskTest_ItemIconsCount(void);
const char* OoT_BlastMaskTest_Icon(uint8_t item);
int OoT_BlastMaskTest_AgeReq(uint8_t item);
int OoT_BlastMaskTest_ChildAge(void);
int OoT_BlastMaskTest_TradeSlot(void);
void OoT_BlastMaskTest_SetTradeSlot(uint8_t item);
// games/oot/soh/Enhancements/randomizer/StoneMaskOoT.cpp
void OoT_StoneMask_RegisterRuntime(void);
void OoT_StoneMask_Forget(void);
// games/oot/soh/oot_foreign_mask_test.cpp
void OoT_ForeignMaskTest_ArmLive(void);
void OoT_ForeignMaskTest_SetStored(uint8_t value);
void OoT_TradeCycleTest_Set(uint8_t item, uint16_t ownedBits);
uint8_t OoT_TradeCycleTest_Next(void);
uint8_t OoT_TradeCycleTest_Prev(void);
uint8_t OoT_TradeCycleTest_FirstItem(void);
int OoT_TradeCycleTest_ChildTradeCount(void);
void OoT_TradeCycleTest_ClearExtras(void);
// z_kaleido_scope_PAL.c and z_player.c (#983, #984): the two tables' entry counts
// and sItemActions' raw entry.
extern const uint32_t gItemAgeReqsCount;
extern const uint32_t gItemActionsCount;
int32_t OoT_Player_ItemActionTableEntry(uint32_t item);
#ifdef __cplusplus
}
#endif

static int sStmFailures;

#define STM_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sStmFailures++;                           \
        }                                             \
    } while (0)

namespace {

// The rule bits, spelled as #976 reserved them (literals, so the lock states the
// .redsave format rather than reading it back from the header under test).
const uint32_t kStmStoneOoT = 0x00000040u;
const uint32_t kStmSharedStone = 0x00001000u;
const uint32_t kStmBlastOoT = 0x00000020u;
const uint32_t kStmSharedBlast = 0x00000800u;
const uint32_t kStmSettingsHash = 0x09790140u;
const uint32_t kStmProfileDigest = 0x5701E5A5u;
const char* const kStmIconPath = "__OTR__icon_item_static_yar/gItemIconStoneMaskTex";
// OoTMM's shouldActorIgnorePlayer: 36 actors, and the Armos as the enemy.
const int kStmListedActors = 37;

/** A paired world frozen with the shipped combo record and @p flags as its item
 *  rules (test_cross_item_rules.c's XirFreezeWorld). */
void StmFreezeWorld(uint32_t flags) {
    ComboContext_Init();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0979570Eu;
    gComboCtx.sharedRandoSettingsHash = kStmSettingsHash;
    gComboCtx.mmProfileDigest = kStmProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = flags;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

void StmClearKeys() {
    Combo_ComboSettingClear(COMBO_SETTING_STONE_MASK_OOT);
    Combo_ComboSettingClear(COMBO_SETTING_SHARED_MASK_STONE);
    Combo_ComboSettingClear(COMBO_SETTING_BLAST_MASK_OOT);
    Combo_ComboSettingClear(COMBO_SETTING_SHARED_MASK_BLAST);
}

/** True when the embedded manifest carries `otrPath` ("__OTR__<path>") in the
 *  mm->oot half, by its path or a directory entry. */
bool StmCuratedIntoOoT(const char* otrPath) {
    static const char kOtr[] = "__OTR__";
    if (otrPath == nullptr || strncmp(otrPath, kOtr, sizeof(kOtr) - 1) != 0) {
        return false;
    }
    const std::string path(otrPath + sizeof(kOtr) - 1);
    std::istringstream in{ std::string(rsbs::curated::EmbeddedManifest()) };
    std::string line;
    static const std::string kDir = "mm->oot ";
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.compare(0, kDir.size(), kDir) != 0) {
            continue;
        }
        const std::string entry = line.substr(kDir.size());
        const bool dir = !entry.empty() && entry.back() == '/';
        if (dir ? path.compare(0, entry.size(), entry) == 0 : path == entry) {
            return true;
        }
    }
    return false;
}

} // namespace

// ============================================================================
// stone-mask-oot-rule
// ============================================================================

TestResult StoneMaskOoTRule_Run(void) {
    printf("[TEST] stone-mask-oot-rule: the Stone Mask (OoT) and Shared Stone Mask keys author the item rules, the "
           "shared one only with its extension, a file's values come back at load, and OoTMM's actors lose sight of "
           "the wearer (#979)\n");
    sStmFailures = 0;
    STM_CHECK(Combo_ComboSettingStoreAvailable(), "no CVar store: every key leg below would pass on the defaults");
    ComboContext_Init();
    StmClearKeys();

    // ---- (1) The keys, their labels, defaults and value spaces -------------------
    {
        const std::string prefix = RSBS::kComboIdentityKeyPrefix;
        STM_CHECK(Combo_ComboSettingKey(COMBO_SETTING_STONE_MASK_OOT) == prefix + "StoneMaskOoT" &&
                      Combo_ComboSettingKey(COMBO_SETTING_SHARED_MASK_STONE) == prefix + "SharedMaskStone",
                  "the keys are %s and %s", Combo_ComboSettingKey(COMBO_SETTING_STONE_MASK_OOT),
                  Combo_ComboSettingKey(COMBO_SETTING_SHARED_MASK_STONE));
        STM_CHECK(strcmp(Combo_ComboSettingLabel(COMBO_SETTING_STONE_MASK_OOT), "Stone Mask (OoT)") == 0 &&
                      strcmp(Combo_ComboSettingLabel(COMBO_SETTING_SHARED_MASK_STONE), "Shared Stone Mask") == 0,
                  "the rows are not labelled with OoTMM's names");
        STM_CHECK(Combo_ComboSettingDefault(COMBO_SETTING_STONE_MASK_OOT) == 0 &&
                      Combo_ComboSettingDefault(COMBO_SETTING_SHARED_MASK_STONE) == 0,
                  "every Stone Mask rule must ship off (OoTMM's defaults; no world moves)");
        for (const ComboSettingId id : { COMBO_SETTING_STONE_MASK_OOT, COMBO_SETTING_SHARED_MASK_STONE }) {
            STM_CHECK(Combo_ComboSettingValueValid(id, 0) && Combo_ComboSettingValueValid(id, 1) &&
                          !Combo_ComboSettingValueValid(id, 2) && !Combo_ComboSettingValueValid(id, -1),
                      "'%s' is not exactly 0 or 1", Combo_ComboSettingKey(id));
        }
    }

    // ---- (2) The resolver ---------------------------------------------------------
    {
        const uint32_t mine = kStmStoneOoT | kStmSharedStone;
        ComboItemRulesRecord r;
        Combo_ResolveComboItemRules(&r);
        STM_CHECK((r.crossItemFlags & mine) == 0u, "unset keys resolved flags %08X", (unsigned)r.crossItemFlags);

        STM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_STONE_MASK_OOT, 1) == 1, "the writer refused Stone Mask (OoT)");
        Combo_ResolveComboItemRules(&r);
        const uint32_t masks = mine | kStmBlastOoT | kStmSharedBlast | (uint32_t)RSBS_XITEM_KAMARO_MASK_OOT;
        STM_CHECK((r.crossItemFlags & masks) == kStmStoneOoT && Combo_ComboItemRulesValid(&r),
                  "Stone Mask (OoT) alone resolved %08X; it must author its own bit and no other mask's",
                  (unsigned)r.crossItemFlags);

        StmClearKeys();
        STM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_STONE, 1) == 1,
                  "the writer refused Shared Stone Mask");
        Combo_ResolveComboItemRules(&r);
        STM_CHECK((r.crossItemFlags & mine) == 0u && Combo_ComboItemRulesValid(&r),
                  "the Shared Stone Mask without its extension resolved %08X; OoTMM's cond makes it off, and a "
                  "record with it alone is not a world (Combo_ComboItemRulesValid)",
                  (unsigned)r.crossItemFlags);

        // The Blast Mask's extension is not the Stone Mask's.
        STM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_OOT, 1) == 1, "the writer refused Blast Mask (OoT)");
        Combo_ResolveComboItemRules(&r);
        STM_CHECK((r.crossItemFlags & mine) == 0u && (r.crossItemFlags & kStmBlastOoT) == kStmBlastOoT,
                  "with the Blast Mask (OoT) on, the Shared Stone Mask still needs its own extension (%08X)",
                  (unsigned)r.crossItemFlags);

        STM_CHECK(Combo_ComboSettingSet(COMBO_SETTING_STONE_MASK_OOT, 1) == 1, "the writer refused Stone Mask (OoT)");
        Combo_ResolveComboItemRules(&r);
        STM_CHECK((r.crossItemFlags & mine) == mine && Combo_ComboItemRulesValid(&r), "both on resolved flags %08X",
                  (unsigned)r.crossItemFlags);
        STM_CHECK((Combo_ComboItemRuleKeyedBits() & mine) == mine, "the two rule bits are not keyed (%08X)",
                  (unsigned)Combo_ComboItemRuleKeyedBits());
        STM_CHECK((Combo_ComboItemRuleKeyedBits() & (uint32_t)RSBS_XITEM_KAMARO_MASK_OOT) == 0u,
                  "Kamaro's Mask is not this lane's: its bit must stay unkeyed");
        StmClearKeys();
    }

    // ---- (3) Frozen wins at load ----------------------------------------------------
    {
        StmFreezeWorld(kStmStoneOoT | kStmSharedStone);
        uint32_t bits = Combo_ComboSettingsDivergence();
        STM_CHECK(bits == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                  "a session with every key unset diverges by %08X from a file holding both", (unsigned)bits);
        STM_CHECK(Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits) == 1,
                  "the load cannot restore the file's Stone Mask rules");
        char names[200];
        ComboSettingsKeyUndo undo;
        int ok = Combo_ComboSettingsRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits, names,
                                                sizeof(names), &undo);
        printf("[TEST] (3) restored: %s\n", names);
        STM_CHECK(ok == 1 && strcmp(names, "Stone Mask (OoT), Shared Stone Mask") == 0,
                  "the restore wrote '%s', not the two rows in page order", names);
        STM_CHECK(Combo_ComboSettingsDivergence() == 0u, "after the restore the pair still diverges by %08X",
                  (unsigned)Combo_ComboSettingsDivergence());
        Combo_ComboSettingsRestoreUndo(&undo);
        STM_CHECK(!Combo_ComboSettingIsExplicit(COMBO_SETTING_STONE_MASK_OOT) &&
                      !Combo_ComboSettingIsExplicit(COMBO_SETTING_SHARED_MASK_STONE),
                  "the undo did not put the keys back to unset");

        // Beside the Blast Mask's: OoTMM's page order, the masks added to OoT, then
        // their shared variants.
        StmFreezeWorld(kStmBlastOoT | kStmStoneOoT | kStmSharedBlast | kStmSharedStone);
        bits = Combo_ComboSettingsDivergence();
        ok = Combo_ComboSettingsRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits, names,
                                            sizeof(names), &undo);
        printf("[TEST] (3) restored with the Blast Mask's: %s\n", names);
        STM_CHECK(ok == 1 &&
                      strcmp(names, "Blast Mask (OoT), Stone Mask (OoT), Shared Blast Mask, Shared Stone Mask") == 0,
                  "the restore wrote '%s', not OoTMM's order", names);
        Combo_ComboSettingsRestoreUndo(&undo);
        ComboContext_Init();
        StmClearKeys();
    }

    // ---- (4) OoTMM's actors that lose sight of the wearer ---------------------------
    {
        int firstBad = -1;
        int listed = 0;
        const int mismatches = OoT_StoneMaskTest_IgnoreListMismatches(&firstBad, &listed);
        printf("[TEST] (4) %d actor id(s) lose sight of the wearer, %d answer(s) differ from OoTMM's list\n", listed,
               mismatches);
        STM_CHECK(mismatches == 0,
                  "the ignore list differs from OoTMM's shouldActorIgnorePlayer, first at actor 0x%04X",
                  (unsigned)firstBad);
        STM_CHECK(listed == kStmListedActors, "%d actor id(s) are listed, OoTMM lists %d", listed, kStmListedActors);
    }

    printf("[TEST] stone-mask-oot-rule: %s\n", sStmFailures == 0 ? "PASS" : "FAIL");
    return sStmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// stone-mask-oot-items
// ============================================================================

TestResult Test_StoneMaskOoTItems(void) {
    printf("[TEST] stone-mask-oot-items: the Stone Mask's custom ItemID stays inside OoT's tables and out of SLOT(), "
           "is owned and cycled only under the frozen rule, pairs into one bag row under the shared rule, OoT's logic "
           "credits it only then, and a listed actor's update runs with Link out of sight (#979)\n");
    sStmFailures = 0;
    STM_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table is not available");
    const uint8_t stone = OoT_StoneMaskTest_ItemId();
    const uint8_t blast = OoT_StoneMaskTest_BlastItemId();
    const uint8_t rocs = OoT_BlastMaskTest_RocsFeatherId();

    // ---- (1) THE TABLE-BOUNDS LOCK for the custom ItemIDs ----------------------------
    {
        STM_CHECK(stone == (uint8_t)(blast + 1u),
                  "the Stone Mask is ItemID 0x%02X, not the one after the Blast "
                  "Mask's (0x%02X), OoTMM's order",
                  (unsigned)stone, (unsigned)blast);
        OoT_StoneMask_RegisterRuntime();
        for (unsigned id = rocs; id <= stone; id++) {
            STM_CHECK((int)id >= OoT_BlastMaskTest_ItemSlotsCount(),
                      "custom ItemID 0x%02X is inside OoT_gItemSlots (%d entries): SLOT() would name a real slot", id,
                      OoT_BlastMaskTest_ItemSlotsCount());
            STM_CHECK(id < gItemAgeReqsCount, "custom ItemID 0x%02X is past gItemAgeReqs (%u)", id,
                      (unsigned)gItemAgeReqsCount);
            STM_CHECK(id < gItemActionsCount && OoT_Player_ItemActionTableEntry(id) == 0,
                      "custom ItemID 0x%02X is past sItemActions (%u) or not PLAYER_IA_NONE", id,
                      (unsigned)gItemActionsCount);
            const char* icon = OoT_BlastMaskTest_Icon((uint8_t)id);
            STM_CHECK(id < OoT_BlastMaskTest_ItemIconsCount() && icon != nullptr && icon[0] != '\0',
                      "custom ItemID 0x%02X has no OoT_gItemIcons row (%u entries)", id,
                      (unsigned)OoT_BlastMaskTest_ItemIconsCount());
        }
        const char* icon = OoT_BlastMaskTest_Icon(stone);
        STM_CHECK(icon != nullptr && strcmp(icon, kStmIconPath) == 0 && StmCuratedIntoOoT(icon),
                  "the Stone Mask's icon is '%s', not MM's own icon curated in OoT's half", icon ? icon : "(null)");
        STM_CHECK(StmCuratedIntoOoT("__OTR__item_name_static/gItemNameStoneMaskENGTex") &&
                      StmCuratedIntoOoT("__OTR__objects/object_mask_stone/object_mask_stone_DL_000820"),
                  "the Stone Mask's pause name or its worn list is not curated in OoT's half");
        STM_CHECK(OoT_BlastMaskTest_AgeReq(stone) == OoT_BlastMaskTest_ChildAge(),
                  "the Stone Mask's age row is %d, not the child's %d", OoT_BlastMaskTest_AgeReq(stone),
                  OoT_BlastMaskTest_ChildAge());
        STM_CHECK(OoT_StoneMaskTest_IsAdvancement() == 1, "RG_STONE_MASK is not an advancement item");
    }

    // ---- (2) Ownership, gated by the frozen rule --------------------------------------
    {
        OoT_ForeignMaskTest_ArmLive();
        OoT_StoneMask_RegisterRuntime();
        OoT_StoneMaskTest_SetOwned(1);
        ComboContext_Init();
        STM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_STONE) == 0,
                  "with nothing frozen OoT owns the Stone Mask");
        StmFreezeWorld(0u);
        STM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_STONE) == 0,
                  "a world frozen with the rule off owns the Stone Mask");
        StmFreezeWorld(kStmBlastOoT);
        STM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_STONE) == 0,
                  "the Blast Mask's rule owns the Stone Mask");
        StmFreezeWorld(kStmStoneOoT);
        STM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_STONE) == 1,
                  "a world frozen with the rule on does not own the Stone Mask its flag says it holds");
        STM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_BLAST) == 0,
                  "the Stone Mask's flag and rule own the Blast Mask");
        OoT_StoneMaskTest_SetOwned(0);
        STM_CHECK(Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_STONE) == 0,
                  "the rule alone owns the Stone Mask without its flag");
    }

    // ---- (3) The child trade cycle: only while owned under the rule, after Blast ------
    {
        // The production registration from a clean list: the Blast Mask's first,
        // then the Stone Mask's (other rows clear the extras or register their own).
        OoT_TradeCycleTest_ClearExtras();
        OoT_StoneMask_RegisterRuntime();
        const uint8_t first = OoT_TradeCycleTest_FirstItem();
        const int count = OoT_TradeCycleTest_ChildTradeCount();
        const uint8_t truth = (uint8_t)(first + count - 1);
        const uint16_t truthOnly = (uint16_t)(1u << (count - 1));
        OoT_ForeignMaskTest_ArmLive();
        OoT_StoneMaskTest_SetOwned(1);
        StmFreezeWorld(kStmStoneOoT);
        OoT_TradeCycleTest_Set(truth, truthOnly);
        STM_CHECK(OoT_TradeCycleTest_Next() == stone,
                  "after the Mask of Truth the cycle gave 0x%02X, not the Stone Mask",
                  (unsigned)OoT_TradeCycleTest_Next());
        OoT_TradeCycleTest_Set(stone, truthOnly);
        STM_CHECK(OoT_TradeCycleTest_Next() == truth && OoT_TradeCycleTest_Prev() == truth,
                  "from the Stone Mask: next 0x%02X, prev 0x%02X, expected the Mask of Truth both ways",
                  (unsigned)OoT_TradeCycleTest_Next(), (unsigned)OoT_TradeCycleTest_Prev());
        // Both masks owned under both rules: OoTMM's order, Truth, Blast, Stone.
        OoT_StoneMaskTest_SetBlastOwned(1);
        StmFreezeWorld(kStmBlastOoT | kStmStoneOoT);
        OoT_TradeCycleTest_Set(truth, truthOnly);
        STM_CHECK(OoT_TradeCycleTest_Next() == blast, "after the Mask of Truth came 0x%02X, not the Blast Mask",
                  (unsigned)OoT_TradeCycleTest_Next());
        OoT_TradeCycleTest_Set(blast, truthOnly);
        STM_CHECK(OoT_TradeCycleTest_Next() == stone, "after the Blast Mask came 0x%02X, not the Stone Mask",
                  (unsigned)OoT_TradeCycleTest_Next());
        OoT_TradeCycleTest_Set(truth, truthOnly);
        STM_CHECK(OoT_TradeCycleTest_Prev() == stone,
                  "before the Mask of Truth (wrapping) came 0x%02X, not the Stone "
                  "Mask",
                  (unsigned)OoT_TradeCycleTest_Prev());
        OoT_StoneMaskTest_SetBlastOwned(0);
        StmFreezeWorld(0u);
        OoT_TradeCycleTest_Set(truth, truthOnly);
        STM_CHECK(OoT_TradeCycleTest_Next() == truth, "with the rule off the cycle still visited 0x%02X",
                  (unsigned)OoT_TradeCycleTest_Next());
        OoT_StoneMaskTest_SetOwned(0);
    }

    // ---- (4) The shared member's give: the flag and an empty child trade slot --------
    {
        ComboSharedFamilyMember oot;
        memset(&oot, 0, sizeof(oot));
        const bool registered = Combo_SharedFamilyMemberGet(GAME_OOT, kStmSharedStone, &oot);
        STM_CHECK(registered && (int)oot.nativeId == OoT_StoneMaskTest_PoolId(),
                  "OoT's Stone Mask is not the Shared Stone Mask family's OoT member");
        if (registered && oot.give != nullptr && oot.held != nullptr) {
            OoT_ForeignMaskTest_ArmLive();
            STM_CHECK(!oot.held(oot.nativeId), "a fresh save holds the Stone Mask");
            oot.give(oot.nativeId);
            STM_CHECK(oot.held(oot.nativeId) && OoT_StoneMaskTest_Owned() == 1 &&
                          OoT_BlastMaskTest_TradeSlot() == (int)stone,
                      "the give left owned %d, trade slot 0x%02X", OoT_StoneMaskTest_Owned(),
                      (unsigned)OoT_BlastMaskTest_TradeSlot());
            OoT_ForeignMaskTest_ArmLive();
            OoT_BlastMaskTest_SetTradeSlot(OoT_TradeCycleTest_FirstItem());
            oot.give(oot.nativeId);
            STM_CHECK(OoT_BlastMaskTest_TradeSlot() == (int)OoT_TradeCycleTest_FirstItem(),
                      "the give replaced the child trade slot's own item");
        }
    }

    // ---- (5) THE SHARED PAIR: one bag row, only under the shared rule -----------------
    {
        SharedItem mmStone;
        memset(&mmStone, 0, sizeof(mmStone));
        STM_CHECK(TestNamedItem((uint8_t)GAME_MM, "Stone Mask", &mmStone), "MM has no item named \"Stone Mask\"");
        uint16_t mmId = 0;
        STM_CHECK(Combo_SharedFamilyMemberId(GAME_MM, kStmSharedStone, &mmId) && mmId == mmStone.id,
                  "MM's Stone Mask (%u) is not the Shared Stone Mask family's MM member (%u)", (unsigned)mmStone.id,
                  (unsigned)mmId);
        ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
        StmFreezeWorld(kStmStoneOoT);
        int n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        STM_CHECK(n == 0, "the Stone Mask (OoT) rule alone built %d shared pair(s)", n);
        StmFreezeWorld(kStmStoneOoT | kStmSharedStone);
        n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        printf("[TEST] (5) shared rule on: %d pair(s)%s", n, n > 0 ? ":" : "");
        for (int i = 0; i < n; i++) {
            printf(" (%u, %u)", (unsigned)pairs[i].ootId, (unsigned)pairs[i].mmId);
        }
        printf(" (expected 1: (%d, %u))\n", OoT_StoneMaskTest_PoolId(), (unsigned)mmStone.id);
        STM_CHECK(n == 1 && (int)pairs[0].ootId == OoT_StoneMaskTest_PoolId() && pairs[0].mmId == mmStone.id,
                  "the Shared Stone Mask did not compose exactly one row from OoT's and MM's Stone Masks");
        STM_CHECK((Combo_SharedFamiliesArmed() & kStmSharedStone) == kStmSharedStone,
                  "the shared rule did not arm its family");
    }

    // ---- (6) OoT's logic credits it for a child only under the rule ----------------
    {
        static const char* const kWhich[] = { "CanPassEnemy(Big Skulltula)", "CanPassEnemy(Gerudo guard)",
                                              "CanPassEnemy(break room guard)", "CanUse" };
        StmFreezeWorld(0u);
        for (int w = 0; w < 4; w++) {
            const int got = OoT_StoneMaskTest_Logic(1, 1, w);
            STM_CHECK(got == 0, "rule off: %s answered %d for a child holding the Stone Mask", kWhich[w], got);
        }
        StmFreezeWorld(kStmStoneOoT);
        for (int w = 0; w < 4; w++) {
            const int child = OoT_StoneMaskTest_Logic(1, 1, w);
            const int adult = OoT_StoneMaskTest_Logic(1, 0, w);
            const int none = OoT_StoneMaskTest_Logic(0, 1, w);
            STM_CHECK(child == 1 && adult == 0 && none == 0,
                      "rule on: %s answered child %d, adult %d, child without it %d (expected 1, 0, 0)", kWhich[w],
                      child, adult, none);
        }
        // OoTMM's can_use_mask_stone is in no explosive macro: it never blasts.
        STM_CHECK(OoT_StoneMaskTest_Logic(1, 1, 4) == 0,
                  "rule on: BlastOrSmash credited a child holding only the Stone Mask");
        // OoTMM's get_past_skultullas_no_stone (the MQ Forest antichamber alcove,
        // SoH's basement pot room way in): the same Big Skulltula, no Stone credit.
        STM_CHECK(OoT_StoneMaskTest_Logic(1, 1, 5) == 0,
                  "rule on: CanPassEnemy(Big Skulltula) without the Stone credit passed a child holding only the "
                  "Stone Mask");
    }

    // ---- (7) THE EFFECT's seam: Link out of sight for a listed actor's update -------
    {
        OoT_ForeignMaskTest_ArmLive();
        OoT_StoneMask_Forget();
        StmFreezeWorld(kStmStoneOoT);
        OoT_ForeignMaskTest_SetStored((uint8_t)COMBO_FOREIGN_MASK_MM_STONE);

        // A listed actor (a Skulltula): far away during its update, true after.
        OoT_StoneMaskTest_ArmActor(0, 0);
        STM_CHECK(OoT_StoneMaskTest_Before(0, 7u, 1) == 1 && OoT_StoneMaskTest_Seen(0) == 2,
                  "a Skulltula's update did not run with Link far away (seen %d)", OoT_StoneMaskTest_Seen(0));
        OoT_StoneMaskTest_After(0);
        STM_CHECK(OoT_StoneMaskTest_Seen(0) == 1,
                  "after its update the Skulltula did not get its true fields back "
                  "(seen %d)",
                  OoT_StoneMaskTest_Seen(0));

        // Not listed: a Lizalfos, and the Armos statue; the Armos enemy is.
        OoT_StoneMaskTest_ArmActor(1, 3);
        STM_CHECK(OoT_StoneMaskTest_Before(1, 7u, 1) == 0 && OoT_StoneMaskTest_Seen(1) == 1,
                  "a Lizalfos, which OoTMM does not list, lost sight of the wearer");
        OoT_StoneMaskTest_ArmActor(1, 1);
        STM_CHECK(OoT_StoneMaskTest_Before(1, 7u, 1) == 0 && OoT_StoneMaskTest_Seen(1) == 1,
                  "an Armos statue lost sight of the wearer");
        OoT_StoneMaskTest_ArmActor(1, 2);
        STM_CHECK(OoT_StoneMaskTest_Before(1, 7u, 1) == 1 && OoT_StoneMaskTest_Seen(1) == 2,
                  "an Armos enemy did not lose sight of the wearer");
        OoT_StoneMaskTest_After(1);
        STM_CHECK(OoT_StoneMaskTest_Seen(1) == 1, "the Armos enemy did not get its true fields back");

        // An update that will not run is never touched.
        OoT_StoneMaskTest_ArmActor(0, 0);
        STM_CHECK(OoT_StoneMaskTest_Before(0, 7u, 0) == 0 && OoT_StoneMaskTest_Seen(0) == 1,
                  "a Skulltula whose update will not run had its fields replaced");

        // Another handler kept the update from running (no OnActorUpdate): the next
        // actor's ShouldActorUpdate, in the same pass, puts the fields back.
        OoT_StoneMaskTest_ArmActor(0, 0);
        OoT_StoneMaskTest_ArmActor(1, 3);
        STM_CHECK(OoT_StoneMaskTest_Before(0, 7u, 1) == 1, "the Skulltula was not hidden from");
        OoT_StoneMaskTest_Before(1, 7u, 1);
        STM_CHECK(OoT_StoneMaskTest_Seen(0) == 1,
                  "the next actor's update in the same pass did not put back the "
                  "fields of a Skulltula whose update did not run (seen %d)",
                  OoT_StoneMaskTest_Seen(0));

        // Across frames a held actor is forgotten, never touched (it was recomputed
        // since, or it is gone).
        OoT_StoneMaskTest_ArmActor(0, 0);
        STM_CHECK(OoT_StoneMaskTest_Before(0, 7u, 1) == 1, "the Skulltula was not hidden from");
        OoT_StoneMaskTest_Before(1, 8u, 1);
        STM_CHECK(OoT_StoneMaskTest_Seen(0) == 2,
                  "a later frame's update wrote to an actor held from an earlier frame "
                  "(seen %d)",
                  OoT_StoneMaskTest_Seen(0));
        OoT_StoneMaskTest_After(0);
        STM_CHECK(OoT_StoneMaskTest_Seen(0) == 2, "a forgotten actor's OnActorUpdate still wrote its fields");

        // Not worn under the rule: the rule off, or another mask worn.
        StmFreezeWorld(0u);
        OoT_StoneMaskTest_ArmActor(0, 0);
        STM_CHECK(OoT_StoneMaskTest_Before(0, 9u, 1) == 0 && OoT_StoneMaskTest_Seen(0) == 1,
                  "with the rule off the stored Stone Mask hid Link");
        StmFreezeWorld(kStmBlastOoT | kStmStoneOoT);
        OoT_ForeignMaskTest_SetStored((uint8_t)COMBO_FOREIGN_MASK_MM_BLAST);
        STM_CHECK(OoT_StoneMaskTest_Before(0, 9u, 1) == 0 && OoT_StoneMaskTest_Seen(0) == 1,
                  "the Blast Mask hid Link from a Skulltula");
        OoT_StoneMask_Forget();
    }

    ComboContext_Init();
    OoT_ForeignMaskTest_ArmLive();
    printf("[TEST] stone-mask-oot-items: %s\n", sStmFailures == 0 ? "PASS" : "FAIL");
    return sStmFailures == 0 ? TEST_PASS : TEST_FAIL;
}
