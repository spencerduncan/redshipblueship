/**
 * @file test_shared_swim.c
 * @brief Locks for the Shared Swim cross-game rule and the pair's one Shuffle Swim /
 *        Shuffle Ocarina Buttons (#1369; operator ruling 2026-11-02).
 *
 * Three CTest rows (label "redship"), all display-free and ROM-free:
 *
 *   SharedSwimRule ("shared-swim-rule") - THE RULE, THE FREEZE AND THE MARKER.
 *   The bit (0x08000000) is allocated, a family, named, and the one
 *   SHUFFLE-GATED rule. Its key ships ON, yet the resolver never sets the bit:
 *   the creation does (Combo_ComboItemRulesApplyCreationShuffles), and only
 *   while some game shuffles swim, so a world nobody shuffles swim in keeps its
 *   all-clear record and the fingerprint every world had before the rule existed.
 *   The coupled-buttons marker is set only with the Shared Ocarina Buttons rule
 *   and a button shuffle. The pair's one shuffle (Combo_ComboSharedShuffleCoupled)
 *   reads the frozen record: swim with the bit, buttons with the rule AND the
 *   marker, so a world written before the marker keeps its inert-unless-both
 *   rule. A session compare reads a gated bit as the file decided it against
 *   its key: a key ON beside a file that froze it OFF is not a divergence, a
 *   key OFF beside a file that froze it ON is restorable, and a restore never
 *   writes the gated key of a file that froze it off. Needs the CVar store.
 *
 *   SharedSwimPairs ("shared-swim-pairs") - THE CREDITING AND THE COUPLING.
 *   Both games register their swim (OoT's standalone RG_BRONZE_SCALE, MM's
 *   RI_ABILITY_SWIM) at the rule's slot; the bag pairs them only while both are
 *   pooled; the real composer makes the pair ONE shared row both solvers credit;
 *   OoT's Progressive Scale starts at silver only under the frozen rule with OoT
 *   shuffling swim; and MM's sites (the gate's and pool's predicate, the computed
 *   start, the give-capability publish) take the pair's one shuffle from the
 *   frozen record, except on Vanilla logic.
 *
 *   SharedSwimArrival ("shared-swim-arrival") - THE RUNTIME. Through both games'
 *   REAL harvest and apply shims: swim found in one game is held in the other on
 *   arrival (the flag each game's water gate reads), MM's own shuffle off but the
 *   pair's on; a Vanilla-logic MM, which starts able to swim, shares nothing;
 *   with the rule off nothing crosses. And the coupled buttons: a coupled MM whose
 *   own shuffle is off shares the button it found, while a pre-marker world's MM
 *   (starting with all five) shares none.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage.
 */

#include "../combo_logic.h"
#include "../combo_mm_options_view.h"
#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../context.h"
#include "../cvar_shared_keys.h"
#include "../foreign_items.h"
#include "../shared_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>

#ifdef __cplusplus
extern "C" {
#endif
// Each game's production registration (GameExports_SingleExe.cpp).
void OoT_RegisterSharedSwimFamily(void);
void MM_RegisterSharedSwimFamily(void);
// The live-save helpers (games/oot/soh/oot_shared_swim_test.cpp,
// games/mm/2s2h/mm_shared_swim_test.cpp, and the sword and button rows' helpers).
void OoT_SharedSwordTest_ArmLive(void);
void MM_SharedSwordTest_ArmLive(void);
void OoT_SharedSwimTest_Begin(void);
void OoT_SharedSwimTest_End(void);
void OoT_SharedSwimTest_Set(int shuffle, int held);
int OoT_SharedSwimTest_Held(void);
int OoT_SharedSwimTest_PoolId(void);
int OoT_SharedSwimTest_ProgressiveScaleId(void);
int OoT_SharedSwimTest_SplitsScale(void);
void MM_SharedSwimTest_Set(int shuffle, int held, int vanilla);
int MM_SharedSwimTest_Held(void);
int MM_SharedSwimTest_PoolId(void);
int MM_SharedSwimTest_WorldShuffles(int ownSwim, int ownButtons, int vanilla);
int MM_SharedSwimTest_ComputedStart(int ownSwim, int ownButtons, int vanilla);
void OoT_SharedButtonsTest_Begin(void);
void OoT_SharedButtonsTest_End(void);
void OoT_SharedButtonsTest_Set(int shuffle, unsigned heldMask);
unsigned OoT_SharedButtonsTest_Held(void);
void MM_SharedButtonsTest_Set(int shuffle, unsigned heldMask);
unsigned MM_SharedButtonsTest_Held(void);
void OoT_RegisterSharedOcarinaButtonFamilies(void);
void MM_RegisterSharedOcarinaButtonFamilies(void);
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
#ifdef __cplusplus
}
#endif

static int sSswFailures;

#define SSW_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSswFailures++;                           \
        }                                             \
    } while (0)

