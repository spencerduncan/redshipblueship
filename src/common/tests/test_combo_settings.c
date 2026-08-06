/**
 * @file test_combo_settings.c
 * @brief ROM-free locks for the frozen combo-level rule record (ADR 0011
 *        increment 1, #498).
 *
 * Four rows, split by what each one can prove:
 *
 *   combo-settings-format — the carve is FORMAT. sizeof and every member
 *   offset, the pinned value spaces, the shipped defaults (which must
 *   reproduce today's world exactly), the freeze/frozen-predicate pair, the
 *   pool-size resolution with its unfrozen fallback, a byte-exact .redsave
 *   round trip of both new fields, and the zero-extension of a record written
 *   before the carve existed.
 *
 *   combo-settings-canonical — the GOLDEN VECTOR, modelled on
 *   test_netplay_relay.c's wire-format vectors. A fixed ComboSettingsRecord
 *   maps to a fixed byte string and a fixed digest, so a layout change, a
 *   member reorder, an endianness slip or a "simplify canonical() into a
 *   memcpy" refactor is a RED TEST rather than a silently different world
 *   identity. Includes the construction-order leg (the same values assembled
 *   in a different order digest identically), the whole-pair-fold leg (moving
 *   either half-digest moves the fingerprint — without it O6 would be
 *   vacuous), and the zero-displacement leg.
 *
 *   combo-settings-divergence — the field-level diff behind the named refusal
 *   (decision 1.1 justification 2). Each field diverged ALONE names exactly
 *   itself; the describe form renders the names the refusal message
 *   interpolates; an unreadable (future-version) record refuses rather than
 *   guessing; a record and a fingerprint that disagree are caught.
 *
 *   NON-VACUITY lives here and is load-bearing for the arrival gate MM-side:
 *   the gate's condition is literally `Combo_ComboSettingsDivergence() != 0`,
 *   so proving the diff returns 0 for a healthy pair proves the gate does not
 *   fire for one. A diff that refused everything would be red here.
 *
 *   combo-settings-legacy-freeze — the O5 transitional writer (decision 4.4).
 *   A paired file whose record reads absent freezes the SHIPPED DEFAULTS at
 *   its first crossing and COMPARES thereafter; an unpaired session freezes
 *   nothing. The "compares rather than re-freezes" leg is what stops the
 *   transitional writer from degenerating into a self-healing overwrite, which
 *   would make every arrival-time divergence disappear the instant it was
 *   detected.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as
 * C++, like test_save_roundtrip.c) for the rsbs::SaveManager half; every
 * symbol under test is C-linkage.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define CS_ASSERT(cond, msg)                    \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

namespace {

const char* const kComboSettingsTestDir = "rsbs_test_combo_settings";

/** Give the commit choke point something to stage. StageCommit refuses when the
 *  context shadows are absent, and a Tier-1-only test would otherwise never get
 *  past it — the .redsave is one whole-file commit (ADR 0009 decision 4), not a
 *  tier the caller can write in isolation. */
void ComboSettingsSeedShadows(uint8_t fill) {
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, fill);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, (uint8_t)(fill ^ 0xFFu));
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

/** Publish the live pairing carrier the way Playthrough_Init stamps it. */
void ComboSettingsArmPairing(uint32_t seed, uint32_t settingsHash, uint32_t profileDigest) {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = seed;
    gComboCtx.sharedRandoSettingsHash = settingsHash;
    gComboCtx.mmProfileDigest = profileDigest;
}

/** A record with every field distinct from every other field's value, so a
 *  member swap in the encoder cannot cancel out. */
