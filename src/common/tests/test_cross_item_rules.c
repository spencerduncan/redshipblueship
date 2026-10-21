/**
 * @file test_cross_item_rules.c
 * @brief ROM-free locks for the cross-game item rules and the shared-family
 *        bitset (#976, epic #975 P1).
 *
 * Two rows, split by what each one can prove:
 *
 *   cross-item-rules-record (CTest CrossItemRulesRecord) -- the FORMAT. The
 *   sibling record's carve (offsets 900 and 908, reserved[] from 912, still
 *   above ADR 0009's 64-byte floor), its pinned bit and cooldown spaces, the
 *   shipped defaults (every rule off) and the session resolver that returns
 *   them, the validity rule (an allocated bit, an extension under every shared
 *   extension, a cooldown in its space, zero spares), canonical() byte for byte,
 *   and THE FINGERPRINT: an all-clear record leaves comboSettingsHash exactly
 *   what it was before the record existed (both pinned vectors of
 *   test_combo_settings.c reproduce), every allocated bit, the cooldown and each
 *   spare MOVE it, and two golden vectors computed outside this build pin the
 *   encoding. Then the freeze (FreezeComboSettingsEx stores the rules and stamps
 *   over them; the old freeze stores every rule off; the file-create
 *   invalidation KEEPS them with the record), the divergence diff (a frozen rule
 *   the session cannot honour is named, is not damage, and no key can restore
 *   it; a rule changed after the stamp is FINGERPRINT damage; a load that read
 *   the rules but did not fold them would see damage too), and THE LOAD: a
 *   .redsave whose rules are on is refused by this build and nothing is
 *   quarantined, while the same world with every rule off loads.
 *
 *   shared-family-merge (CTest SharedFamilyMerge) -- the RUNTIME. The OR
 *   discipline over gComboCtx.sharedFamilyOwned: a family whose rule is off is
 *   never harvested or applied, a harvest only ever adds bits (both directions),
 *   an apply owes exactly the families owned and not held, a legacy record reads
 *   all clear, and the member registry refuses what is not one family bit. Then
 *   THE WIRING through both games' REAL shims: a family OoT's harvest shim sees
 *   held is given to MM by MM's apply shim (and the other way round), a held
 *   family is not given twice, and with the rule off neither shim calls a
 *   member.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage.
 */

#include "../combo_settings_view.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../test_scratch.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif
void OoT_SharedSwordTest_ArmLive(void);
void MM_SharedSwordTest_ArmLive(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
#ifdef __cplusplus
}
#endif

static int sXirFailures;

#define XIR_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sXirFailures++;                           \
        }                                             \
    } while (0)

namespace {

const uint32_t kXirSettingsHash = 0xDEADBEEFu;
const uint32_t kXirProfileDigest = 0x0BADF00Du;
const char* const kXirLoadDir = "rsbs_test_cross_item_rules_load";

/** A valid ON record: two shared masks, the Blast Mask in OoT and its shared
 *  variant, and the SHORT cooldown. */
ComboItemRulesRecord XirOnRecord() {
    ComboItemRulesRecord r;
    memset(&r, 0, sizeof(r));
    r.crossItemFlags = RSBS_XITEM_SHARED_GORON | RSBS_XITEM_SHARED_BUNNY | RSBS_XITEM_BLAST_MASK_OOT |
                       RSBS_XITEM_SHARED_BLAST; // 0x00000825
    r.blastMaskCooldown = (uint8_t)RSBS_BLAST_COOLDOWN_SHORT;
    return r;
}

/** Publish a paired identity the way Playthrough_Init stamps one. */
void XirArmPairing(uint32_t seed) {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = seed;
    gComboCtx.sharedRandoSettingsHash = kXirSettingsHash;
    gComboCtx.mmProfileDigest = kXirProfileDigest;
}

/** Freeze a paired world with the shipped combo record and @p rules. */
void XirFreezeWorld(const ComboItemRulesRecord* rules) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    XirArmPairing(0x0976F00Du);
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx(&rec, rules);
}

/** Give the commit choke point two shadows to stage (test_combo_settings.c's
 *  helper, restated so this file stands alone). */
void XirSeedShadows(uint8_t fill) {
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, fill);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, (uint8_t)(fill ^ 0xFFu));
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

// ---- the shared-family stub members: one "item" per game ---------------------
bool sXirHeld[3];
int sXirGives[3];
int sXirHeldCalls[3];

bool XirHeldOoT(uint16_t id) {
    (void)id;
    sXirHeldCalls[GAME_OOT]++;
    return sXirHeld[GAME_OOT];
}
void XirGiveOoT(uint16_t id) {
    (void)id;
    sXirGives[GAME_OOT]++;
    sXirHeld[GAME_OOT] = true;
}
bool XirHeldMM(uint16_t id) {
    (void)id;
    sXirHeldCalls[GAME_MM]++;
    return sXirHeld[GAME_MM];
}
void XirGiveMM(uint16_t id) {
    (void)id;
    sXirGives[GAME_MM]++;
    sXirHeld[GAME_MM] = true;
}

void XirResetMembers() {
    memset(sXirHeld, 0, sizeof(sXirHeld));
    memset(sXirGives, 0, sizeof(sXirGives));
    memset(sXirHeldCalls, 0, sizeof(sXirHeldCalls));
}

} // namespace

