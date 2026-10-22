/**
 * @file test_shared_masks.c
 * @brief Locks for the five shared masks (#978, epic #975 P2; OoTMM
 *        sharedMaskGoron, sharedMaskZora, sharedMaskBunny, sharedMaskKeaton,
 *        sharedMaskTruth), five more families on the #976 mechanism after
 *        #1065's Shared Lens of Truth, #1066's shared songs and #968's shared
 *        arrows, and #969's Bunny Hood speed in Hyrule, which rides the shared
 *        Bunny Hood rule.
 *
 * Four rows, split by what each one needs (test_shared_arrows.c's split):
 *
 *   shared-masks-rule (CTest SharedMasksRule, redship tier) -- THE RULES. Each
 *   mask's bit is the one #976 reserved for it (0x01, 0x02, 0x04, 0x08, 0x10),
 *   allocated and a shared family since P1; each now has its own tier-4 key,
 *   labelled for the Cross-Game Rules page, taking exactly 0 or 1; each key
 *   reaches its own bit of the resolved item rules and nothing else; the
 *   creation freezes them and the fingerprint moves with each (and not with all
 *   five off); a session that walked away from a masks-on file diverges by
 *   name, and the load puts the file's values back into the keys (frozen wins
 *   at load, #781), through both the restore API and a real .redsave Load.
 *
 *   shared-masks-merge (CTest SharedMasksMerge, redship tier) -- THE RUNTIME.
 *   Both games REGISTER their native copy of each mask as that family's member
 *   (the ids the two games place the masks as), and through both games' REAL
 *   harvest and apply shims a mask found in either game is owned in the other
 *   at arrival: in MM its mask slot, in OoT its RAND_INF ownership flag and,
 *   when the child trade slot is empty, that slot; each rule carries only its
 *   own mask; with the rules off nothing is carried.
 *
 *   shared-masks-bag (CTest SharedMasksBag, rando tier) -- THE SHARED ROWS. A
 *   real paired generation with all five keys on: the composer pairs each
 *   mask's two copies into ONE row both solvers credit (OoT's logic through its
 *   RG_* half, MM's Snowhead, Great Bay and other mask logic through its RI_*
 *   half), each is placed exactly once over both games as a shared pair, MM's
 *   own world holds no second copy, and the GOAL is still proven. Then OoT's
 *   logic answers HasItem for each of the five masks from its ownership flag
 *   (Keaton, Bunny, Goron and Zora had no case and fell to assert(false)).
 *
 *   shared-masks-bunny-boost (CTest SharedMasksBunnyBoost, redship tier) --
 *   #969. With the shared Bunny Hood rule FROZEN on, OoT's Bunny Hood behaves
 *   as SoH's "Bunny Hood Effect" enhancement does when on (OoT_BunnyHoodEffect
 *   Mode, the one reader every OoT site asks), whatever that enhancement's own
 *   setting; a stronger setting of the player's own stands; another mask's rule
 *   does not boost; the session key alone does not; and the effect is never
 *   written into the enhancement's CVar.
 *
 * Every key is spelled from the identity prefix rather than as a literal, so
 * the tier-4 classification scan sees only the manifest's spelling.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage.
 */

#include "../combo_logic.h"
#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../context.h"
#include "../cvar_shared_keys.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../test_scratch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#ifdef __cplusplus
extern "C" {
#endif
void OoT_SharedSwordTest_ArmLive(void);
void MM_SharedSwordTest_ArmLive(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
void OoT_SharedMasksTest_SetHeld(int mask, int held);
int OoT_SharedMasksTest_Held(int mask);
int OoT_SharedMasksTest_PoolId(int mask);
int OoT_SharedMasksTest_ItemOf(int mask);
void OoT_SharedMasksTest_SetTradeSlot(int mask);
int OoT_SharedMasksTest_TradeSlot(void);
int OoT_SharedMasksTest_LogicHasMask(int mask, int held);
void MM_SharedMasksTest_SetHeld(int mask, int held);
int MM_SharedMasksTest_Held(int mask);
int MM_SharedMasksTest_PoolId(int mask);
int OoT_BunnyHoodEffectMode(void);
int Rando_HeadlessSeedTest(const char* seedStr);
int MM_Rando_HeadlessPairedHalf(void);
int MM_ComboLogic_TestShuffledItems(uint16_t* outItems, uint16_t* outChecks, int cap);
#ifdef __cplusplus
}
#endif

static int sSmFailures;

#define SM_CHECK(cond, ...)                           \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSmFailures++;                            \
        }                                             \
    } while (0)

