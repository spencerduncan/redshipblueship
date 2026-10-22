/**
 * @file test_shared_songs.c
 * @brief Locks for the Shared Epona's Song and Shared Song of Storms cross-game
 *        rules (#1066; OoTMM sharedSongEpona, sharedSongStorms), two more
 *        families on the #976 mechanism after #1065's Shared Lens of Truth.
 *
 * Three rows, split by what each one needs (test_shared_lens.c's split):
 *
 *   shared-songs-rule (CTest SharedSongsRule, redship tier) -- THE RULES. Each
 *   song's bit is allocated (0x00400000 and 0x00800000, the next free
 *   RSBS_XITEM_* bits after the lens) and is a shared family; each has its own
 *   tier-4 key, labelled for the Cross-Game Rules page, taking exactly 0 or 1;
 *   each key reaches its own bit of the resolved item rules and nothing else;
 *   the creation freezes them and the fingerprint moves with each (and not with
 *   both off); a session that walked away from a songs-on file diverges by name,
 *   and the load puts the file's values back into the keys (frozen wins at
 *   load, #781), through both the restore API and a real .redsave Load.
 *
 *   shared-songs-merge (CTest SharedSongsMerge, redship tier) -- THE RUNTIME.
 *   Both games REGISTER their native copy of each song as that family's member
 *   (the ids the two games place the songs as), and through both games' REAL
 *   harvest and apply shims a song learnt in either game is known, and so
 *   playable on the ocarina, in the other at arrival; each rule carries only
 *   its own song; with the rules off nothing is carried.
 *
 *   shared-songs-bag (CTest SharedSongsBag, rando tier) -- THE SHARED ROWS. A
 *   real paired generation with both keys on and OoT's songs shuffled anywhere
 *   (so both games' copies are general-pass bag rows): the composer pairs each
 *   song's two copies into ONE row both solvers credit, each is placed exactly
 *   once over both games as a shared pair, MM's own world holds no second copy,
 *   and the GOAL is still proven.
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
void OoT_SharedSongsTest_SetHeld(int song, int held);
int OoT_SharedSongsTest_Held(int song);
int OoT_SharedSongsTest_PoolId(int song);
void MM_SharedSongsTest_SetHeld(int song, int held);
int MM_SharedSongsTest_Held(int song);
int MM_SharedSongsTest_PoolId(int song);
int Rando_HeadlessSeedTest(const char* seedStr);
int MM_Rando_HeadlessPairedHalf(void);
int MM_ComboLogic_TestShuffledItems(uint16_t* outItems, uint16_t* outChecks, int cap);
#ifdef __cplusplus
}
#endif

static int sSsFailures;

#define SS_CHECK(cond, ...)                           \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSsFailures++;                            \
        }                                             \
    } while (0)

namespace {

/** One shared song: its rule bit, spelled as the issue allocates it (#1066,
 *  literals so the lock states the .redsave format rather than reading it back
 *  from the header under test), its key's suffix, its row label, OoTMM's name
 *  for the rule, and each game's display name for its copy. */
struct SsSong {
    uint32_t bit;
    const char* keySuffix;
    const char* label;
    const char* ruleName;
    const char* itemName;
};

const SsSong kSsSongs[2] = {
    { 0x00400000u, "SharedSongEpona", "Shared Epona's Song", "sharedSongEpona", "Epona's Song" },
    { 0x00800000u, "SharedSongStorms", "Shared Song of Storms", "sharedSongStorms", "Song of Storms" },
};
const uint32_t kSsBoth = 0x00C00000u;
/** A bit no build allocates yet: a later build's rule, which no key restores. */
const uint32_t kSsLaterBuildBit = 0x40000000u;
const uint32_t kSsSettingsHash = 0xDEADBEEFu;
const uint32_t kSsProfileDigest = 0x0BADF00Du;
const char* const kSsLoadDir = "rsbs_test_shared_songs_load";

std::string SsKey(int song) {
    return std::string(RSBS::kComboIdentityKeyPrefix) + kSsSongs[song].keySuffix;
}

