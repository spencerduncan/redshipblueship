/**
 * @file test_shared_lens.c
 * @brief Locks for the Shared Lens of Truth cross-game rule (#1065; OoTMM
 *        sharedLens), the first family on the #976 mechanism that a player can
 *        turn on.
 *
 * Three rows, split by what each one needs:
 *
 *   shared-lens-rule (CTest SharedLensRule, redship tier) -- THE RULE. Its bit
 *   is allocated (0x00200000, the next free RSBS_XITEM_* bit) and is a shared
 *   family; its tier-4 key exists, is labelled for the Cross-Game Rules page and
 *   takes exactly 0 or 1; the key reaches the resolved item rules and nothing
 *   else does; the creation freezes it and the fingerprint moves with it (and
 *   does not move with it off); a session that walked away from a lens-on file
 *   diverges by name, and the load puts the file's value back into the key
 *   (frozen wins at load, #781) instead of refusing the file, through both the
 *   restore API and a real .redsave Load. Needs the CVar store, so the runner
 *   brings up the display-free shared context first.
 *
 *   shared-lens-merge (CTest SharedLensMerge, redship tier) -- THE RUNTIME. Both
 *   games REGISTER their native lens as the family's member (the ids the two
 *   games place their lens as), and through both games' REAL harvest and apply
 *   shims a lens found in either game is in the other's inventory at arrival;
 *   with the rule off it is not.
 *
 *   shared-lens-bag (CTest SharedLensBag, rando tier) -- THE SHARED ROW. A real
 *   paired generation with the key on: the single bag holds ONE lens row that
 *   both solvers credit (the composer paired the two games' lenses and trimmed
 *   one), it is placed exactly once over both games as a shared pair, MM's own
 *   world holds no second lens, and the GOAL is still proven.
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
void OoT_SharedLensTest_SetHeld(int held);
int OoT_SharedLensTest_Held(void);
int OoT_SharedLensTest_PoolLensId(void);
void MM_SharedLensTest_SetHeld(int held);
int MM_SharedLensTest_Held(void);
int MM_SharedLensTest_PoolLensId(void);
int Rando_HeadlessSeedTest(const char* seedStr);
int MM_Rando_HeadlessPairedHalf(void);
int MM_ComboLogic_TestShuffledItems(uint16_t* outItems, uint16_t* outChecks, int cap);
#ifdef __cplusplus
}
#endif

static int sSlFailures;

#define SL_CHECK(cond, ...)                           \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSlFailures++;                            \
        }                                             \
    } while (0)

namespace {

/** The rule's bit, spelled as the issue allocates it (#1065): the next free
 *  RSBS_XITEM_* bit after #976's 0x001FFFFF. A literal, so the lock states the
 *  .redsave format rather than reading it back from the header under test. */
const uint32_t kSlLens = 0x00200000u;
/** A bit no build allocates yet: a later build's rule, which no key restores. */
const uint32_t kSlLaterBuildBit = 0x40000000u;
const uint32_t kSlSettingsHash = 0xDEADBEEFu;
const uint32_t kSlProfileDigest = 0x0BADF00Du;
const char* const kSlLoadDir = "rsbs_test_shared_lens_load";

std::string SlKey() {
    return std::string(RSBS::kComboIdentityKeyPrefix) + "SharedLens";
}

/** The authoring id whose key is the lens rule's, or COMBO_SETTING_COUNT. */
ComboSettingId SlFindSetting() {
    const std::string key = SlKey();
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        if (key == Combo_ComboSettingKey((ComboSettingId)i)) {
            return (ComboSettingId)i;
        }
    }
    return COMBO_SETTING_COUNT;
}

ComboItemRulesRecord SlRules(uint32_t flags) {
    ComboItemRulesRecord r;
    memset(&r, 0, sizeof(r));
    r.crossItemFlags = flags;
    return r;
}