namespace {

/** The rule's bit, spelled as #1369 allocates it: a literal, so the lock states
 *  the .redsave format rather than reading it back from the header under test. */
const uint32_t kSswSwim = 0x08000000u;
/** The coupled-buttons marker's comboFlags bit, likewise. */
const uint8_t kSswCoupledButtons = 0x04u;
const uint32_t kSswSettingsHash = 0x1369CA5Eu;
const uint32_t kSswProfileDigest = 0x1369D16Eu;

/** The process's published give caps, put back when a row ends (the AllTests
 *  process runs every row in one process). */
struct SswGiveCaps {
    bool published[2];
    uint32_t caps[2];
};

SswGiveCaps SswSaveGiveCaps() {
    SswGiveCaps g;
    for (int i = 0; i < 2; ++i) {
        const uint8_t game = i == 0 ? (uint8_t)GAME_OOT : (uint8_t)GAME_MM;
        g.published[i] = Combo_ForeignGiveCapsPublished(game);
        g.caps[i] = Combo_ForeignGiveCaps(game);
    }
    return g;
}

void SswRestoreGiveCaps(const SswGiveCaps& g) {
    Combo_ClearForeignGiveCaps();
    for (int i = 0; i < 2; ++i) {
        if (g.published[i]) {
            Combo_PublishForeignGiveCaps(i == 0 ? (uint8_t)GAME_OOT : (uint8_t)GAME_MM, g.caps[i]);
        }
    }
}

/** Freeze a paired world the way a creation event does, with @p flags as its item
 *  rules and @p comboFlags as its combo record's flags, and register both games'
 *  real swim (and button) members. */
void SswFreezeWorld(uint32_t flags, uint8_t comboFlags) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1369F00Du;
    gComboCtx.sharedRandoSettingsHash = kSswSettingsHash;
    gComboCtx.mmProfileDigest = kSswProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    rec.comboFlags = comboFlags;
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = flags;
    Combo_FreezeComboSettingsEx(&rec, &rules);
    // Another row may have cleared the registry; the registrations are idempotent.
    OoT_RegisterSharedSwimFamily();
    MM_RegisterSharedSwimFamily();
    OoT_RegisterSharedOcarinaButtonFamilies();
    MM_RegisterSharedOcarinaButtonFamilies();
}

/** Both games' swim shuffles as the bag reads them at generation: OoT's from its
 *  seed's option, MM's from its published give caps (@p mm < 0: unpublished). */
void SswSetShuffles(int oot, int mm) {
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedSwimTest_Set(oot, 0);
    Combo_ClearForeignGiveCaps();
    if (mm >= 0) {
        Combo_PublishForeignGiveCaps((uint8_t)GAME_MM, mm != 0 ? (uint32_t)RSBS_GIVECAP_SWIM : 0u);
    }
}

void SswClearAllKeys() {
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        CVarClear(Combo_ComboSettingKey((ComboSettingId)i));
    }
}

} // namespace

// ============================================================================
// shared-swim-rule
// ============================================================================