TestResult Test_CrossItemRulesRecord(void) {
    printf("[TEST] cross-item-rules-record: the cross-game item rules are a frozen .redsave record folded into the "
           "fingerprint only when set, diverge-checked on load (#976)\n");
    sXirFailures = 0;

    // ---- (0) The carve -------------------------------------------------------
    XIR_CHECK(offsetof(ComboContext, comboItemRules) == 900u, "comboItemRules is at %u, expected 900",
              (unsigned)offsetof(ComboContext, comboItemRules));
    XIR_CHECK(offsetof(ComboContext, sharedFamilyOwned) == 908u, "sharedFamilyOwned is at %u, expected 908",
              (unsigned)offsetof(ComboContext, sharedFamilyOwned));
    XIR_CHECK(offsetof(ComboContext, reserved) == 912u && sizeof(((ComboContext*)0)->reserved) == 92u,
              "reserved[] is %u bytes at %u, expected 92 at 912", (unsigned)sizeof(((ComboContext*)0)->reserved),
              (unsigned)offsetof(ComboContext, reserved));
    XIR_CHECK(sizeof(ComboContext) == 1004u, "sizeof(ComboContext) is %u; the record size must not change",
              (unsigned)sizeof(ComboContext));
    XIR_CHECK(sizeof(ComboItemRulesRecord) == 8u, "sizeof(ComboItemRulesRecord) is %u, expected 8",
              (unsigned)sizeof(ComboItemRulesRecord));

    // ---- (1) The pinned value spaces ----------------------------------------
    XIR_CHECK(RSBS_XITEM_SHARED_GORON == 0x1u && RSBS_XITEM_SHARED_BUNNY == 0x4u && RSBS_XITEM_SHARED_TRUTH == 0x10u &&
                  RSBS_XITEM_BLAST_MASK_OOT == 0x20u && RSBS_XITEM_SPOOKY_MASK_MM == 0x400u &&
                  RSBS_XITEM_SHARED_BLAST == 0x800u && RSBS_XITEM_SHARED_SPOOKY == 0x10000u &&
                  RSBS_XITEM_SHARED_ARROW_FIRE == 0x20000u && RSBS_XITEM_SHARED_ARROW_LIGHT == 0x80000u &&
                  RSBS_XITEM_SHARED_BOTTLES == 0x100000u && RSBS_XITEM_SHARED_OCARINA_BUTTONS == 0x01000000u &&
                  RSBS_XITEM_SHARED_SHIELDS == 0x02000000u,
              "an RSBS_XITEM_* bit moved; they are .redsave format, append-only");
    // #1068 allocates the Shared Shields bit and #1067 the Shared Ocarina Buttons
    // bit: each ALLOCATED (a record holding it is valid and the bit has a name)
    // and a FAMILY (the Hylian/Hero's pair; the A buttons).
    XIR_CHECK(RSBS_XITEM_ALL_V1 == 0x031FFFFFu,
              "RSBS_XITEM_ALL_V1 is %08X, expected 031FFFFF (#976's 21 bits, #1067's Shared Ocarina Buttons and "
              "#1068's Shared Shields)",
              (unsigned)RSBS_XITEM_ALL_V1);
    XIR_CHECK(RSBS_XITEM_FAMILY_MASK == 0x030FF81Fu,
              "RSBS_XITEM_FAMILY_MASK is %08X, expected 030FF81F (the five shared masks, the six shared extensions, "
              "the three arrows, the shared ocarina buttons and the shared shields; not the extensions, not the "
              "bottles)",
              (unsigned)RSBS_XITEM_FAMILY_MASK);
    {
        // #1067: ONE rule bit over five pairs. The four C-button pairs own extra
        // family slots at the EXTENSION rules' bits (never families), so the
        // rule bit is the only crossItemFlags bit the rule spends.
        ComboItemRulesRecord buttons;
        memset(&buttons, 0, sizeof(buttons));
        buttons.crossItemFlags = RSBS_XITEM_SHARED_OCARINA_BUTTONS;
        XIR_CHECK(Combo_ComboItemRulesValid(&buttons) &&
                      strcmp(Combo_ComboItemRuleName(RSBS_XITEM_SHARED_OCARINA_BUTTONS), "sharedOcarinaButtons") == 0,
                  "a record with only the Shared Ocarina Buttons rule must be valid and the rule named "
                  "sharedOcarinaButtons (#1067); this build would refuse its own world");
        const uint32_t cSlots = RSBS_XFAMILY_SLOT_OCARINA_C_UP | RSBS_XFAMILY_SLOT_OCARINA_C_DOWN |
                                RSBS_XFAMILY_SLOT_OCARINA_C_LEFT | RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT;
        XIR_CHECK(cSlots == 0x000001E0u && (RSBS_XFAMILY_SLOTS_EXTRA & cSlots) == cSlots &&
                      (RSBS_XITEM_FAMILY_MASK & cSlots) == 0u,
                  "the four C-button family slots are not the extension rules' bits 0x1E0, or not extra slots, or a "
                  "family rule's (extra slots %08X)",
                  (unsigned)RSBS_XFAMILY_SLOTS_EXTRA);
        XIR_CHECK(Combo_SharedFamilySlotsArmedBy(RSBS_XITEM_SHARED_OCARINA_BUTTONS) ==
                          (RSBS_XITEM_SHARED_OCARINA_BUTTONS | cSlots) &&
                      Combo_SharedFamilySlotsArmedBy(RSBS_XITEM_BLAST_MASK_OOT | RSBS_XITEM_STONE_MASK_OOT |
                                                     RSBS_XITEM_KAMARO_MASK_OOT | RSBS_XITEM_GERUDO_MASK_MM) == 0u,
                  "the Shared Ocarina Buttons rule must arm its own bit and the four C slots, and the extension rules "
                  "whose bits the slots reuse must arm nothing (armed %08X)",
                  (unsigned)Combo_SharedFamilySlotsArmedBy(RSBS_XITEM_SHARED_OCARINA_BUTTONS));
        // The fingerprint: an all-clear world keeps its identity, the rule moves it.
        ComboItemRulesRecord clear;
        Combo_ComboItemRulesDefaults(&clear);
        ComboSettingsRecord rec;
        Combo_ComboSettingsDefaults(&rec);
        const uint32_t hClear = Combo_ComputeComboSettingsHashEx(&rec, &clear, 0x1067u, 0x1067u);
        const uint32_t hOn = Combo_ComputeComboSettingsHashEx(&rec, &buttons, 0x1067u, 0x1067u);
        XIR_CHECK(hClear != hOn, "the Shared Ocarina Buttons rule did not move the fingerprint (%08X)", (unsigned)hOn);
    }
    {
        ComboItemRulesRecord shields;
        memset(&shields, 0, sizeof(shields));
        shields.crossItemFlags = RSBS_XITEM_SHARED_SHIELDS;
        XIR_CHECK(Combo_ComboItemRulesValid(&shields) &&
                      strcmp(Combo_ComboItemRuleName(RSBS_XITEM_SHARED_SHIELDS), "sharedShields") == 0,
                  "a record with only the Shared Shields rule must be valid and the rule named sharedShields (#1068); "
                  "this build would refuse its own world");
        XIR_CHECK(strcmp(Combo_ComboItemRuleName(RSBS_XFAMILY_SLOT_MIRROR_SHIELD), "(unknown)") == 0 &&
                      (RSBS_XITEM_ALL_V1 & RSBS_XFAMILY_SLOT_MIRROR_SHIELD) == 0u,
                  "the Mirror Shield's family slot is not a rule");
    }
    XIR_CHECK(RSBS_BLAST_COOLDOWN_DEFAULT == 0u && RSBS_BLAST_COOLDOWN_MAX == 5u,
              "the cooldown space moved; DEFAULT must stay 0");
    for (uint32_t b = 0; b < 32u; ++b) {
        const uint32_t bit = 1u << b;
        const bool allocated = (RSBS_XITEM_ALL_V1 & bit) != 0u;
        XIR_CHECK(allocated == (strcmp(Combo_ComboItemRuleName(bit), "(unknown)") != 0),
                  "bit %u: allocated %d but its name is '%s'", (unsigned)b, allocated ? 1 : 0,
                  Combo_ComboItemRuleName(bit));
    }
    XIR_CHECK(Combo_ComboItemRuleRequires(RSBS_XITEM_SHARED_BLAST) == RSBS_XITEM_BLAST_MASK_OOT &&
                  Combo_ComboItemRuleRequires(RSBS_XITEM_SHARED_SPOOKY) == RSBS_XITEM_SPOOKY_MASK_MM &&
                  Combo_ComboItemRuleRequires(RSBS_XITEM_SHARED_GORON) == 0u,
              "a shared extension must require its extension, and only a shared extension requires anything");

    // ---- (2) Defaults, resolver, validity -------------------------------------
    ComboItemRulesRecord defaults;
    memset(&defaults, 0xEE, sizeof(defaults));
    Combo_ComboItemRulesDefaults(&defaults);
    XIR_CHECK(Combo_ComboItemRulesClear(&defaults), "the shipped default must be every rule off, every byte zero");
    ComboItemRulesRecord live;
    memset(&live, 0xEE, sizeof(live));
    Combo_ResolveComboItemRules(&live);
    XIR_CHECK(memcmp(&live, &defaults, sizeof(live)) == 0,
              "the session resolver must be the shipped defaults while no rule key is set");
    XIR_CHECK(Combo_ComboItemRulesValid(&defaults) && Combo_ComboItemRulesValid(NULL), "the defaults are valid");
    {
        const ComboItemRulesRecord on = XirOnRecord();
        XIR_CHECK(Combo_ComboItemRulesValid(&on), "the ON record is valid");
        ComboItemRulesRecord r = on;
        r.crossItemFlags &= ~RSBS_XITEM_BLAST_MASK_OOT;
        XIR_CHECK(!Combo_ComboItemRulesValid(&r), "a shared extension without its extension must be invalid");
        r = on;
        r.crossItemFlags |= 0x00200000u;
        XIR_CHECK(!Combo_ComboItemRulesValid(&r), "an unallocated bit must be invalid");
        r = on;
        r.blastMaskCooldown = 6u;
        XIR_CHECK(!Combo_ComboItemRulesValid(&r), "a cooldown past VERY_LONG must be invalid");
        r = on;
        r.spare[1] = 1u;
        XIR_CHECK(!Combo_ComboItemRulesValid(&r), "a nonzero spare must be invalid");
    }

    // ---- (3) canonical() --------------------------------------------------------
    {
        const ComboItemRulesRecord on = XirOnRecord();
        uint8_t canon[RSBS_COMBO_ITEM_RULES_CANONICAL_LEN];
        memset(canon, 0xEE, sizeof(canon));
        Combo_ComboItemRulesCanonical(&on, canon);
        static const uint8_t kGold[RSBS_COMBO_ITEM_RULES_CANONICAL_LEN] = { 0x25, 0x08, 0x00, 0x00,
                                                                            0x03, 0x00, 0x00, 0x00 };
        XIR_CHECK(memcmp(canon, kGold, sizeof(kGold)) == 0,
                  "canonical(item rules) is %02X %02X %02X %02X %02X %02X %02X %02X, expected 25 08 00 00 03 00 00 00",
                  canon[0], canon[1], canon[2], canon[3], canon[4], canon[5], canon[6], canon[7]);
        ComboItemRulesRecord hi;
        memset(&hi, 0, sizeof(hi));
        hi.crossItemFlags = 0x12345678u;
        Combo_ComboItemRulesCanonical(&hi, canon);
        XIR_CHECK(canon[0] == 0x78 && canon[1] == 0x56 && canon[2] == 0x34 && canon[3] == 0x12,
                  "crossItemFlags must be encoded little-endian a byte at a time");
    }

    // ---- (4) THE FINGERPRINT: all clear changes nothing ------------------------
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    const uint32_t base = Combo_ComputeComboSettingsHash(&rec, kXirSettingsHash, kXirProfileDigest);
    // Vectors at the generator version (#1003): version 0 is the label-free value,
    // and RSBS_GENERATOR_VERSION 1 extends the same FNV state by ':' || LE32(1).
    XIR_CHECK(base == 0xFC9F21ABu, "the shipped defaults' fingerprint is %08X, expected FC9F21AB", (unsigned)base);
    XIR_CHECK(Combo_ComputeComboSettingsHashForGeneratorEx(&rec, NULL, kXirSettingsHash, kXirProfileDigest, 0u) ==
                  0xBA5FC364u,
              "the shipped defaults' version-0 fingerprint must stay BA5FC364");
    XIR_CHECK(Combo_ComputeComboSettingsHashEx(&rec, NULL, kXirSettingsHash, kXirProfileDigest) == base &&
                  Combo_ComputeComboSettingsHashEx(&rec, &defaults, kXirSettingsHash, kXirProfileDigest) == base,
              "an all-clear item-rules record MOVED the fingerprint: every world without a rule must keep its "
              "identity, its single-bag seed and its goldens");

    // ---- (5) Every rule, the cooldown and each spare MOVE it -------------------
    {
        uint32_t seen[RSBS_XITEM_BIT_COUNT + 4u];
        int seenCount = 0;
        for (uint32_t b = 0; b < RSBS_XITEM_BIT_COUNT; ++b) {
            const uint32_t bit = 1u << b;
            if ((RSBS_XITEM_ALL_V1 & bit) == 0u) {
                continue;
            }
            ComboItemRulesRecord r;
            memset(&r, 0, sizeof(r));
            r.crossItemFlags = bit;
            const uint32_t h = Combo_ComputeComboSettingsHashEx(&rec, &r, kXirSettingsHash, kXirProfileDigest);
            XIR_CHECK(h != base, "rule %s did not move the fingerprint: a world rule outside the world's identity",
                      Combo_ComboItemRuleName(bit));
            for (int k = 0; k < seenCount; ++k) {
                XIR_CHECK(seen[k] != h, "rule %s collides with an earlier rule's fingerprint",
                          Combo_ComboItemRuleName(bit));
            }
            seen[seenCount++] = h;
        }
        ComboItemRulesRecord r;
        memset(&r, 0, sizeof(r));
        r.blastMaskCooldown = (uint8_t)RSBS_BLAST_COOLDOWN_SHORT;
        const uint32_t hc = Combo_ComputeComboSettingsHashEx(&rec, &r, kXirSettingsHash, kXirProfileDigest);
        XIR_CHECK(hc == 0xD7F65DF5u, "the SHORT-cooldown fingerprint is %08X, expected D7F65DF5", (unsigned)hc);
        XIR_CHECK(Combo_ComputeComboSettingsHashForGeneratorEx(&rec, &r, kXirSettingsHash, kXirProfileDigest, 0u) ==
                      0x1E7FDB76u,
                  "the SHORT-cooldown version-0 fingerprint must be 1E7FDB76");
        for (int sp = 0; sp < 3; ++sp) {
            memset(&r, 0, sizeof(r));
            r.spare[sp] = 1u;
            XIR_CHECK(Combo_ComputeComboSettingsHashEx(&rec, &r, kXirSettingsHash, kXirProfileDigest) != base,
                      "spare byte %d did not move the fingerprint", sp);
        }
        const ComboItemRulesRecord on = XirOnRecord();
        const uint32_t hon = Combo_ComputeComboSettingsHashEx(&rec, &on, kXirSettingsHash, kXirProfileDigest);
        XIR_CHECK(hon == 0x5033F222u, "the ON record's fingerprint is %08X, expected 5033F222", (unsigned)hon);
        XIR_CHECK(Combo_ComputeComboSettingsHashForGeneratorEx(&rec, &on, kXirSettingsHash, kXirProfileDigest, 0u) ==
                      0x3AC849EBu,
                  "the ON record's version-0 fingerprint must be 3AC849EB");
        printf("[TEST] (5) %d rules each move the fingerprint (base %08X, ON %08X, cooldown %08X)\n", seenCount,
               (unsigned)base, (unsigned)hon, (unsigned)hc);
    }

    // ---- (6) The freeze and the KEEP set -----------------------------------------
    {
        const ComboItemRulesRecord on = XirOnRecord();
        XirFreezeWorld(&on);
        XIR_CHECK(memcmp(&gComboCtx.comboItemRules, &on, sizeof(on)) == 0,
                  "FreezeComboSettingsEx must store the rules");
        XIR_CHECK(gComboCtx.comboSettingsHash == 0x5033F222u,
                  "the stamped fingerprint is %08X; the freeze must fold the rules (expected 5033F222)",
                  (unsigned)gComboCtx.comboSettingsHash);
        // The bag folds the label-free value over the record AND its rules (#976 x
        // #1003): the rules stay in the world's seed, only the version comes off.
        XIR_CHECK(Combo_ComboSettingsWorldHash() == 0x3AC849EBu,
                  "the world hash of a ruled world is %08X; expected the version-0 value 3AC849EB",
                  (unsigned)Combo_ComboSettingsWorldHash());
        // A file a pre-#1003 build of the rules stamped (version 0) is the same
        // healthy identity; only a hash no version reproduces is damage.
        XIR_CHECK(Combo_ComboSettingsHashGeneratorVersionEx(&gComboCtx.comboSettings, &on, kXirSettingsHash,
                                                            kXirProfileDigest, 0x3AC849EBu) == 0 &&
                      Combo_ComboSettingsHashGeneratorVersionEx(&gComboCtx.comboSettings, &on, kXirSettingsHash,
                                                                kXirProfileDigest,
                                                                0x5033F222u) == (int)RSBS_GENERATOR_VERSION,
                  "a ruled fingerprint must verify at every generator version this build knows");
        XIR_CHECK(Combo_ComboCrossItemFlags() == on.crossItemFlags &&
                      Combo_ComboCrossItemRule(RSBS_XITEM_SHARED_GORON) &&
                      !Combo_ComboCrossItemRule(RSBS_XITEM_SHARED_ZORA) &&
                      Combo_ComboBlastMaskCooldown() == (uint8_t)RSBS_BLAST_COOLDOWN_SHORT,
                  "the frozen accessors must read the frozen record");
        Context_InvalidateSessionState(RSBS_SEED_STAMP_KEEP);
        XIR_CHECK(memcmp(&gComboCtx.comboItemRules, &on, sizeof(on)) == 0 && gComboCtx.comboSettingsHash == 0x5033F222u,
                  "the file-create invalidation dropped the item rules while keeping the fingerprint over them");
        // Kept with its fingerprint, the record is not damage: the only fields the
        // session diff names are the two rules this session cannot author ((7)).
        XIR_CHECK((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) == 0u,
                  "a kept world's rules no longer reproduce its kept fingerprint");
        Context_InvalidateSessionState(RSBS_SEED_STAMP_DROP);
        XIR_CHECK(Combo_ComboItemRulesClear(&gComboCtx.comboItemRules) && Combo_ComboCrossItemFlags() == 0u,
                  "a DROP invalidation must drop the rules with the record");

        ComboContext_Init();
        XirArmPairing(0x0976F00Eu);
        gComboCtx.comboItemRules = on; // a stale value the plain freeze must not keep
        Combo_FreezeComboSettings(&rec);
        XIR_CHECK(Combo_ComboItemRulesClear(&gComboCtx.comboItemRules) && gComboCtx.comboSettingsHash == base,
                  "Combo_FreezeComboSettings must freeze every rule off and stamp today's fingerprint");
        ComboContext_Init();
        gComboCtx.comboItemRules = on; // bytes with no frozen record behind them
        XIR_CHECK(Combo_ComboCrossItemFlags() == 0u &&
                      Combo_ComboBlastMaskCooldown() == (uint8_t)RSBS_BLAST_COOLDOWN_DEFAULT,
                  "nothing is frozen: no rule may read as on");
        ComboContext_Init();
    }

    // ---- (6b) The creation's two failure exits undo the rules too ---------------
    // Playthrough_Init's rollback (snapshot -> restore) and
    // RetractFailedPairedCreation (retract) both go through these three calls.
    {
        // The "untouched" state: a frozen world with every rule off.
        XirFreezeWorld(NULL);
        ComboCreationRecords prior;
        memset(&prior, 0xEE, sizeof(prior));
        Combo_SnapshotCreationRecords(&prior);
        // A generation that failed after the freeze: a different record, the
        // rules on (so a new fingerprint), and a triforce record.
        ComboSettingsRecord failed;
        Combo_ComboSettingsDefaults(&failed);
        failed.comboFlags ^= 0x01u;
        const ComboItemRulesRecord on = XirOnRecord();
        Combo_FreezeComboSettingsEx(&failed, &on);
        gComboCtx.comboTriforce.totalOoT = 2u;
        gComboCtx.comboTriforce.requiredOoT = 1u;
        Combo_RestoreCreationRecords(&prior);
        const bool settingsBack =
            memcmp(&gComboCtx.comboSettings, &prior.comboSettings, sizeof(prior.comboSettings)) == 0;
        const bool hashBack = gComboCtx.comboSettingsHash == prior.comboSettingsHash;
        const bool triforceBack =
            memcmp(&gComboCtx.comboTriforce, &prior.comboTriforce, sizeof(prior.comboTriforce)) == 0;
        const bool rulesBack =
            memcmp(&gComboCtx.comboItemRules, &prior.comboItemRules, sizeof(prior.comboItemRules)) == 0;
        printf(
            "[TEST] (6b) rollback: record %d, fingerprint %d, triforce %d, item rules %d restored (expected all 1)\n",
            settingsBack ? 1 : 0, hashBack ? 1 : 0, triforceBack ? 1 : 0, rulesBack ? 1 : 0);
        XIR_CHECK(settingsBack && hashBack && triforceBack && rulesBack,
                  "the creation rollback did not restore all four creation records: a failed generation must leave "
                  "the world untouched");
        XIR_CHECK((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) == 0u,
                  "after the rollback the restored fingerprint does not reproduce over the resident rules: a failed "
                  "generation turned the prior world into FINGERPRINT damage");

        // A creation that failed: retracted, every record ABSENT.
        XirFreezeWorld(&on);
        gComboCtx.comboTriforce.totalOoT = 2u;
        gComboCtx.comboTriforce.requiredOoT = 1u;
        Combo_RetractCreationRecords();
        ComboSettingsRecord zeroSettings;
        memset(&zeroSettings, 0, sizeof(zeroSettings));
        ComboTriforceRecord zeroTriforce;
        memset(&zeroTriforce, 0, sizeof(zeroTriforce));
        XIR_CHECK(memcmp(&gComboCtx.comboSettings, &zeroSettings, sizeof(zeroSettings)) == 0 &&
                      gComboCtx.comboSettingsHash == 0u &&
                      memcmp(&gComboCtx.comboTriforce, &zeroTriforce, sizeof(zeroTriforce)) == 0 &&
                      Combo_ComboItemRulesClear(&gComboCtx.comboItemRules),
                  "the creation retraction left a creation record behind (record %u, fingerprint %08X, items %08X)",
                  (unsigned)gComboCtx.comboSettings.formatVersion, (unsigned)gComboCtx.comboSettingsHash,
                  (unsigned)gComboCtx.comboItemRules.crossItemFlags);
        Context_InvalidateSessionState(RSBS_SEED_STAMP_KEEP);
        XIR_CHECK(Combo_ComboItemRulesClear(&gComboCtx.comboItemRules),
                  "the KEEP invalidation after a retraction carried rule bytes (%08X) behind an ABSENT record",
                  (unsigned)gComboCtx.comboItemRules.crossItemFlags);
        ComboContext_Init();
    }

    // ---- (7) Divergence ----------------------------------------------------------
    {
        const ComboItemRulesRecord on = XirOnRecord();
        XirFreezeWorld(&on);
        const uint32_t bits = Combo_ComboSettingsDivergence();
        char fields[256];
        Combo_ComboSettingsDivergenceDescribe(bits, fields, sizeof(fields));
        printf("[TEST] (7) rules on vs a session that cannot author them: %08X (%s)\n", (unsigned)bits, fields);
        XIR_CHECK(bits == (RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS | RSBS_COMBO_DIVERGE_BLAST_MASK_COOLDOWN),
                  "the session diff is %08X; a frozen rule this session would not freeze must be named, alone",
                  (unsigned)bits);
        XIR_CHECK(!Combo_ComboSettingsDivergenceIsDamage(bits), "a rule divergence is the session's, not damage");
        XIR_CHECK(Combo_ComboSettingsUnrestorableBits(&gComboCtx.comboItemRules, bits) == bits &&
                      Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits) == 0,
                  "no key of this build authors these rules (only #1068's Shared Shields has one), so no load may "
                  "claim to restore them");
        XIR_CHECK(strcmp(Combo_ComboSettingsDivergenceFieldName(RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS),
                         "crossItemFlags") == 0 &&
                      strcmp(Combo_ComboSettingsDivergenceFieldName(RSBS_COMBO_DIVERGE_BLAST_MASK_COOLDOWN),
                             "blastMaskCooldown") == 0 &&
                      strcmp(Combo_ComboSettingsDivergenceFieldName(RSBS_COMBO_DIVERGE_ITEM_RULES_SPARE),
                             "itemRulesSpare") == 0,
                  "the three new divergence bits must be named");
        XIR_CHECK(RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS == 0x4000u && RSBS_COMBO_DIVERGE_BLAST_MASK_COOLDOWN == 0x8000u &&
                      RSBS_COMBO_DIVERGE_ITEM_RULES_SPARE == 0x10000u,
                  "the divergence bits must be contiguous after 0x2000");

        // A rule changed AFTER the stamp: the record and the hash describe
        // different worlds.
        gComboCtx.comboItemRules.crossItemFlags |= RSBS_XITEM_SHARED_ZORA;
        XIR_CHECK((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) != 0u,
                  "a rule flipped after the stamp must be FINGERPRINT damage");
        Combo_StampComboSettingsHash();

        // A check that read the record but did not fold the rules would call a
        // healthy world damaged: the Ex form is the only correct load check.
        const uint32_t noRules =
            Combo_ComboSettingsDivergenceFor(&gComboCtx.comboSettings, gComboCtx.comboSettingsHash,
                                             gComboCtx.sharedRandoSettingsHash, gComboCtx.mmProfileDigest);
        XIR_CHECK((noRules & RSBS_COMBO_DIVERGE_FINGERPRINT) != 0u,
                  "the rules-blind check must not reproduce a fingerprint stamped over rules");

        // The rules OFF: healthy.
        XirFreezeWorld(NULL);
        XIR_CHECK(Combo_ComboSettingsDivergence() == 0, "a world with every rule off must compare healthy");
        ComboItemRulesRecord sp;
        memset(&sp, 0, sizeof(sp));
        sp.spare[2] = 7u;
        XIR_CHECK(Combo_ComboItemRulesDivergenceBetween(&sp, NULL) == RSBS_COMBO_DIVERGE_ITEM_RULES_SPARE,
                  "a spare byte from a later build must be named itemRulesSpare");
    }

    // ---- (8) THE LOAD: a file with a rule on is refused, nothing quarantined ---
    {
        // A per-process scratch directory (#926), and the previous save
        // directory put back afterwards, so no other row sees this one's files.
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        const std::string priorSaveDir = std::filesystem::path(mgr.SlotPath(0)).parent_path().string();
        mgr.SetSaveDirectory(rsbs::test::ScratchDir(kXirLoadDir).string());

        // Control: every rule off saves and loads.
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        ComboContext_Init();
        XirSeedShadows(0x76u);
        XirFreezeWorld(NULL);
        XirSeedShadows(0x76u);
        XIR_CHECK(mgr.Save(0), "Save(0) failed for the rules-off file");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        XIR_CHECK(mgr.Load(0), "the rules-off file must load");

        // A later build's file: a rule on.
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        const ComboItemRulesRecord on = XirOnRecord();
        XirFreezeWorld(&on);
        XirSeedShadows(0x77u);
        XIR_CHECK(mgr.Save(0), "Save(0) failed for the rules-on file");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        const bool loaded = mgr.Load(0);
        printf("[TEST] (8) rules-on file: loaded %d, quarantine %d, file present %d (expected 0, 0, 1)\n",
               loaded ? 1 : 0, RsbsSave_HasQuarantine(0), RsbsSave_HasSave(0));
        XIR_CHECK(!loaded, "a file whose frozen item rules this build cannot honour LOADED: it would play as if "
                           "the rules were off");
        XIR_CHECK(RsbsSave_HasQuarantine(0) == 0 && RsbsSave_HasSave(0) == 1,
                  "a rule divergence is not damage: the .redsave must stay where it is");

        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        mgr.SetSaveDirectory(priorSaveDir);
        ComboContext_Init();
    }

    printf("[TEST] cross-item-rules-record: %s\n", sXirFailures == 0 ? "PASS" : "FAIL");
    return sXirFailures == 0 ? TEST_PASS : TEST_FAIL;
}

