/**
 * @file test_shared_arrows.c
 * @brief Locks for the Shared Fire, Ice and Light Arrows cross-game rules
 *        (#968; OoTMM sharedMagicArrowFire, sharedMagicArrowIce,
 *        sharedMagicArrowLight), three more families on the #976 mechanism
 *        after #1065's Shared Lens of Truth and #1066's shared songs.
 *
 * Three rows, split by what each one needs (test_shared_lens.c's split):
 *
 *   shared-arrows-rule (CTest SharedArrowsRule, redship tier) -- THE RULES.
 *   Each arrow's bit is the one #976 reserved for it (0x00020000, 0x00040000,
 *   0x00080000), allocated and a shared family since P1; each now has its own
 *   tier-4 key, labelled for the Cross-Game Rules page, taking exactly 0 or 1;
 *   each key reaches its own bit of the resolved item rules and nothing else;
 *   the creation freezes them and the fingerprint moves with each (and not with
 *   all three off); a session that walked away from an arrows-on file diverges
 *   by name, and the load puts the file's values back into the keys (frozen
 *   wins at load, #781), through both the restore API and a real .redsave Load.
 *
 *   shared-arrows-merge (CTest SharedArrowsMerge, redship tier) -- THE RUNTIME.
 *   Both games REGISTER their native copy of each arrow as that family's member
 *   (the ids the two games place the arrows as), and through both games' REAL
 *   harvest and apply shims an arrow found in either game is in the other's
 *   inventory at arrival; each rule carries only its own arrow; with the rules
 *   off nothing is carried.
 *
 *   shared-arrows-bag (CTest SharedArrowsBag, rando tier) -- THE SHARED ROWS. A
 *   real paired generation with all three keys on: the composer pairs each
 *   arrow's two copies into ONE row both solvers credit (OoT's logic through
 *   its RG_* half, MM's Snowhead, Great Bay and Stone Tower logic through its
 *   RI_* half), each is placed exactly once over both games as a shared pair,
 *   MM's own world holds no second copy, and the GOAL is still proven.
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
void OoT_SharedArrowsTest_SetHeld(int arrow, int held);
int OoT_SharedArrowsTest_Held(int arrow);
int OoT_SharedArrowsTest_PoolId(int arrow);
void MM_SharedArrowsTest_SetHeld(int arrow, int held);
int MM_SharedArrowsTest_Held(int arrow);
int MM_SharedArrowsTest_PoolId(int arrow);
int Rando_HeadlessSeedTest(const char* seedStr);
int MM_Rando_HeadlessPairedHalf(void);
int MM_ComboLogic_TestShuffledItems(uint16_t* outItems, uint16_t* outChecks, int cap);
#ifdef __cplusplus
}
#endif

static int sSaFailures;

#define SA_CHECK(cond, ...)                           \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSaFailures++;                            \
        }                                             \
    } while (0)

namespace {

/** One shared arrow: its rule bit, spelled as #976 reserved it (literals, so
 *  the lock states the .redsave format rather than reading it back from the
 *  header under test), its key's suffix, its row label, the rule's name, and
 *  each game's display name for its copy. */
struct SaArrow {
    uint32_t bit;
    const char* keySuffix;
    const char* label;
    const char* ruleName;
    const char* itemName;
};

const int kSaArrowCount = 3;
const SaArrow kSaArrows[kSaArrowCount] = {
    { 0x00020000u, "SharedArrowFire", "Shared Fire Arrows", "sharedFireArrows", "Fire Arrows" },
    { 0x00040000u, "SharedArrowIce", "Shared Ice Arrows", "sharedIceArrows", "Ice Arrows" },
    { 0x00080000u, "SharedArrowLight", "Shared Light Arrows", "sharedLightArrows", "Light Arrows" },
};
const uint32_t kSaAll = 0x000E0000u;
/** A bit no build allocates yet: a later build's rule, which no key restores. */
const uint32_t kSaLaterBuildBit = 0x40000000u;
const uint32_t kSaSettingsHash = 0xDEADBEEFu;
const uint32_t kSaProfileDigest = 0x0BADF00Du;
const char* const kSaLoadDir = "rsbs_test_shared_arrows_load";