/** The authoring id whose key is @p song's rule, or COMBO_SETTING_COUNT. */
ComboSettingId SsFindSetting(int song) {
    const std::string key = SsKey(song);
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        if (key == Combo_ComboSettingKey((ComboSettingId)i)) {
            return (ComboSettingId)i;
        }
    }
    return COMBO_SETTING_COUNT;
}

ComboItemRulesRecord SsRules(uint32_t flags) {
    ComboItemRulesRecord r;
    memset(&r, 0, sizeof(r));
    r.crossItemFlags = flags;
    return r;
}

/** Freeze a paired world with the shipped combo record and @p rules, the way
 *  Playthrough_Init stamps one (test_shared_lens.c's SlFreezeWorld). */
void SsFreezeWorld(const ComboItemRulesRecord* rules) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1066F00Du;
    gComboCtx.sharedRandoSettingsHash = kSsSettingsHash;
    gComboCtx.mmProfileDigest = kSsProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx(&rec, rules);
}

void SsSeedShadows(uint8_t fill) {
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, fill);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, (uint8_t)(fill ^ 0xFFu));
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

void SsClearAllKeys() {
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        CVarClear(Combo_ComboSettingKey((ComboSettingId)i));
    }
    for (int s = 0; s < 2; s++) {
        CVarClear(SsKey(s).c_str());
    }
}

/** Both games forget both songs, then arm their live saves. */
void SsForgetAll() {
    OoT_SharedSwordTest_ArmLive();
    MM_SharedSwordTest_ArmLive();
    for (int s = 0; s < 2; s++) {
        OoT_SharedSongsTest_SetHeld(s, 0);
        MM_SharedSongsTest_SetHeld(s, 0);
    }
}

} // namespace

// ============================================================================
// shared-songs-rule: the bits, the keys, the freeze, the fingerprint, the restore
// ============================================================================