namespace {

/** One shared mask: its rule bit, spelled as #976 reserved it (literals, so the
 *  lock states the .redsave format rather than reading it back from the header
 *  under test), its key's suffix, its row label, the rule's name, and a word
 *  both games' names for the item contain. The order is the helpers' `mask`
 *  index (0 Goron, 1 Zora, 2 Bunny, 3 Keaton, 4 Truth), the bits' order. */
struct SmMask {
    uint32_t bit;
    const char* keySuffix;
    const char* label;
    const char* ruleName;
    const char* itemName;
};

const int kSmMaskCount = 5;
const SmMask kSmMasks[kSmMaskCount] = {
    { 0x00000001u, "SharedMaskGoron", "Shared Goron Mask", "sharedGoronMask", "Goron Mask" },
    { 0x00000002u, "SharedMaskZora", "Shared Zora Mask", "sharedZoraMask", "Zora Mask" },
    { 0x00000004u, "SharedMaskBunny", "Shared Bunny Hood", "sharedBunnyHood", "Bunny Hood" },
    { 0x00000008u, "SharedMaskKeaton", "Shared Keaton Mask", "sharedKeatonMask", "Keaton Mask" },
    { 0x00000010u, "SharedMaskTruth", "Shared Mask of Truth", "sharedMaskOfTruth", "Mask of Truth" },
};
const uint32_t kSmAll = 0x0000001Fu;
const int kSmBunny = 2;
/** A bit no build allocates yet: a later build's rule, which no key restores. */
const uint32_t kSmLaterBuildBit = 0x40000000u;
const uint32_t kSmSettingsHash = 0xDEADBEEFu;
const uint32_t kSmProfileDigest = 0x0BADF00Du;
const char* const kSmLoadDir = "rsbs_test_shared_masks_load";

/** SoH's "Bunny Hood Effect" enhancement (CVAR_ENHANCEMENT("MMBunnyHood")) and
 *  its BunnyHoodMode values (soh/Enhancements/enhancementTypes.h). */
const char* const kSmBunnyEffectKey = "gEnhancements.MMBunnyHood";
const int kSmBunnyVanilla = 0;
const int kSmBunnyFast = 1;
const int kSmBunnyFastAndJump = 2;

std::string SmKey(int mask) {
    return std::string(RSBS::kComboIdentityKeyPrefix) + kSmMasks[mask].keySuffix;
}

/** The authoring id whose key is @p mask's rule, or COMBO_SETTING_COUNT. */
ComboSettingId SmFindSetting(int mask) {
    const std::string key = SmKey(mask);
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        if (key == Combo_ComboSettingKey((ComboSettingId)i)) {
            return (ComboSettingId)i;
        }
    }
    return COMBO_SETTING_COUNT;
}

ComboItemRulesRecord SmRules(uint32_t flags) {
    ComboItemRulesRecord r;
    memset(&r, 0, sizeof(r));
    r.crossItemFlags = flags;
    return r;
}

/** Freeze a paired world with the shipped combo record and @p rules, the way
 *  Playthrough_Init stamps one (test_shared_lens.c's SlFreezeWorld). */
void SmFreezeWorld(const ComboItemRulesRecord* rules) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0978F00Du;
    gComboCtx.sharedRandoSettingsHash = kSmSettingsHash;
    gComboCtx.mmProfileDigest = kSmProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx(&rec, rules);
}

void SmSeedShadows(uint8_t fill) {
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, fill);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, (uint8_t)(fill ^ 0xFFu));
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

void SmClearAllKeys() {
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        CVarClear(Combo_ComboSettingKey((ComboSettingId)i));
    }
    for (int m = 0; m < kSmMaskCount; m++) {
        CVarClear(SmKey(m).c_str());
    }
}

/** Arm OoT's live save holding none of the five masks, its child trade slot
 *  empty (the state a new file starts in). */
void SmArmOoT() {
    OoT_SharedSwordTest_ArmLive();
    for (int m = 0; m < kSmMaskCount; m++) {
        OoT_SharedMasksTest_SetHeld(m, 0);
    }
    OoT_SharedMasksTest_SetTradeSlot(-1);
}

/** Arm MM's live save holding none of the five masks. */
void SmArmMM() {
    MM_SharedSwordTest_ArmLive();
    for (int m = 0; m < kSmMaskCount; m++) {
        MM_SharedMasksTest_SetHeld(m, 0);
    }
}

} // namespace

// ============================================================================
// shared-masks-rule: the bits, the keys, the freeze, the fingerprint, the restore
// ============================================================================