std::string SaKey(int arrow) {
    return std::string(RSBS::kComboIdentityKeyPrefix) + kSaArrows[arrow].keySuffix;
}

/** The authoring id whose key is @p arrow's rule, or COMBO_SETTING_COUNT. */
ComboSettingId SaFindSetting(int arrow) {
    const std::string key = SaKey(arrow);
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        if (key == Combo_ComboSettingKey((ComboSettingId)i)) {
            return (ComboSettingId)i;
        }
    }
    return COMBO_SETTING_COUNT;
}

ComboItemRulesRecord SaRules(uint32_t flags) {
    ComboItemRulesRecord r;
    memset(&r, 0, sizeof(r));
    r.crossItemFlags = flags;
    return r;
}

/** Freeze a paired world with the shipped combo record and @p rules, the way
 *  Playthrough_Init stamps one (test_shared_lens.c's SlFreezeWorld). */
void SaFreezeWorld(const ComboItemRulesRecord* rules) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0968F00Du;
    gComboCtx.sharedRandoSettingsHash = kSaSettingsHash;
    gComboCtx.mmProfileDigest = kSaProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx(&rec, rules);
}

void SaSeedShadows(uint8_t fill) {
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, fill);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, (uint8_t)(fill ^ 0xFFu));
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

void SaClearAllKeys() {
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        CVarClear(Combo_ComboSettingKey((ComboSettingId)i));
    }
    for (int a = 0; a < kSaArrowCount; a++) {
        CVarClear(SaKey(a).c_str());
    }
}

/** Both games lose all three arrows, then arm their live saves. */
void SaDropAll() {
    OoT_SharedSwordTest_ArmLive();
    MM_SharedSwordTest_ArmLive();
    for (int a = 0; a < kSaArrowCount; a++) {
        OoT_SharedArrowsTest_SetHeld(a, 0);
        MM_SharedArrowsTest_SetHeld(a, 0);
    }
}

} // namespace

// ============================================================================
// shared-arrows-rule: the bits, the keys, the freeze, the fingerprint, the restore
// ============================================================================