TestResult SharedSongsRule_Run(void) {
    printf("[TEST] shared-songs-rule: Shared Epona's Song and Shared Song of Storms are two Cross-Game Rules, off by "
           "default, frozen at creation into the fingerprint, and restored from the file at load (#1066)\n");
    sSsFailures = 0;
    if (!Combo_ComboSettingStoreAvailable()) {
        printf("[TEST] FAIL: no CVar store: every authoring leg below would pass vacuously on the defaults\n");
        return TEST_FAIL;
    }
    ComboContext_Init();
    SsClearAllKeys();

    // ---- (1) The bits: allocated, families, named, requiring nothing ---------
    for (int s = 0; s < 2; s++) {
        const uint32_t bit = kSsSongs[s].bit;
        SS_CHECK((RSBS_XITEM_ALL_V1 & bit) == bit,
                 "bit %08X is not allocated (RSBS_XITEM_ALL_V1 %08X): a file with the rule on would be refused as a "
                 "later build's",
                 (unsigned)bit, (unsigned)RSBS_XITEM_ALL_V1);
        SS_CHECK((RSBS_XITEM_FAMILY_MASK & bit) == bit,
                 "bit %08X is not a shared family (RSBS_XITEM_FAMILY_MASK %08X): nothing would merge or pair it",
                 (unsigned)bit, (unsigned)RSBS_XITEM_FAMILY_MASK);
        SS_CHECK(strcmp(Combo_ComboItemRuleName(bit), kSsSongs[s].ruleName) == 0,
                 "bit %08X is named '%s', expected OoTMM's '%s'", (unsigned)bit, Combo_ComboItemRuleName(bit),
                 kSsSongs[s].ruleName);
        SS_CHECK(Combo_ComboItemRuleRequires(bit) == 0u, "%s requires no other rule", kSsSongs[s].ruleName);
        const ComboItemRulesRecord on = SsRules(bit);
        SS_CHECK(Combo_ComboItemRulesValid(&on), "a record with only %s on must be a valid world",
                 kSsSongs[s].ruleName);
    }
    {
        const ComboItemRulesRecord both = SsRules(kSsBoth);
        SS_CHECK(Combo_ComboItemRulesValid(&both), "a record with both shared songs on must be a valid world");
    }

    // ---- (2) The keys: one Cross-Game Rules row each, off by default, 0 or 1 -
    ComboSettingId ids[2];
    for (int s = 0; s < 2; s++) {
        ids[s] = SsFindSetting(s);
        if (ids[s] == COMBO_SETTING_COUNT) {
            printf("[TEST] FAIL: no combo setting authors '%s': the rule has no Cross-Game Rules row and no player "
                   "can turn it on (%s:%d)\n",
                   SsKey(s).c_str(), __FILE__, __LINE__);
            printf("[TEST] shared-songs-rule: FAIL\n");
            return TEST_FAIL;
        }
        SS_CHECK(strcmp(Combo_ComboSettingLabel(ids[s]), kSsSongs[s].label) == 0,
                 "the rule's row is labelled '%s', expected '%s'", Combo_ComboSettingLabel(ids[s]), kSsSongs[s].label);
        SS_CHECK(Combo_ComboSettingDefault(ids[s]) == 0, "%s must ship OFF (OoTMM's own default)",
                 kSsSongs[s].ruleName);
        SS_CHECK(Combo_ComboSettingValueValid(ids[s], 0) && Combo_ComboSettingValueValid(ids[s], 1) &&
                     !Combo_ComboSettingValueValid(ids[s], 2) && !Combo_ComboSettingValueValid(ids[s], -1),
                 "%s's value space must be exactly 0 or 1", kSsSongs[s].ruleName);
    }
    SS_CHECK(ids[0] != ids[1], "the two songs must be two rows, not one");

    // ---- (3) The resolver: each key its own bit, and nothing else ------------
    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    {
        ComboItemRulesRecord r;
        Combo_ResolveComboItemRules(&r);
        SS_CHECK(Combo_ComboItemRulesClear(&r), "with no key set the resolved rules must be every rule off (%08X)",
                 (unsigned)r.crossItemFlags);
        for (int s = 0; s < 2; s++) {
            SS_CHECK(Combo_ComboSettingSet(ids[s], 1) == 1, "%s must be authorable before a world exists",
                     kSsSongs[s].ruleName);
            Combo_ResolveComboItemRules(&r);
            SS_CHECK(r.crossItemFlags == kSsSongs[s].bit && r.blastMaskCooldown == 0u && r.spare[0] == 0u &&
                         r.spare[1] == 0u && r.spare[2] == 0u,
                     "%s's key alone resolves to rules %08X, expected exactly %08X", kSsSongs[s].ruleName,
                     (unsigned)r.crossItemFlags, (unsigned)kSsSongs[s].bit);
            ComboSettingsRecord live;
            Combo_ResolveComboSettings(&live);
            SS_CHECK(memcmp(&live, &defaults, sizeof(live)) == 0,
                     "%s's key moved the combo record itself: it is an item rule, not a combo setting",
                     kSsSongs[s].ruleName);
            SS_CHECK(Combo_ComboSettingSet(ids[s], 0) == 1, "%s must be authorable back off", kSsSongs[s].ruleName);
            Combo_ResolveComboItemRules(&r);
            SS_CHECK(Combo_ComboItemRulesClear(&r), "%s's key explicitly 0 must resolve every rule off (%08X)",
                     kSsSongs[s].ruleName, (unsigned)r.crossItemFlags);
            CVarSetInteger(SsKey(s).c_str(), 2);
            Combo_ResolveComboItemRules(&r);
            SS_CHECK(Combo_ComboItemRulesClear(&r),
                     "an out-of-space stored 2 for %s must resolve to the shipped default (off)", kSsSongs[s].ruleName);
            CVarClear(SsKey(s).c_str());
        }
        SS_CHECK(Combo_ComboSettingSet(ids[0], 1) == 1 && Combo_ComboSettingSet(ids[1], 1) == 1,
                 "both rules must be authorable together");
        Combo_ResolveComboItemRules(&r);
        SS_CHECK(r.crossItemFlags == kSsBoth, "both keys on resolve to rules %08X, expected exactly %08X",
                 (unsigned)r.crossItemFlags, (unsigned)kSsBoth);
        SsClearAllKeys();
    }

    // ---- (4) The creation: frozen, and the fingerprint moves with each -------
    const uint32_t hashOff = Combo_ComputeComboSettingsHashEx(&defaults, NULL, kSsSettingsHash, kSsProfileDigest);
    SS_CHECK(hashOff == Combo_ComputeComboSettingsHash(&defaults, kSsSettingsHash, kSsProfileDigest),
             "the rules off must leave the fingerprint every world had before they existed");
    uint32_t hashOne[2] = { 0u, 0u };
    for (int s = 0; s < 2; s++) {
        const ComboItemRulesRecord on = SsRules(kSsSongs[s].bit);
        hashOne[s] = Combo_ComputeComboSettingsHashEx(&defaults, &on, kSsSettingsHash, kSsProfileDigest);
        SS_CHECK(hashOne[s] != hashOff, "%s did not move the fingerprint: a world rule outside its identity",
                 kSsSongs[s].ruleName);
    }
    SS_CHECK(hashOne[0] != hashOne[1], "the two songs' rules must be two different worlds");
    {
        ComboContext_Init();
        ComboItemRulesRecord resolved;
        Combo_ResolveComboItemRules(&resolved);
        SsFreezeWorld(&resolved);
        SS_CHECK((gComboCtx.comboItemRules.crossItemFlags & kSsBoth) == 0u && gComboCtx.comboSettingsHash == hashOff,
                 "with the keys unset the creation froze rules %08X and fingerprint %08X (expected 0 and %08X)",
                 (unsigned)gComboCtx.comboItemRules.crossItemFlags, (unsigned)gComboCtx.comboSettingsHash,
                 (unsigned)hashOff);
        SS_CHECK((Combo_SharedFamiliesArmed() & kSsBoth) == 0u, "no song family may be armed with the rules off");

        for (int s = 0; s < 2; s++) {
            ComboContext_Init();
            SsClearAllKeys();
            SS_CHECK(Combo_ComboSettingSet(ids[s], 1) == 1, "turn %s on before the creation", kSsSongs[s].ruleName);
            Combo_ResolveComboItemRules(&resolved);
            SsFreezeWorld(&resolved);
            SS_CHECK(Combo_ComboCrossItemRule(kSsSongs[s].bit) && !Combo_ComboCrossItemRule(kSsSongs[1 - s].bit),
                     "the creation froze rules %08X with only %s on", (unsigned)gComboCtx.comboItemRules.crossItemFlags,
                     kSsSongs[s].ruleName);
            SS_CHECK(gComboCtx.comboSettingsHash == hashOne[s],
                     "the stamped fingerprint is %08X, expected %08X: the freeze must fold %s",
                     (unsigned)gComboCtx.comboSettingsHash, (unsigned)hashOne[s], kSsSongs[s].ruleName);
            SS_CHECK((Combo_SharedFamiliesArmed() & kSsBoth) == kSsSongs[s].bit,
                     "the frozen %s arms families %08X, expected only its own", kSsSongs[s].ruleName,
                     (unsigned)(Combo_SharedFamiliesArmed() & kSsBoth));
            SS_CHECK(Combo_ComboSettingsDivergence() == 0u,
                     "a session whose key still says on must compare healthy (diverged %08X)",
                     (unsigned)Combo_ComboSettingsDivergence());
            SS_CHECK(Combo_ComboSettingSet(ids[s], 0) == 0, "%s's writer must refuse while a world is frozen",
                     kSsSongs[s].ruleName);
        }
        SsClearAllKeys();
        ComboContext_Init();
    }

    // ---- (5) Frozen wins at load: the restore API -----------------------------
    {
        const ComboItemRulesRecord bothOn = SsRules(kSsBoth);
        SsFreezeWorld(&bothOn);
        // The session never turned the rules on: both keys resolve off.
        const uint32_t bits = Combo_ComboSettingsDivergence();
        SS_CHECK(bits == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                 "a songs-on file met by a songs-off session diverged %08X, expected crossItemFlags alone",
                 (unsigned)bits);
        SS_CHECK(Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits) == 1,
                 "the file-select probe would refuse a songs-on file the keys can restore");
        char names[256] = { 0 };
        ComboSettingsKeyUndo undo;
        const int restored = Combo_ComboSettingsRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits,
                                                            names, sizeof(names), &undo);
        printf("[TEST] (5) restore: %d, restored '%s', diverged after %08X\n", restored, names,
               (unsigned)Combo_ComboSettingsDivergence());
        SS_CHECK(restored == 1 && strstr(names, kSsSongs[0].label) != NULL && strstr(names, kSsSongs[1].label) != NULL,
                 "the load restore did not put both of the file's song rules back (returned %d, '%s')", restored,
                 names);
        for (int s = 0; s < 2; s++) {
            int32_t stored = 0;
            SS_CHECK(Combo_ComboSettingReadStore(ids[s], &stored) && stored == 1,
                     "%s's restored key reads %d, expected 1", kSsSongs[s].ruleName, (int)stored);
        }
        SS_CHECK(Combo_ComboSettingsDivergence() == 0u, "after the restore the session must agree with the file");
        Combo_ComboSettingsRestoreUndo(&undo);
        SS_CHECK(!Combo_ComboSettingIsExplicit(ids[0]) && !Combo_ComboSettingIsExplicit(ids[1]) &&
                     Combo_ComboSettingsDivergence() == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                 "the undo must put both keys back to unset");

        // A later build's bit beside the songs is never restored.
        const ComboItemRulesRecord later = SsRules(kSsBoth | kSsLaterBuildBit);
        SS_CHECK(Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &later,
                                                   RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS) == 0,
                 "a frozen rule no key authors must not be claimed restorable");
        ComboContext_Init();
    }

    // ---- (6) Frozen wins at load: a real .redsave ------------------------------
    {
        const ComboItemRulesRecord bothOn = SsRules(kSsBoth);
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        const std::string priorSaveDir = std::filesystem::path(mgr.SlotPath(0)).parent_path().string();
        mgr.SetSaveDirectory(rsbs::test::ScratchDir(kSsLoadDir).string());
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        ComboContext_Init();
        SsSeedShadows(0x66u);
        SsFreezeWorld(&bothOn);
        SsSeedShadows(0x66u);
        SS_CHECK(mgr.Save(0), "Save(0) failed for the songs-on file");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        SsClearAllKeys();
        const bool loaded = mgr.Load(0);
        int32_t stored[2] = { 0, 0 };
        const bool keySet[2] = { Combo_ComboSettingReadStore(ids[0], &stored[0]),
                                 Combo_ComboSettingReadStore(ids[1], &stored[1]) };
        printf("[TEST] (6) songs-on file under a songs-off session: loaded %d, keys %d/%d, %d/%d, quarantine %d\n",
               loaded ? 1 : 0, keySet[0] ? 1 : 0, (int)stored[0], keySet[1] ? 1 : 0, (int)stored[1],
               RsbsSave_HasQuarantine(0));
        SS_CHECK(loaded, "a songs-on file was refused under a session that only turned the rules off: frozen wins "
                         "at load (#781), the keys go back to the file's values");
        SS_CHECK(keySet[0] && stored[0] == 1 && keySet[1] && stored[1] == 1,
                 "the load did not restore both of the file's song rules into their keys");
        SS_CHECK(!loaded || (Combo_ComboCrossItemRule(kSsSongs[0].bit) && Combo_ComboCrossItemRule(kSsSongs[1].bit)),
                 "the loaded world does not carry both shared songs");
        SS_CHECK(RsbsSave_HasQuarantine(0) == 0 && RsbsSave_HasSave(0) == 1, "a rule divergence is not damage");

        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        mgr.SetSaveDirectory(priorSaveDir);
        ComboContext_Init();
    }

    SsClearAllKeys();
    ComboContext_Init();
    printf("[TEST] shared-songs-rule: %s\n", sSsFailures == 0 ? "PASS" : "FAIL");
    return sSsFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-songs-merge: both games' songs are the families, through the real shims