/** Freeze a paired world with the shipped combo record and @p rules, the way
 *  Playthrough_Init stamps one (test_cross_item_rules.c's XirFreezeWorld). */
void SlFreezeWorld(const ComboItemRulesRecord* rules) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1065F00Du;
    gComboCtx.sharedRandoSettingsHash = kSlSettingsHash;
    gComboCtx.mmProfileDigest = kSlProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx(&rec, rules);
}

void SlSeedShadows(uint8_t fill) {
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, fill);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, (uint8_t)(fill ^ 0xFFu));
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

void SlClearAllKeys() {
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        CVarClear(Combo_ComboSettingKey((ComboSettingId)i));
    }
    CVarClear(SlKey().c_str());
}

} // namespace

// ============================================================================
// shared-lens-rule: the bit, the key, the freeze, the fingerprint, the restore
// ============================================================================

TestResult SharedLensRule_Run(void) {
    printf("[TEST] shared-lens-rule: the Shared Lens of Truth is one Cross-Game Rule, off by default, frozen at "
           "creation into the fingerprint, and restored from the file at load (#1065)\n");
    sSlFailures = 0;
    if (!Combo_ComboSettingStoreAvailable()) {
        printf("[TEST] FAIL: no CVar store: every authoring leg below would pass vacuously on the defaults\n");
        return TEST_FAIL;
    }
    ComboContext_Init();
    SlClearAllKeys();

    // ---- (1) The bit: allocated, a family, named, requiring nothing ----------
    SL_CHECK((RSBS_XITEM_ALL_V1 & kSlLens) == kSlLens,
             "bit %08X is not allocated (RSBS_XITEM_ALL_V1 %08X): a file with the rule on would be refused as a "
             "later build's",
             (unsigned)kSlLens, (unsigned)RSBS_XITEM_ALL_V1);
    SL_CHECK((RSBS_XITEM_FAMILY_MASK & kSlLens) == kSlLens,
             "bit %08X is not a shared family (RSBS_XITEM_FAMILY_MASK %08X): nothing would merge or pair it",
             (unsigned)kSlLens, (unsigned)RSBS_XITEM_FAMILY_MASK);
    SL_CHECK(strcmp(Combo_ComboItemRuleName(kSlLens), "sharedLens") == 0,
             "bit %08X is named '%s', expected OoTMM's 'sharedLens'", (unsigned)kSlLens,
             Combo_ComboItemRuleName(kSlLens));
    SL_CHECK(Combo_ComboItemRuleRequires(kSlLens) == 0u, "the shared lens requires no other rule");
    {
        const ComboItemRulesRecord on = SlRules(kSlLens);
        SL_CHECK(Combo_ComboItemRulesValid(&on), "a record with only the shared lens on must be a valid world");
    }

    // ---- (2) The key: one Cross-Game Rules row, off by default, 0 or 1 -------
    const ComboSettingId id = SlFindSetting();
    if (id == COMBO_SETTING_COUNT) {
        printf("[TEST] FAIL: no combo setting authors '%s': the rule has no Cross-Game Rules row and no player can "
               "turn it on (%s:%d)\n",
               SlKey().c_str(), __FILE__, __LINE__);
        printf("[TEST] shared-lens-rule: FAIL\n");
        return TEST_FAIL;
    }
    SL_CHECK(strcmp(Combo_ComboSettingLabel(id), "Shared Lens of Truth") == 0,
             "the rule's row is labelled '%s', expected 'Shared Lens of Truth'", Combo_ComboSettingLabel(id));
    SL_CHECK(Combo_ComboSettingDefault(id) == 0, "the rule must ship OFF (OoTMM's own default)");
    SL_CHECK(Combo_ComboSettingValueValid(id, 0) && Combo_ComboSettingValueValid(id, 1) &&
                 !Combo_ComboSettingValueValid(id, 2) && !Combo_ComboSettingValueValid(id, -1),
             "the rule's value space must be exactly 0 or 1");

    // ---- (3) The resolver: the key and nothing else --------------------------
    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    {
        ComboItemRulesRecord r;
        Combo_ResolveComboItemRules(&r);
        SL_CHECK(Combo_ComboItemRulesClear(&r), "with no key set the resolved rules must be every rule off (%08X)",
                 (unsigned)r.crossItemFlags);
        SL_CHECK(Combo_ComboSettingSet(id, 1) == 1, "the rule must be authorable before a world exists");
        Combo_ResolveComboItemRules(&r);
        SL_CHECK(r.crossItemFlags == kSlLens && r.blastMaskCooldown == 0u && r.spare[0] == 0u && r.spare[1] == 0u &&
                     r.spare[2] == 0u,
                 "the key on resolves to rules %08X (cooldown %u), expected exactly %08X", (unsigned)r.crossItemFlags,
                 (unsigned)r.blastMaskCooldown, (unsigned)kSlLens);
        ComboSettingsRecord live;
        Combo_ResolveComboSettings(&live);
        SL_CHECK(memcmp(&live, &defaults, sizeof(live)) == 0,
                 "the lens key moved the combo record itself: it is an item rule, not a combo setting");
        SL_CHECK(Combo_ComboSettingSet(id, 0) == 1, "the rule must be authorable back off");
        Combo_ResolveComboItemRules(&r);
        SL_CHECK(Combo_ComboItemRulesClear(&r), "the key explicitly 0 must resolve every rule off (%08X)",
                 (unsigned)r.crossItemFlags);
        CVarSetInteger(SlKey().c_str(), 2);
        Combo_ResolveComboItemRules(&r);
        SL_CHECK(Combo_ComboItemRulesClear(&r),
                 "an out-of-space stored 2 must resolve to the shipped default (off), not to 'nonzero is on'");
        CVarClear(SlKey().c_str());
    }

    // ---- (4) The creation: frozen, and the fingerprint moves with it --------
    const ComboItemRulesRecord lensOn = SlRules(kSlLens);
    const uint32_t hashOff = Combo_ComputeComboSettingsHashEx(&defaults, NULL, kSlSettingsHash, kSlProfileDigest);
    const uint32_t hashOn = Combo_ComputeComboSettingsHashEx(&defaults, &lensOn, kSlSettingsHash, kSlProfileDigest);
    SL_CHECK(hashOff == Combo_ComputeComboSettingsHash(&defaults, kSlSettingsHash, kSlProfileDigest),
             "the rule off must leave the fingerprint every world had before it existed");
    SL_CHECK(hashOn != hashOff, "the shared lens did not move the fingerprint: a world rule outside its identity");
    {
        // Off: the key unset, the creation freezes nothing and the hash is the old one.
        ComboContext_Init();
        ComboItemRulesRecord resolved;
        Combo_ResolveComboItemRules(&resolved);
        SlFreezeWorld(&resolved);
        SL_CHECK(!Combo_ComboCrossItemRule(kSlLens) && gComboCtx.comboSettingsHash == hashOff,
                 "with the key unset the creation froze rules %08X and fingerprint %08X (expected 0 and %08X)",
                 (unsigned)gComboCtx.comboItemRules.crossItemFlags, (unsigned)gComboCtx.comboSettingsHash,
                 (unsigned)hashOff);
        SL_CHECK((Combo_SharedFamiliesArmed() & kSlLens) == 0u, "the family must not be armed with the rule off");

        // On: the creation reads the key through the one resolver.
        ComboContext_Init();
        SL_CHECK(Combo_ComboSettingSet(id, 1) == 1, "turn the rule on before the creation");
        Combo_ResolveComboItemRules(&resolved);
        SlFreezeWorld(&resolved);
        SL_CHECK(Combo_ComboCrossItemRule(kSlLens), "the creation did not freeze the shared lens (rules %08X)",
                 (unsigned)gComboCtx.comboItemRules.crossItemFlags);
        SL_CHECK(gComboCtx.comboSettingsHash == hashOn,
                 "the stamped fingerprint is %08X, expected %08X: the freeze must fold the lens",
                 (unsigned)gComboCtx.comboSettingsHash, (unsigned)hashOn);
        SL_CHECK((Combo_SharedFamiliesArmed() & kSlLens) == kSlLens,
                 "the frozen rule does not arm the lens family (armed %08X)", (unsigned)Combo_SharedFamiliesArmed());
        SL_CHECK(Combo_ComboSettingsDivergence() == 0u,
                 "a session whose key still says on must compare healthy (diverged %08X)",
                 (unsigned)Combo_ComboSettingsDivergence());
        SL_CHECK(Combo_ComboSettingSet(id, 0) == 0, "the rule's writer must refuse while a world is frozen");
    }

    // ---- (5) Frozen wins at load: the restore API -----------------------------
    {
        // The session walked away from the file: the key now resolves off.
        CVarClear(SlKey().c_str());
        const uint32_t bits = Combo_ComboSettingsDivergence();
        SL_CHECK(bits == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                 "a lens-on file met by a lens-off session diverged %08X, expected crossItemFlags alone",
                 (unsigned)bits);
        SL_CHECK((Combo_ComboSettingsRestorableMask() & RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS) != 0u,
                 "a key authors crossItemFlags now, so the restorable mask must name it");
        SL_CHECK(Combo_ComboSettingsCanRestoreLiveEx(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits) == 1,
                 "the file-select probe would refuse a lens-on file the key can restore");
        char names[192] = { 0 };
        ComboSettingsKeyUndo undo;
        const int restored = Combo_ComboSettingsRestoreLiveEx(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits,
                                                              names, sizeof(names), &undo);
        printf("[TEST] (5) restore: %d, restored '%s', diverged after %08X\n", restored, names,
               (unsigned)Combo_ComboSettingsDivergence());
        SL_CHECK(restored == 1 && strstr(names, "Shared Lens of Truth") != NULL,
                 "the load restore did not put the file's lens rule back (returned %d, '%s')", restored, names);
        int32_t stored = 0;
        SL_CHECK(Combo_ComboSettingReadStore(id, &stored) && stored == 1, "the restored key reads %d, expected 1",
                 (int)stored);
        SL_CHECK(Combo_ComboSettingsDivergence() == 0u, "after the restore the session must agree with the file");
        Combo_ComboSettingsRestoreUndo(&undo);
        SL_CHECK(!Combo_ComboSettingIsExplicit(id) &&
                     Combo_ComboSettingsDivergence() == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                 "the undo must put the key back to unset");

        // A rule no key authors (a later build's bit) is never restored, and
        // neither is the Blast Mask cooldown.
        const ComboItemRulesRecord later = SlRules(kSlLens | kSlLaterBuildBit);
        SL_CHECK(Combo_ComboSettingsCanRestoreLiveEx(&gComboCtx.comboSettings, &later,
                                                     RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS) == 0,
                 "a frozen rule no key authors must not be claimed restorable");
        ComboItemRulesRecord cooldown = SlRules(kSlLens);
        cooldown.blastMaskCooldown = (uint8_t)RSBS_BLAST_COOLDOWN_SHORT;
        SL_CHECK(Combo_ComboSettingsCanRestoreLiveEx(&gComboCtx.comboSettings, &cooldown,
                                                     RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS |
                                                         RSBS_COMBO_DIVERGE_BLAST_MASK_COOLDOWN) == 0,
                 "the Blast Mask cooldown has no key, so it must not be claimed restorable");
        ComboContext_Init();
    }

    // ---- (6) Frozen wins at load: a real .redsave ------------------------------
    {
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        const std::string priorSaveDir = std::filesystem::path(mgr.SlotPath(0)).parent_path().string();
        mgr.SetSaveDirectory(rsbs::test::ScratchDir(kSlLoadDir).string());
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        ComboContext_Init();
        SlSeedShadows(0x65u);
        SlFreezeWorld(&lensOn);
        SlSeedShadows(0x65u);
        SL_CHECK(mgr.Save(0), "Save(0) failed for the lens-on file");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        CVarClear(SlKey().c_str());
        const bool loaded = mgr.Load(0);
        int32_t stored = 0;
        const bool keySet = Combo_ComboSettingReadStore(id, &stored);
        printf("[TEST] (6) lens-on file under a lens-off session: loaded %d, key %s %d, quarantine %d\n",
               loaded ? 1 : 0, keySet ? "set" : "unset", (int)stored, RsbsSave_HasQuarantine(0));
        SL_CHECK(loaded, "a lens-on file was refused under a session that only turned the rule off: frozen wins at "
                         "load (#781), the key goes back to the file's value");
        SL_CHECK(keySet && stored == 1, "the load did not restore the file's lens rule into the key");
        SL_CHECK(!loaded || Combo_ComboCrossItemRule(kSlLens), "the loaded world does not carry the shared lens");
        SL_CHECK(RsbsSave_HasQuarantine(0) == 0 && RsbsSave_HasSave(0) == 1, "a rule divergence is not damage");

        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        mgr.SetSaveDirectory(priorSaveDir);
        ComboContext_Init();
    }

    SlClearAllKeys();
    ComboContext_Init();
    printf("[TEST] shared-lens-rule: %s\n", sSlFailures == 0 ? "PASS" : "FAIL");
    return sSlFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-lens-merge: both games' lens is the family, through the real shims
// ============================================================================

TestResult Test_SharedLensMerge(void) {
    printf("[TEST] shared-lens-merge: both games register their Lens of Truth as the shared family's member, and a "
           "lens found in either game is in the other's inventory at arrival through the real shims (#1065)\n");
    sSlFailures = 0;

    // ---- (0) The registration: each game's own lens, the ids it places --------
    uint16_t ootId = 0;
    uint16_t mmId = 0;
    const bool ootReg = Combo_SharedFamilyMemberId(GAME_OOT, kSlLens, &ootId);
    const bool mmReg = Combo_SharedFamilyMemberId(GAME_MM, kSlLens, &mmId);
    printf("[TEST] (0) registered: OoT %d (id %u, pool lens %d), MM %d (id %u, pool lens %d)\n", ootReg ? 1 : 0,
           (unsigned)ootId, OoT_SharedLensTest_PoolLensId(), mmReg ? 1 : 0, (unsigned)mmId,
           MM_SharedLensTest_PoolLensId());
    SL_CHECK(ootReg && (int)ootId == OoT_SharedLensTest_PoolLensId(),
             "OoT's Lens of Truth is not registered as the shared lens family's OoT member (the bag pair and the "
             "arrival give both read this registration)");
    SL_CHECK(mmReg && (int)mmId == MM_SharedLensTest_PoolLensId(),
             "MM's Lens of Truth is not registered as the shared lens family's MM member");

    const ComboItemRulesRecord lensOn = SlRules(kSlLens);

    // ---- (1) Found in Hyrule, given in Termina ------------------------------
    SlFreezeWorld(&lensOn);
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedLensTest_SetHeld(1);
    OoT_HarvestSharedResources();
    const uint32_t afterOoT = Combo_SharedFamiliesOwned();
    MM_SharedSwordTest_ArmLive();
    MM_SharedLensTest_SetHeld(0);
    MM_ApplySharedResources();
    printf("[TEST] (1) OoT -> MM: pool %08X, MM holds the lens %d (expected %08X, 1)\n", (unsigned)afterOoT,
           MM_SharedLensTest_Held(), (unsigned)kSlLens);
    SL_CHECK((afterOoT & kSlLens) == kSlLens, "OoT's harvest shim did not put OoT's lens into the shared pool");
    SL_CHECK(MM_SharedLensTest_Held() == 1, "MM's apply shim did not give MM its Lens of Truth");
    MM_ApplySharedResources();
    SL_CHECK(MM_SharedLensTest_Held() == 1, "a second MM arrival lost the lens");

    // ---- (2) Found in Termina, given in Hyrule ------------------------------
    SlFreezeWorld(&lensOn);
    MM_SharedSwordTest_ArmLive();
    MM_SharedLensTest_SetHeld(1);
    MM_HarvestSharedResources();
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedLensTest_SetHeld(0);
    OoT_ApplySharedResources();
    printf("[TEST] (2) MM -> OoT: pool %08X, OoT holds the lens %d (expected %08X, 1)\n",
           (unsigned)Combo_SharedFamiliesOwned(), OoT_SharedLensTest_Held(), (unsigned)kSlLens);
    SL_CHECK((Combo_SharedFamiliesOwned() & kSlLens) == kSlLens,
             "MM's harvest shim did not put MM's lens into the shared pool");
    SL_CHECK(OoT_SharedLensTest_Held() == 1, "OoT's apply shim did not give OoT its Lens of Truth");

    // ---- (3) The rule off: each game keeps its own lens ----------------------
    SlFreezeWorld(NULL);
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedLensTest_SetHeld(1);
    OoT_HarvestSharedResources();
    MM_SharedSwordTest_ArmLive();
    MM_SharedLensTest_SetHeld(0);
    MM_ApplySharedResources();
    SL_CHECK(Combo_SharedFamiliesOwned() == 0u && MM_SharedLensTest_Held() == 0,
             "with the rule off a lens found in Hyrule reached Termina (pool %08X)",
             (unsigned)Combo_SharedFamiliesOwned());

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] shared-lens-merge: %s\n", sSlFailures == 0 ? "PASS" : "FAIL");
    return sSlFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-lens-bag: one lens row both solvers credit, over a real generation
// ============================================================================

TestResult SharedLensBag_Run(void) {
    printf("[TEST] shared-lens-bag: with the rule on the single bag holds ONE Lens of Truth that both solvers "
           "credit, placed once over both games, and the GOAL is still proven (#1065)\n");
    sSlFailures = 0;
    ComboContext_Init();
    SlClearAllKeys();
    CVarSetInteger(SlKey().c_str(), 1);

    const int gen = Rando_HeadlessSeedTest("RSBSSHAREDLENS1");
    SL_CHECK(gen == 0, "the paired OoT generation failed");
    SL_CHECK(Combo_ComboSettingsFrozen() && Combo_ComboCrossItemRule(kSlLens),
             "the creation did not freeze the shared lens from the key (rules %08X)",
             (unsigned)gComboCtx.comboItemRules.crossItemFlags);
    const int mmHalf = (gen == 0) ? MM_Rando_HeadlessPairedHalf() : -1;
    SL_CHECK(mmHalf == 0, "MM's creation-time half did not produce a rando world");
    if (gen != 0 || mmHalf != 0) {
        CVarClear(SlKey().c_str());
        printf("[TEST] shared-lens-bag: FAIL\n");
        return TEST_FAIL;
    }
    const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
    printf("[TEST] shared-lens-bag: bag %d rows, %d shared pair(s), %d placed, goal proven %d\n", bag.bagCount,
           bag.compose.sharedPairs, bag.fill.placed, bag.fill.goalProven ? 1 : 0);
    SL_CHECK(bag.status == RSBS_COMBO_LOGIC_OK && bag.fill.goalProven,
             "the single-bag fill did not prove the GOAL with the shared lens");
    SL_CHECK(bag.compose.sharedPairs == 1,
             "the composer made %d shared pair(s), expected exactly the lens: the two games' lenses were not "
             "paired into one row",
             bag.compose.sharedPairs);

    uint16_t ootId = 0;
    uint16_t mmId = 0;
    SL_CHECK(Combo_SharedFamilyMemberId(GAME_OOT, kSlLens, &ootId) &&
                 Combo_SharedFamilyMemberId(GAME_MM, kSlLens, &mmId),
             "the lens family is not registered in both games");
    SharedItem ootLens;
    memset(&ootLens, 0, sizeof(ootLens));
    ootLens.originGame = (uint8_t)GAME_OOT;
    ootLens.id = ootId;
    SharedItem mmLens;
    memset(&mmLens, 0, sizeof(mmLens));
    mmLens.originGame = (uint8_t)GAME_MM;
    mmLens.id = mmId;
    const char* ootName = Combo_DescribeItemName(ootLens);
    const char* mmName = Combo_DescribeItemName(mmLens);
    SL_CHECK(ootName != NULL && strcmp(ootName, "Lens of Truth") == 0 && mmName != NULL &&
                 strcmp(mmName, "Lens of Truth") == 0,
             "the registered pair names '%s' and '%s', expected each game's Lens of Truth",
             ootName != NULL ? ootName : "(null)", mmName != NULL ? mmName : "(null)");

    // Exactly one placement of either lens over both games, and it is the pair.
    int copies = 0;
    int pairedCopies = 0;
    GameId hostOfCopy = GAME_NONE;
    for (GameId host : { GAME_OOT, GAME_MM }) {
        ComboLogicPlacement p;
        for (int i = 0; i < Combo_Logic_PlacementCount(host); i++) {
            if (!Combo_Logic_PlacementAt(host, i, &p)) {
                continue;
            }
            const bool isOoTLens = p.item.originGame == (uint8_t)GAME_OOT && p.item.id == ootId;
            const bool isMMLens = p.item.originGame == (uint8_t)GAME_MM && p.item.id == mmId;
            if (!isOoTLens && !isMMLens) {
                continue;
            }
            copies++;
            hostOfCopy = host;
            const SharedItem farHalf = isOoTLens ? mmLens : ootLens;
            const bool ownHalf = (host == GAME_OOT) ? isOoTLens : isMMLens;
            if (ownHalf && p.pair.originGame == farHalf.originGame && p.pair.id == farHalf.id) {
                pairedCopies++;
            }
            printf("[TEST] shared-lens-bag: lens on %s check %u (%s half, pair %u:%u)\n", Game_ToString(host),
                   (unsigned)p.hostCheck, isOoTLens ? "OoT" : "MM", (unsigned)p.pair.originGame, (unsigned)p.pair.id);
        }
    }
    SL_CHECK(copies == 1 && pairedCopies == 1,
             "the coordinator placed %d lens(es), %d of them as the shared pair: expected ONE row, the host's own "
             "half with the other game's beside it",
             copies, pairedCopies);

    // MM's own world: a second lens would be the trimmed half back in play.
    const int shuffledTotal = MM_ComboLogic_TestShuffledItems(nullptr, nullptr, 0);
    std::vector<uint16_t> items((size_t)(shuffledTotal > 0 ? shuffledTotal : 1));
    const int shuffled = MM_ComboLogic_TestShuffledItems(items.data(), nullptr, (int)items.size());
    int mmLensChecks = 0;
    for (int i = 0; i < shuffled && i < (int)items.size(); i++) {
        if (items[(size_t)i] == mmId) {
            mmLensChecks++;
        }
    }
    const int expectMM = (hostOfCopy == GAME_MM) ? 1 : 0;
    printf("[TEST] shared-lens-bag: MM's world holds %d Lens of Truth check(s) (expected %d)\n", mmLensChecks,
           expectMM);
    SL_CHECK(shuffled > 0 && mmLensChecks == expectMM,
             "MM's world holds %d Lens of Truth check(s), expected %d: the trimmed half is still a lens", mmLensChecks,
             expectMM);

    CVarClear(SlKey().c_str());
    printf("[TEST] shared-lens-bag: %s\n", sSlFailures == 0 ? "PASS" : "FAIL");
    return sSlFailures == 0 ? TEST_PASS : TEST_FAIL;
}
