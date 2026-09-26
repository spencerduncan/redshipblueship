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
 *   combo-settings-authoring (ADR 0011 increment 2) — the five tier-4 keys.
 *   A RunHeadless bridge rather than a TestResult body, because the keys live
 *   in the CVar store on the Ship::Context singleton: with nothing authored
 *   the resolver still produces the shipped record and its pinned fingerprint
 *   byte for byte; authored values reach the record BEFORE the freeze and move
 *   the fingerprint; the writers refuse once frozen (the gate is on the
 *   writers, not the widget); the pane's summary serves the save and not the
 *   CVar; an out-of-space store value resolves to the shipped default and
 *   never to a new enumerator; and a session divergence is distinguishable
 *   from damage (Combo_ComboSettingsDivergenceIsDamage).
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as
 * C++, like test_save_roundtrip.c) for the rsbs::SaveManager half; every
 * symbol under test is C-linkage.
 */

#include "../combo_settings_view.h"
#include "../context.h"
#include "../cvar_shared_keys.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#define CS_ASSERT(cond, msg)                    \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

namespace {

const char* const kComboSettingsTestDir = "rsbs_test_combo_settings";
// Its own directory for the load-side leg, because RsbsSave_HasQuarantine
// reports ANY slotN.redsave*.bak beside the slot and an earlier run's
// evidence would confound "nothing was quarantined by THIS load".
const char* const kComboSettingsLoadTestDir = "rsbs_test_combo_settings_load";

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
    // comboFlags stays CLEAR in the golden record on purpose (#668): the
    // pinned bytes and the pinned fingerprint below are the ones this project
    // has shipped since increment 1, and spending byte 10 must not move them.
    // The flag's own vector is a separate, additional constant.
    r.comboFlags = 0u;
    r.spare1 = 0u;
    return r;
}