TestResult SharedSwimRule_Run(void) {
    printf("[TEST] shared-swim-rule: the Shared Swim rule ships on and freezes only while a game shuffles swim; the "
           "coupled-buttons marker freezes only with the buttons rule and a button shuffle (#1369)\n");
    sSswFailures = 0;
    if (!Combo_ComboSettingStoreAvailable()) {
        printf("[TEST] FAIL: no CVar store: every authoring leg below would pass vacuously on the defaults\n");
        return TEST_FAIL;
    }
    ComboContext_Init();
    SswClearAllKeys();

    // ---- (1) The bit: allocated, a family, named, the one shuffle-gated rule ---
    {
        ComboItemRulesRecord r;
        memset(&r, 0, sizeof(r));
        r.crossItemFlags = kSswSwim;
        SSW_CHECK(RSBS_XITEM_SHARED_SWIM == kSswSwim && (RSBS_XITEM_ALL_V1 & kSswSwim) == kSswSwim &&
                      (RSBS_XITEM_FAMILY_MASK & kSswSwim) == kSswSwim && RSBS_XITEM_SHUFFLE_GATED == kSswSwim,
                  "the Shared Swim bit is not 08000000, allocated, a family and the shuffle-gated set");
        SSW_CHECK(strcmp(Combo_ComboItemRuleName(kSswSwim), "sharedSwim") == 0 &&
                      Combo_ComboItemRuleRequires(kSswSwim) == 0u && Combo_ComboItemRulesValid(&r),
                  "the Shared Swim bit is named '%s' (want sharedSwim), or requires a rule, or is not a valid world",
                  Combo_ComboItemRuleName(kSswSwim));
        SSW_CHECK(RSBS_COMBO_FLAG_COUPLED_OCARINA_BUTTONS == kSswCoupledButtons &&
                      (RSBS_COMBO_FLAGS_ALL_V1 & kSswCoupledButtons) == kSswCoupledButtons,
                  "the coupled-buttons marker is not comboFlags bit 04, allocated");
    }

    // ---- (2) The key ships ON, 0 or 1, and the resolver never sets the bit ------
    {
        SSW_CHECK(strcmp(Combo_ComboSettingLabel(COMBO_SETTING_SHARED_SWIM), "Shared Swim") == 0 &&
                      strcmp(Combo_ComboSettingKey(COMBO_SETTING_SHARED_SWIM), RSBS_CVAR_COMBO_RANDO_SHARED_SWIM) == 0,
                  "the rule's row is '%s' over '%s'", Combo_ComboSettingLabel(COMBO_SETTING_SHARED_SWIM),
                  Combo_ComboSettingKey(COMBO_SETTING_SHARED_SWIM));
        SSW_CHECK(Combo_ComboSettingDefault(COMBO_SETTING_SHARED_SWIM) == 1 &&
                      Combo_ComboSettingResolved(COMBO_SETTING_SHARED_SWIM) == 1,
                  "the Shared Swim key must ship ON (operator ruling 2026-11-02)");
        SSW_CHECK(Combo_ComboSettingValueValid(COMBO_SETTING_SHARED_SWIM, 0) &&
                      Combo_ComboSettingValueValid(COMBO_SETTING_SHARED_SWIM, 1) &&
                      !Combo_ComboSettingValueValid(COMBO_SETTING_SHARED_SWIM, 2),
                  "the rule's value space must be exactly 0 or 1");
        ComboItemRulesRecord r;
        Combo_ResolveComboItemRules(&r);
        SSW_CHECK(Combo_ComboItemRulesClear(&r),
                  "with nothing authored the resolved rules are %08X; the gated bit must never come from the resolver",
                  (unsigned)r.crossItemFlags);
        SSW_CHECK((Combo_ComboCouplingAuthored() & RSBS_GIVECAP_SWIM) != 0u &&
                      (Combo_ComboCouplingAuthored() & RSBS_GIVECAP_OCARINA_BUTTONS) == 0u,
                  "with nothing authored the coupling must be swim (key on) and not buttons (key off): %04X",
                  (unsigned)Combo_ComboCouplingAuthored());
        SSW_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_SWIM, 0) == 1 &&
                      (Combo_ComboCouplingAuthored() & RSBS_GIVECAP_SWIM) == 0u,
                  "the key authored off must couple nothing");
        SswClearAllKeys();
    }

    // ---- (3) The creation's gated decisions (the defaults world keeps its bytes) ---
    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    {
        static const struct {
            int swimKey; // -1 unset (ships on), else the key's value
            uint32_t startRules;
            uint32_t shuffled;
            uint32_t wantRules;
            uint8_t wantFlags;
            const char* what;
        } kCases[] = {
            { -1, 0u, 0u, 0u, 0u, "nothing shuffled (every default world)" },
            { -1, 0u, RSBS_GIVECAP_SWIM, kSswSwim, 0u, "swim shuffled, key on" },
            { 0, 0u, RSBS_GIVECAP_SWIM, 0u, 0u, "swim shuffled, key off" },
            { -1, 0u, RSBS_GIVECAP_OCARINA_BUTTONS, 0u, 0u, "buttons shuffled, no buttons rule" },
            { -1, RSBS_XITEM_SHARED_OCARINA_BUTTONS, 0u, RSBS_XITEM_SHARED_OCARINA_BUTTONS, 0u,
              "buttons rule, nothing shuffled" },
            { -1, RSBS_XITEM_SHARED_OCARINA_BUTTONS, RSBS_GIVECAP_OCARINA_BUTTONS, RSBS_XITEM_SHARED_OCARINA_BUTTONS,
              kSswCoupledButtons, "buttons rule, buttons shuffled" },
            { -1, RSBS_XITEM_SHARED_OCARINA_BUTTONS, RSBS_GIVECAP_SWIM | RSBS_GIVECAP_OCARINA_BUTTONS,
              RSBS_XITEM_SHARED_OCARINA_BUTTONS | kSswSwim, kSswCoupledButtons, "both rules, both shuffled" },
            { -1, kSswSwim, 0u, 0u, 0u, "a stale gated bit, nothing shuffled" },
        };
        for (const auto& c : kCases) {
            SswClearAllKeys();
            if (c.swimKey >= 0) {
                CVarSetInteger(RSBS_CVAR_COMBO_RANDO_SHARED_SWIM, c.swimKey);
            }
            ComboSettingsRecord settings = defaults;
            settings.comboFlags = (uint8_t)(c.wantFlags == 0u ? kSswCoupledButtons : 0u); // a stale marker clears
            ComboItemRulesRecord rules;
            memset(&rules, 0, sizeof(rules));
            rules.crossItemFlags = c.startRules;
            Combo_ComboItemRulesApplyCreationShuffles(&settings, &rules, c.shuffled);
            printf("[TEST] (3) %s: rules %08X flags %02X (want %08X, %02X)\n", c.what, (unsigned)rules.crossItemFlags,
                   (unsigned)settings.comboFlags, (unsigned)c.wantRules, (unsigned)c.wantFlags);
            SSW_CHECK(rules.crossItemFlags == c.wantRules && settings.comboFlags == c.wantFlags,
                      "%s: the creation froze rules %08X and flags %02X, expected %08X and %02X", c.what,
                      (unsigned)rules.crossItemFlags, (unsigned)settings.comboFlags, (unsigned)c.wantRules,
                      (unsigned)c.wantFlags);
        }
        SswClearAllKeys();
        // The default world freezes the all-clear record: its fingerprint is every world's before the rule.
        ComboSettingsRecord settings = defaults;
        ComboItemRulesRecord rules;
        Combo_ResolveComboItemRules(&rules);
        Combo_ComboItemRulesApplyCreationShuffles(&settings, &rules, 0u);
        const uint32_t legacy = Combo_ComputeComboSettingsHash(&defaults, kSswSettingsHash, kSswProfileDigest);
        SSW_CHECK(Combo_ComboItemRulesClear(&rules) && settings.comboFlags == 0u &&
                      Combo_ComputeComboSettingsHashEx(&settings, &rules, kSswSettingsHash, kSswProfileDigest) ==
                          legacy,
                  "a world nobody shuffles swim in moved its fingerprint: the rule ships on but must freeze nothing");
        ComboItemRulesRecord swimRules;
        memset(&swimRules, 0, sizeof(swimRules));
        swimRules.crossItemFlags = kSswSwim;
        SSW_CHECK(Combo_ComputeComboSettingsHashEx(&defaults, &swimRules, kSswSettingsHash, kSswProfileDigest) !=
                      legacy,
                  "the Shared Swim bit did not move the fingerprint");
        ComboSettingsRecord marked = defaults;
        marked.comboFlags = kSswCoupledButtons;
        SSW_CHECK(Combo_ComputeComboSettingsHash(&marked, kSswSettingsHash, kSswProfileDigest) != legacy,
                  "the coupled-buttons marker did not move the fingerprint");
    }

    // ---- (4) The pair's one shuffle, from the frozen record only ---------------
    {
        ComboContext_Init();
        SSW_CHECK(!Combo_ComboSharedShuffleCoupled(RSBS_GIVECAP_SWIM) &&
                      !Combo_ComboSharedShuffleCoupled(RSBS_GIVECAP_OCARINA_BUTTONS),
                  "nothing is frozen, so nothing may couple");
        SswFreezeWorld(kSswSwim, 0u);
        SSW_CHECK(Combo_ComboSharedShuffleCoupled(RSBS_GIVECAP_SWIM) &&
                      !Combo_ComboSharedShuffleCoupled(RSBS_GIVECAP_OCARINA_BUTTONS) &&
                      !Combo_ComboSharedShuffleCoupled(RSBS_GIVECAP_SOULS),
                  "the frozen Shared Swim bit must couple swim and nothing else");
        SswFreezeWorld(RSBS_XITEM_SHARED_OCARINA_BUTTONS, 0u);
        SSW_CHECK(!Combo_ComboSharedShuffleCoupled(RSBS_GIVECAP_OCARINA_BUTTONS),
                  "a world with the buttons rule but no marker (written before #1369) must stay inert unless both "
                  "games shuffle");
        SswFreezeWorld(RSBS_XITEM_SHARED_OCARINA_BUTTONS, kSswCoupledButtons);
        SSW_CHECK(Combo_ComboSharedShuffleCoupled(RSBS_GIVECAP_OCARINA_BUTTONS) &&
                      !Combo_ComboSharedShuffleCoupled(RSBS_GIVECAP_SWIM),
                  "the buttons rule with its marker must couple the buttons and not swim");
        SswFreezeWorld(0u, kSswCoupledButtons);
        SSW_CHECK(!Combo_ComboSharedShuffleCoupled(RSBS_GIVECAP_OCARINA_BUTTONS),
                  "the marker without the rule must couple nothing");
    }

    // ---- (5) The compare reads a gated bit as the file decided it ---------------
    {
        // A swim world, the key unset (on): healthy.
        SswClearAllKeys();
        SswFreezeWorld(kSswSwim, 0u);
        SSW_CHECK(Combo_ComboSettingsDivergence() == 0u, "a Shared Swim world met by its own key diverged %08X",
                  (unsigned)Combo_ComboSettingsDivergence());
        // The key turned off: crossItemFlags, restorable, and the restore writes the key back on.
        CVarSetInteger(RSBS_CVAR_COMBO_RANDO_SHARED_SWIM, 0);
        const uint32_t bits = Combo_ComboSettingsDivergence();
        SSW_CHECK(bits == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS &&
                      Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits) == 1,
                  "a Shared Swim world met by a key off diverged %08X; it must be crossItemFlags, restorable",
                  (unsigned)bits);
        char names[160] = { 0 };
        const int restored = Combo_ComboSettingsRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits,
                                                            names, sizeof(names), NULL);
        SSW_CHECK(restored == 1 && strcmp(names, "Shared Swim") == 0 &&
                      Combo_ComboSettingResolved(COMBO_SETTING_SHARED_SWIM) == 1 &&
                      Combo_ComboSettingsDivergence() == 0u,
                  "the load did not restore the file's Shared Swim (returned %d, '%s')", restored, names);

        // A world nobody shuffled swim in (the bit frozen off): healthy with the key on AND off.
        SswClearAllKeys();
        SswFreezeWorld(0u, 0u);
        SSW_CHECK(Combo_ComboSettingsDivergence() == 0u,
                  "the rule inert at creation read as a divergence beside its shipped key (%08X)",
                  (unsigned)Combo_ComboSettingsDivergence());
        CVarSetInteger(RSBS_CVAR_COMBO_RANDO_SHARED_SWIM, 0);
        SSW_CHECK(Combo_ComboSettingsDivergence() == 0u,
                  "the rule inert at creation read as a divergence beside a key off (%08X)",
                  (unsigned)Combo_ComboSettingsDivergence());
        // Another rule diverges: its restore writes that rule only, never the gated key of a file that froze it
        // off. The Swim key is left at its shipped ON, which differs from that file's OFF bit.
        SswClearAllKeys();
        SswFreezeWorld(RSBS_XITEM_SHARED_SHIELDS, 0u);
        const uint32_t shieldBits = Combo_ComboSettingsDivergence();
        char shieldNames[160] = { 0 };
        const int shieldRestored = Combo_ComboSettingsRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules,
                                                                  shieldBits, shieldNames, sizeof(shieldNames), NULL);
        printf("[TEST] (5) a Shared Shields file met by unset keys: diverged %08X, restored %d '%s'\n",
               (unsigned)shieldBits, shieldRestored, shieldNames);
        SSW_CHECK(shieldBits == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS && shieldRestored == 1 &&
                      strcmp(shieldNames, "Shared Shields") == 0 &&
                      !Combo_ComboSettingIsExplicit(COMBO_SETTING_SHARED_SWIM) && Combo_ComboSettingsDivergence() == 0u,
                  "restoring a Shared Shields file wrote '%s' (want only Shared Shields; the Shared Swim key, inert "
                  "in that file, must stay unset)",
                  shieldNames);

        // The coupled-buttons marker: the session never authors it, so the compare reads the file's.
        SswClearAllKeys();
        CVarSetInteger(RSBS_CVAR_COMBO_RANDO_SHARED_OCARINA_BUTTONS, 1);
        SswFreezeWorld(RSBS_XITEM_SHARED_OCARINA_BUTTONS, kSswCoupledButtons);
        SSW_CHECK(Combo_ComboSettingsDivergence() == 0u,
                  "a coupled-buttons world met by its own keys diverged %08X; the marker is the file's",
                  (unsigned)Combo_ComboSettingsDivergence());
    }

    SswClearAllKeys();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSswFailures != 0) {
        printf("[TEST] FAIL: shared-swim-rule (%d failure(s))\n", sSswFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-swim-rule\n");
    return TEST_PASS;
}

// ============================================================================
// shared-swim-pairs
// ============================================================================

TestResult Test_SharedSwimPairs(void) {
    printf("[TEST] shared-swim-pairs: both games register their swim as the Shared Swim family, the bag pairs them "
           "into one row both solvers credit, and MM's sites take the pair's one shuffle (#1369)\n");
    sSswFailures = 0;
    const SswGiveCaps priorCaps = SswSaveGiveCaps();
    OoT_SharedSwimTest_Begin();
    const uint16_t ootId = (uint16_t)OoT_SharedSwimTest_PoolId();
    const uint16_t mmId = (uint16_t)MM_SharedSwimTest_PoolId();

    // ---- (1) THE REGISTRATIONS ---------------------------------------------------
    {
        Combo_RegisterSharedFamilyMember(GAME_OOT, kSswSwim, NULL);
        Combo_RegisterSharedFamilyMember(GAME_MM, kSswSwim, NULL);
        OoT_RegisterSharedSwimFamily();
        MM_RegisterSharedSwimFamily();
        uint16_t o = 0;
        uint16_t m = 0;
        const bool okO = Combo_SharedFamilyMemberId(GAME_OOT, kSswSwim, &o);
        const bool okM = Combo_SharedFamilyMemberId(GAME_MM, kSswSwim, &m);
        printf("[TEST] (1) Shared Swim members: OoT %d/%u, MM %d/%u (expected 1/%u, 1/%u)\n", okO ? 1 : 0, (unsigned)o,
               okM ? 1 : 0, (unsigned)m, (unsigned)ootId, (unsigned)mmId);
        SSW_CHECK(okO && o == ootId, "OoT did not register its standalone bronze tier at the Shared Swim slot");
        SSW_CHECK(okM && m == mmId, "MM did not register its Ability to Swim at the Shared Swim slot");
        SSW_CHECK(
            ootId != (uint16_t)OoT_SharedSwimTest_ProgressiveScaleId(),
            "OoT's member must be the standalone RG_BRONZE_SCALE, never Progressive Scale: a shared copy credited "
            "as a scale would hand the solver a silver scale the give never grants");
    }

    // ---- (2) OFF: nothing armed, no pair ------------------------------------------
    {
        SswFreezeWorld(0u, 0u);
        SswSetShuffles(1, 1);
        ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
        const int n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        SSW_CHECK((Combo_SharedFamiliesArmed() & kSswSwim) == 0u && n == 0,
                  "OFF: the rule off armed %08X and built %d pair(s)", (unsigned)Combo_SharedFamiliesArmed(), n);
    }

    // ---- (3) ON, both pooled: one pair --------------------------------------------
    ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
    int pairCount = 0;
    {
        SswFreezeWorld(kSswSwim, 0u);
        SswSetShuffles(1, 1);
        pairCount = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        printf("[TEST] (3) on: armed %08X, %d pair(s) (expected %08X, 1)\n", (unsigned)Combo_SharedFamiliesArmed(),
               pairCount, (unsigned)kSswSwim);
        SSW_CHECK(Combo_SharedFamiliesArmed() == kSswSwim && pairCount == 1 && pairs[0].ootId == ootId &&
                      pairs[0].mmId == mmId,
                  "ON: the bag built %d pair(s); it must pair OoT's bronze tier with MM's Ability to Swim", pairCount);
    }

    // ---- (3b) ON, one side not pooled: no pair (the copy stays its game's own) -----
    {
        static const struct {
            int oot;
            int mm;
            const char* what;
        } kOneOff[] = {
            { 1, 0, "OoT on, MM off (a Vanilla-logic MM)" },
            { 0, 1, "OoT off, MM on" },
            { 1, -1, "OoT on, MM unpublished" },
        };
        for (const auto& k : kOneOff) {
            SswFreezeWorld(kSswSwim, 0u);
            SswSetShuffles(k.oot, k.mm);
            ComboLogicSharedPair none[RSBS_XITEM_BIT_COUNT];
            const int n = Combo_SingleBag_SharedPairs(none, (int)RSBS_XITEM_BIT_COUNT);
            printf("[TEST] (3b) on, %s: %d pair(s) (expected 0)\n", k.what, n);
            SSW_CHECK(n == 0,
                      "ON, %s: the bag built %d swim pair(s); a pair hosted in the game without the shuffle "
                      "is never delivered",
                      k.what, n);
        }
    }

    // ---- (4) THE CREDITING: the real composer over both real classifications ------
    {
        SSW_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table did not come up");
        ComboLogicPoolRow pool[2];
        memset(pool, 0, sizeof(pool));
        pool[0].item.originGame = (uint8_t)GAME_OOT;
        pool[0].item.id = ootId;
        pool[1].item.originGame = (uint8_t)GAME_MM;
        pool[1].item.id = mmId;
        const int dO = Combo_Logic_ComposeDisposition(pool[0].item, 0u, 0u);
        const int dM = Combo_Logic_ComposeDisposition(pool[1].item, 0u, (uint32_t)RSBS_FILL_ARM_SWIM);
        printf("[TEST] (4) dispositions: OoT bronze %d, MM swim %d (expected %d, %d)\n", dO, dM,
               RSBS_COMBO_COMPOSE_REQUIRED, RSBS_COMBO_COMPOSE_REQUIRED);
        SSW_CHECK(dO == RSBS_COMBO_COMPOSE_REQUIRED && dM == RSBS_COMBO_COMPOSE_REQUIRED,
                  "the two swims are not REQUIRED in their games; the row's premise moved");
        ComboLogicComposeRequest req;
        memset(&req, 0, sizeof(req));
        req.rows = pool;
        req.rowCount = 2;
        req.armedMM = (uint32_t)RSBS_FILL_ARM_SWIM;
        req.quantityFlags = RSBS_COMBO_QUANTITY_KEEP_ALL;
        req.sharedPairs = pairCount > 0 ? pairs : NULL;
        req.sharedPairCount = pairCount;
        ComboLogicBagItem bag[4];
        int index[4];
        ComboLogicComposeResult res;
        memset(&res, 0, sizeof(res));
        const int rc = Combo_Logic_ComposeBag(&req, bag, 4, index, &res);
        const bool shared = res.bagCount == 1 && (bag[0].bagFlags & RSBS_COMBO_BAG_SHARED_PAIR) != 0u &&
                            bag[0].item.originGame == (uint8_t)GAME_OOT && bag[0].item.id == ootId &&
                            bag[0].pairId == mmId;
        printf("[TEST] (4) compose: rc %d, %d bag row(s), %d pair(s), %d trimmed (expected 0, 1, 1, 1)\n", rc,
               res.bagCount, res.sharedPairs, Combo_Logic_ComposeTrimmedCount());
        SSW_CHECK(rc == RSBS_COMBO_LOGIC_OK && res.sharedPairs == 1 && shared && Combo_Logic_ComposeTrimmedCount() == 1,
                  "the two swims are not ONE shared row (OoT's bronze tier, naming MM's) with MM's copy trimmed");
    }

    // ---- (5) OoT's Progressive Scale starts at silver only under the frozen rule ----
    {
        static const struct {
            uint32_t flags;
            int ootShuffle;
            int want;
            const char* what;
        } kSplit[] = {
            { kSswSwim, 1, 1, "rule frozen, OoT shuffles swim" },
            { kSswSwim, 0, 0, "rule frozen, OoT does not shuffle swim" },
            { 0u, 1, 0, "rule off, OoT shuffles swim (SoH's own bronze tier)" },
        };
        for (const auto& k : kSplit) {
            SswFreezeWorld(k.flags, 0u);
            SswSetShuffles(k.ootShuffle, 1);
            const int split = OoT_SharedSwimTest_SplitsScale();
            printf("[TEST] (5) %s: the bronze tier %s its own row (expected %s)\n", k.what, split ? "is" : "is not",
                   k.want ? "is" : "is not");
            SSW_CHECK(split == k.want, "%s: OoT_SharedSwimSplitsScale answered %d, expected %d", k.what, split, k.want);
        }
    }

    // ---- (6) MM's sites take the pair's one shuffle from the frozen record ---------
    {
        static const struct {
            uint32_t flags;
            uint8_t comboFlags;
            int ownSwim;
            int ownButtons;
            int vanilla;
            int wantShuffles; // bit 0 swim, bit 1 buttons
            int wantStart;    // bit 0 the start holds swim, bit 1 all five buttons
            const char* what;
        } kMM[] = {
            { 0u, 0u, 0, 0, 0, 0, 3, "no rule, nothing shuffled" },
            { 0u, 0u, 1, 1, 0, 3, 0, "no rule, MM shuffles both" },
            { kSswSwim, 0u, 0, 0, 0, 1, 2, "Shared Swim frozen, MM's own swim off" },
            { kSswSwim, 0u, 0, 0, 1, 0, 3, "Shared Swim frozen, MM on Vanilla logic" },
            { RSBS_XITEM_SHARED_OCARINA_BUTTONS, 0u, 0, 0, 0, 0, 3, "buttons rule without the marker (pre-#1369)" },
            { RSBS_XITEM_SHARED_OCARINA_BUTTONS, kSswCoupledButtons, 0, 0, 0, 2, 1, "buttons rule with the marker" },
            { RSBS_XITEM_SHARED_OCARINA_BUTTONS, kSswCoupledButtons, 0, 0, 1, 0, 3,
              "buttons rule with the marker, MM on Vanilla logic" },
        };
        for (const auto& k : kMM) {
            SswFreezeWorld(k.flags, k.comboFlags);
            const int shuffles = MM_SharedSwimTest_WorldShuffles(k.ownSwim, k.ownButtons, k.vanilla);
            const int start = MM_SharedSwimTest_ComputedStart(k.ownSwim, k.ownButtons, k.vanilla);
            printf("[TEST] (6) %s: MM shuffles %d (want %d), its start %d (want %d)\n", k.what, shuffles,
                   k.wantShuffles, start, k.wantStart);
            SSW_CHECK(shuffles == k.wantShuffles && start == k.wantStart,
                      "%s: MM's sites shuffle %d and start %d, expected %d and %d", k.what, shuffles, start,
                      k.wantShuffles, k.wantStart);
        }
        // The give capabilities a hydrated MM half publishes: the pair's swim arms MM's swim row into the bag; MM's
        // OWN shuffles (what the creation decides the gated rules from) stay its own.
        SswFreezeWorld(kSswSwim, 0u);
        MM_SharedSwordTest_ArmLive();
        MM_SharedSwimTest_Set(0, 0, 0);
        MM_Rando_PublishProfileGiveCaps(/*fromSave=*/1);
        const uint32_t caps = Combo_ForeignGiveCaps((uint8_t)GAME_MM);
        const uint32_t own = MM_Rando_ResolveOwnShuffleCaps(/*fromSave=*/1);
        printf("[TEST] (6) Shared Swim frozen, MM's own swim off: published caps %04X, own %04X\n", (unsigned)caps,
               (unsigned)own);
        SSW_CHECK((caps & RSBS_GIVECAP_SWIM) != 0u && (own & RSBS_GIVECAP_SWIM) == 0u,
                  "the publish must arm swim under the pair's one shuffle while MM's own shuffles stay its own");
        MM_SharedSwimTest_Set(0, 0, 1);
        MM_Rando_PublishProfileGiveCaps(/*fromSave=*/1);
        SSW_CHECK((Combo_ForeignGiveCaps((uint8_t)GAME_MM) & RSBS_GIVECAP_SWIM) == 0u,
                  "a Vanilla-logic MM must not arm swim: it places nothing without a check of its own");
    }

    OoT_SharedSwimTest_End();
    // Leave both live saves plain, non-randomizer files for the next row.
    OoT_SharedSwordTest_ArmLive();
    MM_SharedSwordTest_ArmLive();
    SswRestoreGiveCaps(priorCaps);
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSswFailures != 0) {
        printf("[TEST] FAIL: shared-swim-pairs (%d failure(s))\n", sSswFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-swim-pairs\n");
    return TEST_PASS;
}

// ============================================================================
// shared-swim-arrival
// ============================================================================

TestResult Test_SharedSwimArrival(void) {
    printf("[TEST] shared-swim-arrival: with the Shared Swim rule on, swim found in one game is held in the other on "
           "arrival; a coupled MM shares the button it found; off, nothing crosses (#1369)\n");
    sSswFailures = 0;
    OoT_SharedSwimTest_Begin();
    OoT_SharedButtonsTest_Begin();

    // ---- (1) OFF: nothing crosses -------------------------------------------------
    {
        SswFreezeWorld(0u, 0u);
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSwimTest_Set(1, 1);
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        MM_SharedSwimTest_Set(1, 0, 0);
        MM_ApplySharedResources();
        SSW_CHECK(MM_SharedSwimTest_Held() == 0 && (Combo_SharedFamiliesOwned() & kSswSwim) == 0u,
                  "OFF: OoT's swim reached MM");
    }

    // ---- (2) ON, OoT -> MM, MM's own shuffle off: the pair's one shuffle ----------
    {
        SswFreezeWorld(kSswSwim, 0u);
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSwimTest_Set(1, 1);
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        MM_SharedSwimTest_Set(0, 0, 0);
        MM_ApplySharedResources();
        printf("[TEST] (2) on, OoT swim -> MM (own shuffle off): MM holds %d (expected 1)\n", MM_SharedSwimTest_Held());
        SSW_CHECK(MM_SharedSwimTest_Held() == 1,
                  "ON: MM arrived without the swim OoT found; Termina Link would respawn on every water touch");
    }

    // ---- (3) ON, MM -> OoT: the flag OoT's void-on-water gate reads ----------------
    {
        SswFreezeWorld(kSswSwim, 0u);
        MM_SharedSwordTest_ArmLive();
        MM_SharedSwimTest_Set(0, 1, 0);
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSwimTest_Set(1, 0);
        OoT_ApplySharedResources();
        printf("[TEST] (3) on, MM swim -> OoT: OoT holds %d (expected 1)\n", OoT_SharedSwimTest_Held());
        SSW_CHECK(OoT_SharedSwimTest_Held() == 1,
                  "ON: OoT arrived without the swim MM found; Hyrule Link would void out in water");
    }

    // ---- (4) ON, a Vanilla-logic MM starts able to swim and shares nothing ---------
    {
        SswFreezeWorld(kSswSwim, 0u);
        MM_SharedSwordTest_ArmLive();
        MM_SharedSwimTest_Set(0, 1, 1); // the start's swim, not a find
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSwimTest_Set(1, 0);
        OoT_ApplySharedResources();
        SSW_CHECK(OoT_SharedSwimTest_Held() == 0 && (Combo_SharedFamiliesOwned() & kSswSwim) == 0u,
                  "ON: a Vanilla-logic MM gave OoT the swim it starts with, which nobody found");
    }

    // ---- (5) ON, MONOTONIC: a later stint without it never takes it back -----------
    {
        SswFreezeWorld(kSswSwim, 0u);
        MM_SharedSwordTest_ArmLive();
        MM_SharedSwimTest_Set(0, 1, 0);
        MM_HarvestSharedResources();
        MM_SharedSwimTest_Set(0, 0, 0);
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSwimTest_Set(1, 0);
        OoT_ApplySharedResources();
        SSW_CHECK(OoT_SharedSwimTest_Held() == 1, "ON: the shared swim was lost from the pair");
    }

    // ---- (6) THE COUPLED BUTTONS: MM's own shuffle off, the marker on ---------------
    {
        // A coupled world: MM's own button shuffle is off, yet the pair shuffles them, so the C-Up MM found
        // reaches OoT.
        SswFreezeWorld(RSBS_XITEM_SHARED_OCARINA_BUTTONS, kSswCoupledButtons);
        MM_SharedSwordTest_ArmLive();
        MM_SharedButtonsTest_Set(0, 0x02u); // C-Up, in the row's (OoT's) order
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedButtonsTest_Set(1, 0u);
        OoT_ApplySharedResources();
        const unsigned coupled = OoT_SharedButtonsTest_Held();
        // The same MM in a world written before the marker: its own shuffle off means it started with all five,
        // which nobody found, so none may cross.
        SswFreezeWorld(RSBS_XITEM_SHARED_OCARINA_BUTTONS, 0u);
        MM_SharedSwordTest_ArmLive();
        MM_SharedButtonsTest_Set(0, 0x1Fu);
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedButtonsTest_Set(1, 0u);
        OoT_ApplySharedResources();
        const unsigned legacy = OoT_SharedButtonsTest_Held();
        printf("[TEST] (6) coupled MM C-Up -> OoT: OoT holds %02X (expected 02); pre-marker MM with all five -> "
               "OoT: %02X (expected 00)\n",
               coupled, legacy);
        SSW_CHECK(coupled == 0x02u, "a coupled MM's C-Up did not reach OoT (OoT holds %02X)", coupled);
        SSW_CHECK(legacy == 0u, "a pre-marker world's MM gave OoT the five buttons it started with (OoT holds %02X)",
                  legacy);
    }

    OoT_SharedButtonsTest_End();
    OoT_SharedSwimTest_End();
    OoT_SharedSwordTest_ArmLive();
    MM_SharedSwordTest_ArmLive();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSswFailures != 0) {
        printf("[TEST] FAIL: shared-swim-arrival (%d failure(s))\n", sSswFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-swim-arrival\n");
    return TEST_PASS;
}