// ============================================================================

TestResult Test_SharedSongsMerge(void) {
    printf("[TEST] shared-songs-merge: both games register Epona's Song and the Song of Storms as the shared "
           "families' members, and a song learnt in either game is known in the other at arrival through the real "
           "shims (#1066)\n");
    sSsFailures = 0;

    // ---- (0) The registrations: each game's own song, the id it places --------
    for (int s = 0; s < 2; s++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        const bool ootReg = Combo_SharedFamilyMemberId(GAME_OOT, kSsSongs[s].bit, &ootId);
        const bool mmReg = Combo_SharedFamilyMemberId(GAME_MM, kSsSongs[s].bit, &mmId);
        printf("[TEST] (0) %s registered: OoT %d (id %u, pool %d), MM %d (id %u, pool %d)\n", kSsSongs[s].ruleName,
               ootReg ? 1 : 0, (unsigned)ootId, OoT_SharedSongsTest_PoolId(s), mmReg ? 1 : 0, (unsigned)mmId,
               MM_SharedSongsTest_PoolId(s));
        SS_CHECK(ootReg && (int)ootId == OoT_SharedSongsTest_PoolId(s),
                 "OoT's %s is not registered as the %s family's OoT member (the bag pair and the arrival give both "
                 "read this registration)",
                 kSsSongs[s].itemName, kSsSongs[s].ruleName);
        SS_CHECK(mmReg && (int)mmId == MM_SharedSongsTest_PoolId(s),
                 "MM's %s is not registered as the %s family's MM member", kSsSongs[s].itemName, kSsSongs[s].ruleName);
    }

    const ComboItemRulesRecord bothOn = SsRules(kSsBoth);
    for (int s = 0; s < 2; s++) {
        // ---- (1) Learnt in Hyrule, known in Termina --------------------------
        SsFreezeWorld(&bothOn);
        SsForgetAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSongsTest_SetHeld(s, 1);
        OoT_HarvestSharedResources();
        const uint32_t afterOoT = Combo_SharedFamiliesOwned();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        printf("[TEST] (1) %s OoT -> MM: pool %08X, MM knows it %d, the other song %d\n", kSsSongs[s].ruleName,
               (unsigned)afterOoT, MM_SharedSongsTest_Held(s), MM_SharedSongsTest_Held(1 - s));
        SS_CHECK((afterOoT & kSsBoth) == kSsSongs[s].bit,
                 "OoT's harvest shim put pool %08X for %s, expected exactly its own family", (unsigned)afterOoT,
                 kSsSongs[s].itemName);
        SS_CHECK(MM_SharedSongsTest_Held(s) == 1, "MM's apply shim did not teach MM %s", kSsSongs[s].itemName);
        SS_CHECK(MM_SharedSongsTest_Held(1 - s) == 0, "learning %s in Hyrule taught MM %s too", kSsSongs[s].itemName,
                 kSsSongs[1 - s].itemName);
        MM_ApplySharedResources();
        SS_CHECK(MM_SharedSongsTest_Held(s) == 1, "a second MM arrival lost %s", kSsSongs[s].itemName);

        // ---- (2) Learnt in Termina, known in Hyrule --------------------------
        SsFreezeWorld(&bothOn);
        SsForgetAll();
        MM_SharedSwordTest_ArmLive();
        MM_SharedSongsTest_SetHeld(s, 1);
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_ApplySharedResources();
        printf("[TEST] (2) %s MM -> OoT: pool %08X, OoT knows it %d, the other song %d\n", kSsSongs[s].ruleName,
               (unsigned)Combo_SharedFamiliesOwned(), OoT_SharedSongsTest_Held(s), OoT_SharedSongsTest_Held(1 - s));
        SS_CHECK((Combo_SharedFamiliesOwned() & kSsBoth) == kSsSongs[s].bit,
                 "MM's harvest shim did not put MM's %s alone into the shared pool", kSsSongs[s].itemName);
        SS_CHECK(OoT_SharedSongsTest_Held(s) == 1, "OoT's apply shim did not teach OoT %s", kSsSongs[s].itemName);
        SS_CHECK(OoT_SharedSongsTest_Held(1 - s) == 0, "learning %s in Termina taught OoT %s too", kSsSongs[s].itemName,
                 kSsSongs[1 - s].itemName);

        // ---- (3) Only this song's rule on: the other song is not carried -----
        const ComboItemRulesRecord otherOnly = SsRules(kSsSongs[1 - s].bit);
        SsFreezeWorld(&otherOnly);
        SsForgetAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSongsTest_SetHeld(s, 1);
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        SS_CHECK((Combo_SharedFamiliesOwned() & kSsSongs[s].bit) == 0u && MM_SharedSongsTest_Held(s) == 0,
                 "with only %s's rule on, %s learnt in Hyrule reached Termina (pool %08X)", kSsSongs[1 - s].ruleName,
                 kSsSongs[s].itemName, (unsigned)Combo_SharedFamiliesOwned());
    }

    // ---- (4) The rules off: each game keeps its own songs --------------------
    SsFreezeWorld(NULL);
    SsForgetAll();
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedSongsTest_SetHeld(0, 1);
    OoT_SharedSongsTest_SetHeld(1, 1);
    OoT_HarvestSharedResources();
    MM_SharedSwordTest_ArmLive();
    MM_ApplySharedResources();
    SS_CHECK(Combo_SharedFamiliesOwned() == 0u && MM_SharedSongsTest_Held(0) == 0 && MM_SharedSongsTest_Held(1) == 0,
             "with the rules off a song learnt in Hyrule reached Termina (pool %08X)",
             (unsigned)Combo_SharedFamiliesOwned());

    SsForgetAll();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] shared-songs-merge: %s\n", sSsFailures == 0 ? "PASS" : "FAIL");
    return sSsFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-songs-bag: one row per song both solvers credit, over a real generation