TestResult SharedMasksRule_Run(void) {
    printf("[TEST] shared-masks-rule: the Shared Goron, Zora, Keaton and Truth masks and the Shared Bunny Hood are "
           "five Cross-Game Rules, off by default, frozen at creation into the fingerprint, and restored from the "
           "file at load (#978)\n");
    sSmFailures = 0;
    if (!Combo_ComboSettingStoreAvailable()) {
        printf("[TEST] FAIL: no CVar store: every authoring leg below would pass vacuously on the defaults\n");
        return TEST_FAIL;
    }
    ComboContext_Init();
    SmClearAllKeys();

    // ---- (1) The bits: #976's, allocated, families, named, requiring nothing -
    for (int m = 0; m < kSmMaskCount; m++) {
        const uint32_t bit = kSmMasks[m].bit;
        SM_CHECK((RSBS_XITEM_ALL_V1 & bit) == bit,
                 "bit %08X is not allocated (RSBS_XITEM_ALL_V1 %08X): a file with the rule on would be refused as a "
                 "later build's",
                 (unsigned)bit, (unsigned)RSBS_XITEM_ALL_V1);
        SM_CHECK((RSBS_XITEM_FAMILY_MASK & bit) == bit,
                 "bit %08X is not a shared family (RSBS_XITEM_FAMILY_MASK %08X): nothing would merge or pair it",
                 (unsigned)bit, (unsigned)RSBS_XITEM_FAMILY_MASK);
        SM_CHECK(strcmp(Combo_ComboItemRuleName(bit), kSmMasks[m].ruleName) == 0,
                 "bit %08X is named '%s', expected '%s'", (unsigned)bit, Combo_ComboItemRuleName(bit),
                 kSmMasks[m].ruleName);
        SM_CHECK(Combo_ComboItemRuleRequires(bit) == 0u, "%s requires no other rule", kSmMasks[m].ruleName);
        const ComboItemRulesRecord on = SmRules(bit);
        SM_CHECK(Combo_ComboItemRulesValid(&on), "a record with only %s on must be a valid world",
                 kSmMasks[m].ruleName);
    }
    {
        const ComboItemRulesRecord all = SmRules(kSmAll);
        SM_CHECK(Combo_ComboItemRulesValid(&all), "a record with all five shared masks on must be a valid world");
    }

    // ---- (2) The keys: one Cross-Game Rules row each, off by default, 0 or 1 -
    ComboSettingId ids[kSmMaskCount];
    for (int m = 0; m < kSmMaskCount; m++) {
        ids[m] = SmFindSetting(m);
        if (ids[m] == COMBO_SETTING_COUNT) {
            printf("[TEST] FAIL: no combo setting authors '%s': the rule has no Cross-Game Rules row and no player "
                   "can turn it on (%s:%d)\n",
                   SmKey(m).c_str(), __FILE__, __LINE__);
            printf("[TEST] shared-masks-rule: FAIL\n");
            return TEST_FAIL;
        }
        SM_CHECK(strcmp(Combo_ComboSettingLabel(ids[m]), kSmMasks[m].label) == 0,
                 "the rule's row is labelled '%s', expected '%s'", Combo_ComboSettingLabel(ids[m]), kSmMasks[m].label);
        SM_CHECK(Combo_ComboSettingDefault(ids[m]) == 0, "%s must ship OFF (OoTMM's own default)",
                 kSmMasks[m].ruleName);
        SM_CHECK(Combo_ComboSettingValueValid(ids[m], 0) && Combo_ComboSettingValueValid(ids[m], 1) &&
                     !Combo_ComboSettingValueValid(ids[m], 2) && !Combo_ComboSettingValueValid(ids[m], -1),
                 "%s's value space must be exactly 0 or 1", kSmMasks[m].ruleName);
        for (int o = 0; o < m; o++) {
            SM_CHECK(ids[o] != ids[m], "%s and %s must be two rows (OoTMM's five options), not one",
                     kSmMasks[o].ruleName, kSmMasks[m].ruleName);
        }
    }

    // ---- (3) The resolver: each key its own bit, and nothing else ------------
    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    {
        ComboItemRulesRecord r;
        Combo_ResolveComboItemRules(&r);
        SM_CHECK(Combo_ComboItemRulesClear(&r), "with no key set the resolved rules must be every rule off (%08X)",
                 (unsigned)r.crossItemFlags);
        for (int m = 0; m < kSmMaskCount; m++) {
            SM_CHECK(Combo_ComboSettingSet(ids[m], 1) == 1, "%s must be authorable before a world exists",
                     kSmMasks[m].ruleName);
            Combo_ResolveComboItemRules(&r);
            SM_CHECK(r.crossItemFlags == kSmMasks[m].bit && r.blastMaskCooldown == 0u && r.spare[0] == 0u &&
                         r.spare[1] == 0u && r.spare[2] == 0u,
                     "%s's key alone resolves to rules %08X, expected exactly %08X", kSmMasks[m].ruleName,
                     (unsigned)r.crossItemFlags, (unsigned)kSmMasks[m].bit);
            ComboSettingsRecord live;
            Combo_ResolveComboSettings(&live);
            SM_CHECK(memcmp(&live, &defaults, sizeof(live)) == 0,
                     "%s's key moved the combo record itself: it is an item rule, not a combo setting",
                     kSmMasks[m].ruleName);
            SM_CHECK(Combo_ComboSettingSet(ids[m], 0) == 1, "%s must be authorable back off", kSmMasks[m].ruleName);
            Combo_ResolveComboItemRules(&r);
            SM_CHECK(Combo_ComboItemRulesClear(&r), "%s's key explicitly 0 must resolve every rule off (%08X)",
                     kSmMasks[m].ruleName, (unsigned)r.crossItemFlags);
            CVarSetInteger(SmKey(m).c_str(), 2);
            Combo_ResolveComboItemRules(&r);
            SM_CHECK(Combo_ComboItemRulesClear(&r),
                     "an out-of-space stored 2 for %s must resolve to the shipped default (off)", kSmMasks[m].ruleName);
            CVarClear(SmKey(m).c_str());
        }
        bool allSet = true;
        for (int m = 0; m < kSmMaskCount; m++) {
            allSet = (Combo_ComboSettingSet(ids[m], 1) == 1) && allSet;
        }
        SM_CHECK(allSet, "all five rules must be authorable together");
        Combo_ResolveComboItemRules(&r);
        SM_CHECK(r.crossItemFlags == kSmAll, "all five keys on resolve to rules %08X, expected exactly %08X",
                 (unsigned)r.crossItemFlags, (unsigned)kSmAll);
        SmClearAllKeys();
    }

    // ---- (4) The creation: frozen, and the fingerprint moves with each -------
    const uint32_t hashOff = Combo_ComputeComboSettingsHashEx(&defaults, NULL, kSmSettingsHash, kSmProfileDigest);
    SM_CHECK(hashOff == Combo_ComputeComboSettingsHash(&defaults, kSmSettingsHash, kSmProfileDigest),
             "the rules off must leave the fingerprint every world had before they existed");
    uint32_t hashOne[kSmMaskCount] = { 0u, 0u, 0u, 0u, 0u };
    for (int m = 0; m < kSmMaskCount; m++) {
        const ComboItemRulesRecord on = SmRules(kSmMasks[m].bit);
        hashOne[m] = Combo_ComputeComboSettingsHashEx(&defaults, &on, kSmSettingsHash, kSmProfileDigest);
        SM_CHECK(hashOne[m] != hashOff, "%s did not move the fingerprint: a world rule outside its identity",
                 kSmMasks[m].ruleName);
        for (int o = 0; o < m; o++) {
            SM_CHECK(hashOne[o] != hashOne[m], "%s and %s must be two different worlds", kSmMasks[o].ruleName,
                     kSmMasks[m].ruleName);
        }
    }
    {
        ComboContext_Init();
        ComboItemRulesRecord resolved;
        Combo_ResolveComboItemRules(&resolved);
        SmFreezeWorld(&resolved);
        SM_CHECK((gComboCtx.comboItemRules.crossItemFlags & kSmAll) == 0u && gComboCtx.comboSettingsHash == hashOff,
                 "with the keys unset the creation froze rules %08X and fingerprint %08X (expected 0 and %08X)",
                 (unsigned)gComboCtx.comboItemRules.crossItemFlags, (unsigned)gComboCtx.comboSettingsHash,
                 (unsigned)hashOff);
        SM_CHECK((Combo_SharedFamiliesArmed() & kSmAll) == 0u, "no mask family may be armed with the rules off");

        for (int m = 0; m < kSmMaskCount; m++) {
            ComboContext_Init();
            SmClearAllKeys();
            SM_CHECK(Combo_ComboSettingSet(ids[m], 1) == 1, "turn %s on before the creation", kSmMasks[m].ruleName);
            Combo_ResolveComboItemRules(&resolved);
            SmFreezeWorld(&resolved);
            SM_CHECK((gComboCtx.comboItemRules.crossItemFlags & kSmAll) == kSmMasks[m].bit,
                     "the creation froze rules %08X, expected only %s on",
                     (unsigned)gComboCtx.comboItemRules.crossItemFlags, kSmMasks[m].ruleName);
            SM_CHECK(gComboCtx.comboSettingsHash == hashOne[m],
                     "the stamped fingerprint is %08X, expected %08X: the freeze must fold %s",
                     (unsigned)gComboCtx.comboSettingsHash, (unsigned)hashOne[m], kSmMasks[m].ruleName);
            SM_CHECK((Combo_SharedFamiliesArmed() & kSmAll) == kSmMasks[m].bit,
                     "the frozen %s arms families %08X, expected only its own", kSmMasks[m].ruleName,
                     (unsigned)(Combo_SharedFamiliesArmed() & kSmAll));
            SM_CHECK(Combo_ComboSettingsDivergence() == 0u,
                     "a session whose key still says on must compare healthy (diverged %08X)",
                     (unsigned)Combo_ComboSettingsDivergence());
            SM_CHECK(Combo_ComboSettingSet(ids[m], 0) == 0, "%s's writer must refuse while a world is frozen",
                     kSmMasks[m].ruleName);
        }
        SmClearAllKeys();
        ComboContext_Init();
    }

    // ---- (5) Frozen wins at load: the restore API -----------------------------
    {
        const ComboItemRulesRecord allOn = SmRules(kSmAll);
        SmFreezeWorld(&allOn);
        // The session never turned the rules on: every key resolves off.
        const uint32_t bits = Combo_ComboSettingsDivergence();
        SM_CHECK(bits == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                 "a masks-on file met by a masks-off session diverged %08X, expected crossItemFlags alone",
                 (unsigned)bits);
        SM_CHECK(Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits) == 1,
                 "the file-select probe would refuse a masks-on file the keys can restore");
        char names[512] = { 0 };
        ComboSettingsKeyUndo undo;
        const int restored = Combo_ComboSettingsRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits,
                                                            names, sizeof(names), &undo);
        printf("[TEST] (5) restore: %d, restored '%s', diverged after %08X\n", restored, names,
               (unsigned)Combo_ComboSettingsDivergence());
        bool allNamed = true;
        for (int m = 0; m < kSmMaskCount; m++) {
            allNamed = allNamed && strstr(names, kSmMasks[m].label) != NULL;
        }
        SM_CHECK(restored == 1 && allNamed,
                 "the load restore did not put all five of the file's mask rules back (returned %d, '%s')", restored,
                 names);
        for (int m = 0; m < kSmMaskCount; m++) {
            int32_t stored = 0;
            SM_CHECK(Combo_ComboSettingReadStore(ids[m], &stored) && stored == 1,
                     "%s's restored key reads %d, expected 1", kSmMasks[m].ruleName, (int)stored);
        }
        SM_CHECK(Combo_ComboSettingsDivergence() == 0u, "after the restore the session must agree with the file");
        Combo_ComboSettingsRestoreUndo(&undo);
        bool anyExplicit = false;
        for (int m = 0; m < kSmMaskCount; m++) {
            anyExplicit = anyExplicit || Combo_ComboSettingIsExplicit(ids[m]);
        }
        SM_CHECK(!anyExplicit && Combo_ComboSettingsDivergence() == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                 "the undo must put all five keys back to unset");

        // A later build's bit beside the masks is never restored.
        const ComboItemRulesRecord later = SmRules(kSmAll | kSmLaterBuildBit);
        SM_CHECK(Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &later,
                                                   RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS) == 0,
                 "a frozen rule no key authors must not be claimed restorable");
        ComboContext_Init();
    }

    // ---- (6) Frozen wins at load: a real .redsave ------------------------------
    {
        const ComboItemRulesRecord allOn = SmRules(kSmAll);
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        const std::string priorSaveDir = std::filesystem::path(mgr.SlotPath(0)).parent_path().string();
        mgr.SetSaveDirectory(rsbs::test::ScratchDir(kSmLoadDir).string());
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        ComboContext_Init();
        SmSeedShadows(0x78u);
        SmFreezeWorld(&allOn);
        SmSeedShadows(0x78u);
        SM_CHECK(mgr.Save(0), "Save(0) failed for the masks-on file");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        SmClearAllKeys();
        const bool loaded = mgr.Load(0);
        int32_t stored[kSmMaskCount] = { 0, 0, 0, 0, 0 };
        bool keysRestored = true;
        for (int m = 0; m < kSmMaskCount; m++) {
            const bool set = Combo_ComboSettingReadStore(ids[m], &stored[m]);
            keysRestored = keysRestored && set && stored[m] == 1;
        }
        printf("[TEST] (6) masks-on file under a masks-off session: loaded %d, keys %d %d %d %d %d, quarantine %d\n",
               loaded ? 1 : 0, (int)stored[0], (int)stored[1], (int)stored[2], (int)stored[3], (int)stored[4],
               RsbsSave_HasQuarantine(0));
        SM_CHECK(loaded, "a masks-on file was refused under a session that only turned the rules off: frozen wins "
                         "at load (#781), the keys go back to the file's values");
        SM_CHECK(keysRestored, "the load did not restore all five of the file's mask rules into their keys");
        SM_CHECK(!loaded || (gComboCtx.comboItemRules.crossItemFlags & kSmAll) == kSmAll,
                 "the loaded world does not carry all five shared masks (rules %08X)",
                 (unsigned)gComboCtx.comboItemRules.crossItemFlags);
        SM_CHECK(RsbsSave_HasQuarantine(0) == 0 && RsbsSave_HasSave(0) == 1, "a rule divergence is not damage");

        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        mgr.SetSaveDirectory(priorSaveDir);
        ComboContext_Init();
    }

    SmClearAllKeys();
    ComboContext_Init();
    printf("[TEST] shared-masks-rule: %s\n", sSmFailures == 0 ? "PASS" : "FAIL");
    return sSmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-masks-merge: both games' masks are the families, through the real shims