ComboSettingsRecord ComboSettingsGoldenRecord() {
    ComboSettingsRecord r;
    memset(&r, 0, sizeof(r));
    r.formatVersion = 1u;
    r.direction = (uint8_t)RSBS_COMBO_DIR_FORWARD;         // 2
    r.poolSizeOoT = 3u;
    r.poolSizeMM = 5u;
    r.itemClassOoT = 0x1234u;
    r.itemClassMM = 0xABCDu;
    r.goal = (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT;       // 3
    r.logicRung = (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE;  // 3
    r.spare0 = 0u;
    r.spare1 = 0u;
    return r;
}

bool ComboSettingsRecordsEqual(const ComboSettingsRecord& a, const ComboSettingsRecord& b) {
    return a.formatVersion == b.formatVersion && a.direction == b.direction && a.poolSizeOoT == b.poolSizeOoT &&
           a.poolSizeMM == b.poolSizeMM && a.itemClassOoT == b.itemClassOoT && a.itemClassMM == b.itemClassMM &&
           a.goal == b.goal && a.logicRung == b.logicRung && a.spare0 == b.spare0 && a.spare1 == b.spare1;
}

} // namespace

// ============================================================================
// combo-settings-format
// ============================================================================

TestResult Test_ComboSettingsFormat(void) {
    printf("[TEST] combo-settings-format: the ADR 0011 carve is .redsave format and its defaults reproduce "
           "today's world\n");

    // ---- The struct itself -------------------------------------------------
    // Companion runtime assertions to context.h's static_asserts. Cheap, and
    // they put the numbers in the CI log where a reviewer reading a format
    // change can see them.
    CS_ASSERT(sizeof(ComboSettingsRecord) == 12, "ComboSettingsRecord is not 12 bytes");
    CS_ASSERT(offsetof(ComboSettingsRecord, formatVersion) == 0, "formatVersion moved off byte 0");
    CS_ASSERT(offsetof(ComboSettingsRecord, direction) == 1, "direction moved off byte 1");
    CS_ASSERT(offsetof(ComboSettingsRecord, poolSizeOoT) == 2, "poolSizeOoT moved off byte 2");
    CS_ASSERT(offsetof(ComboSettingsRecord, poolSizeMM) == 3, "poolSizeMM moved off byte 3");
    CS_ASSERT(offsetof(ComboSettingsRecord, itemClassOoT) == 4, "itemClassOoT moved off byte 4");
    CS_ASSERT(offsetof(ComboSettingsRecord, itemClassMM) == 6, "itemClassMM moved off byte 6");
    CS_ASSERT(offsetof(ComboSettingsRecord, goal) == 8, "goal moved off byte 8");
    CS_ASSERT(offsetof(ComboSettingsRecord, logicRung) == 9, "logicRung moved off byte 9");
    CS_ASSERT(offsetof(ComboSettingsRecord, spare0) == 10, "spare0 moved off byte 10");
    CS_ASSERT(offsetof(ComboSettingsRecord, spare1) == 11, "spare1 moved off byte 11");

    // ---- The carve's position and the budget it leaves ---------------------
    CS_ASSERT(offsetof(ComboContext, comboSettingsHash) == 880u, "comboSettingsHash moved off .redsave offset 880");
    CS_ASSERT(offsetof(ComboContext, comboSettings) == 884u, "comboSettings moved off .redsave offset 884");
    CS_ASSERT(offsetof(ComboContext, reserved) == 896u, "reserved[] moved off .redsave offset 896");
    CS_ASSERT(sizeof(((ComboContext*)0)->reserved) == 108u,
              "reserved[] is not 108 bytes after the ADR 0011 carve — ADR 0009's budget table and context.h "
              "must reconcile");
    CS_ASSERT(sizeof(((ComboContext*)0)->reserved) >= 64u, "reserved[] fell below ADR 0009's 64-byte floor");
    CS_ASSERT(sizeof(ComboContext) <= RSBS_COMBO_CONTEXT_RECORD_SIZE, "ComboContext outgrew its Tier-1 budget");

    // ---- The pinned value spaces (decision 1.2.1) --------------------------
    // Runtime twins of the compile-time pinning asserts. A renumbering must be
    // impossible to land quietly, and CI logs are read more often than headers.
    CS_ASSERT(RSBS_COMBO_DIR_OFF == 1u && RSBS_COMBO_DIR_FORWARD == 2u && RSBS_COMBO_DIR_REVERSE == 3u &&
                  RSBS_COMBO_DIR_BOTH == 4u,
              "RSBS_COMBO_DIR_* renumbered — every already-written record now reads different rules");
    CS_ASSERT(RSBS_COMBO_DIR_OFF != 0u,
              "RSBS_COMBO_DIR_OFF must not be 0: a legacy record zero-extends, so OFF at zero would silently "
              "strip the crossings from every pre-carve paired save");
    CS_ASSERT(RSBS_COMBO_GOAL_BEAT_BOTH == 1u && RSBS_COMBO_GOAL_BEAT_EITHER == 2u &&
                  RSBS_COMBO_GOAL_TRIFORCE_HUNT == 3u,
              "RSBS_COMBO_GOAL_* renumbered");
    CS_ASSERT(RSBS_COMBO_RUNG_NONE == 1u && RSBS_COMBO_RUNG_BEATABLE == 2u && RSBS_COMBO_RUNG_ALL_REACHABLE == 3u,
              "RSBS_COMBO_RUNG_* renumbered");
    CS_ASSERT(RSBS_ITEMCLASS_PROGRESSION == 0x0001u && RSBS_ITEMCLASS_SONGS == 0x0002u &&
                  RSBS_ITEMCLASS_MASKS == 0x0004u && RSBS_ITEMCLASS_DUNGEON_ITEMS == 0x0008u &&
                  RSBS_ITEMCLASS_DUNGEON_REWARD == 0x0010u && RSBS_ITEMCLASS_SIDEQUEST == 0x0020u,
              "RSBS_ITEMCLASS_* bit positions moved — a bit may be appended, never re-pointed");

    // ---- The shipped defaults ---------------------------------------------
    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    CS_ASSERT(defaults.formatVersion == RSBS_COMBO_SETTINGS_FORMAT_VERSION, "defaults must be a FORMATTED record");
    CS_ASSERT(defaults.direction == RSBS_COMBO_DIR_BOTH,
              "the shipped default direction must be BOTH (accepted answer O2) — anything else silently changes "
              "every new world the moment the setting lands");
    CS_ASSERT(defaults.poolSizeOoT == RSBS_FOREIGN_PLACEMENT_CAP && defaults.poolSizeMM == RSBS_FOREIGN_PLACEMENT_CAP,
              "the shipped default pool sizes must be the cap — that is exactly what both passes place today");
    CS_ASSERT(defaults.itemClassOoT == RSBS_ITEMCLASS_ALL_V1 && defaults.itemClassMM == RSBS_ITEMCLASS_ALL_V1,
              "the shipped default item classes must be every allocated bit — today's pools are filtered by no "
              "class at all, so a narrower default silently narrows them at increment 3");
    CS_ASSERT(defaults.goal == RSBS_COMBO_GOAL_BEAT_BOTH, "the default GOAL must stay beat-both (ADR 0010 O11)");
    CS_ASSERT(defaults.logicRung == RSBS_COMBO_RUNG_BEATABLE,
              "the default rung must be the proved no-tricks rung, never base `none` (ADR 0010 O11)");
    CS_ASSERT(defaults.spare0 == 0 && defaults.spare1 == 0,
              "unallocated spares must read 0 in a formatVersion 1 record");

    // The resolver is what both the creation stamp and the arrival compare go
    // through; at increment 1 it must BE the defaults, or the two would already
    // disagree before any CVar exists.
    ComboSettingsRecord live;
    Combo_ResolveComboSettings(&live);
    CS_ASSERT(ComboSettingsRecordsEqual(defaults, live),
              "the session resolver and the shipped defaults disagree at increment 1 — every freshly created "
              "world would refuse itself at its first arrival");

    // ---- Predicates and the pool-size fallback -----------------------------
    ComboContext_Init();
    CS_ASSERT(!Combo_ComboSettingsFrozen(), "a freshly initialized context must read as NOT frozen");
    CS_ASSERT(Combo_ForeignPairingRequested(),
              "the pre-condition predicate must answer YES under the shipped defaults, or nothing generates");
    // The load-bearing fallback: an unfrozen (zero-extended) record must NOT
    // resolve to pool size 0, which would silently generate a paired world with
    // no crossings at all.
    CS_ASSERT(Combo_ComboPoolSizeFor((uint8_t)GAME_OOT) == (int)RSBS_FOREIGN_PLACEMENT_CAP,
              "an UNFROZEN record must fall back to the shipped pool size, not to zero");
    CS_ASSERT(Combo_ComboPoolSizeFor((uint8_t)GAME_MM) == (int)RSBS_FOREIGN_PLACEMENT_CAP,
              "an UNFROZEN record must fall back to the shipped pool size, not to zero");
    CS_ASSERT(Combo_ComboPoolSizeFor((uint8_t)GAME_NONE) == 0, "GAME_NONE has no pool");
    CS_ASSERT(Combo_ComboDirection() == RSBS_COMBO_DIR_BOTH, "an unfrozen direction resolves to the default");

    // ---- The direction GATE (ADR 0011 increment 4, #493) -------------------
    //
    // Combo_ComboDirectionArms is what each placement pass reads to decide
    // whether to run at all, and the ORIGIN argument is the pass's own origin,
    // not the host's: GAME_OOT is the FORWARD pass (OoT items into MM checks),
    // GAME_MM is the REVERSE pass (MM items into OoT checks). Getting that
    // backwards compiles, generates, and produces a world with exactly the
    // crossings the player did not ask for — so the mapping is pinned here,
    // value by value, rather than left to the two call sites to agree on.
    //
    // The unfrozen row first, because it is the one that must never narrow: a
    // legacy or pre-freeze world places what it places today.
    CS_ASSERT(Combo_ComboDirectionArms((uint8_t)GAME_OOT) && Combo_ComboDirectionArms((uint8_t)GAME_MM),
              "an UNFROZEN record must arm BOTH directions — the shipped default is what already ships (O2)");
    CS_ASSERT(!Combo_ComboDirectionArms((uint8_t)GAME_NONE), "GAME_NONE is not a crossing origin");
    {
        static const struct {
            uint8_t direction;
            bool armsForward;
            bool armsReverse;
        } kDirectionTruth[] = {
            { (uint8_t)RSBS_COMBO_DIR_OFF, false, false },    // a real, chooseable world: paired, zero crossings
            { (uint8_t)RSBS_COMBO_DIR_FORWARD, true, false }, // OoT-origin items into MM checks only
            { (uint8_t)RSBS_COMBO_DIR_REVERSE, false, true }, // MM-origin items into OoT checks only
            { (uint8_t)RSBS_COMBO_DIR_BOTH, true, true },     // today's shipped behaviour
        };
        for (int i = 0; i < (int)(sizeof(kDirectionTruth) / sizeof(kDirectionTruth[0])); i++) {
            ComboSettingsRecord dirRec = defaults;
            dirRec.direction = kDirectionTruth[i].direction;
            Combo_FreezeComboSettings(&dirRec);
            CS_ASSERT(Combo_ComboDirection() == kDirectionTruth[i].direction,
                      "the frozen record is the authority for a created world's direction");
            CS_ASSERT(Combo_ComboDirectionArms((uint8_t)GAME_OOT) == kDirectionTruth[i].armsForward,
                      "the FORWARD pass (GAME_OOT origin) is armed by the wrong direction values");
            CS_ASSERT(Combo_ComboDirectionArms((uint8_t)GAME_MM) == kDirectionTruth[i].armsReverse,
                      "the REVERSE pass (GAME_MM origin) is armed by the wrong direction values");
        }
        ComboContext_Init();
    }

    // A frozen record is the authority, and an over-cap value is CLAMPED rather
    // than honoured: a count that can exceed the table's capacity is a setting
    // that lies (accepted answer O4).
    {
        ComboSettingsRecord over = defaults;
        over.poolSizeOoT = 200u;
        over.poolSizeMM = 2u;
        Combo_FreezeComboSettings(&over);
        CS_ASSERT(Combo_ComboSettingsFrozen(), "freezing must set the occupancy tag");
        CS_ASSERT(Combo_ComboPoolSizeFor((uint8_t)GAME_OOT) == (int)RSBS_FOREIGN_PLACEMENT_CAP,
                  "an over-cap frozen pool size must clamp to RSBS_FOREIGN_PLACEMENT_CAP");
        CS_ASSERT(Combo_ComboPoolSizeFor((uint8_t)GAME_MM) == 2, "a frozen in-range pool size must be honoured");
    }

    // Freezing a record whose tag says ABSENT must still produce a FORMATTED
    // record — a caller must not be able to freeze something that reads unset.
    {
        ComboSettingsRecord untagged = defaults;
        untagged.formatVersion = 0u;
        Combo_FreezeComboSettings(&untagged);
        CS_ASSERT(gComboCtx.comboSettings.formatVersion == RSBS_COMBO_SETTINGS_FORMAT_VERSION,
                  "freezing must force the occupancy tag; a record that reads absent is not frozen");
        CS_ASSERT(gComboCtx.comboSettingsHash != 0, "a frozen record must carry a nonzero fingerprint");
    }

    // ---- Byte-exact .redsave round trip ------------------------------------
    {
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        mgr.SetSaveDirectory(kComboSettingsTestDir);
        mgr.DeleteSave(0);

        ComboContext_Init();
        ComboSettingsSeedShadows(0x3Cu);
        ComboSettingsArmPairing(0x51EED000u, 0x5E77A11Fu, 0x11FEDCBAu);
        // The HEALTHY case: a record frozen from the same resolver the load
        // re-runs. A file this build wrote must be a file this build loads —
        // the identity check must not refuse its own output.
        ComboSettingsRecord written;
        Combo_ResolveComboSettings(&written);
        const uint32_t writtenHash = Combo_FreezeComboSettings(&written);

        CS_ASSERT(mgr.Save(0), "Save(0) failed");

        // Scribble both fields so a pass cannot come from leftovers.
        memset(&gComboCtx.comboSettings, 0x5A, sizeof(gComboCtx.comboSettings));
        gComboCtx.comboSettingsHash = 0x5A5A5A5Au;

        CS_ASSERT(mgr.Load(0),
                  "Load refused a .redsave carrying a healthy combo record — the ADR 0011 identity check must "
                  "pass a file it just wrote");
        CS_ASSERT(ComboSettingsRecordsEqual(gComboCtx.comboSettings, written),
                  "the frozen combo record did not survive a .redsave round trip byte-exact");
        CS_ASSERT(gComboCtx.comboSettingsHash == writtenHash,
                  "comboSettingsHash did not survive a .redsave round trip");
        mgr.DeleteSave(0);
    }

    // ---- The LOAD-side refusal (decision 4: "compare at every arrival AND
    // load"). A slot whose stored rules are not this session's must refuse
    // through the #533/#568 surface rather than commit its bytes over the
    // resident context — the check runs on the record just READ, precisely
    // because a check that read gComboCtx would be checking the world the load
    // is about to replace.
    {
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        mgr.SetSaveDirectory(kComboSettingsTestDir);
        mgr.DeleteSave(0);

        ComboContext_Init();
        ComboSettingsSeedShadows(0x4Du);
        ComboSettingsArmPairing(0x51EED001u, 0x5E77A120u, 0x11FEDCBBu);
        ComboSettingsRecord divergent;
        Combo_ResolveComboSettings(&divergent);
        divergent.direction = (uint8_t)RSBS_COMBO_DIR_FORWARD; // this session resolves BOTH
        Combo_FreezeComboSettings(&divergent);
        CS_ASSERT(mgr.Save(0), "Save(0) failed for the divergent-record leg");

        // Fresh session state, so the refusal observed is THIS load's.
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        CS_ASSERT(!mgr.Load(0),
                  "a .redsave whose frozen combo rules diverge from this session was COMMITTED — divergence is "
                  "corruption to refuse, never a choice to honour");
        CS_ASSERT(RsbsSave_GetSlotRefuseReason(0) == (int)RSBS_REFUSE_IDENTITY,
                  "the load-side combo divergence must refuse as RSBS_REFUSE_IDENTITY");
        CS_ASSERT(RsbsSave_IsSlotWritable(0) == 0, "a refused load must latch the slot against writes (#533)");
        CS_ASSERT(!Combo_ComboSettingsFrozen(),
                  "a refused load must not have committed the divergent record over the resident context");

        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
    }

    // ---- Zero-extension: a pre-carve record reads as ABSENT ----------------
    // The growth contract made concrete for THIS carve. Simulated the way the
    // loader produces it — a short Tier-1 leaves the tail zero — by clearing
    // the two fields and re-reading the predicates.
    {
        ComboContext_Init();
        memset(&gComboCtx.comboSettings, 0, sizeof(gComboCtx.comboSettings));
        gComboCtx.comboSettingsHash = 0;
        CS_ASSERT(!Combo_ComboSettingsFrozen(), "an all-zero record must read as ABSENT, never as a formatted one");
        CS_ASSERT(Combo_ComboSettingsDivergence() == 0,
                  "an ABSENT record must be EXEMPT from comparison — refusing it would orphan every already-written "
                  "paired .redsave to detect a divergence that cannot have happened");
        CS_ASSERT(gComboCtx.comboSettings.direction == 0 && gComboCtx.comboSettings.direction != RSBS_COMBO_DIR_OFF,
                  "zero must not be a legal direction enumerator");
    }

    ComboContext_Init();
    printf("[TEST] PASS: the combo record is format (12 B at 884, digest at 880, reserved[108]) and its defaults "
           "reproduce today's world\n");
    return TEST_PASS;
}

// ============================================================================
// combo-settings-canonical — the golden vector (decision 1.4)
// ============================================================================

TestResult Test_ComboSettingsCanonical(void) {
    printf("[TEST] combo-settings-canonical: canonical() and the whole-pair fingerprint are pinned byte-for-byte "
           "(ADR 0011 decision 1.4)\n");

    // --- The golden record -> golden bytes ---------------------------------
    // Every uint16 is byte-swapped-visible (0x1234 -> 34 12), so an
    // endianness slip or a "just memcpy the struct" refactor cannot pass.
    const ComboSettingsRecord golden = ComboSettingsGoldenRecord();
    uint8_t canon[RSBS_COMBO_SETTINGS_CANONICAL_LEN];
    memset(canon, 0xEE, sizeof(canon));
    Combo_ComboSettingsCanonical(&golden, canon);

    static const uint8_t kCanonGold[RSBS_COMBO_SETTINGS_CANONICAL_LEN] = {
        0x01,       // formatVersion
        0x02,       // direction == RSBS_COMBO_DIR_FORWARD
        0x03,       // poolSizeOoT
        0x05,       // poolSizeMM
        0x34, 0x12, // itemClassOoT LE
        0xCD, 0xAB, // itemClassMM  LE
        0x03,       // goal == RSBS_COMBO_GOAL_TRIFORCE_HUNT
        0x03,       // logicRung == RSBS_COMBO_RUNG_ALL_REACHABLE
        0x00,       // spare0
        0x00,       // spare1
    };
    CS_ASSERT(memcmp(canon, kCanonGold, sizeof(kCanonGold)) == 0,
              "canonical() no longer emits the pinned byte string — the digest input is .redsave-adjacent format "
              "and a layout/endianness change here silently re-identifies every world");
    CS_ASSERT(RSBS_COMBO_SETTINGS_CANONICAL_LEN == sizeof(ComboSettingsRecord),
              "the canonical encoding must cover the whole record");

    // --- The golden digest --------------------------------------------------
    // Hash( canonical || ':' || LE32(settingsHash) || ':' || LE32(mmProfileDigest) )
    // over FNV-1a 32, computed independently of this build.
    static const uint32_t kSettingsHash = 0xDEADBEEFu;
    static const uint32_t kProfileDigest = 0x0BADF00Du;
    static const uint32_t kGoldenFingerprint = 0xDE17C51Du;
    const uint32_t fingerprint = Combo_ComputeComboSettingsHash(&golden, kSettingsHash, kProfileDigest);
    CS_ASSERT(fingerprint == kGoldenFingerprint,
              "the whole-pair fingerprint moved off its golden vector — this is the lock #574's identity handshake "
              "inherits, so a drift here is a cross-peer disagreement, not a local detail");

    // The SHIPPED DEFAULTS get their own vector: they are what every legacy
    // pair and every increment-1 world freezes, so a default change must be a
    // deliberate red test rather than an invisible identity shift.
    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    Combo_ComboSettingsCanonical(&defaults, canon);
    static const uint8_t kDefaultsCanonGold[RSBS_COMBO_SETTINGS_CANONICAL_LEN] = {
        0x01, 0x04, 0x08, 0x08, 0x3F, 0x00, 0x3F, 0x00, 0x01, 0x02, 0x00, 0x00,
    };
    CS_ASSERT(memcmp(canon, kDefaultsCanonGold, sizeof(kDefaultsCanonGold)) == 0,
              "the shipped defaults' canonical bytes changed — every new world's identity just moved");
    CS_ASSERT(Combo_ComputeComboSettingsHash(&defaults, kSettingsHash, kProfileDigest) == 0xBA5FC364u,
              "the shipped defaults' fingerprint changed");

    // --- Construction order is irrelevant; VALUES are the identity ----------
    // The property the byte-at-a-time encoder exists to guarantee: the digest
    // is a function of the values, not of how the struct got them.
    {
        ComboSettingsRecord other;
        memset(&other, 0xFF, sizeof(other)); // deliberately dirty before assignment
        other.spare1 = 0u;
        other.spare0 = 0u;
        other.logicRung = (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE;
        other.goal = (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT;
        other.itemClassMM = 0xABCDu;
        other.itemClassOoT = 0x1234u;
        other.poolSizeMM = 5u;
        other.poolSizeOoT = 3u;
        other.direction = (uint8_t)RSBS_COMBO_DIR_FORWARD;
        other.formatVersion = 1u;
        CS_ASSERT(Combo_ComputeComboSettingsHash(&other, kSettingsHash, kProfileDigest) == kGoldenFingerprint,
                  "the same values assembled in a different order produced a different digest");
    }

    // --- Every field moves the digest ---------------------------------------
    // Without this a term could be silently dropped from the encoding and the
    // golden vector alone would not notice (it would just be a different
    // constant on the day it was written).
    {
        const char* names[10] = { "formatVersion", "direction", "poolSizeOoT", "poolSizeMM", "itemClassOoT",
                                  "itemClassMM",   "goal",      "logicRung",   "spare0",     "spare1" };
        for (int i = 0; i < 10; i++) {
            ComboSettingsRecord moved = golden;
            switch (i) {
                case 0: moved.formatVersion = 2u; break;
                case 1: moved.direction = (uint8_t)RSBS_COMBO_DIR_BOTH; break;
                case 2: moved.poolSizeOoT = 7u; break;
                case 3: moved.poolSizeMM = 7u; break;
                case 4: moved.itemClassOoT = 0x4321u; break;
                case 5: moved.itemClassMM = 0xDCBAu; break;
                case 6: moved.goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER; break;
                case 7: moved.logicRung = (uint8_t)RSBS_COMBO_RUNG_NONE; break;
                case 8: moved.spare0 = 0x77u; break;
                default: moved.spare1 = 0x77u; break;
            }
            if (Combo_ComputeComboSettingsHash(&moved, kSettingsHash, kProfileDigest) == kGoldenFingerprint) {
                printf("[TEST] FAIL: changing '%s' did not change the fingerprint — that field is not in the "
                       "canonical encoding\n",
                       names[i]);
                return TEST_FAIL;
            }
        }
    }

    // --- The WHOLE-PAIR fold (accepted answer O6) ---------------------------
    // A digest narrower than the generator's input set is vacuous: same seed,
    // same digest, different world. Both half-digests must move it.
    CS_ASSERT(Combo_ComputeComboSettingsHash(&golden, kSettingsHash + 1u, kProfileDigest) != kGoldenFingerprint,
              "sharedRandoSettingsHash is not folded into comboSettingsHash — the fingerprint is vacuous");
    CS_ASSERT(Combo_ComputeComboSettingsHash(&golden, kSettingsHash, kProfileDigest + 1u) != kGoldenFingerprint,
              "mmProfileDigest is not folded into comboSettingsHash — the fingerprint is vacuous");
    // And the two terms must not be interchangeable: a fold that concatenated
    // them without separation would let a swap cancel out.
    CS_ASSERT(Combo_ComputeComboSettingsHash(&golden, kProfileDigest, kSettingsHash) != kGoldenFingerprint,
              "swapping the two half-digests produced the same fingerprint — they are not positionally distinct");

    // --- Zero displaces ------------------------------------------------------
    // Not a search for a preimage (that would be a slow test for no benefit):
    // assert the invariant the displacement exists to hold — the computation
    // never returns 0 for anything, including an all-zero record.
    {
        ComboSettingsRecord empty;
        memset(&empty, 0, sizeof(empty));
        CS_ASSERT(Combo_ComputeComboSettingsHash(&empty, 0, 0) != 0,
                  "the fingerprint must never be 0: zero is this field's 'not frozen', so a real identity hashing "
                  "to 0 becomes an undetectable mismatch");
        CS_ASSERT(Combo_ComputeComboSettingsHash(NULL, 0, 0) ==
                      Combo_ComputeComboSettingsHash(&empty, 0, 0),
                  "a NULL record must encode as an all-zero record");
    }

    printf("[TEST] PASS: canonical() emits its pinned bytes and the fingerprint folds the whole pair\n");
    return TEST_PASS;
}

// ============================================================================
// combo-settings-divergence — the field-level diff behind the named refusal
// ============================================================================

TestResult Test_ComboSettingsDivergence(void) {
    printf("[TEST] combo-settings-divergence: the diff names WHICH rule diverged (ADR 0011 decision 1.1 "
           "justification 2)\n");

    ComboSettingsRecord live;
    Combo_ResolveComboSettings(&live);

    // ---- NON-VACUITY, first, because everything below depends on it --------
    // The MM arrival gate's refusal condition is literally
    // `Combo_ComboSettingsDivergence() != 0`, so this leg is what proves the
    // gate does NOT fire for a healthy pair. A diff that refused everything
    // would pass every divergence leg below and prove nothing.
    {
        ComboContext_Init();
        ComboSettingsArmPairing(0xC0FFEE01u, 0x1111AAAAu, 0x2222BBBBu);
        ComboSettingsRecord healthy;
        Combo_ResolveComboSettings(&healthy);
        Combo_FreezeComboSettings(&healthy);
        CS_ASSERT(Combo_ComboSettingsDivergence() == 0,
                  "a healthy pair — frozen from the same resolver the arrival re-runs — reported divergence; the "
                  "arrival gate would refuse every well-formed world");
    }

    // ---- Each field alone names exactly itself -----------------------------
    struct Leg {
        uint32_t bit;
        const char* name;
    };
    static const Leg kLegs[] = {
        { RSBS_COMBO_DIVERGE_DIRECTION, "direction" },
        { RSBS_COMBO_DIVERGE_POOL_SIZE_OOT, "poolSizeOoT" },
        { RSBS_COMBO_DIVERGE_POOL_SIZE_MM, "poolSizeMM" },
        { RSBS_COMBO_DIVERGE_ITEM_CLASS_OOT, "itemClassOoT" },
        { RSBS_COMBO_DIVERGE_ITEM_CLASS_MM, "itemClassMM" },
        { RSBS_COMBO_DIVERGE_GOAL, "goal" },
        { RSBS_COMBO_DIVERGE_LOGIC_RUNG, "logicRung" },
        { RSBS_COMBO_DIVERGE_SPARE0, "spare0" },
        { RSBS_COMBO_DIVERGE_SPARE1, "spare1" },
    };

    for (size_t i = 0; i < sizeof(kLegs) / sizeof(kLegs[0]); i++) {
        ComboSettingsRecord frozen = live;
        switch (kLegs[i].bit) {
            case RSBS_COMBO_DIVERGE_DIRECTION: frozen.direction = (uint8_t)RSBS_COMBO_DIR_FORWARD; break;
            case RSBS_COMBO_DIVERGE_POOL_SIZE_OOT: frozen.poolSizeOoT = 1u; break;
            case RSBS_COMBO_DIVERGE_POOL_SIZE_MM: frozen.poolSizeMM = 1u; break;
            case RSBS_COMBO_DIVERGE_ITEM_CLASS_OOT: frozen.itemClassOoT = RSBS_ITEMCLASS_SONGS; break;
            case RSBS_COMBO_DIVERGE_ITEM_CLASS_MM: frozen.itemClassMM = RSBS_ITEMCLASS_MASKS; break;
            case RSBS_COMBO_DIVERGE_GOAL: frozen.goal = (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT; break;
            case RSBS_COMBO_DIVERGE_LOGIC_RUNG: frozen.logicRung = (uint8_t)RSBS_COMBO_RUNG_NONE; break;
            case RSBS_COMBO_DIVERGE_SPARE0: frozen.spare0 = 1u; break;
            default: frozen.spare1 = 1u; break;
        }

        const uint32_t bits = Combo_ComboSettingsDivergenceBetween(&frozen, &live);
        if (bits != kLegs[i].bit) {
            printf("[TEST] FAIL: diverging '%s' alone reported bits %04X, expected %04X — a refusal that names the "
                   "wrong rule is worse than one that names none\n",
                   kLegs[i].name, (unsigned)bits, (unsigned)kLegs[i].bit);
            return TEST_FAIL;
        }
        if (strcmp(Combo_ComboSettingsDivergenceFieldName(bits), kLegs[i].name) != 0) {
            printf("[TEST] FAIL: bit %04X names '%s', expected '%s'\n", (unsigned)bits,
                   Combo_ComboSettingsDivergenceFieldName(bits), kLegs[i].name);
            return TEST_FAIL;
        }

        char text[192];
        const int named = Combo_ComboSettingsDivergenceDescribe(bits, text, sizeof(text));
        if (named != 1 || strcmp(text, kLegs[i].name) != 0) {
            printf("[TEST] FAIL: describe(%04X) rendered '%s' (%d fields), expected '%s'\n", (unsigned)bits, text,
                   named, kLegs[i].name);
            return TEST_FAIL;
        }
    }

    // ---- Several at once render as a list ----------------------------------
    {
        ComboSettingsRecord frozen = live;
        frozen.direction = (uint8_t)RSBS_COMBO_DIR_REVERSE;
        frozen.goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER;
        const uint32_t bits = Combo_ComboSettingsDivergenceBetween(&frozen, &live);
        CS_ASSERT(bits == (RSBS_COMBO_DIVERGE_DIRECTION | RSBS_COMBO_DIVERGE_GOAL), "two diverged fields, two bits");
        char text[192];
        CS_ASSERT(Combo_ComboSettingsDivergenceDescribe(bits, text, sizeof(text)) == 2, "two fields named");
        CS_ASSERT(strcmp(text, "direction, goal") == 0, "the field list is not rendered as a readable list");
        // Truncation must not run off the end — the refusal message is built
        // into a fixed buffer on a boot path.
        char tiny[6];
        Combo_ComboSettingsDivergenceDescribe(bits, tiny, sizeof(tiny));
        CS_ASSERT(tiny[sizeof(tiny) - 1] == '\0', "describe() must always NUL-terminate");
        CS_ASSERT(Combo_ComboSettingsDivergenceDescribe(0, text, sizeof(text)) == 0 && strcmp(text, "(none)") == 0,
                  "no divergence must render as '(none)', never as an empty string that truncates the sentence");
    }

    // ---- An ABSENT record is exempt; a FUTURE record is unreadable ---------
    {
        ComboSettingsRecord absent;
        memset(&absent, 0, sizeof(absent));
        CS_ASSERT(Combo_ComboSettingsDivergenceBetween(&absent, &live) == 0,
                  "an absent record must be exempt from comparison (decision 4.2), not divergent");

        ComboSettingsRecord future = live;
        future.formatVersion = (uint8_t)(RSBS_COMBO_SETTINGS_FORMAT_VERSION + 1u);
        const uint32_t bits = Combo_ComboSettingsDivergenceBetween(&future, &live);
        CS_ASSERT(bits == RSBS_COMBO_DIVERGE_UNREADABLE,
                  "a record from a NEWER build must refuse as UNREADABLE rather than being compared field by "
                  "field against rules this build cannot know are authoritative");
        CS_ASSERT(strcmp(Combo_ComboSettingsDivergenceFieldName(bits), "formatVersion") == 0,
                  "the unreadable refusal must name formatVersion");
    }

    // ---- A record and a fingerprint that disagree --------------------------
    // The cross-check that has teeth TODAY, before increment 2 gives the
    // resolver any CVars to disagree with: the record, both half-digests and
    // the hash all ride one Tier-1 write, so a mismatch between them is not a
    // settings change — it is damage.
    {
        ComboContext_Init();
        ComboSettingsArmPairing(0xC0FFEE02u, 0x3333CCCCu, 0x4444DDDDu);
        ComboSettingsRecord healthy;
        Combo_ResolveComboSettings(&healthy);
        Combo_FreezeComboSettings(&healthy);
        CS_ASSERT(Combo_ComboSettingsDivergence() == 0, "control: a freshly frozen identity is self-consistent");

        gComboCtx.comboSettingsHash ^= 0x00000001u;
        const uint32_t bits = Combo_ComboSettingsDivergence();
        CS_ASSERT(bits == RSBS_COMBO_DIVERGE_FINGERPRINT, "a corrupted fingerprint must be reported as such");
        CS_ASSERT(strcmp(Combo_ComboSettingsDivergenceFieldName(bits), "comboSettingsHash") == 0,
                  "the fingerprint refusal must name comboSettingsHash");

        // Moving a half-digest under a frozen record moves the fingerprint too:
        // this is the fold made observable at session level.
        Combo_FreezeComboSettings(&healthy);
        CS_ASSERT(Combo_ComboSettingsDivergence() == 0, "re-freezing restores self-consistency");
        gComboCtx.mmProfileDigest ^= 0x00000001u;
        CS_ASSERT((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) != 0,
                  "moving mmProfileDigest under a frozen record must break the fingerprint — otherwise the "
                  "whole-pair fold is not actually being checked");
    }

    ComboContext_Init();
    printf("[TEST] PASS: the divergence diff names the rule, exempts an absent record, and refuses an unreadable "
           "one\n");
    return TEST_PASS;
}

// ============================================================================
// combo-settings-legacy-freeze — the O5 transitional writer (decision 4.4)
// ============================================================================

TestResult Test_ComboSettingsLegacyFreeze(void) {
    printf("[TEST] combo-settings-legacy-freeze: a legacy paired file freezes the shipped defaults at its first "
           "crossing and COMPARES thereafter (accepted answer O5)\n");

    // ---- An UNPAIRED session freezes nothing -------------------------------
    // An unpaired file must not acquire a combo identity: there is no world for
    // those rules to describe, and Combo_ComboSettingsFrozen gates the (future)
    // authoring surface.
    {
        ComboContext_Init();
        CS_ASSERT(Combo_FreezeLegacyComboSettings() == 0, "an unpaired session must not freeze combo rules");
        CS_ASSERT(!Combo_ComboSettingsFrozen(), "an unpaired session must leave the record absent");
        CS_ASSERT(gComboCtx.comboSettingsHash == 0, "an unpaired session must leave the fingerprint at 0");
    }

    // ---- A LEGACY paired file freezes the shipped defaults -----------------
    {
        ComboContext_Init();
        ComboSettingsArmPairing(0x1EAC0000u, 0x7E57A11Fu, 0x9911FFEEu);
        CS_ASSERT(Combo_ForeignPairingActive(), "the leg needs a live pairing");
        CS_ASSERT(!Combo_ComboSettingsFrozen(), "the leg starts from a pre-carve record");

        // The pane's read surface (ADR 0004 §6 state 4): before the freeze a
        // paired world reports NOT frozen, and afterwards the values it shows
        // come FROM THE SAVE. This is the requirement a digest could never have
        // satisfied, and the reason twelve bytes were carved.
        ComboSettingsSummary before;
        Combo_ComboSettingsSummary(&before);
        CS_ASSERT(before.paired && !before.frozen, "a legacy pair must present as paired-but-not-frozen");

        CS_ASSERT(Combo_FreezeLegacyComboSettings() == 1, "the first crossing must freeze a legacy pair");
        CS_ASSERT(Combo_ComboSettingsFrozen(), "the record must read as frozen after the transitional write");
        CS_ASSERT(gComboCtx.comboSettings.formatVersion == RSBS_COMBO_SETTINGS_FORMAT_VERSION,
                  "the transitional write must stamp the current format version");

        ComboSettingsRecord defaults;
        Combo_ComboSettingsDefaults(&defaults);
        CS_ASSERT(ComboSettingsRecordsEqual(gComboCtx.comboSettings, defaults),
                  "the transitional write must freeze the SHIPPED DEFAULTS — a legacy pair was generated when "
                  "there was only ever one rule set, and that set is what the defaults record");
        CS_ASSERT(gComboCtx.comboSettingsHash ==
                      Combo_ComputeComboSettingsHash(&defaults, gComboCtx.sharedRandoSettingsHash,
                                                     gComboCtx.mmProfileDigest),
                  "the transitional write must recompute the fingerprint over BOTH half-digests (decision 4.1's "
                  "order), or the next arrival refuses a healthy pair");
        CS_ASSERT(gComboCtx.comboSettingsHash != 0, "the transitional write must leave a nonzero fingerprint");

        ComboSettingsSummary after;
        Combo_ComboSettingsSummary(&after);
        CS_ASSERT(after.paired && after.frozen, "the summary must report the record as frozen after the write");
        CS_ASSERT(ComboSettingsRecordsEqual(after.record, defaults) &&
                      after.comboSettingsHash == gComboCtx.comboSettingsHash,
                  "the summary must serve the values FROM THE SAVE (ADR 0004 §6 state 4)");

        // ---- A SECOND crossing COMPARES rather than re-freezing ------------
        // The property that makes 4.4 a behaviour and not a promise: a
        // transitional writer that overwrote on every crossing would be a
        // self-heal, and every arrival-time divergence would vanish the instant
        // it was detected.
        ComboSettingsRecord tampered = defaults;
        tampered.direction = (uint8_t)RSBS_COMBO_DIR_REVERSE;
        Combo_FreezeComboSettings(&tampered);
        const uint32_t tamperedHash = gComboCtx.comboSettingsHash;

        CS_ASSERT(Combo_FreezeLegacyComboSettings() == 0, "a second crossing must not re-freeze");
        CS_ASSERT(gComboCtx.comboSettings.direction == RSBS_COMBO_DIR_REVERSE,
                  "a second crossing SELF-HEALED the frozen record — divergence is corruption to refuse, never a "
                  "value to overwrite");
        CS_ASSERT(gComboCtx.comboSettingsHash == tamperedHash, "a second crossing must not restamp the fingerprint");
        CS_ASSERT((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_DIRECTION) != 0,
                  "the divergent record must now be REPORTED as divergent, which is what 'compares normally "
                  "thereafter' means");
    }

    // ---- Half-digest re-stamp restores the ordering invariant --------------
    // A doubly-legacy pair (no combo record AND no MM profile digest) freezes
    // its combo rules while mmProfileDigest is still 0; ResolvePairedProfile
    // re-stamps the fingerprint after it freezes the profile. This is the
    // src/common half of that contract.
    {
        ComboContext_Init();
        ComboSettingsArmPairing(0xD00Du, 0x0BADCAFEu, /*profileDigest=*/0u);
        CS_ASSERT(Combo_FreezeLegacyComboSettings() == 1, "a doubly-legacy pair still freezes its combo rules");
        const uint32_t beforeProfile = gComboCtx.comboSettingsHash;
        CS_ASSERT(Combo_ComboSettingsDivergence() == 0, "self-consistent while the profile digest is still 0");

        gComboCtx.mmProfileDigest = 0xFEEDFACEu; // what ResolvePairedProfile's legacy branch stamps
        CS_ASSERT((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) != 0,
                  "without a re-stamp the fingerprint must go stale — this is WHY the re-stamp exists");
        const uint32_t after = Combo_StampComboSettingsHash();
        CS_ASSERT(after != 0 && after != beforeProfile, "the re-stamp must fold the freshly frozen profile digest");
        CS_ASSERT(Combo_ComboSettingsDivergence() == 0, "the re-stamp must restore self-consistency");
    }

    ComboContext_Init();
    printf("[TEST] PASS: the legacy transitional writer freezes once, compares thereafter, and never self-heals\n");
    return TEST_PASS;
}