TestResult SharedArrowsRule_Run(void) {
    printf("[TEST] shared-arrows-rule: Shared Fire, Ice and Light Arrows are three Cross-Game Rules, off by default, "
           "frozen at creation into the fingerprint, and restored from the file at load (#968)\n");
    sSaFailures = 0;
    if (!Combo_ComboSettingStoreAvailable()) {
        printf("[TEST] FAIL: no CVar store: every authoring leg below would pass vacuously on the defaults\n");
        return TEST_FAIL;
    }
    ComboContext_Init();
    SaClearAllKeys();

    // ---- (1) The bits: #976's, allocated, families, named, requiring nothing -
    for (int a = 0; a < kSaArrowCount; a++) {
        const uint32_t bit = kSaArrows[a].bit;
        SA_CHECK((RSBS_XITEM_ALL_V1 & bit) == bit,
                 "bit %08X is not allocated (RSBS_XITEM_ALL_V1 %08X): a file with the rule on would be refused as a "
                 "later build's",
                 (unsigned)bit, (unsigned)RSBS_XITEM_ALL_V1);
        SA_CHECK((RSBS_XITEM_FAMILY_MASK & bit) == bit,
                 "bit %08X is not a shared family (RSBS_XITEM_FAMILY_MASK %08X): nothing would merge or pair it",
                 (unsigned)bit, (unsigned)RSBS_XITEM_FAMILY_MASK);
        SA_CHECK(strcmp(Combo_ComboItemRuleName(bit), kSaArrows[a].ruleName) == 0,
                 "bit %08X is named '%s', expected '%s'", (unsigned)bit, Combo_ComboItemRuleName(bit),
                 kSaArrows[a].ruleName);
        SA_CHECK(Combo_ComboItemRuleRequires(bit) == 0u, "%s requires no other rule", kSaArrows[a].ruleName);
        const ComboItemRulesRecord on = SaRules(bit);
        SA_CHECK(Combo_ComboItemRulesValid(&on), "a record with only %s on must be a valid world",
                 kSaArrows[a].ruleName);
    }
    {
        const ComboItemRulesRecord all = SaRules(kSaAll);
        SA_CHECK(Combo_ComboItemRulesValid(&all), "a record with all three shared arrows on must be a valid world");
    }

    // ---- (2) The keys: one Cross-Game Rules row each, off by default, 0 or 1 -
    ComboSettingId ids[kSaArrowCount];
    for (int a = 0; a < kSaArrowCount; a++) {
        ids[a] = SaFindSetting(a);
        if (ids[a] == COMBO_SETTING_COUNT) {
            printf("[TEST] FAIL: no combo setting authors '%s': the rule has no Cross-Game Rules row and no player "
                   "can turn it on (%s:%d)\n",
                   SaKey(a).c_str(), __FILE__, __LINE__);
            printf("[TEST] shared-arrows-rule: FAIL\n");
            return TEST_FAIL;
        }
        SA_CHECK(strcmp(Combo_ComboSettingLabel(ids[a]), kSaArrows[a].label) == 0,
                 "the rule's row is labelled '%s', expected '%s'", Combo_ComboSettingLabel(ids[a]), kSaArrows[a].label);
        SA_CHECK(Combo_ComboSettingDefault(ids[a]) == 0, "%s must ship OFF (OoTMM's own default)",
                 kSaArrows[a].ruleName);
        SA_CHECK(Combo_ComboSettingValueValid(ids[a], 0) && Combo_ComboSettingValueValid(ids[a], 1) &&
                     !Combo_ComboSettingValueValid(ids[a], 2) && !Combo_ComboSettingValueValid(ids[a], -1),
                 "%s's value space must be exactly 0 or 1", kSaArrows[a].ruleName);
    }
    SA_CHECK(ids[0] != ids[1] && ids[1] != ids[2] && ids[0] != ids[2],
             "the three arrows must be three rows (OoTMM's three options), not one");

    // ---- (3) The resolver: each key its own bit, and nothing else ------------
    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    {
        ComboItemRulesRecord r;
        Combo_ResolveComboItemRules(&r);
        SA_CHECK(Combo_ComboItemRulesClear(&r), "with no key set the resolved rules must be every rule off (%08X)",
                 (unsigned)r.crossItemFlags);
        for (int a = 0; a < kSaArrowCount; a++) {
            SA_CHECK(Combo_ComboSettingSet(ids[a], 1) == 1, "%s must be authorable before a world exists",
                     kSaArrows[a].ruleName);
            Combo_ResolveComboItemRules(&r);
            SA_CHECK(r.crossItemFlags == kSaArrows[a].bit && r.blastMaskCooldown == 0u && r.spare[0] == 0u &&
                         r.spare[1] == 0u && r.spare[2] == 0u,
                     "%s's key alone resolves to rules %08X, expected exactly %08X", kSaArrows[a].ruleName,
                     (unsigned)r.crossItemFlags, (unsigned)kSaArrows[a].bit);
            ComboSettingsRecord live;
            Combo_ResolveComboSettings(&live);
            SA_CHECK(memcmp(&live, &defaults, sizeof(live)) == 0,
                     "%s's key moved the combo record itself: it is an item rule, not a combo setting",
                     kSaArrows[a].ruleName);
            SA_CHECK(Combo_ComboSettingSet(ids[a], 0) == 1, "%s must be authorable back off", kSaArrows[a].ruleName);
            Combo_ResolveComboItemRules(&r);
            SA_CHECK(Combo_ComboItemRulesClear(&r), "%s's key explicitly 0 must resolve every rule off (%08X)",
                     kSaArrows[a].ruleName, (unsigned)r.crossItemFlags);
            CVarSetInteger(SaKey(a).c_str(), 2);
            Combo_ResolveComboItemRules(&r);
            SA_CHECK(Combo_ComboItemRulesClear(&r),
                     "an out-of-space stored 2 for %s must resolve to the shipped default (off)",
                     kSaArrows[a].ruleName);
            CVarClear(SaKey(a).c_str());
        }
        SA_CHECK(Combo_ComboSettingSet(ids[0], 1) == 1 && Combo_ComboSettingSet(ids[1], 1) == 1 &&
                     Combo_ComboSettingSet(ids[2], 1) == 1,
                 "all three rules must be authorable together");
        Combo_ResolveComboItemRules(&r);
        SA_CHECK(r.crossItemFlags == kSaAll, "all three keys on resolve to rules %08X, expected exactly %08X",
                 (unsigned)r.crossItemFlags, (unsigned)kSaAll);
        SaClearAllKeys();
    }

    // ---- (4) The creation: frozen, and the fingerprint moves with each -------
    const uint32_t hashOff = Combo_ComputeComboSettingsHashEx(&defaults, NULL, kSaSettingsHash, kSaProfileDigest);
    SA_CHECK(hashOff == Combo_ComputeComboSettingsHash(&defaults, kSaSettingsHash, kSaProfileDigest),
             "the rules off must leave the fingerprint every world had before they existed");
    uint32_t hashOne[kSaArrowCount] = { 0u, 0u, 0u };
    for (int a = 0; a < kSaArrowCount; a++) {
        const ComboItemRulesRecord on = SaRules(kSaArrows[a].bit);
        hashOne[a] = Combo_ComputeComboSettingsHashEx(&defaults, &on, kSaSettingsHash, kSaProfileDigest);
        SA_CHECK(hashOne[a] != hashOff, "%s did not move the fingerprint: a world rule outside its identity",
                 kSaArrows[a].ruleName);
    }
    SA_CHECK(hashOne[0] != hashOne[1] && hashOne[1] != hashOne[2] && hashOne[0] != hashOne[2],
             "the three arrows' rules must be three different worlds");
    {
        ComboContext_Init();
        ComboItemRulesRecord resolved;
        Combo_ResolveComboItemRules(&resolved);
        SaFreezeWorld(&resolved);
        SA_CHECK((gComboCtx.comboItemRules.crossItemFlags & kSaAll) == 0u && gComboCtx.comboSettingsHash == hashOff,
                 "with the keys unset the creation froze rules %08X and fingerprint %08X (expected 0 and %08X)",
                 (unsigned)gComboCtx.comboItemRules.crossItemFlags, (unsigned)gComboCtx.comboSettingsHash,
                 (unsigned)hashOff);
        SA_CHECK((Combo_SharedFamiliesArmed() & kSaAll) == 0u, "no arrow family may be armed with the rules off");

        for (int a = 0; a < kSaArrowCount; a++) {
            ComboContext_Init();
            SaClearAllKeys();
            SA_CHECK(Combo_ComboSettingSet(ids[a], 1) == 1, "turn %s on before the creation", kSaArrows[a].ruleName);
            Combo_ResolveComboItemRules(&resolved);
            SaFreezeWorld(&resolved);
            SA_CHECK((gComboCtx.comboItemRules.crossItemFlags & kSaAll) == kSaArrows[a].bit,
                     "the creation froze rules %08X, expected only %s on",
                     (unsigned)gComboCtx.comboItemRules.crossItemFlags, kSaArrows[a].ruleName);
            SA_CHECK(gComboCtx.comboSettingsHash == hashOne[a],
                     "the stamped fingerprint is %08X, expected %08X: the freeze must fold %s",
                     (unsigned)gComboCtx.comboSettingsHash, (unsigned)hashOne[a], kSaArrows[a].ruleName);
            SA_CHECK((Combo_SharedFamiliesArmed() & kSaAll) == kSaArrows[a].bit,
                     "the frozen %s arms families %08X, expected only its own", kSaArrows[a].ruleName,
                     (unsigned)(Combo_SharedFamiliesArmed() & kSaAll));
            SA_CHECK(Combo_ComboSettingsDivergence() == 0u,
                     "a session whose key still says on must compare healthy (diverged %08X)",
                     (unsigned)Combo_ComboSettingsDivergence());
            SA_CHECK(Combo_ComboSettingSet(ids[a], 0) == 0, "%s's writer must refuse while a world is frozen",
                     kSaArrows[a].ruleName);
        }
        SaClearAllKeys();
        ComboContext_Init();
    }

    // ---- (5) Frozen wins at load: the restore API -----------------------------
    {
        const ComboItemRulesRecord allOn = SaRules(kSaAll);
        SaFreezeWorld(&allOn);
        // The session never turned the rules on: every key resolves off.
        const uint32_t bits = Combo_ComboSettingsDivergence();
        SA_CHECK(bits == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                 "an arrows-on file met by an arrows-off session diverged %08X, expected crossItemFlags alone",
                 (unsigned)bits);
        SA_CHECK(Combo_ComboSettingsCanRestoreLiveEx(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits) == 1,
                 "the file-select probe would refuse an arrows-on file the keys can restore");
        char names[256] = { 0 };
        ComboSettingsKeyUndo undo;
        const int restored = Combo_ComboSettingsRestoreLiveEx(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits,
                                                              names, sizeof(names), &undo);
        printf("[TEST] (5) restore: %d, restored '%s', diverged after %08X\n", restored, names,
               (unsigned)Combo_ComboSettingsDivergence());
        SA_CHECK(restored == 1 && strstr(names, kSaArrows[0].label) != NULL &&
                     strstr(names, kSaArrows[1].label) != NULL && strstr(names, kSaArrows[2].label) != NULL,
                 "the load restore did not put all three of the file's arrow rules back (returned %d, '%s')", restored,
                 names);
        for (int a = 0; a < kSaArrowCount; a++) {
            int32_t stored = 0;
            SA_CHECK(Combo_ComboSettingReadStore(ids[a], &stored) && stored == 1,
                     "%s's restored key reads %d, expected 1", kSaArrows[a].ruleName, (int)stored);
        }
        SA_CHECK(Combo_ComboSettingsDivergence() == 0u, "after the restore the session must agree with the file");
        Combo_ComboSettingsRestoreUndo(&undo);
        SA_CHECK(!Combo_ComboSettingIsExplicit(ids[0]) && !Combo_ComboSettingIsExplicit(ids[1]) &&
                     !Combo_ComboSettingIsExplicit(ids[2]) &&
                     Combo_ComboSettingsDivergence() == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                 "the undo must put all three keys back to unset");

        // A later build's bit beside the arrows is never restored.
        const ComboItemRulesRecord later = SaRules(kSaAll | kSaLaterBuildBit);
        SA_CHECK(Combo_ComboSettingsCanRestoreLiveEx(&gComboCtx.comboSettings, &later,
                                                     RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS) == 0,
                 "a frozen rule no key authors must not be claimed restorable");
        ComboContext_Init();
    }

    // ---- (6) Frozen wins at load: a real .redsave ------------------------------
    {
        const ComboItemRulesRecord allOn = SaRules(kSaAll);
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        const std::string priorSaveDir = std::filesystem::path(mgr.SlotPath(0)).parent_path().string();
        mgr.SetSaveDirectory(rsbs::test::ScratchDir(kSaLoadDir).string());
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        ComboContext_Init();
        SaSeedShadows(0x68u);
        SaFreezeWorld(&allOn);
        SaSeedShadows(0x68u);
        SA_CHECK(mgr.Save(0), "Save(0) failed for the arrows-on file");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        SaClearAllKeys();
        const bool loaded = mgr.Load(0);
        int32_t stored[kSaArrowCount] = { 0, 0, 0 };
        bool keySet[kSaArrowCount];
        for (int a = 0; a < kSaArrowCount; a++) {
            keySet[a] = Combo_ComboSettingReadStore(ids[a], &stored[a]);
        }
        printf("[TEST] (6) arrows-on file under an arrows-off session: loaded %d, keys %d/%d, %d/%d, %d/%d, "
               "quarantine %d\n",
               loaded ? 1 : 0, keySet[0] ? 1 : 0, (int)stored[0], keySet[1] ? 1 : 0, (int)stored[1], keySet[2] ? 1 : 0,
               (int)stored[2], RsbsSave_HasQuarantine(0));
        SA_CHECK(loaded, "an arrows-on file was refused under a session that only turned the rules off: frozen wins "
                         "at load (#781), the keys go back to the file's values");
        SA_CHECK(keySet[0] && stored[0] == 1 && keySet[1] && stored[1] == 1 && keySet[2] && stored[2] == 1,
                 "the load did not restore all three of the file's arrow rules into their keys");
        SA_CHECK(!loaded || (gComboCtx.comboItemRules.crossItemFlags & kSaAll) == kSaAll,
                 "the loaded world does not carry all three shared arrows (rules %08X)",
                 (unsigned)gComboCtx.comboItemRules.crossItemFlags);
        SA_CHECK(RsbsSave_HasQuarantine(0) == 0 && RsbsSave_HasSave(0) == 1, "a rule divergence is not damage");

        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        mgr.SetSaveDirectory(priorSaveDir);
        ComboContext_Init();
    }

    SaClearAllKeys();
    ComboContext_Init();
    printf("[TEST] shared-arrows-rule: %s\n", sSaFailures == 0 ? "PASS" : "FAIL");
    return sSaFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-arrows-merge: both games' arrows are the families, through the real shims
// ============================================================================

TestResult Test_SharedArrowsMerge(void) {
    printf("[TEST] shared-arrows-merge: both games register their Fire, Ice and Light Arrows as the shared families' "
           "members, and an arrow found in either game is in the other's inventory at arrival through the real shims "
           "(#968)\n");
    sSaFailures = 0;

    // ---- (0) The registrations: each game's own arrow, the id it places -------
    for (int a = 0; a < kSaArrowCount; a++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        const bool ootReg = Combo_SharedFamilyMemberId(GAME_OOT, kSaArrows[a].bit, &ootId);
        const bool mmReg = Combo_SharedFamilyMemberId(GAME_MM, kSaArrows[a].bit, &mmId);
        printf("[TEST] (0) %s registered: OoT %d (id %u, pool %d), MM %d (id %u, pool %d)\n", kSaArrows[a].ruleName,
               ootReg ? 1 : 0, (unsigned)ootId, OoT_SharedArrowsTest_PoolId(a), mmReg ? 1 : 0, (unsigned)mmId,
               MM_SharedArrowsTest_PoolId(a));
        SA_CHECK(ootReg && (int)ootId == OoT_SharedArrowsTest_PoolId(a),
                 "OoT's %s are not registered as the %s family's OoT member (the bag pair and the arrival give both "
                 "read this registration)",
                 kSaArrows[a].itemName, kSaArrows[a].ruleName);
        SA_CHECK(mmReg && (int)mmId == MM_SharedArrowsTest_PoolId(a),
                 "MM's %s are not registered as the %s family's MM member", kSaArrows[a].itemName,
                 kSaArrows[a].ruleName);
    }

    const ComboItemRulesRecord allOn = SaRules(kSaAll);
    for (int a = 0; a < kSaArrowCount; a++) {
        // ---- (1) Found in Hyrule, given in Termina ---------------------------
        SaFreezeWorld(&allOn);
        SaDropAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedArrowsTest_SetHeld(a, 1);
        OoT_HarvestSharedResources();
        const uint32_t afterOoT = Combo_SharedFamiliesOwned();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        int mmOthers = 0;
        for (int o = 0; o < kSaArrowCount; o++) {
            mmOthers += (o != a && MM_SharedArrowsTest_Held(o) == 1) ? 1 : 0;
        }
        printf("[TEST] (1) %s OoT -> MM: pool %08X, MM holds it %d, other arrows held %d\n", kSaArrows[a].ruleName,
               (unsigned)afterOoT, MM_SharedArrowsTest_Held(a), mmOthers);
        SA_CHECK((afterOoT & kSaAll) == kSaArrows[a].bit,
                 "OoT's harvest shim put pool %08X for its %s, expected exactly their own family", (unsigned)afterOoT,
                 kSaArrows[a].itemName);
        SA_CHECK(MM_SharedArrowsTest_Held(a) == 1, "MM's apply shim did not give MM the %s", kSaArrows[a].itemName);
        SA_CHECK(mmOthers == 0, "finding the %s in Hyrule gave MM %d other arrow(s) too", kSaArrows[a].itemName,
                 mmOthers);
        MM_ApplySharedResources();
        SA_CHECK(MM_SharedArrowsTest_Held(a) == 1, "a second MM arrival lost the %s", kSaArrows[a].itemName);

        // ---- (2) Found in Termina, given in Hyrule ---------------------------
        SaFreezeWorld(&allOn);
        SaDropAll();
        MM_SharedSwordTest_ArmLive();
        MM_SharedArrowsTest_SetHeld(a, 1);
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_ApplySharedResources();
        int ootOthers = 0;
        for (int o = 0; o < kSaArrowCount; o++) {
            ootOthers += (o != a && OoT_SharedArrowsTest_Held(o) == 1) ? 1 : 0;
        }
        printf("[TEST] (2) %s MM -> OoT: pool %08X, OoT holds it %d, other arrows held %d\n", kSaArrows[a].ruleName,
               (unsigned)Combo_SharedFamiliesOwned(), OoT_SharedArrowsTest_Held(a), ootOthers);
        SA_CHECK((Combo_SharedFamiliesOwned() & kSaAll) == kSaArrows[a].bit,
                 "MM's harvest shim did not put MM's %s alone into the shared pool", kSaArrows[a].itemName);
        SA_CHECK(OoT_SharedArrowsTest_Held(a) == 1, "OoT's apply shim did not give OoT the %s", kSaArrows[a].itemName);
        SA_CHECK(ootOthers == 0, "finding the %s in Termina gave OoT %d other arrow(s) too", kSaArrows[a].itemName,
                 ootOthers);

        // ---- (3) Every rule but this arrow's: this arrow is not carried ------
        const ComboItemRulesRecord othersOnly = SaRules(kSaAll & ~kSaArrows[a].bit);
        SaFreezeWorld(&othersOnly);
        SaDropAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedArrowsTest_SetHeld(a, 1);
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        SA_CHECK((Combo_SharedFamiliesOwned() & kSaArrows[a].bit) == 0u && MM_SharedArrowsTest_Held(a) == 0,
                 "with %s off and the other arrows' rules on, the %s found in Hyrule reached Termina (pool %08X)",
                 kSaArrows[a].ruleName, kSaArrows[a].itemName, (unsigned)Combo_SharedFamiliesOwned());
    }

    // ---- (4) The rules off: each game keeps its own arrows -------------------
    SaFreezeWorld(NULL);
    SaDropAll();
    OoT_SharedSwordTest_ArmLive();
    for (int a = 0; a < kSaArrowCount; a++) {
        OoT_SharedArrowsTest_SetHeld(a, 1);
    }
    OoT_HarvestSharedResources();
    MM_SharedSwordTest_ArmLive();
    MM_ApplySharedResources();
    SA_CHECK(Combo_SharedFamiliesOwned() == 0u && MM_SharedArrowsTest_Held(0) == 0 &&
                 MM_SharedArrowsTest_Held(1) == 0 && MM_SharedArrowsTest_Held(2) == 0,
             "with the rules off an arrow found in Hyrule reached Termina (pool %08X)",
             (unsigned)Combo_SharedFamiliesOwned());

    SaDropAll();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] shared-arrows-merge: %s\n", sSaFailures == 0 ? "PASS" : "FAIL");
    return sSaFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-arrows-bag: one row per arrow both solvers credit, over a real generation
// ============================================================================

TestResult SharedArrowsBag_Run(void) {
    printf("[TEST] shared-arrows-bag: with all three rules on the single bag holds ONE Fire, ONE Ice and ONE Light "
           "Arrows row that both solvers credit, each placed once over both games, and the GOAL is still proven "
           "(#968)\n");
    sSaFailures = 0;
    ComboContext_Init();
    SaClearAllKeys();
    for (int a = 0; a < kSaArrowCount; a++) {
        CVarSetInteger(SaKey(a).c_str(), 1);
    }
    struct Restore {
        ~Restore() {
            for (int a = 0; a < kSaArrowCount; a++) {
                CVarClear(SaKey(a).c_str());
            }
        }
    } restore;

    const int gen = Rando_HeadlessSeedTest("RSBSSHAREDARROWS1");
    SA_CHECK(gen == 0, "the paired OoT generation failed");
    SA_CHECK(Combo_ComboSettingsFrozen() && (gComboCtx.comboItemRules.crossItemFlags & kSaAll) == kSaAll,
             "the creation did not freeze all three shared arrows from the keys (rules %08X)",
             (unsigned)gComboCtx.comboItemRules.crossItemFlags);
    const int mmHalf = (gen == 0) ? MM_Rando_HeadlessPairedHalf() : -1;
    SA_CHECK(mmHalf == 0, "MM's creation-time half did not produce a rando world");
    if (gen != 0 || mmHalf != 0) {
        printf("[TEST] shared-arrows-bag: FAIL\n");
        return TEST_FAIL;
    }
    const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
    printf("[TEST] shared-arrows-bag: bag %d rows, %d shared pair(s), %d placed, goal proven %d\n", bag.bagCount,
           bag.compose.sharedPairs, bag.fill.placed, bag.fill.goalProven ? 1 : 0);
    SA_CHECK(bag.status == RSBS_COMBO_LOGIC_OK && bag.fill.goalProven,
             "the single-bag fill did not prove the GOAL with the shared arrows");
    SA_CHECK(bag.compose.sharedPairs == kSaArrowCount,
             "the composer made %d shared pair(s), expected exactly the three arrows: the games' copies were not "
             "paired",
             bag.compose.sharedPairs);

    const int shuffledTotal = MM_ComboLogic_TestShuffledItems(nullptr, nullptr, 0);
    std::vector<uint16_t> items((size_t)(shuffledTotal > 0 ? shuffledTotal : 1));
    const int shuffled = MM_ComboLogic_TestShuffledItems(items.data(), nullptr, (int)items.size());

    for (int a = 0; a < kSaArrowCount; a++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        SA_CHECK(Combo_SharedFamilyMemberId(GAME_OOT, kSaArrows[a].bit, &ootId) &&
                     Combo_SharedFamilyMemberId(GAME_MM, kSaArrows[a].bit, &mmId),
                 "the %s family is not registered in both games", kSaArrows[a].ruleName);
        SharedItem ootArrow;
        memset(&ootArrow, 0, sizeof(ootArrow));
        ootArrow.originGame = (uint8_t)GAME_OOT;
        ootArrow.id = ootId;
        SharedItem mmArrow;
        memset(&mmArrow, 0, sizeof(mmArrow));
        mmArrow.originGame = (uint8_t)GAME_MM;
        mmArrow.id = mmId;
        const char* ootName = Combo_DescribeItemName(ootArrow);
        const char* mmName = Combo_DescribeItemName(mmArrow);
        SA_CHECK(ootName != NULL && strcmp(ootName, kSaArrows[a].itemName) == 0 && mmName != NULL &&
                     strcmp(mmName, kSaArrows[a].itemName) == 0,
                 "the registered pair names '%s' and '%s', expected each game's %s",
                 ootName != NULL ? ootName : "(null)", mmName != NULL ? mmName : "(null)", kSaArrows[a].itemName);

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
                const SharedItem farHalf = isOoT ? mmArrow : ootArrow;
                const bool ownHalf = (host == GAME_OOT) ? isOoT : isMM;
                if (ownHalf && p.pair.originGame == farHalf.originGame && p.pair.id == farHalf.id) {
                    pairedCopies++;
                }
                printf("[TEST] shared-arrows-bag: %s on %s check %u (%s half, pair %u:%u)\n", kSaArrows[a].itemName,
                       Game_ToString(host), (unsigned)p.hostCheck, isOoT ? "OoT" : "MM", (unsigned)p.pair.originGame,
                       (unsigned)p.pair.id);
            }
        }
        SA_CHECK(copies == 1 && pairedCopies == 1,
                 "the coordinator placed %d %s, %d of them as the shared pair: expected ONE row, the host's own half "
                 "with the other game's beside it",
                 copies, kSaArrows[a].itemName, pairedCopies);

        // MM's own world: a second copy would be the trimmed half back in play.
        int mmChecks = 0;
        for (int i = 0; i < shuffled && i < (int)items.size(); i++) {
            if (items[(size_t)i] == mmId) {
                mmChecks++;
            }
        }
        const int expectMM = (hostOfCopy == GAME_MM) ? 1 : 0;
        printf("[TEST] shared-arrows-bag: MM's world holds %d %s check(s) (expected %d)\n", mmChecks,
               kSaArrows[a].itemName, expectMM);
        SA_CHECK(shuffled > 0 && mmChecks == expectMM,
                 "MM's world holds %d %s check(s), expected %d: the trimmed half is still the arrow", mmChecks,
                 kSaArrows[a].itemName, expectMM);
    }

    printf("[TEST] shared-arrows-bag: %s\n", sSaFailures == 0 ? "PASS" : "FAIL");
    return sSaFailures == 0 ? TEST_PASS : TEST_FAIL;
}