// ============================================================================

namespace {
/** OoT's song shuffle for this row: anywhere (RO_SONG_SHUFFLE_ANYWHERE), so
 *  OoT's songs are general-pass bag rows like MM's and each pair has both
 *  halves. Under the default (song locations) OoT's restricted pass places its
 *  own copy before the bag, and the composer keeps MM's copy as the shared row
 *  alone (combo_logic.c pass 2b's one-side case). */
const char* const kSsSongShuffleKey = "gRandoSettings.ShuffleSongs";
const int kSsSongShuffleAnywhere = 3;
} // namespace

TestResult SharedSongsBag_Run(void) {
    printf("[TEST] shared-songs-bag: with both rules on the single bag holds ONE Epona's Song and ONE Song of Storms "
           "that both solvers credit, each placed once over both games, and the GOAL is still proven (#1066)\n");
    sSsFailures = 0;
    ComboContext_Init();
    SsClearAllKeys();
    CVarSetInteger(SsKey(0).c_str(), 1);
    CVarSetInteger(SsKey(1).c_str(), 1);
    const int priorShuffle = CVarGetInteger(kSsSongShuffleKey, -1);
    CVarSetInteger(kSsSongShuffleKey, kSsSongShuffleAnywhere);
    struct Restore {
        int prior;
        ~Restore() {
            if (prior < 0) {
                CVarClear(kSsSongShuffleKey);
            } else {
                CVarSetInteger(kSsSongShuffleKey, prior);
            }
            CVarClear(SsKey(0).c_str());
            CVarClear(SsKey(1).c_str());
        }
    } restore{ priorShuffle };

    const int gen = Rando_HeadlessSeedTest("RSBSSHAREDSONGS1");
    SS_CHECK(gen == 0, "the paired OoT generation failed");
    SS_CHECK(Combo_ComboSettingsFrozen() && Combo_ComboCrossItemRule(kSsSongs[0].bit) &&
                 Combo_ComboCrossItemRule(kSsSongs[1].bit),
             "the creation did not freeze both shared songs from the keys (rules %08X)",
             (unsigned)gComboCtx.comboItemRules.crossItemFlags);
    const int mmHalf = (gen == 0) ? MM_Rando_HeadlessPairedHalf() : -1;
    SS_CHECK(mmHalf == 0, "MM's creation-time half did not produce a rando world");
    if (gen != 0 || mmHalf != 0) {
        printf("[TEST] shared-songs-bag: FAIL\n");
        return TEST_FAIL;
    }
    const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
    printf("[TEST] shared-songs-bag: bag %d rows, %d shared pair(s), %d placed, goal proven %d\n", bag.bagCount,
           bag.compose.sharedPairs, bag.fill.placed, bag.fill.goalProven ? 1 : 0);
    SS_CHECK(bag.status == RSBS_COMBO_LOGIC_OK && bag.fill.goalProven,
             "the single-bag fill did not prove the GOAL with the shared songs");
    SS_CHECK(bag.compose.sharedPairs == 2,
             "the composer made %d shared pair(s), expected exactly the two songs: the games' copies were not paired",
             bag.compose.sharedPairs);

    const int shuffledTotal = MM_ComboLogic_TestShuffledItems(nullptr, nullptr, 0);
    std::vector<uint16_t> items((size_t)(shuffledTotal > 0 ? shuffledTotal : 1));
    const int shuffled = MM_ComboLogic_TestShuffledItems(items.data(), nullptr, (int)items.size());

    for (int s = 0; s < 2; s++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        SS_CHECK(Combo_SharedFamilyMemberId(GAME_OOT, kSsSongs[s].bit, &ootId) &&
                     Combo_SharedFamilyMemberId(GAME_MM, kSsSongs[s].bit, &mmId),
                 "the %s family is not registered in both games", kSsSongs[s].ruleName);
        SharedItem ootSong;
        memset(&ootSong, 0, sizeof(ootSong));
        ootSong.originGame = (uint8_t)GAME_OOT;
        ootSong.id = ootId;
        SharedItem mmSong;
        memset(&mmSong, 0, sizeof(mmSong));
        mmSong.originGame = (uint8_t)GAME_MM;
        mmSong.id = mmId;
        const char* ootName = Combo_DescribeItemName(ootSong);
        const char* mmName = Combo_DescribeItemName(mmSong);
        SS_CHECK(ootName != NULL && strcmp(ootName, kSsSongs[s].itemName) == 0 && mmName != NULL &&
                     strcmp(mmName, kSsSongs[s].itemName) == 0,
                 "the registered pair names '%s' and '%s', expected each game's %s",
                 ootName != NULL ? ootName : "(null)", mmName != NULL ? mmName : "(null)", kSsSongs[s].itemName);

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
                const SharedItem farHalf = isOoT ? mmSong : ootSong;
                const bool ownHalf = (host == GAME_OOT) ? isOoT : isMM;
                if (ownHalf && p.pair.originGame == farHalf.originGame && p.pair.id == farHalf.id) {
                    pairedCopies++;
                }
                printf("[TEST] shared-songs-bag: %s on %s check %u (%s half, pair %u:%u)\n", kSsSongs[s].itemName,
                       Game_ToString(host), (unsigned)p.hostCheck, isOoT ? "OoT" : "MM", (unsigned)p.pair.originGame,
                       (unsigned)p.pair.id);
            }
        }
        SS_CHECK(copies == 1 && pairedCopies == 1,
                 "the coordinator placed %d %s(s), %d of them as the shared pair: expected ONE row, the host's own "
                 "half with the other game's beside it",
                 copies, kSsSongs[s].itemName, pairedCopies);

        // MM's own world: a second copy would be the trimmed half back in play.
        int mmChecks = 0;
        for (int i = 0; i < shuffled && i < (int)items.size(); i++) {
            if (items[(size_t)i] == mmId) {
                mmChecks++;
            }
        }
        const int expectMM = (hostOfCopy == GAME_MM) ? 1 : 0;
        printf("[TEST] shared-songs-bag: MM's world holds %d %s check(s) (expected %d)\n", mmChecks,
               kSsSongs[s].itemName, expectMM);
        SS_CHECK(shuffled > 0 && mmChecks == expectMM,
                 "MM's world holds %d %s check(s), expected %d: the trimmed half is still the song", mmChecks,
                 kSsSongs[s].itemName, expectMM);
    }

    printf("[TEST] shared-songs-bag: %s\n", sSsFailures == 0 ? "PASS" : "FAIL");
    return sSsFailures == 0 ? TEST_PASS : TEST_FAIL;
}