// ============================================================================

TestResult Test_SharedMasksMerge(void) {
    printf("[TEST] shared-masks-merge: both games register their Goron, Zora, Keaton and Truth masks and Bunny Hood "
           "as the shared families' members, and a mask found in either game is owned in the other at arrival "
           "through the real shims (#978)\n");
    sSmFailures = 0;

    // ---- (0) The registrations: each game's own mask, the id it places -------
    for (int m = 0; m < kSmMaskCount; m++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        const bool ootReg = Combo_SharedFamilyMemberId(GAME_OOT, kSmMasks[m].bit, &ootId);
        const bool mmReg = Combo_SharedFamilyMemberId(GAME_MM, kSmMasks[m].bit, &mmId);
        printf("[TEST] (0) %s registered: OoT %d (id %u, pool %d), MM %d (id %u, pool %d)\n", kSmMasks[m].ruleName,
               ootReg ? 1 : 0, (unsigned)ootId, OoT_SharedMasksTest_PoolId(m), mmReg ? 1 : 0, (unsigned)mmId,
               MM_SharedMasksTest_PoolId(m));
        SM_CHECK(ootReg && (int)ootId == OoT_SharedMasksTest_PoolId(m),
                 "OoT's %s is not registered as the %s family's OoT member (the bag pair and the arrival give both "
                 "read this registration)",
                 kSmMasks[m].itemName, kSmMasks[m].ruleName);
        SM_CHECK(mmReg && (int)mmId == MM_SharedMasksTest_PoolId(m),
                 "MM's %s is not registered as the %s family's MM member", kSmMasks[m].itemName, kSmMasks[m].ruleName);
    }

    const ComboItemRulesRecord allOn = SmRules(kSmAll);
    for (int m = 0; m < kSmMaskCount; m++) {
        // ---- (1) Found in Hyrule, given in Termina ---------------------------
        SmFreezeWorld(&allOn);
        SmArmMM();
        SmArmOoT();
        OoT_SharedMasksTest_SetHeld(m, 1);
        OoT_HarvestSharedResources();
        const uint32_t afterOoT = Combo_SharedFamiliesOwned();
        SmArmMM();
        MM_ApplySharedResources();
        int mmOthers = 0;
        for (int o = 0; o < kSmMaskCount; o++) {
            mmOthers += (o != m && MM_SharedMasksTest_Held(o) == 1) ? 1 : 0;
        }
        printf("[TEST] (1) %s OoT -> MM: pool %08X, MM holds it %d, other masks held %d\n", kSmMasks[m].ruleName,
               (unsigned)afterOoT, MM_SharedMasksTest_Held(m), mmOthers);
        SM_CHECK((afterOoT & kSmAll) == kSmMasks[m].bit,
                 "OoT's harvest shim put pool %08X for its %s, expected exactly its own family", (unsigned)afterOoT,
                 kSmMasks[m].itemName);
        SM_CHECK(MM_SharedMasksTest_Held(m) == 1, "MM's apply shim did not give MM the %s", kSmMasks[m].itemName);
        SM_CHECK(mmOthers == 0, "finding the %s in Hyrule gave MM %d other mask(s) too", kSmMasks[m].itemName,
                 mmOthers);
        MM_ApplySharedResources();
        SM_CHECK(MM_SharedMasksTest_Held(m) == 1, "a second MM arrival lost the %s", kSmMasks[m].itemName);

        // ---- (2) Found in Termina, given in Hyrule (empty child trade slot) --
        SmFreezeWorld(&allOn);
        SmArmOoT();
        SmArmMM();
        MM_SharedMasksTest_SetHeld(m, 1);
        MM_HarvestSharedResources();
        SmArmOoT();
        OoT_ApplySharedResources();
        int ootOthers = 0;
        for (int o = 0; o < kSmMaskCount; o++) {
            ootOthers += (o != m && OoT_SharedMasksTest_Held(o) == 1) ? 1 : 0;
        }
        printf("[TEST] (2) %s MM -> OoT: pool %08X, OoT owns it %d, trade slot %d (mask item %d), other masks %d\n",
               kSmMasks[m].ruleName, (unsigned)Combo_SharedFamiliesOwned(), OoT_SharedMasksTest_Held(m),
               OoT_SharedMasksTest_TradeSlot(), OoT_SharedMasksTest_ItemOf(m), ootOthers);
        SM_CHECK((Combo_SharedFamiliesOwned() & kSmAll) == kSmMasks[m].bit,
                 "MM's harvest shim did not put MM's %s alone into the shared pool", kSmMasks[m].itemName);
        SM_CHECK(OoT_SharedMasksTest_Held(m) == 1, "OoT's apply shim did not give OoT the %s (its ownership flag)",
                 kSmMasks[m].itemName);
        SM_CHECK(OoT_SharedMasksTest_TradeSlot() == OoT_SharedMasksTest_ItemOf(m),
                 "OoT's child trade slot holds %d after the %s arrived into an empty slot, expected the mask (%d), as "
                 "OoT's own get-item does",
                 OoT_SharedMasksTest_TradeSlot(), kSmMasks[m].itemName, OoT_SharedMasksTest_ItemOf(m));
        SM_CHECK(ootOthers == 0, "finding the %s in Termina gave OoT %d other mask(s) too", kSmMasks[m].itemName,
                 ootOthers);

        // ---- (2b) An occupied child trade slot is left as the player set it --
        {
            const int other = (m + 1) % kSmMaskCount;
            SmFreezeWorld(&allOn);
            SmArmMM();
            MM_SharedMasksTest_SetHeld(m, 1);
            MM_HarvestSharedResources();
            // OoT owns `other` and its child trade slot holds it, as OoT's own
            // get-item left it.
            SmArmOoT();
            OoT_SharedMasksTest_SetHeld(other, 1);
            OoT_SharedMasksTest_SetTradeSlot(other);
            OoT_ApplySharedResources();
            SM_CHECK(OoT_SharedMasksTest_Held(m) == 1 &&
                         OoT_SharedMasksTest_TradeSlot() == OoT_SharedMasksTest_ItemOf(other),
                     "the %s arriving into an occupied child trade slot left ownership %d and the slot %d, expected "
                     "the mask owned and the slot still holding the %s (%d)",
                     kSmMasks[m].itemName, OoT_SharedMasksTest_Held(m), OoT_SharedMasksTest_TradeSlot(),
                     kSmMasks[other].itemName, OoT_SharedMasksTest_ItemOf(other));
        }
    }

    // ---- (3) Each rule only its own mask ------------------------------------
    for (int m = 0; m < kSmMaskCount; m++) {
        const ComboItemRulesRecord othersOnly = SmRules(kSmAll & ~kSmMasks[m].bit);
        SmFreezeWorld(&othersOnly);
        SmArmMM();
        SmArmOoT();
        OoT_SharedMasksTest_SetHeld(m, 1);
        OoT_HarvestSharedResources();
        SmArmMM();
        MM_ApplySharedResources();
        SM_CHECK((Combo_SharedFamiliesOwned() & kSmMasks[m].bit) == 0u && MM_SharedMasksTest_Held(m) == 0,
                 "with %s off and the other masks' rules on, the %s found in Hyrule reached Termina (pool %08X)",
                 kSmMasks[m].ruleName, kSmMasks[m].itemName, (unsigned)Combo_SharedFamiliesOwned());
    }

    // ---- (4) The rules off: each game keeps its own masks --------------------
    SmFreezeWorld(NULL);
    SmArmMM();
    SmArmOoT();
    for (int m = 0; m < kSmMaskCount; m++) {
        OoT_SharedMasksTest_SetHeld(m, 1);
    }
    OoT_HarvestSharedResources();
    SmArmMM();
    MM_ApplySharedResources();
    int mmHeld = 0;
    for (int m = 0; m < kSmMaskCount; m++) {
        mmHeld += MM_SharedMasksTest_Held(m);
    }
    SM_CHECK(Combo_SharedFamiliesOwned() == 0u && mmHeld == 0,
             "with the rules off %d mask(s) found in Hyrule reached Termina (pool %08X)", mmHeld,
             (unsigned)Combo_SharedFamiliesOwned());
    SmArmMM();
    for (int m = 0; m < kSmMaskCount; m++) {
        MM_SharedMasksTest_SetHeld(m, 1);
    }
    MM_HarvestSharedResources();
    SmArmOoT();
    OoT_ApplySharedResources();
    int ootHeld = 0;
    for (int m = 0; m < kSmMaskCount; m++) {
        ootHeld += OoT_SharedMasksTest_Held(m);
    }
    SM_CHECK(Combo_SharedFamiliesOwned() == 0u && ootHeld == 0 && OoT_SharedMasksTest_TradeSlot() == -1,
             "with the rules off %d mask(s) found in Termina reached Hyrule (pool %08X, trade slot %d)", ootHeld,
             (unsigned)Combo_SharedFamiliesOwned(), OoT_SharedMasksTest_TradeSlot());

    SmArmOoT();
    SmArmMM();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] shared-masks-merge: %s\n", sSmFailures == 0 ? "PASS" : "FAIL");
    return sSmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-masks-bag: one row per mask both solvers credit, over a real generation