TestResult Test_SharedFamilyMerge(void) {
    printf("[TEST] shared-family-merge: the shared-family bitset merges by OR in both directions, arms only from the "
           "frozen rules, and reaches both games through their real shims (#976)\n");
    sXirFailures = 0;
    const uint32_t G = RSBS_XITEM_SHARED_GORON;
    const uint32_t Z = RSBS_XITEM_SHARED_ZORA;
    const uint32_t K = RSBS_XITEM_SHARED_KEATON;

    // ---- (0) The registry refuses what is not one family bit ------------------
    Combo_ClearSharedFamilyMembers();
    {
        const ComboSharedFamilyMember m = { 7u, XirHeldOoT, XirGiveOoT };
        const ComboSharedFamilyMember noGive = { 7u, XirHeldOoT, nullptr };
        // The Skull Mask's extension bit: #1067's C-button family slots reuse the
        // Blast/Stone/Kamaro/Gerudo extension bits (0x1E0) as EXTRA slots, so
        // those four are slots now; the remaining extension bits are not.
        XIR_CHECK(!Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SKULL_MASK_MM, &m),
                  "an extension bit is not a family");
        XIR_CHECK(!Combo_RegisterSharedFamilyMember(GAME_OOT, G | Z, &m), "two bits are not one family");
        XIR_CHECK(!Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_BOTTLES, &m),
                  "the bottles' bit is reserved, not a family");
        XIR_CHECK(!Combo_RegisterSharedFamilyMember(GAME_NONE, G, &m), "GAME_NONE has no copy");
        XIR_CHECK(!Combo_RegisterSharedFamilyMember(GAME_OOT, G, &noGive), "a member needs both callbacks");
        uint16_t id = 0;
        XIR_CHECK(!Combo_SharedFamilyMemberId(GAME_OOT, G, &id), "nothing registered yet");
    }

    // ---- (1) OFF: nothing is armed, nothing merges ------------------------------
    XirFreezeWorld(NULL);
    XIR_CHECK(Combo_SharedFamiliesArmed() == 0u, "with every rule off no family may be armed");
    XIR_CHECK(Combo_SharedFamiliesHarvest(GAME_OOT, G | Z | K) == 0u && gComboCtx.sharedFamilyOwned == 0u,
              "a harvest under no armed family wrote the bitset (it must stay the legacy all-zero bytes)");
    XIR_CHECK(Combo_SharedFamiliesOwed(GAME_MM, 0u) == 0u, "nothing is owed when nothing is armed");
    ComboContext_Init();
    XIR_CHECK(Combo_SharedFamiliesArmed() == 0u && Combo_SharedFamiliesOwned() == 0u,
              "a legacy (zero-extended) record must read all clear");

    // ---- (2) ON: OR in both directions, never taking away ------------------------
    {
        ComboItemRulesRecord r;
        memset(&r, 0, sizeof(r));
        r.crossItemFlags = G | Z;
        XirFreezeWorld(&r);
        XIR_CHECK(Combo_SharedFamiliesArmed() == (G | Z), "the frozen rules arm exactly their families");
        XIR_CHECK(Combo_SharedFamiliesHarvest(GAME_OOT, G | K) == G,
                  "OoT's harvest must add Goron and NOT the unarmed Keaton family");
        XIR_CHECK(Combo_SharedFamiliesHarvest(GAME_MM, 0u) == G,
                  "MM's harvest holding nothing must not clear Goron: the OR never takes the other game's copy");
        XIR_CHECK(Combo_SharedFamiliesOwed(GAME_MM, 0u) == G, "MM is owed Goron");
        XIR_CHECK(Combo_SharedFamiliesHarvest(GAME_MM, Z) == (G | Z), "MM's Zora joins the pool");
        XIR_CHECK(Combo_SharedFamiliesOwed(GAME_OOT, G) == Z, "OoT, holding Goron, is owed only Zora");
        XIR_CHECK(Combo_SharedFamiliesHarvest(GAME_OOT, 0u) == (G | Z),
                  "OoT's harvest holding nothing must not clear either family");
        XIR_CHECK(Combo_SharedFamiliesOwed(GAME_OOT, G | Z) == 0u, "a game holding both is owed nothing");
    }

    // ---- (3) THE WIRING: through both games' real shims --------------------------
    {
        const ComboSharedFamilyMember ootMember = { 0x0123u, XirHeldOoT, XirGiveOoT };
        const ComboSharedFamilyMember mmMember = { 0x0456u, XirHeldMM, XirGiveMM };
        XIR_CHECK(Combo_RegisterSharedFamilyMember(GAME_OOT, G, &ootMember) &&
                      Combo_RegisterSharedFamilyMember(GAME_MM, G, &mmMember),
                  "both games' copies of the Goron family register");
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        XIR_CHECK(Combo_SharedFamilyMemberId(GAME_OOT, G, &ootId) && ootId == 0x0123u &&
                      Combo_SharedFamilyMemberId(GAME_MM, G, &mmId) && mmId == 0x0456u,
                  "the registry answers each game's own id (the bag's pair ids)");

        ComboItemRulesRecord r;
        memset(&r, 0, sizeof(r));
        r.crossItemFlags = G;

        // OoT finds it, MM arrives without it.
        XirFreezeWorld(&r);
        XirResetMembers();
        OoT_SharedSwordTest_ArmLive();
        sXirHeld[GAME_OOT] = true;
        OoT_HarvestSharedResources();
        const uint32_t afterOoT = Combo_SharedFamiliesOwned();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        printf("[TEST] (3) OoT -> MM: pool %08X, MM given %d, MM holds %d (expected %08X, 1, 1)\n", (unsigned)afterOoT,
               sXirGives[GAME_MM], sXirHeld[GAME_MM] ? 1 : 0, (unsigned)G);
        XIR_CHECK(afterOoT == G, "OoT's real harvest shim did not put the family OoT holds into the pool");
        XIR_CHECK(sXirGives[GAME_MM] == 1 && sXirHeld[GAME_MM], "MM's real apply shim did not give MM its copy");
        // OoT's own next arrival: it holds its copy, so it is owed nothing.
        OoT_SharedSwordTest_ArmLive();
        OoT_ApplySharedResources();
        XIR_CHECK(sXirGives[GAME_OOT] == 0, "OoT, which held its copy, was given another at its own arrival");
        MM_SharedSwordTest_ArmLive();

        // A second arrival gives nothing: MM holds it now.
        MM_ApplySharedResources();
        XIR_CHECK(sXirGives[GAME_MM] == 1, "an MM arrival holding its copy was given a second one");

        // MM finds it, OoT arrives without it.
        XirFreezeWorld(&r);
        XirResetMembers();
        MM_SharedSwordTest_ArmLive();
        sXirHeld[GAME_MM] = true;
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_ApplySharedResources();
        printf("[TEST] (3) MM -> OoT: pool %08X, OoT given %d (expected %08X, 1)\n",
               (unsigned)Combo_SharedFamiliesOwned(), sXirGives[GAME_OOT], (unsigned)G);
        XIR_CHECK(Combo_SharedFamiliesOwned() == G, "MM's real harvest shim did not put MM's family into the pool");
        XIR_CHECK(sXirGives[GAME_OOT] == 1 && sXirHeld[GAME_OOT], "OoT's real apply shim did not give OoT its copy");

        // OFF: neither shim asks a member anything.
        XirFreezeWorld(NULL);
        XirResetMembers();
        sXirHeld[GAME_OOT] = true;
        OoT_SharedSwordTest_ArmLive();
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        XIR_CHECK(sXirHeldCalls[GAME_OOT] == 0 && sXirHeldCalls[GAME_MM] == 0 && sXirGives[GAME_MM] == 0 &&
                      Combo_SharedFamiliesOwned() == 0u,
                  "with the rule off a shim asked a member or gave a copy");
    }

    Combo_ClearSharedFamilyMembers();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] shared-family-merge: %s\n", sXirFailures == 0 ? "PASS" : "FAIL");
    return sXirFailures == 0 ? TEST_PASS : TEST_FAIL;
}