bool ComboSettingsRecordsEqual(const ComboSettingsRecord& a, const ComboSettingsRecord& b) {
    return a.formatVersion == b.formatVersion && a.direction == b.direction && a.poolSizeOoT == b.poolSizeOoT &&
           a.poolSizeMM == b.poolSizeMM && a.itemClassOoT == b.itemClassOoT && a.itemClassMM == b.itemClassMM &&
           a.goal == b.goal && a.logicRung == b.logicRung && a.comboFlags == b.comboFlags && a.spare1 == b.spare1;
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
    CS_ASSERT(offsetof(ComboSettingsRecord, comboFlags) == 10,
              "comboFlags moved off byte 10 (the byte formerly named spare0, spent by #668)");
    CS_ASSERT(offsetof(ComboSettingsRecord, spare1) == 11, "spare1 moved off byte 11");

    // ---- The carve's position and the budget it leaves ---------------------
    CS_ASSERT(offsetof(ComboContext, comboSettingsHash) == 880u, "comboSettingsHash moved off .redsave offset 880");
    CS_ASSERT(offsetof(ComboContext, comboSettings) == 884u, "comboSettings moved off .redsave offset 884");
    // ADR 0010 answer O10 carved the 4-byte triforce record from the front of
    // the 108 this carve left (ADR 0011's 2026-09-27 amendment).
    CS_ASSERT(offsetof(ComboContext, comboTriforce) == 896u, "comboTriforce moved off .redsave offset 896");
    CS_ASSERT(offsetof(ComboContext, reserved) == 900u, "reserved[] moved off .redsave offset 900");
    CS_ASSERT(sizeof(((ComboContext*)0)->reserved) == 104u,
              "reserved[] is not 104 bytes after the ADR 0011 and O10 carves — ADR 0009's budget table and "
              "context.h must reconcile");
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
    CS_ASSERT(RSBS_COMBO_FLAG_SHARED_OCARINA == 0x01u && RSBS_COMBO_FLAGS_ALL_V1 == 0x01u,
              "RSBS_COMBO_FLAG_* bit positions moved — comboFlags is .redsave format, append-only (#668)");
    CS_ASSERT(RSBS_COMBO_SETTINGS_FORMAT_VERSION == 1u,
              "the record format version moved. Spending a spare does NOT earn a bump — #668 spent byte 10 with "
              "a bitset whose CLEAR state is the pre-#668 behaviour, so 'absent' and 'zero' are the same world. "
              "A bump is canonical()[0] and moves EVERY world's fingerprint; only spend it for a field whose "
              "zero would change an existing world");

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
    CS_ASSERT(defaults.comboFlags == 0,
              "the shipped default must have every comboFlags bit CLEAR (#668): each one changes a world, and "
              "'reproduce today's world exactly' binds every default in this file");
    CS_ASSERT(defaults.spare1 == 0, "unallocated spares must read 0 in a formatVersion 1 record");

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
    CS_ASSERT(Combo_ForeignCrossingsRequested(),
              "the shipped default direction is BOTH, so crossings must be requested under the defaults (#667)");
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
    printf("[TEST] PASS: the combo record is format (12 B at 884, digest at 880, reserved[104]) and its defaults "
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

    // --- The comboFlags vector: SET moves it, CLEAR does not (#668) ---------
    // Both halves matter and they are different claims.
    //
    // CLEAR is the DIGEST GUARANTEE: byte 10 was spent without a formatVersion
    // bump precisely so that a world nobody changed keeps the identity it
    // always had, and the two constants just above are the evidence. The two
    // vectors above already assert it (both records carry comboFlags == 0), so
    // this leg restates it explicitly against a record built from the defaults
    // with the flag left alone — the shape the resolver produces for every
    // player who never touches the row.
    //
    // SET is the OTHER half: a bit that did not reach canonical() would be a
    // world rule outside the world's identity, which is exactly the vacuity ADR
    // 0009 decision 1 refuses. The byte moves, and it moves at position 10.
    {
        ComboSettingsRecord flagged = defaults;
        flagged.comboFlags = (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA;
        static const uint8_t kSharedOcarinaCanonGold[RSBS_COMBO_SETTINGS_CANONICAL_LEN] = {
            0x01, 0x04, 0x08, 0x08, 0x3F, 0x00, 0x3F, 0x00, 0x01, 0x02, 0x01, 0x00,
        };
        uint8_t flaggedCanon[RSBS_COMBO_SETTINGS_CANONICAL_LEN];
        Combo_ComboSettingsCanonical(&flagged, flaggedCanon);
        CS_ASSERT(memcmp(flaggedCanon, kSharedOcarinaCanonGold, sizeof(kSharedOcarinaCanonGold)) == 0,
                  "the shared-ocarina record's canonical bytes moved off their vector — the flag rides byte 10, "
                  "the byte formerly named spare0, and nothing else may shift with it");
        CS_ASSERT(memcmp(flaggedCanon, kDefaultsCanonGold, 10) == 0 && flaggedCanon[11] == kDefaultsCanonGold[11],
                  "setting the shared-ocarina flag changed a byte OTHER than byte 10");
        CS_ASSERT(Combo_ComputeComboSettingsHash(&flagged, kSettingsHash, kProfileDigest) == 0x0B1A2145u,
                  "the shared-ocarina world's fingerprint moved off its vector");
        CS_ASSERT(Combo_ComputeComboSettingsHash(&flagged, kSettingsHash, kProfileDigest) !=
                      Combo_ComputeComboSettingsHash(&defaults, kSettingsHash, kProfileDigest),
                  "turning the shared ocarina ON did not change comboSettingsHash — a world rule outside the "
                  "world's identity is the vacuity the record exists to prevent");

        ComboSettingsRecord cleared = flagged;
        cleared.comboFlags = 0u;
        CS_ASSERT(Combo_ComputeComboSettingsHash(&cleared, kSettingsHash, kProfileDigest) == 0xBA5FC364u,
                  "clearing the shared-ocarina flag did not return the fingerprint to the shipped defaults' — "
                  "the off state must be byte-identical to a world written before the flag existed");
    }

    // --- Construction order is irrelevant; VALUES are the identity ----------
    // The property the byte-at-a-time encoder exists to guarantee: the digest
    // is a function of the values, not of how the struct got them.
    {
        ComboSettingsRecord other;
        memset(&other, 0xFF, sizeof(other)); // deliberately dirty before assignment
        other.spare1 = 0u;
        other.comboFlags = 0u;
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
                                  "itemClassMM",   "goal",      "logicRung",   "comboFlags", "spare1" };
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
                case 8: moved.comboFlags = 0x77u; break;
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
        // comboFlags diffs PER RULE (#668): an ALLOCATED bit names the rule it
        // carries, and only an UNALLOCATED one falls back to the byte's own
        // name. Decision 1.1's justification for storing twelve bytes rather
        // than a digest is that the refusal names WHICH RULE moved, and
        // "comboFlags" is a field name, not a rule.
        { RSBS_COMBO_DIVERGE_SHARED_OCARINA, "sharedOcarina" },
        { RSBS_COMBO_DIVERGE_COMBO_FLAGS, "comboFlags" },
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
            case RSBS_COMBO_DIVERGE_SHARED_OCARINA:
                frozen.comboFlags = (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA;
                break;
            // 0x80 is UNALLOCATED at formatVersion 1: the byte-level bit is the
            // honest report for a state a version-1 record may not be in at all.
            case RSBS_COMBO_DIVERGE_COMBO_FLAGS: frozen.comboFlags = 0x80u; break;
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

    // ---- The shared ocarina diverges in BOTH directions (#668) -------------
    // The loop above sets the bit in the frozen record; this is the other half,
    // and it is not the same claim. comboFlags is a BITSET and the diff is
    // computed from an XOR, so a sign error or a one-sided `&` would pass the
    // loop and still let a world CREATED with one ocarina be loaded by a
    // session that resolved two — the direction a player actually reaches by
    // turning the row off at the title screen. Both halves must refuse, and
    // both must refuse BY THE RULE'S NAME.
    {
        ComboSettingsRecord shared = live;
        shared.comboFlags = (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA;

        const uint32_t setFrozen = Combo_ComboSettingsDivergenceBetween(&shared, &live);
        CS_ASSERT(setFrozen == RSBS_COMBO_DIVERGE_SHARED_OCARINA,
                  "a file created with the shared ocarina ON, met by a session resolving it OFF, did not diverge as "
                  "the shared-ocarina rule");
        const uint32_t setLive = Combo_ComboSettingsDivergenceBetween(&live, &shared);
        CS_ASSERT(setLive == RSBS_COMBO_DIVERGE_SHARED_OCARINA,
                  "a file created with the shared ocarina OFF, met by a session resolving it ON, did not diverge as "
                  "the shared-ocarina rule — the flag diff is one-sided");
        CS_ASSERT(strcmp(Combo_ComboSettingsDivergenceFieldName(setLive), "sharedOcarina") == 0,
                  "the refusal must name the RULE; 'comboFlags' is where it lives, not what it is");
        CS_ASSERT(!Combo_ComboSettingsDivergenceIsDamage(setLive),
                  "a session that picked a different ocarina rule is a SETTINGS divergence, not damage: the healthy "
                  "file must not be quarantined for it");
        // And the two records must agree with themselves — otherwise every leg
        // above is passing because the diff refuses everything.
        CS_ASSERT(Combo_ComboSettingsDivergenceBetween(&shared, &shared) == 0,
                  "two identical shared-ocarina records reported divergence");
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

// ============================================================================
// combo-settings-authoring — the tier-4 keys reach the record BEFORE the
// freeze, and not after it (ADR 0011 increment 2)
// ============================================================================
//
// A RunHeadless bridge rather than a TestResult body: the five keys live in the
// CVar store on the Ship::Context singleton, so this row needs the
// display-free shared bring-up that lives (static) in test_runner.cpp. The
// rows above deliberately run WITHOUT one — that is how they prove the
// resolver falls back to the shipped defaults when nothing is authored, which
// is the same fallback this row's first leg re-proves with a store present.

extern "C" int Combo_SettingsAuthoring_RunHeadless(void) {
    printf("[TEST] combo-settings-authoring: the five tier-4 keys author the record before the freeze, move the "
           "fingerprint, and are refused once frozen (ADR 0011 increment 2)\n");

    CS_ASSERT(Combo_ComboSettingStoreAvailable(),
              "the bridge brought up no CVar store — every leg below would pass vacuously on the defaults");

    // ---- Clean slate: nothing frozen, nothing authored ----------------------
    ComboContext_Init();
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        const ComboSettingId id = (ComboSettingId)i;
        CS_ASSERT(Combo_ComboSettingClear(id) == 1, "clear must succeed while nothing is frozen");
        CS_ASSERT(!Combo_ComboSettingIsExplicit(id), "a cleared key must read as unset");
    }
    CS_ASSERT(Combo_ComboSettingReadOnlyReason() == NULL,
              "nothing is frozen, so there is no read-only reason: the pane must draw these editable");

    // ---- (1) Nothing authored => the shipped record, byte for byte, and its
    // pinned fingerprint. This is the acceptance bar the whole increment is
    // bounded by: with the store present but empty, the resolver must produce
    // exactly what it produced before any key existed, or SeedDeterminism,
    // MMRandoGen and HeadlessForeignDigest all move.
    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    ComboSettingsRecord live;
    Combo_ResolveComboSettings(&live);
    CS_ASSERT(ComboSettingsRecordsEqual(defaults, live),
              "with no key set the resolver must produce the shipped defaults — anything else moves every new world");
    // The same vector Test_ComboSettingsCanonical pins; restated here so THIS
    // row is red on its own if the defaults drift, without a reader having to
    // notice that a different row went red for a related reason.
    static const uint8_t kDefaultsCanon[RSBS_COMBO_SETTINGS_CANONICAL_LEN] = {
        0x01, 0x04, 0x08, 0x08, 0x3F, 0x00, 0x3F, 0x00, 0x01, 0x02, 0x00, 0x00,
    };
    static const uint32_t kSettingsHash = 0xDEADBEEFu;
    static const uint32_t kProfileDigest = 0x0BADF00Du;
    static const uint32_t kDefaultsFingerprint = 0xBA5FC364u;
    uint8_t canon[RSBS_COMBO_SETTINGS_CANONICAL_LEN];
    Combo_ComboSettingsCanonical(&live, canon);
    CS_ASSERT(memcmp(canon, kDefaultsCanon, sizeof(kDefaultsCanon)) == 0,
              "the resolved defaults' canonical bytes moved — the identity of every unauthored world just changed");
    CS_ASSERT(Combo_ComputeComboSettingsHash(&live, kSettingsHash, kProfileDigest) == kDefaultsFingerprint,
              "the resolved defaults' fingerprint moved off its golden vector");
    CS_ASSERT(Combo_ComboSettingDefault(COMBO_SETTING_DIRECTION) == (int32_t)defaults.direction &&
                  Combo_ComboSettingDefault(COMBO_SETTING_POOL_SIZE_OOT) == (int32_t)defaults.poolSizeOoT &&
                  Combo_ComboSettingDefault(COMBO_SETTING_POOL_SIZE_MM) == (int32_t)defaults.poolSizeMM &&
                  Combo_ComboSettingDefault(COMBO_SETTING_ITEM_CLASS_OOT) == (int32_t)defaults.itemClassOoT &&
                  Combo_ComboSettingDefault(COMBO_SETTING_ITEM_CLASS_MM) == (int32_t)defaults.itemClassMM,
              "each key's default must be the defaults record's own field — one definition of 'what ships'");
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        const ComboSettingId id = (ComboSettingId)i;
        CS_ASSERT(Combo_ComboSettingResolved(id) == Combo_ComboSettingDefault(id),
                  "an unset key must resolve to its default");
        CS_ASSERT(Combo_ComboSettingKey(id) != NULL && strncmp(Combo_ComboSettingKey(id), RSBS::kComboIdentityKeyPrefix,
                                                               strlen(RSBS::kComboIdentityKeyPrefix)) == 0,
                  "every authoring key lives under the tier-4 identity namespace (ADR 0003 naming)");
    }

    // ---- (2) The pinned value spaces (decision 1.2.1) ----------------------
    {
        struct Probe {
            ComboSettingId id;
            int32_t value;
            bool valid;
        };
        static const Probe kProbes[] = {
            { COMBO_SETTING_DIRECTION, 0, false }, // 0 is unreachable inside a formatted record
            { COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_OFF, true },
            { COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_BOTH, true },
            { COMBO_SETTING_DIRECTION, 5, false }, // one past the table: never a new enumerator
            { COMBO_SETTING_DIRECTION, -1, false },
            { COMBO_SETTING_DIRECTION, 255, false },
            { COMBO_SETTING_POOL_SIZE_OOT, 0, false }, // the direction byte says "off", not a zero pool
            { COMBO_SETTING_POOL_SIZE_OOT, 1, true },
            { COMBO_SETTING_POOL_SIZE_OOT, (int32_t)RSBS_FOREIGN_PLACEMENT_CAP, true },
            { COMBO_SETTING_POOL_SIZE_OOT, (int32_t)RSBS_FOREIGN_PLACEMENT_CAP + 1, false }, // a count that lies
            { COMBO_SETTING_POOL_SIZE_MM, -3, false },
            { COMBO_SETTING_POOL_SIZE_MM, (int32_t)RSBS_FOREIGN_PLACEMENT_CAP, true },
            { COMBO_SETTING_ITEM_CLASS_OOT, 0, true }, // "no classes armed" is a legitimate world (decision 3.3)
            { COMBO_SETTING_ITEM_CLASS_OOT, (int32_t)RSBS_ITEMCLASS_ALL_V1, true },
            { COMBO_SETTING_ITEM_CLASS_OOT, (int32_t)RSBS_ITEMCLASS_PROGRESSION, true },
            { COMBO_SETTING_ITEM_CLASS_OOT, 0x0040, false }, // the first UNALLOCATED bit must read 0
            { COMBO_SETTING_ITEM_CLASS_MM, (int32_t)(RSBS_ITEMCLASS_SONGS | RSBS_ITEMCLASS_SIDEQUEST), true },
            { COMBO_SETTING_ITEM_CLASS_MM, 0x8000, false },
            { COMBO_SETTING_ITEM_CLASS_MM, 0x10000, false }, // past uint16
            { COMBO_SETTING_ITEM_CLASS_MM, -1, false },
            // A comboFlags BIT is EXACTLY 0 or 1 (#668), never "nonzero is
            // true": the record stores one bit, so a coercion would make two
            // different stored values produce the same world — the very thing
            // the pinned value spaces exist to prevent — and would silently
            // turn a typo into "on".
            { COMBO_SETTING_SHARED_OCARINA, 0, true },
            { COMBO_SETTING_SHARED_OCARINA, 1, true },
            { COMBO_SETTING_SHARED_OCARINA, 2, false },
            { COMBO_SETTING_SHARED_OCARINA, -1, false },
            { COMBO_SETTING_SHARED_OCARINA, 0x100, false },
        };
        for (size_t i = 0; i < sizeof(kProbes) / sizeof(kProbes[0]); i++) {
            if (Combo_ComboSettingValueValid(kProbes[i].id, kProbes[i].value) != kProbes[i].valid) {
                printf("[TEST] FAIL: '%s' = %d judged %s, expected %s\n", Combo_ComboSettingKey(kProbes[i].id),
                       (int)kProbes[i].value, kProbes[i].valid ? "INVALID" : "valid",
                       kProbes[i].valid ? "valid" : "INVALID");
                return TEST_FAIL;
            }
        }
        CS_ASSERT(!Combo_ComboSettingValueValid(COMBO_SETTING_COUNT, 1), "an id past the table is never valid");
    }

    // ---- (2b) The shared ocarina, end to end (#668) -------------------------
    // A comboFlags BIT rather than a field of its own, which is the one shape
    // no other key in this table has, so it gets its own leg: key -> resolver
    // -> record byte -> the consumer predicate -> the arrival refusal. Self
    // contained (it re-inits and clears its key at the end) so leg 3 still
    // starts from the untouched store it asserts about.
    {
        ComboContext_Init();
        CS_ASSERT(Combo_ComboSettingDefault(COMBO_SETTING_SHARED_OCARINA) == 0,
                  "the shipped default must be OFF — an existing world has to reproduce byte for byte");
        CS_ASSERT(!Combo_ComboSharedOcarina(),
                  "with nothing authored and nothing frozen the ocarina must not be shared");

        ComboSettingsRecord off;
        Combo_ResolveComboSettings(&off);
        CS_ASSERT(off.comboFlags == 0, "an unauthored flag key must resolve to a CLEAR comboFlags byte");

        CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_SHARED_OCARINA, 1) == 1, "the flag must be authorable");
        ComboSettingsRecord on;
        Combo_ResolveComboSettings(&on);
        CS_ASSERT(on.comboFlags == (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA,
                  "the resolver did not assemble the authored flag into comboFlags");
        CS_ASSERT((on.comboFlags & (uint8_t)~RSBS_COMBO_FLAGS_ALL_V1) == 0,
                  "the resolver set an UNALLOCATED comboFlags bit; every one of them must read 0 in a "
                  "formatVersion-1 record");
        CS_ASSERT(Combo_ComputeComboSettingsHash(&on, kSettingsHash, kProfileDigest) !=
                      Combo_ComputeComboSettingsHash(&off, kSettingsHash, kProfileDigest),
                  "turning the shared ocarina on did not move the world's fingerprint");
        CS_ASSERT(Combo_ComboSharedOcarina(), "the consumer predicate did not follow the live resolution");

        // Freeze it, and the predicate must read THE RECORD: a title-screen
        // toggle after creation changes a world that was already built.
        ComboSettingsArmPairing(0xA07A0668u, kSettingsHash, kProfileDigest);
        Combo_FreezeComboSettings(&on);
        CS_ASSERT(gComboCtx.comboSettings.comboFlags == (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA,
                  "the freeze did not carry the flag into the record");
        CS_ASSERT(Combo_ComboSharedOcarina(), "the frozen record's flag must arm the predicate");
        CS_ASSERT(Combo_ComboSettingsDivergence() == 0, "a world frozen from its own store must compare healthy");

        // THE SESSION WALKS AWAY: the key is cleared out of band (the console,
        // a hand-edited config, or the same player turning the row off at the
        // title screen before loading). The arrival/load compare must refuse it
        // BY THE RULE'S NAME, without treating the healthy file as damaged...
        CVarClear(Combo_ComboSettingKey(COMBO_SETTING_SHARED_OCARINA));
        const uint32_t bits = Combo_ComboSettingsDivergence();
        CS_ASSERT((bits & RSBS_COMBO_DIVERGE_SHARED_OCARINA) != 0,
                  "a session that resolved the shared ocarina OFF against a file created with it ON was not "
                  "refused — the option would silently change an existing world");
        CS_ASSERT(strcmp(Combo_ComboSettingsDivergenceFieldName(bits), "sharedOcarina") == 0,
                  "the refusal must name the shared-ocarina RULE");
        CS_ASSERT(!Combo_ComboSettingsDivergenceIsDamage(bits), "a settings divergence must not quarantine the file");
        CS_ASSERT(Combo_ComboSharedOcarina(),
                  "the predicate followed the SESSION instead of the frozen record — after creation the rules are "
                  "world identity, and a divergent session is refused rather than honoured");

        // ...and the reverse: a file created with it OFF, met by a session that
        // turned it ON. Same bit, same name; the diff is an XOR, not a test of
        // one side.
        Combo_FreezeComboSettings(&off);
        CS_ASSERT(Combo_ComboSettingsDivergence() == 0, "control: the OFF file agrees with the cleared store");
        CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_SHARED_OCARINA, 1) == 0,
                  "the writer must refuse once the record is frozen, whatever the value");
        CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_SHARED_OCARINA), 1);
        const uint32_t reverse = Combo_ComboSettingsDivergence();
        CS_ASSERT((reverse & RSBS_COMBO_DIVERGE_SHARED_OCARINA) != 0,
                  "a session that resolved the shared ocarina ON against a file created with it OFF was not "
                  "refused");
        CS_ASSERT(!Combo_ComboSharedOcarina(), "the OFF file's rule must hold against an ON session");

        ComboContext_Init();
        CS_ASSERT(Combo_ComboSettingClear(COMBO_SETTING_SHARED_OCARINA) == 1, "clear the flag before leg 3");
        CS_ASSERT(!Combo_ComboSettingIsExplicit(COMBO_SETTING_SHARED_OCARINA), "the cleared flag must read unset");
    }

    // ---- (3) Authored values reach the record BEFORE the freeze ------------
    CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_FORWARD) == 1,
              "an in-space write must land while nothing is frozen");
    CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_ITEM_CLASS_OOT,
                                    (int32_t)(RSBS_ITEMCLASS_SONGS | RSBS_ITEMCLASS_MASKS)) == 1,
              "an in-space class bitset must land while nothing is frozen");
    CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_POOL_SIZE_MM, 3) == 1, "an in-space pool size must land");
    CS_ASSERT(Combo_ComboSettingIsExplicit(COMBO_SETTING_DIRECTION) &&
                  Combo_ComboSettingIsExplicit(COMBO_SETTING_ITEM_CLASS_OOT) &&
                  Combo_ComboSettingIsExplicit(COMBO_SETTING_POOL_SIZE_MM),
              "a landed write must read as explicit");
    CS_ASSERT(!Combo_ComboSettingIsExplicit(COMBO_SETTING_POOL_SIZE_OOT), "an untouched key stays unset");
    CS_ASSERT(Combo_ComboSettingResolved(COMBO_SETTING_DIRECTION) == (int32_t)RSBS_COMBO_DIR_FORWARD,
              "the reader must serve the authored direction");

    Combo_ResolveComboSettings(&live);
    CS_ASSERT(live.direction == RSBS_COMBO_DIR_FORWARD, "the resolver did not read the authored direction");
    CS_ASSERT(live.itemClassOoT == (RSBS_ITEMCLASS_SONGS | RSBS_ITEMCLASS_MASKS),
              "the resolver did not read the authored OoT class bitset");
    CS_ASSERT(live.poolSizeMM == 3, "the resolver did not read the authored MM pool size");
    CS_ASSERT(live.poolSizeOoT == defaults.poolSizeOoT && live.itemClassMM == defaults.itemClassMM &&
                  live.goal == defaults.goal && live.logicRung == defaults.logicRung && live.comboFlags == 0 &&
                  live.spare1 == 0 && live.formatVersion == defaults.formatVersion,
              "an unauthored field must keep its shipped default");
    CS_ASSERT(!ComboSettingsRecordsEqual(live, defaults), "an authored record must differ from the defaults");
    const uint32_t authoredFingerprint = Combo_ComputeComboSettingsHash(&live, kSettingsHash, kProfileDigest);
    CS_ASSERT(authoredFingerprint != kDefaultsFingerprint,
              "a different direction and class bitset must change comboSettingsHash — the whole point of folding "
              "the record into the fingerprint");

    // ---- THE OFF RULING (#667) --------------------------------------------
    //
    // Two pre-Fill predicates, and the whole point of the ruling is that they
    // answer DIFFERENTLY under RSBS_COMBO_DIR_OFF:
    //
    //   Combo_ForeignPairingRequested()   -- a paired WORLD is asked for. TRUE
    //                                        for OFF: ADR 0011 decision 2.3's
    //                                        gloss, "a paired world with no
    //                                        crossings — not an unpaired world",
    //                                        and one-game semantics make it
    //                                        binding. This assertion is the
    //                                        inverse of what this file locked
    //                                        before #667.
    //   Combo_ForeignCrossingsRequested() -- CROSSINGS are asked for. FALSE for
    //                                        OFF, which is the question the old
    //                                        body actually answered.
    //
    // Collapsing them again makes OFF read as an unpaired world at the creation
    // gate, which ships a file whose MM half was never authored.
    CS_ASSERT(Combo_ForeignPairingRequested(), "FORWARD asks for a paired world");
    CS_ASSERT(Combo_ForeignCrossingsRequested(), "FORWARD asks for crossings");
    CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_OFF) == 1, "OFF is authorable");
    CS_ASSERT(Combo_ForeignPairingRequested(),
              "OFF must answer 'a paired world IS being asked for' — it is a paired world with zero crossings (ADR "
              "0011 decision 2.3), and a creation gate that reads it as unpaired never authors the MM half (#667)");
    CS_ASSERT(!Combo_ForeignCrossingsRequested(), "OFF must answer 'no crossings are being asked for' (#667)");

    // The RULE itself, over records the resolver cannot produce. This is what
    // keeps the ruling falsifiable rather than a comment: the predicate above
    // resolves its own record from the keys, and the keys reject an out-of-space
    // direction, so the only way to drive the false branches is to hand the rule
    // a record directly.
    {
        ComboSettingsRecord probe;
        Combo_ComboSettingsDefaults(&probe);
        CS_ASSERT(!Combo_ComboSettingsDescribePairedWorld(NULL), "a NULL record describes no world");
        probe.direction = (uint8_t)RSBS_COMBO_DIR_OFF;
        CS_ASSERT(Combo_ComboSettingsDescribePairedWorld(&probe), "OFF describes a paired world (#667)");
        probe.direction = (uint8_t)RSBS_COMBO_DIR_FORWARD;
        CS_ASSERT(Combo_ComboSettingsDescribePairedWorld(&probe), "FORWARD describes a paired world");
        probe.direction = (uint8_t)RSBS_COMBO_DIR_REVERSE;
        CS_ASSERT(Combo_ComboSettingsDescribePairedWorld(&probe), "REVERSE describes a paired world");
        probe.direction = (uint8_t)RSBS_COMBO_DIR_BOTH;
        CS_ASSERT(Combo_ComboSettingsDescribePairedWorld(&probe), "BOTH describes a paired world");
        // 0 is unreachable inside a formatted record by construction (decision
        // 1.3) and 99 is simply not an enumerator; both are rules no consumer can
        // interpret, so a creation must refuse rather than clamp them.
        probe.direction = 0u;
        CS_ASSERT(!Combo_ComboSettingsDescribePairedWorld(&probe),
                  "direction 0 is not a pinned enumerator and must refuse a creation");
        probe.direction = 99u;
        CS_ASSERT(!Combo_ComboSettingsDescribePairedWorld(&probe),
                  "an out-of-space direction must refuse a creation, never be clamped into one");
        probe.direction = (uint8_t)RSBS_COMBO_DIR_BOTH;
        probe.formatVersion = 0u;
        CS_ASSERT(!Combo_ComboSettingsDescribePairedWorld(&probe),
                  "formatVersion 0 is the ABSENT tag (decision 4.2) — freezing it would stamp 'no record' as this "
                  "world's identity");
    }

    // THE IDENTITY THE CREATION GATE RESTS ON (#657). The pre-Fill question
    // (Combo_ForeignCrossingsRequested, from the keys) and the post-freeze one
    // (Combo_ComboDirectionArms, from the record) must be the same fact read from
    // two surfaces. Driven over EVERY pinned direction, so adding a fifth
    // enumerator that arms neither origin — or that arms one while reading as OFF
    // — is caught here rather than at a player's file select.
    {
        const int32_t kPinnedDirections[] = { (int32_t)RSBS_COMBO_DIR_OFF, (int32_t)RSBS_COMBO_DIR_FORWARD,
                                              (int32_t)RSBS_COMBO_DIR_REVERSE, (int32_t)RSBS_COMBO_DIR_BOTH };
        for (size_t i = 0; i < sizeof(kPinnedDirections) / sizeof(kPinnedDirections[0]); i++) {
            CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, kPinnedDirections[i]) == 1,
                      "every pinned direction must be authorable while nothing is frozen");
            // Unfrozen, so Combo_ComboDirectionArms serves the LIVE resolution —
            // which is what makes this an identity over the same input rather
            // than a comparison of two different worlds.
            const bool arms =
                Combo_ComboDirectionArms((uint8_t)GAME_OOT) || Combo_ComboDirectionArms((uint8_t)GAME_MM);
            CS_ASSERT(Combo_ForeignCrossingsRequested() == arms,
                      "the pre-Fill crossings question and the frozen-record one disagree for a pinned direction — "
                      "the creation gate compares them and would refuse every creation (#657)");
            CS_ASSERT(Combo_ForeignPairingRequested(),
                      "every pinned direction describes a paired world, OFF included (#667)");
        }
    }

    CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_FORWARD) == 1, "back to FORWARD");

    // THE CREATION EVENT'S OWN ORDER (Playthrough_Init; decision 4.1): resolve
    // the record from the keys, THEN freeze it. The frozen record must be what
    // the player authored, not the defaults.
    ComboContext_Init();
    ComboSettingsArmPairing(0xA07A0002u, kSettingsHash, kProfileDigest);
    // THE GATE REFUSES AN UNFROZEN WORLD (#657), whatever the pre-Fill ask was.
    // A creation that reached the check with nothing frozen has no identity for
    // the file-create seam or any arrival to read, and formatVersion 0 makes such
    // a world indistinguishable from an uncreated one (decision 4.2).
    CS_ASSERT(!Combo_ComboSettingsFrozen(), "precondition: nothing is frozen yet");
    CS_ASSERT(!Combo_ForeignCreationGateHolds(true), "an unfrozen world must fail the creation gate (#657)");
    CS_ASSERT(!Combo_ForeignCreationGateHolds(false),
              "an unfrozen world must fail the creation gate whichever answer the pre-Fill ask gave (#657)");
    ComboSettingsRecord toFreeze;
    Combo_ResolveComboSettings(&toFreeze);
    const uint32_t frozenHash = Combo_FreezeComboSettings(&toFreeze);
    CS_ASSERT(Combo_ComboSettingsFrozen(), "the freeze must set the occupancy tag");
    CS_ASSERT(gComboCtx.comboSettings.direction == RSBS_COMBO_DIR_FORWARD &&
                  gComboCtx.comboSettings.itemClassOoT == (RSBS_ITEMCLASS_SONGS | RSBS_ITEMCLASS_MASKS) &&
                  gComboCtx.comboSettings.poolSizeMM == 3,
              "the FROZEN record is not what the player authored — the keys were read after the freeze, or not "
              "at all");
    CS_ASSERT(frozenHash == authoredFingerprint && gComboCtx.comboSettingsHash == authoredFingerprint,
              "the stamped fingerprint must be the authored record's, over the same two half-digests");
    CS_ASSERT(Combo_ComboSettingsDivergence() == 0,
              "a world frozen from the store it was authored in must compare healthy at its first arrival");
    // And the consumers the earlier increments armed now read the authored
    // values: the gate (#632), the size (O4), the class rule (#631).
    CS_ASSERT(Combo_ComboDirection() == RSBS_COMBO_DIR_FORWARD, "the gate reads the frozen direction");
    CS_ASSERT(Combo_ComboDirectionArms((uint8_t)GAME_OOT) && !Combo_ComboDirectionArms((uint8_t)GAME_MM),
              "FORWARD arms the forward pass only");
    CS_ASSERT(Combo_ComboPoolSizeFor((uint8_t)GAME_MM) == 3, "the reverse pass reads the frozen MM pool size");
    CS_ASSERT(Combo_ComboItemClassFor((uint8_t)GAME_OOT) == (RSBS_ITEMCLASS_SONGS | RSBS_ITEMCLASS_MASKS),
              "the forward pass reads the frozen OoT class bitset");
    // ...and THE CREATION GATE'S SECOND ACT (#657): the frozen record has to
    // reproduce the answer the CVars gave before Fill(). FORWARD authors
    // crossings, so the gate holds for `true` and REFUSES `false` — the shape of
    // the refusal playthrough.cpp rolls the whole freeze back on, which is what
    // stops a world being filled under rules the player did not ask for.
    CS_ASSERT(Combo_ForeignCreationGateHolds(true),
              "the frozen FORWARD record authors crossings, so the gate must hold for a pre-Fill ask of yes (#657)");
    CS_ASSERT(!Combo_ForeignCreationGateHolds(false),
              "the gate must REFUSE when the frozen record and the pre-Fill ask disagree — otherwise the freeze "
              "preserves nothing and the seam can reach its own conclusion (#657)");

    // ---- (4) The writers REFUSE once frozen (ADR 0004 §6 state 4) ---------
    // Locked at the writers, not the widget: the pane is one caller, and every
    // future caller inherits the gate. Falsifiable — remove the
    // Combo_ComboSettingsFrozen() check from either writer and its half is red.
    {
        CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_BOTH) == 0,
                  "a post-creation write must be REJECTED at the writer");
        int32_t stored = 0;
        CS_ASSERT(Combo_ComboSettingReadStore(COMBO_SETTING_DIRECTION, &stored) &&
                      stored == (int32_t)RSBS_COMBO_DIR_FORWARD,
                  "a refused write must leave the store byte-for-byte what it was");
        CS_ASSERT(Combo_ComboSettingClear(COMBO_SETTING_DIRECTION) == 0, "a post-creation clear must be REJECTED");
        CS_ASSERT(Combo_ComboSettingIsExplicit(COMBO_SETTING_DIRECTION), "a refused clear must leave the key set");
        for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
            // Even a write of the DEFAULT is refused: "already decided" is
            // about the act, not the value.
            const ComboSettingId id = (ComboSettingId)i;
            CS_ASSERT(Combo_ComboSettingSet(id, Combo_ComboSettingDefault(id)) == 0,
                      "every key's writer must refuse while frozen, whatever the value");
            CS_ASSERT(Combo_ComboSettingClear(id) == 0, "every key's clear must refuse while frozen");
        }

        // ADR 0004 §6 state 4's REASON STRING, which the pane renders and this
        // owns: "already decided", not a capability reason. A capability gate
        // ("not yet available") sends the player looking for a missing feature;
        // a freeze tells them the choice was made and is a different fact.
        // Locked here rather than in the window test because no headless row
        // can read pixels, and a string only the renderer holds is unassertable.
        {
            const char* reason = Combo_ComboSettingReadOnlyReason();
            CS_ASSERT(reason != NULL, "a frozen record must give the pane a read-only reason to show");
            CS_ASSERT(strcmp(reason, "already decided") == 0,
                      "state 4's reason string is 'already decided' (ADR 0011 increment 2)");
            CS_ASSERT(strstr(reason, "not yet") == NULL && strstr(reason, "navailable") == NULL,
                      "state 4's reason must NOT be a capability reason — nothing is unavailable, it was chosen");
        }

        // The pane's read surface post-creation: values FROM THE SAVE.
        ComboSettingsSummary summary;
        Combo_ComboSettingsSummary(&summary);
        CS_ASSERT(summary.paired && summary.frozen, "the summary must report a frozen pair");
        CS_ASSERT(summary.record.direction == RSBS_COMBO_DIR_FORWARD && summary.comboSettingsHash == frozenHash,
                  "the summary must serve the frozen values");

        // An OUT-OF-BAND write — the console, a hand-edited config — bypasses
        // the writers. The pane still shows the save (state 4's "from the
        // save, not the CVar"), and the compare that guards every arrival and
        // load sees exactly this as a SESSION divergence, not as damage.
        CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_DIRECTION), (int32_t)RSBS_COMBO_DIR_BOTH);
        Combo_ComboSettingsSummary(&summary);
        CS_ASSERT(summary.record.direction == RSBS_COMBO_DIR_FORWARD,
                  "post-creation the pane's value must come from the save, never from the CVar");
        const uint32_t sessionBits = Combo_ComboSettingsDivergence();
        CS_ASSERT((sessionBits & RSBS_COMBO_DIVERGE_DIRECTION) != 0,
                  "an out-of-band CVar change must be visible to the arrival/load compare, by name");
        CS_ASSERT(!Combo_ComboSettingsDivergenceIsDamage(sessionBits),
                  "a session that walked away from a healthy file is NOT damage — the file must not be quarantined "
                  "for it (RefuseSlotIdentity's contract)");
        CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_DIRECTION), (int32_t)RSBS_COMBO_DIR_FORWARD);
        CS_ASSERT(Combo_ComboSettingsDivergence() == 0, "restoring the store restores agreement");

        // Whereas a fingerprint its own record does not produce IS damage.
        gComboCtx.comboSettingsHash ^= 0x00000001u;
        CS_ASSERT(Combo_ComboSettingsDivergenceIsDamage(Combo_ComboSettingsDivergence()),
                  "a corrupted fingerprint is damage to the stored identity");
        CS_ASSERT(Combo_StampComboSettingsHash() == frozenHash && Combo_ComboSettingsDivergence() == 0,
                  "re-stamping restores the fingerprint");
        CS_ASSERT(Combo_ComboSettingsDivergenceIsDamage(RSBS_COMBO_DIVERGE_UNREADABLE) &&
                      !Combo_ComboSettingsDivergenceIsDamage(0) &&
                      !Combo_ComboSettingsDivergenceIsDamage(RSBS_COMBO_DIVERGE_POOL_SIZE_OOT |
                                                             RSBS_COMBO_DIVERGE_ITEM_CLASS_MM),
                  "damage is UNREADABLE or FINGERPRINT and nothing else");
    }

    // ---- (5) The identity dropped (title screen) => authoring resumes ------
    ComboContext_Init();
    CS_ASSERT(!Combo_ComboSettingsFrozen(), "ComboContext_Init must drop the frozen record");
    CS_ASSERT(Combo_ComboSettingReadOnlyReason() == NULL,
              "dropping the identity must drop the read-only reason with it — the pane goes editable again");
    CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_BOTH) == 1,
              "authoring must resume once nothing is frozen");
    CS_ASSERT(Combo_ComboSettingResolved(COMBO_SETTING_DIRECTION) == (int32_t)RSBS_COMBO_DIR_BOTH, "and be read back");

    // ---- (6) An out-of-space value resolves to the shipped default --------
    // The writers refuse one outright...
    CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, 0) == 0 &&
                  Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, 9) == 0,
              "an out-of-table direction must be refused at the writer");
    CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_POOL_SIZE_OOT, 0) == 0 &&
                  Combo_ComboSettingSet(COMBO_SETTING_POOL_SIZE_OOT, (int32_t)RSBS_FOREIGN_PLACEMENT_CAP + 1) == 0,
              "an out-of-range pool size must be refused at the writer");
    CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_ITEM_CLASS_MM, 0x0040) == 0 &&
                  Combo_ComboSettingSet(COMBO_SETTING_ITEM_CLASS_MM, -1) == 0,
              "an unallocated class bit must be refused at the writer");
    {
        int32_t stored = 0;
        CS_ASSERT(Combo_ComboSettingReadStore(COMBO_SETTING_DIRECTION, &stored) &&
                      stored == (int32_t)RSBS_COMBO_DIR_BOTH,
                  "refused writes must not touch the store");
    }
    // ...so only an out-of-band write can plant one. Plant four, one per
    // shape of wrongness, and resolve.
    CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_DIRECTION), 9);
    CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_POOL_SIZE_OOT), 0);
    CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_POOL_SIZE_MM), 99);
    CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_ITEM_CLASS_MM), 0x8000);
    Combo_ResolveComboSettings(&live);
    CS_ASSERT(live.direction == RSBS_COMBO_DIR_BOTH,
              "an out-of-table direction must resolve to the SHIPPED DEFAULT, never to a new enumerator");
    CS_ASSERT(live.poolSizeOoT == RSBS_FOREIGN_PLACEMENT_CAP && live.poolSizeMM == RSBS_FOREIGN_PLACEMENT_CAP,
              "an out-of-range pool size must resolve to the shipped default, not to a clamp");
    CS_ASSERT(live.itemClassMM == RSBS_ITEMCLASS_ALL_V1,
              "a mask with an unallocated bit must resolve to the shipped default, not to a masked-off value");
    CS_ASSERT(Combo_ComboDirection() == RSBS_COMBO_DIR_BOTH, "the gate sees the default, not the junk");
    {
        int32_t stored = 0;
        CS_ASSERT(Combo_ComboSettingReadStore(COMBO_SETTING_DIRECTION, &stored) && stored == 9,
                  "resolution reports and falls back; it does not silently repair the store");
    }

    // ---- (7) A healthy file met by a divergent SESSION is refused at load
    // WITHOUT being quarantined (RefuseSlotIdentity's contract, save.h) ------
    //
    // The case the authorable keys make ordinary: author FORWARD, create,
    // save; go back to the title screen (the identity drops), pick BOTH; load
    // the FORWARD file. It must refuse by name and latch — and the .redsave
    // must still be exactly where it was, because the file is healthy and the
    // session is what moved. Then set the rule back and the same file loads.
    // Falsifiable: restore the unconditional QuarantineSlotFile in
    // SaveManager::LoadSlot's combo branch and the HasSave assertion is red.
    {
        for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
            CS_ASSERT(Combo_ComboSettingClear((ComboSettingId)i) == 1, "clear the leg-6 junk first");
        }
        std::error_code ec;
        std::filesystem::remove_all(kComboSettingsLoadTestDir, ec); // no stale .bak from an earlier run
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        mgr.SetSaveDirectory(kComboSettingsLoadTestDir);
        RsbsSave_ResetSlotSessionState();
        // ARM THE WRITE LATCH (#533). Save() refuses a slot that was not
        // loaded, created or erased this session, and the reset above cleared
        // whatever the legs before this one had done, so the erase has to come
        // after it -- the same order the round-trip legs use.
        mgr.DeleteSave(0);

        ComboContext_Init();
        ComboSettingsSeedShadows(0x7Au);
        CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_FORWARD) == 1,
                  "author FORWARD for the file");
        ComboSettingsArmPairing(0xA07A0007u, 0x5E77A177u, 0x11FEDC77u);
        ComboSettingsRecord authored;
        Combo_ResolveComboSettings(&authored);
        Combo_FreezeComboSettings(&authored);
        CS_ASSERT(mgr.Save(0), "Save(0) failed for the session-divergence leg");
        CS_ASSERT(RsbsSave_HasSave(0) == 1 && RsbsSave_HasQuarantine(0) == 0, "baseline: one file, no quarantine");

        // The session walks away: back at the title screen the identity drops,
        // authoring resumes, and the player picks BOTH.
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_BOTH) == 1,
                  "authoring resumes once the identity is dropped");
        CS_ASSERT(!mgr.Load(0), "loading a FORWARD file under a BOTH session must be REFUSED (ADR 0011 decision 4)");
        CS_ASSERT(RsbsSave_GetSlotRefuseReason(0) == (int)RSBS_REFUSE_IDENTITY, "refused as RSBS_REFUSE_IDENTITY");
        CS_ASSERT(RsbsSave_IsSlotWritable(0) == 0, "the slot must be latched against writes (#533)");
        CS_ASSERT(!Combo_ComboSettingsFrozen(), "a refused load must not commit the record");
        CS_ASSERT(RsbsSave_HasSave(0) == 1,
                  "the HEALTHY .redsave was renamed away — a settings change at the title screen must never "
                  "quarantine a file that is not damaged");
        CS_ASSERT(RsbsSave_HasQuarantine(0) == 0, "a session divergence must quarantine nothing");

        // Set the rule back: the same file, untouched, loads.
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        CS_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_FORWARD) == 1,
                  "set the rule back");
        CS_ASSERT(mgr.Load(0), "with the rule restored the untouched file must load");
        CS_ASSERT(Combo_ComboSettingsFrozen() && gComboCtx.comboSettings.direction == RSBS_COMBO_DIR_FORWARD,
                  "the loaded record is the authored one");

        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        mgr.SetSaveDirectory(kComboSettingsTestDir);
        ComboContext_Init();
    }

    // ---- Cleanup: leave the process-global store and context clean ---------
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        const ComboSettingId id = (ComboSettingId)i;
        CS_ASSERT(Combo_ComboSettingClear(id) == 1, "cleanup clear");
        CS_ASSERT(!Combo_ComboSettingIsExplicit(id), "cleanup left a key set");
    }
    ComboContext_Init();
    Combo_ResolveComboSettings(&live);
    CS_ASSERT(ComboSettingsRecordsEqual(defaults, live), "cleanup must leave the resolver at the shipped defaults");

    printf("[TEST] PASS: the tier-4 keys author the record before the freeze, move the fingerprint, resolve "
           "out-of-space values to the defaults, and are refused once frozen\n");
    return TEST_PASS;
}