// ============================================================================

TestResult SharedMasksBag_Run(void) {
    printf("[TEST] shared-masks-bag: with all five rules on the single bag holds ONE row per shared mask that both "
           "solvers credit, each placed once over both games, the GOAL is still proven, and OoT's logic answers for "
           "each mask from its ownership (#978)\n");
    sSmFailures = 0;
    ComboContext_Init();
    SmClearAllKeys();
    for (int m = 0; m < kSmMaskCount; m++) {
        CVarSetInteger(SmKey(m).c_str(), 1);
    }
    struct Restore {
        ~Restore() {
            for (int m = 0; m < kSmMaskCount; m++) {
                CVarClear(SmKey(m).c_str());
            }
        }
    } restore;

    const int gen = Rando_HeadlessSeedTest("RSBSSHAREDMASKS1");
    SM_CHECK(gen == 0, "the paired OoT generation failed");
    SM_CHECK(Combo_ComboSettingsFrozen() && (gComboCtx.comboItemRules.crossItemFlags & kSmAll) == kSmAll,
             "the creation did not freeze all five shared masks from the keys (rules %08X)",
             (unsigned)gComboCtx.comboItemRules.crossItemFlags);
    const int mmHalf = (gen == 0) ? MM_Rando_HeadlessPairedHalf() : -1;
    SM_CHECK(mmHalf == 0, "MM's creation-time half did not produce a rando world");
    if (gen != 0 || mmHalf != 0) {
        printf("[TEST] shared-masks-bag: FAIL\n");
        return TEST_FAIL;
    }
    const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
    printf("[TEST] shared-masks-bag: bag %d rows, %d shared pair(s), %d placed, goal proven %d\n", bag.bagCount,
           bag.compose.sharedPairs, bag.fill.placed, bag.fill.goalProven ? 1 : 0);
    SM_CHECK(bag.status == RSBS_COMBO_LOGIC_OK && bag.fill.goalProven,
             "the single-bag fill did not prove the GOAL with the shared masks");
    SM_CHECK(bag.compose.sharedPairs == kSmMaskCount,
             "the composer made %d shared pair(s), expected exactly the five masks: the games' copies were not paired",
             bag.compose.sharedPairs);

    const int shuffledTotal = MM_ComboLogic_TestShuffledItems(nullptr, nullptr, 0);
    std::vector<uint16_t> items((size_t)(shuffledTotal > 0 ? shuffledTotal : 1));
    const int shuffled = MM_ComboLogic_TestShuffledItems(items.data(), nullptr, (int)items.size());

    for (int m = 0; m < kSmMaskCount; m++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        SM_CHECK(Combo_SharedFamilyMemberId(GAME_OOT, kSmMasks[m].bit, &ootId) &&
                     Combo_SharedFamilyMemberId(GAME_MM, kSmMasks[m].bit, &mmId),
                 "the %s family is not registered in both games", kSmMasks[m].ruleName);
        SharedItem ootMask;
        memset(&ootMask, 0, sizeof(ootMask));
        ootMask.originGame = (uint8_t)GAME_OOT;
        ootMask.id = ootId;
        SharedItem mmMask;
        memset(&mmMask, 0, sizeof(mmMask));
        mmMask.originGame = (uint8_t)GAME_MM;
        mmMask.id = mmId;
        const char* ootName = Combo_DescribeItemName(ootMask);
        const char* mmName = Combo_DescribeItemName(mmMask);
        SM_CHECK(ootName != NULL && strstr(ootName, kSmMasks[m].itemName) != NULL && mmName != NULL &&
                     strstr(mmName, kSmMasks[m].itemName) != NULL,
                 "the registered pair names '%s' and '%s', expected each game's %s",
                 ootName != NULL ? ootName : "(null)", mmName != NULL ? mmName : "(null)", kSmMasks[m].itemName);

        // Exactly one placement of either copy over both games, and it is the pair.
        int copies = 0;
        int pairedCopies = 0;
        GameId hostOfCopy = GAME_NONE;
        for (GameId host : { GAME_OOT, GAME_MM }) {
            ComboLogicPlacement p;
            for (int i = 0; i < Combo_Logic_PlacementCount(host); i++) {
                if (!Combo_Logic_PlacementAt(host, i, &p)) {
                    continue;
                }
                const bool isOoT = p.item.originGame == (uint8_t)GAME_OOT && p.item.id == ootId;
                const bool isMM = p.item.originGame == (uint8_t)GAME_MM && p.item.id == mmId;
                if (!isOoT && !isMM) {
                    continue;
                }
                copies++;
                hostOfCopy = host;
                const SharedItem farHalf = isOoT ? mmMask : ootMask;
                const bool ownHalf = (host == GAME_OOT) ? isOoT : isMM;
                if (ownHalf && p.pair.originGame == farHalf.originGame && p.pair.id == farHalf.id) {
                    pairedCopies++;
                }
                printf("[TEST] shared-masks-bag: %s on %s check %u (%s half, pair %u:%u)\n", kSmMasks[m].itemName,
                       Game_ToString(host), (unsigned)p.hostCheck, isOoT ? "OoT" : "MM", (unsigned)p.pair.originGame,
                       (unsigned)p.pair.id);
            }
        }
        SM_CHECK(copies == 1 && pairedCopies == 1,
                 "the coordinator placed %d %s, %d of them as the shared pair: expected ONE row, the host's own half "
                 "with the other game's beside it",
                 copies, kSmMasks[m].itemName, pairedCopies);

        // MM's own world: a second copy would be the trimmed half back in play.
        int mmChecks = 0;
        for (int i = 0; i < shuffled && i < (int)items.size(); i++) {
            if (items[(size_t)i] == mmId) {
                mmChecks++;
            }
        }
        const int expectMM = (hostOfCopy == GAME_MM) ? 1 : 0;
        printf("[TEST] shared-masks-bag: MM's world holds %d %s check(s) (expected %d)\n", mmChecks,
               kSmMasks[m].itemName, expectMM);
        SM_CHECK(shuffled > 0 && mmChecks == expectMM,
                 "MM's world holds %d %s check(s), expected %d: the trimmed half is still the mask", mmChecks,
                 kSmMasks[m].itemName, expectMM);

        // OoT's logic reads each mask from its ownership flag (the shared pair's
        // OoT half is credited through it). -1 means no randomizer context.
        const int without = OoT_SharedMasksTest_LogicHasMask(m, 0);
        const int with = OoT_SharedMasksTest_LogicHasMask(m, 1);
        printf("[TEST] shared-masks-bag: OoT logic HasItem(%s): %d without the flag, %d with it\n",
               kSmMasks[m].itemName, without, with);
        SM_CHECK(without == 0 && with == 1,
                 "OoT's logic answers %d/%d for the %s without/with its ownership flag, expected 0/1: HasItem has no "
                 "case for it",
                 without, with, kSmMasks[m].itemName);
    }

    printf("[TEST] shared-masks-bag: %s\n", sSmFailures == 0 ? "PASS" : "FAIL");
    return sSmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-masks-bunny-boost: #969, the Bunny Hood's speed in Hyrule under the rule
// ============================================================================

TestResult SharedMasksBunnyBoost_Run(void) {
    printf("[TEST] shared-masks-bunny-boost: with the shared Bunny Hood rule frozen on, OoT's Bunny Hood behaves as "
           "SoH's Bunny Hood Effect does when on, whatever the enhancement's own setting (#969)\n");
    sSmFailures = 0;
    if (!Combo_ComboSettingStoreAvailable()) {
        printf("[TEST] FAIL: no CVar store: the enhancement legs below would pass vacuously\n");
        return TEST_FAIL;
    }
    const bool priorSet = CVarExists(kSmBunnyEffectKey);
    const int prior = CVarGetInteger(kSmBunnyEffectKey, kSmBunnyVanilla);
    SmClearAllKeys();
    CVarClear(kSmBunnyEffectKey);

    // (1) No world, the enhancement at its default: vanilla.
    ComboContext_Init();
    SM_CHECK(OoT_BunnyHoodEffectMode() == kSmBunnyVanilla,
             "with nothing frozen and the enhancement unset the Bunny Hood is %d, expected vanilla (%d)",
             OoT_BunnyHoodEffectMode(), kSmBunnyVanilla);

    // (2) A world with every rule off: vanilla.
    SmFreezeWorld(NULL);
    SM_CHECK(OoT_BunnyHoodEffectMode() == kSmBunnyVanilla,
             "with every rule off the Bunny Hood is %d, expected vanilla (%d): the rule off changes nothing",
             OoT_BunnyHoodEffectMode(), kSmBunnyVanilla);

    // (3) The shared Bunny Hood frozen on, the enhancement unset: the speed boost.
    {
        const ComboItemRulesRecord bunny = SmRules(kSmMasks[kSmBunny].bit);
        SmFreezeWorld(&bunny);
        const int mode = OoT_BunnyHoodEffectMode();
        printf("[TEST] (3) shared Bunny Hood frozen on, enhancement unset: mode %d (expected %d)\n", mode,
               kSmBunnyFast);
        SM_CHECK(mode == kSmBunnyFast,
                 "with the shared Bunny Hood frozen on OoT's Bunny Hood is %d, expected the speed boost (%d, SoH's "
                 "Bunny Hood Effect \"Faster Run\") whatever the enhancement says",
                 mode, kSmBunnyFast);
        SM_CHECK(!CVarExists(kSmBunnyEffectKey),
                 "the rule wrote the enhancement's CVar: the effect must be read under the rule, never stored into "
                 "the player's config");

        // (4) The player's own setting: an explicit vanilla is still boosted (the
        // rule says "whatever the enhancement's own setting"), and a stronger
        // choice of the player's own stands.
        CVarSetInteger(kSmBunnyEffectKey, kSmBunnyVanilla);
        SM_CHECK(OoT_BunnyHoodEffectMode() == kSmBunnyFast,
                 "an explicit vanilla enhancement under the frozen rule gave %d, expected %d",
                 OoT_BunnyHoodEffectMode(), kSmBunnyFast);
        CVarSetInteger(kSmBunnyEffectKey, kSmBunnyFastAndJump);
        SM_CHECK(OoT_BunnyHoodEffectMode() == kSmBunnyFastAndJump,
                 "the player's own Faster + Longer Jump under the frozen rule gave %d, expected it to stand (%d)",
                 OoT_BunnyHoodEffectMode(), kSmBunnyFastAndJump);
        CVarSetInteger(kSmBunnyEffectKey, kSmBunnyFast);
        SM_CHECK(OoT_BunnyHoodEffectMode() == kSmBunnyFast, "Faster Run under the frozen rule gave %d, expected %d",
                 OoT_BunnyHoodEffectMode(), kSmBunnyFast);
        CVarClear(kSmBunnyEffectKey);
    }

    // (5) Every other mask's rule on, the Bunny Hood's off: vanilla.
    {
        const ComboItemRulesRecord others = SmRules(kSmAll & ~kSmMasks[kSmBunny].bit);
        SmFreezeWorld(&others);
        SM_CHECK(OoT_BunnyHoodEffectMode() == kSmBunnyVanilla,
                 "another mask's rule boosted the Bunny Hood (%d): only the shared Bunny Hood rule carries #969",
                 OoT_BunnyHoodEffectMode());
    }

    // (6) The session key alone (no world frozen with it): vanilla. The FROZEN
    // record decides, never a CVar: after creation the rules are identity.
    {
        ComboContext_Init();
        CVarSetInteger(SmKey(kSmBunny).c_str(), 1);
        SM_CHECK(OoT_BunnyHoodEffectMode() == kSmBunnyVanilla,
                 "the shared Bunny Hood's session key boosted the Bunny Hood with no world frozen (%d)",
                 OoT_BunnyHoodEffectMode());
        CVarClear(SmKey(kSmBunny).c_str());
    }

    if (priorSet) {
        CVarSetInteger(kSmBunnyEffectKey, prior);
    } else {
        CVarClear(kSmBunnyEffectKey);
    }
    SmClearAllKeys();
    ComboContext_Init();
    printf("[TEST] shared-masks-bunny-boost: %s\n", sSmFailures == 0 ? "PASS" : "FAIL");
    return sSmFailures == 0 ? TEST_PASS : TEST_FAIL;
}
