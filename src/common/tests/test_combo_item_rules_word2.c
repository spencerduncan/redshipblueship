/**
 * @file test_combo_item_rules_word2.c
 * @brief Locks for the SECOND cross-game rules word and its fourteen hidden keys
 *        (#1169 lane XW2; ADR 0011's 2026-11-04 amendment, ADR 0009 claim 19).
 *
 * Two rows, split by what each one needs:
 *
 *   combo-item-rules-second-word (CTest ComboItemRulesSecondWord, redship tier;
 *   needs the CVar store, so the runner brings up the display-free shared
 *   context first) -- THE WORD. Its carve (offset 925 after #1439's Way of the
 *   Hero record, four bytes, reserved[] from 929, still above ADR 0009's floor)
 *   and its little-endian codec; the
 *   pinned bit space (OoTMM's seven warp-song extensions at bits 0-6, the seven
 *   shared songs at 7-13, each requiring its extension); the fourteen keys
 *   (OoTMM's names, off by default, exactly 0 or 1) and the resolver (a shared
 *   song without its extension resolves off); THE HIDDEN GATE: every allocated
 *   bit, an unallocated bit and a shared song alone are refused by the one
 *   predicate Generate, a seed package's validation and a .redsave load ask, and
 *   from source, Generate asks it before the freeze and the package validation
 *   asks it; the fingerprint (folded only when set, pinned vectors computed
 *   outside the build, every bit moves it); the freeze, the KEEP invalidation,
 *   the creation rollback and retraction; divergence (a frozen word the session
 *   cannot author is named, refused as another build's while hidden, even under
 *   session keys equal to it, and restored through the keys once revealed; a
 *   session's stray key is put back to a file's "off"); and THE ROUND TRIPS:
 *   the seed package's world record (WorldRecordBytes -> ApplyComboWorld) and a
 *   real .redsave Save/Load carry the word, a hidden word is refused at load
 *   (with the session's keys unset or equal to it) and not quarantined.
 *
 *   rules-off-goldens-unchanged (CTest RulesOffGoldensUnchanged, redship tier,
 *   ROM-free, no CVar store needed) -- INERT. With every second-word rule off
 *   the fingerprint at every generator version, the world hash, the single
 *   bag's coordinator, trim and foolish seeds, the divergence diff and a seed
 *   package's world record are exactly main's: pinned to vectors computed
 *   outside the build with main's formula, and to the pre-XW2 (Ex) entry points.
 *
 * Every key is spelled from the identity prefix rather than as a literal, so
 * the tier-4 classification scan sees only the manifest's spelling.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Helpers carry the
 * Xw2 prefix because every included test file shares one translation unit.
 */

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
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#ifdef __cplusplus
extern "C" {
#endif
// games/oot/soh/Enhancements/randomizer/SeedPackageSingleExe.cpp: the package's
// world-record copy and its apply, end to end.
int OoT_SeedPackage_WorldRecordRoundTripForTest(const ComboContext* live, ComboContext* outRecordBytes);
#ifdef __cplusplus
}
#endif

static int sXw2Failures;

#define XW2_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sXw2Failures++;                           \
        }                                             \
    } while (0)

namespace {

// The identity terms the fingerprint vectors below were computed over (outside
// the build, FNV-1a 32 over main's byte string; the same terms test_combo_settings.c
// and test_cross_item_rules.c pin their vectors with).
const uint32_t kXw2SettingsHash = 0xDEADBEEFu;
const uint32_t kXw2ProfileDigest = 0x0BADF00Du;
const uint32_t kXw2Seed = 0x0169A0FFu;
const char* const kXw2LoadDir = "rsbs_test_combo_item_rules_word2_load";

// The bits, spelled as literals: they are .redsave and seed-package format, so
// the lock states them rather than reading them back from the header under test.
const uint32_t kXw2Soaring = 0x00000001u;       // songSoaringOot
const uint32_t kXw2SharedSoaring = 0x00000080u; // sharedSongSoaring
const uint32_t kXw2SoaringPair = kXw2Soaring | kXw2SharedSoaring;

/** The fourteen keys, in OoTMM's order, with OoTMM's key and option name. */
struct Xw2Key {
    ComboSettingId id;
    uint32_t bit;
    const char* cvarSuffix; // after the identity prefix
    const char* label;      // OoTMM's option name (data.ts)
    const char* rule;       // OoTMM's key (data.ts)
};
const Xw2Key kXw2Keys[] = {
    { COMBO_SETTING_SONG_SOARING_OOT, 0x0001u, "SongSoaringOoT", "Song of Soaring (OoT)", "songSoaringOot" },
    { COMBO_SETTING_SONG_MINUET_MM, 0x0002u, "SongMinuetMM", "Minuet of Forest (MM)", "songMinuetMm" },
    { COMBO_SETTING_SONG_BOLERO_MM, 0x0004u, "SongBoleroMM", "Bolero of Fire (MM)", "songBoleroMm" },
    { COMBO_SETTING_SONG_SERENADE_MM, 0x0008u, "SongSerenadeMM", "Serenade of Water (MM)", "songSerenadeMm" },
    { COMBO_SETTING_SONG_REQUIEM_MM, 0x0010u, "SongRequiemMM", "Requiem of Spirit (MM)", "songRequiemMm" },
    { COMBO_SETTING_SONG_NOCTURNE_MM, 0x0020u, "SongNocturneMM", "Nocturne of Shadow (MM)", "songNocturneMm" },
    { COMBO_SETTING_SONG_PRELUDE_MM, 0x0040u, "SongPreludeMM", "Prelude of Light (MM)", "songPreludeMm" },
    { COMBO_SETTING_SHARED_SONG_SOARING, 0x0080u, "SharedSongSoaring", "Shared Song of Soaring", "sharedSongSoaring" },
    { COMBO_SETTING_SHARED_SONG_MINUET, 0x0100u, "SharedSongMinuet", "Shared Minuet of Forest", "sharedSongMinuet" },
    { COMBO_SETTING_SHARED_SONG_BOLERO, 0x0200u, "SharedSongBolero", "Shared Bolero of Fire", "sharedSongBolero" },
    { COMBO_SETTING_SHARED_SONG_SERENADE, 0x0400u, "SharedSongSerenade", "Shared Serenade of Water",
      "sharedSongSerenade" },
    { COMBO_SETTING_SHARED_SONG_REQUIEM, 0x0800u, "SharedSongRequiem", "Shared Requiem of Spirit",
      "sharedSongRequiem" },
    { COMBO_SETTING_SHARED_SONG_NOCTURNE, 0x1000u, "SharedSongNocturne", "Shared Nocturne of Shadow",
      "sharedSongNocturne" },
    { COMBO_SETTING_SHARED_SONG_PRELUDE, 0x2000u, "SharedSongPrelude", "Shared Prelude of Light", "sharedSongPrelude" },
};

/** Publish a paired identity and freeze the shipped combo record, every
 *  first-word rule off and @p rules2 as the second word, the way
 *  Playthrough_Init stamps a world. */
void Xw2FreezeWorld(uint32_t rules2) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kXw2Seed;
    gComboCtx.sharedRandoSettingsHash = kXw2SettingsHash;
    gComboCtx.mmProfileDigest = kXw2ProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx2(&rec, NULL, rules2);
}

void Xw2SeedShadows(uint8_t fill) {
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, fill);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, (uint8_t)(fill ^ 0xFFu));
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

void Xw2ClearKeys() {
    if (!Combo_ComboSettingStoreAvailable()) {
        return;
    }
    for (const Xw2Key& k : kXw2Keys) {
        CVarClear(Combo_ComboSettingKey(k.id));
    }
}

/** The text of @p rel under the checkout, or "" when it cannot be read. */
std::string Xw2ReadSource(const char* rel) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
#else
    (void)rel;
    return std::string();
#endif
}

} // namespace

// ============================================================================
// combo-item-rules-second-word
// ============================================================================

TestResult ComboItemRulesSecondWord_Run(void) {
    printf("[TEST] combo-item-rules-second-word: the second cross-game rules word is a frozen world field folded "
           "into the fingerprint only when set, its fourteen warp-song keys exist and are hidden, and the record, "
           "a .redsave and a seed package carry it (#1169 XW2)\n");
    sXw2Failures = 0;
    XW2_CHECK(Combo_ComboSettingStoreAvailable(), "no CVar store: every key leg below would pass on the defaults");
    Combo_ComboItemRules2RevealForTest(0);
    ComboContext_Init();
    Xw2ClearKeys();

    // ---- (0) The carve and the codec ----------------------------------------------
    {
        XW2_CHECK(sizeof(ComboItemRules2Record) == 4u, "sizeof(ComboItemRules2Record) is %u, expected 4",
                  (unsigned)sizeof(ComboItemRules2Record));
        // #1475 then carved the 10-byte MM hint read record (mmHintsRead, 929) after the word.
        XW2_CHECK(offsetof(ComboContext, comboFillWallets) == 913u && offsetof(ComboContext, comboWoth) == 914u &&
                      offsetof(ComboContext, comboItemRules2) == 925u && offsetof(ComboContext, mmHintsRead) == 929u &&
                      offsetof(ComboContext, reserved) == 939u && sizeof(((ComboContext*)0)->reserved) == 65u,
                  "the second word is at %u and reserved[] is %u bytes at %u; expected 925 (after #1439's 11-byte "
                  "comboWoth at 914), then #1475's read record at 929 and 65 at 939",
                  (unsigned)offsetof(ComboContext, comboItemRules2), (unsigned)sizeof(((ComboContext*)0)->reserved),
                  (unsigned)offsetof(ComboContext, reserved));
        XW2_CHECK(sizeof(((ComboContext*)0)->reserved) >= 64u, "the carve took reserved[] under ADR 0009's floor");
        XW2_CHECK(offsetof(ComboContext, reserved) + sizeof(((ComboContext*)0)->reserved) == 1004u &&
                      offsetof(ComboContext, trackerAvailCountOoT) == 1004u && sizeof(ComboContext) == 1844u,
                  "the carve moved the record behind reserved[] (ends %u, captures at %u, sizeof %u): every .redsave "
                  "and every seed package's combo record would be reinterpreted",
                  (unsigned)(offsetof(ComboContext, reserved) + sizeof(((ComboContext*)0)->reserved)),
                  (unsigned)offsetof(ComboContext, trackerAvailCountOoT), (unsigned)sizeof(ComboContext));

        ComboItemRules2Record r;
        memset(&r, 0xEE, sizeof(r));
        Combo_ComboItemRules2Store(&r, 0x04030201u);
        XW2_CHECK(r.crossItemFlags2[0] == 0x01u && r.crossItemFlags2[1] == 0x02u && r.crossItemFlags2[2] == 0x03u &&
                      r.crossItemFlags2[3] == 0x04u && Combo_ComboItemRules2Word(&r) == 0x04030201u,
                  "the word is not stored little-endian a byte at a time (%02X %02X %02X %02X)",
                  (unsigned)r.crossItemFlags2[0], (unsigned)r.crossItemFlags2[1], (unsigned)r.crossItemFlags2[2],
                  (unsigned)r.crossItemFlags2[3]);
        XW2_CHECK(Combo_ComboItemRules2Word(NULL) == 0u, "a NULL record must read as every rule off");
        ComboContext_Init();
        XW2_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == 0u && Combo_ComboCrossItemFlags2() == 0u,
                  "a fresh (zero-extended) record must read every second-word rule off");
    }

    // ---- (1) The pinned bit space and validity --------------------------------------
    {
        uint32_t all = 0u;
        for (const Xw2Key& k : kXw2Keys) {
            all |= k.bit;
            XW2_CHECK(strcmp(Combo_ComboItemRule2Name(k.bit), k.rule) == 0, "bit %04X is named '%s', OoTMM's key is %s",
                      (unsigned)k.bit, Combo_ComboItemRule2Name(k.bit), k.rule);
        }
        XW2_CHECK(all == 0x00003FFFu && RSBS_XITEM2_ALL_V1 == 0x00003FFFu,
                  "the fourteen warp-song rules are not bits 0-13 (%08X, ALL_V1 %08X)", (unsigned)all,
                  (unsigned)RSBS_XITEM2_ALL_V1);
        XW2_CHECK(RSBS_XITEM2_SONG_SOARING_OOT == kXw2Soaring && RSBS_XITEM2_SHARED_SONG_SOARING == kXw2SharedSoaring &&
                      RSBS_XITEM2_SONG_PRELUDE_MM == 0x40u && RSBS_XITEM2_SHARED_SONG_PRELUDE == 0x2000u,
                  "a warp-song bit moved; they are .redsave format, append-only");
        XW2_CHECK(RSBS_XITEM2_HIDDEN == RSBS_XITEM2_ALL_V1, "until XW12 every allocated second-word bit is hidden");
        for (uint32_t b = 0; b < 7u; b++) {
            XW2_CHECK(Combo_ComboItemRule2Requires(1u << (b + 7u)) == (1u << b) &&
                          Combo_ComboItemRule2Requires(1u << b) == 0u,
                      "shared song bit %u must require extension bit %u, and the extension nothing", b + 7u, b);
        }
        XW2_CHECK(Combo_ComboItemRule2Requires(0u) == 0u && Combo_ComboItemRule2Requires(0x4000u) == 0u &&
                      Combo_ComboItemRule2Requires(kXw2SoaringPair) == 0u,
                  "an unallocated bit or a mask of two bits requires nothing");
        XW2_CHECK(strcmp(Combo_ComboItemRule2Name(0x4000u), "(unknown)") == 0 &&
                      strcmp(Combo_ComboItemRule2Name(kXw2SoaringPair), "(unknown)") == 0,
                  "an unallocated bit or a mask of two bits has no name");
        XW2_CHECK(Combo_ComboItemRules2Valid(0u) && Combo_ComboItemRules2Valid(kXw2Soaring) &&
                      Combo_ComboItemRules2Valid(kXw2SoaringPair) && Combo_ComboItemRules2Valid(0x3FFFu),
                  "an allocated word with every shared song's extension is a world");
        XW2_CHECK(!Combo_ComboItemRules2Valid(kXw2SharedSoaring) && !Combo_ComboItemRules2Valid(0x0100u),
                  "a SHARED BIT REQUIRES ITS EXTENSION: a shared song alone is not a world");
        XW2_CHECK(!Combo_ComboItemRules2Valid(0x4000u) && !Combo_ComboItemRules2Valid(0x80000000u),
                  "AN UNALLOCATED BIT IS REFUSED: it is a later build's rule");
    }

    // ---- (2) The keys: OoTMM's names, off by default, exactly 0 or 1 ---------------
    {
        const std::string prefix = RSBS::kComboIdentityKeyPrefix;
        uint32_t keyed = 0u;
        for (const Xw2Key& k : kXw2Keys) {
            XW2_CHECK(Combo_ComboSettingKey(k.id) == prefix + k.cvarSuffix, "the key for %s is %s", k.rule,
                      Combo_ComboSettingKey(k.id));
            XW2_CHECK(strcmp(Combo_ComboSettingLabel(k.id), k.label) == 0, "the %s key is labelled '%s', not '%s'",
                      k.rule, Combo_ComboSettingLabel(k.id), k.label);
            XW2_CHECK(Combo_ComboSettingDefault(k.id) == 0, "%s must ship off (OoTMM's default; no world moves)",
                      k.rule);
            XW2_CHECK(Combo_ComboSettingValueValid(k.id, 0) && Combo_ComboSettingValueValid(k.id, 1) &&
                          !Combo_ComboSettingValueValid(k.id, 2) && !Combo_ComboSettingValueValid(k.id, -1),
                      "%s is not exactly 0 or 1", k.rule);
            XW2_CHECK(Combo_ComboItemRule2BitFor(k.id) == k.bit, "%s authors %08X, not %08X", k.rule,
                      (unsigned)Combo_ComboItemRule2BitFor(k.id), (unsigned)k.bit);
            keyed |= k.bit;
        }
        XW2_CHECK(Combo_ComboItemRule2KeyedBits() == keyed && keyed == RSBS_XITEM2_ALL_V1,
                  "the keyed second-word bits are %08X, expected every allocated bit",
                  (unsigned)Combo_ComboItemRule2KeyedBits());
        XW2_CHECK(Combo_ComboItemRule2BitFor(COMBO_SETTING_SHARED_SONG_TIME) == 0u &&
                      Combo_ComboItemRule2BitFor(COMBO_SETTING_GOAL) == 0u,
                  "a first-word rule or a record field authors no second-word bit");
    }

    // ---- (3) The resolver ------------------------------------------------------------
    {
        XW2_CHECK(Combo_ResolveComboItemRules2() == 0u, "unset keys resolved %08X",
                  (unsigned)Combo_ResolveComboItemRules2());
        XW2_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_SONG_SOARING, 1) == 1,
                  "the writer refused Shared Song of Soaring before creation");
        XW2_CHECK(Combo_ResolveComboItemRules2() == 0u,
                  "the Shared Song of Soaring without its extension resolved %08X; OoTMM's cond makes it off",
                  (unsigned)Combo_ResolveComboItemRules2());
        XW2_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SONG_SOARING_OOT, 1) == 1,
                  "the writer refused Song of Soaring (OoT) before creation");
        XW2_CHECK(Combo_ResolveComboItemRules2() == kXw2SoaringPair, "both Soaring keys on resolved %08X",
                  (unsigned)Combo_ResolveComboItemRules2());
        ComboItemRulesRecord first;
        Combo_ResolveComboItemRules(&first);
        XW2_CHECK(first.crossItemFlags == 0u, "a second-word key reached the FIRST word (%08X)",
                  (unsigned)first.crossItemFlags);
        Xw2ClearKeys();
    }

    // ---- (4) THE HIDDEN GATE: Generate refuses a set bit -----------------------------
    {
        char names[192];
        XW2_CHECK(Combo_ComboItemRules2Refused(0u, names, sizeof(names)) == 0u && names[0] == '\0',
                  "a word with every rule off is refused ('%s')", names);
        for (const Xw2Key& k : kXw2Keys) {
            const uint32_t word = k.bit | Combo_ComboItemRule2Requires(k.bit);
            XW2_CHECK((Combo_ComboItemRules2Refused(word, names, sizeof(names)) & k.bit) != 0u &&
                          strstr(names, k.rule) != NULL,
                      "while hidden, %s is not refused by name ('%s')", k.rule, names);
        }
        // The creation's own question, over the session's resolution: a hidden key
        // set out of band (the console, a hand-made config) refuses Generate by name.
        XW2_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SONG_SOARING_OOT, 1) == 1, "the writer refused the key");
        const uint32_t refused = Combo_ComboItemRules2Refused(Combo_ResolveComboItemRules2(), names, sizeof(names));
        printf("[TEST] (4) Generate with Song of Soaring (OoT) set: refused %08X (%s)\n", (unsigned)refused, names);
        XW2_CHECK(refused == kXw2Soaring && strcmp(names, "songSoaringOot") == 0,
                  "GENERATE MUST REFUSE A SET BIT WHILE HIDDEN: refused %08X ('%s')", (unsigned)refused, names);
        Xw2ClearKeys();

        // Revealed (what XW12 does): the allocated rules pass, validity still holds.
        Combo_ComboItemRules2RevealForTest(1);
        XW2_CHECK(Combo_ComboItemRules2Refused(kXw2SoaringPair, names, sizeof(names)) == 0u,
                  "once revealed, a valid word must be creatable ('%s')", names);
        XW2_CHECK(Combo_ComboItemRules2Refused(kXw2SharedSoaring, names, sizeof(names)) == kXw2SharedSoaring &&
                      strcmp(names, "sharedSongSoaring") == 0,
                  "a shared song without its extension is refused even revealed ('%s')", names);
        XW2_CHECK(Combo_ComboItemRules2Refused(0x00004001u, names, sizeof(names)) == 0x00004000u &&
                      strcmp(names, "bit 14") == 0,
                  "an unallocated bit is refused, named by position ('%s')", names);
        Combo_ComboItemRules2RevealForTest(0);

        // From source: Generate asks before it freezes, and the seed package's
        // validation asks too. (Playthrough_Init needs ROMs; its wiring is
        // checked here, its predicate above.)
#ifdef RSBS_SOURCE_DIR
        const std::string gen = Xw2ReadSource("games/oot/soh/Enhancements/randomizer/3drando/playthrough.cpp");
        const size_t gate = gen.find("Combo_ComboItemRules2Refused(rsbsItemRules2");
        const size_t freeze = gen.find("Combo_FreezeComboSettingsEx2(&comboSettings, &comboItemRules, rsbsItemRules2)");
        const size_t progress = gen.find("Combo_GenProgress_Begin();");
        XW2_CHECK(!gen.empty() && gate != std::string::npos && freeze != std::string::npos &&
                      progress != std::string::npos && gate < progress && progress < freeze,
                  "Playthrough_Init does not refuse the hidden rules before anything is frozen (gate %zu, progress "
                  "%zu, freeze %zu)",
                  gate, progress, freeze);
        const std::string pkg = Xw2ReadSource("games/oot/soh/Enhancements/randomizer/SeedPackageSingleExe.cpp");
        XW2_CHECK(!pkg.empty() && pkg.find("Combo_ComboItemRules2Refused(rules2") != std::string::npos &&
                      pkg.find("memcpy(&rec->comboItemRules2, &live.comboItemRules2") != std::string::npos &&
                      pkg.find("records.comboItemRules2 = rec.comboItemRules2;") != std::string::npos,
                  "the seed package does not copy, apply and validate the second rules word");
#else
        XW2_CHECK(false, "RSBS_SOURCE_DIR is undefined: the Generate and package wiring cannot be checked");
#endif
    }

    // ---- (5) THE FINGERPRINT: folded only when set -----------------------------------
    {
        ComboSettingsRecord rec;
        Combo_ComboSettingsDefaults(&rec);
        // Computed outside the build: canonical(defaults) ':' LE32(DEADBEEF) ':'
        // LE32(0BADF00D) [ '%' LE32(word) ] [ ':' LE32(version) ], FNV-1a 32.
        XW2_CHECK(Combo_ComputeComboSettingsHashForGeneratorEx2(&rec, NULL, kXw2SoaringPair, kXw2SettingsHash,
                                                                kXw2ProfileDigest, 0u) == 0xC5D0FBFBu &&
                      Combo_ComputeComboSettingsHashForGeneratorEx2(&rec, NULL, kXw2SoaringPair, kXw2SettingsHash,
                                                                    kXw2ProfileDigest, 4u) == 0xE59A4747u,
                  "the Soaring pair's fingerprint moved off its vectors (v0 %08X, v4 %08X)",
                  (unsigned)Combo_ComputeComboSettingsHashForGeneratorEx2(&rec, NULL, kXw2SoaringPair, kXw2SettingsHash,
                                                                          kXw2ProfileDigest, 0u),
                  (unsigned)Combo_ComputeComboSettingsHashForGeneratorEx2(&rec, NULL, kXw2SoaringPair, kXw2SettingsHash,
                                                                          kXw2ProfileDigest, 4u));
        // Beside a first-word record (test_cross_item_rules.c's ON record): the
        // second word's term comes after the first's, before the label.
        ComboItemRulesRecord on;
        memset(&on, 0, sizeof(on));
        on.crossItemFlags = 0x00000825u;
        on.blastMaskCooldown = (uint8_t)RSBS_BLAST_COOLDOWN_SHORT;
        XW2_CHECK(Combo_ComputeComboSettingsHashForGeneratorEx2(&rec, &on, kXw2SoaringPair, kXw2SettingsHash,
                                                                kXw2ProfileDigest, 0u) == 0x77069218u &&
                      Combo_ComputeComboSettingsHashForGeneratorEx2(&rec, &on, kXw2SoaringPair, kXw2SettingsHash,
                                                                    kXw2ProfileDigest, 4u) == 0xDC05C1A2u,
                  "both words' fingerprint moved off its vectors");
        static const uint32_t kPerBitV4[14] = {
            0x89FAC0C7u, 0x3E168D92u, 0x415C0378u, 0x9DC96CCCu, 0x7D3E0F04u, 0x10850754u, 0xEB6ABB74u,
            0x993A7EB4u, 0xC9C301A9u, 0xDCD2CD8Eu, 0x71CA8AC8u, 0x91ED73BCu, 0x929C0DC4u, 0x3B4104D4u,
        };
        const uint32_t base =
            Combo_ComputeComboSettingsHashForGeneratorEx2(&rec, NULL, 0u, kXw2SettingsHash, kXw2ProfileDigest, 4u);
        for (uint32_t b = 0; b < 14u; b++) {
            const uint32_t h = Combo_ComputeComboSettingsHashForGeneratorEx2(&rec, NULL, 1u << b, kXw2SettingsHash,
                                                                             kXw2ProfileDigest, 4u);
            XW2_CHECK(h == kPerBitV4[b] && h != base, "bit %u alone gives %08X, expected %08X (and not the base)", b,
                      (unsigned)h, (unsigned)kPerBitV4[b]);
        }
        // Every version this build knows recognizes a ruled fingerprint; the
        // word-blind check does not, so a load that read the record but not the
        // word would call a healthy world damaged.
        const uint32_t stamped =
            Combo_ComputeComboSettingsHashEx2(&rec, NULL, kXw2SoaringPair, kXw2SettingsHash, kXw2ProfileDigest);
        XW2_CHECK(Combo_ComboSettingsHashGeneratorVersionEx2(&rec, NULL, kXw2SoaringPair, kXw2SettingsHash,
                                                             kXw2ProfileDigest,
                                                             stamped) == (int)RSBS_GENERATOR_VERSION &&
                      Combo_ComboSettingsHashGeneratorVersionEx2(&rec, NULL, kXw2SoaringPair, kXw2SettingsHash,
                                                                 kXw2ProfileDigest, 0xC5D0FBFBu) == 0,
                  "a ruled fingerprint must verify at this generator and at the legacy formula");
        XW2_CHECK(Combo_ComboSettingsHashGeneratorVersionEx(&rec, NULL, kXw2SettingsHash, kXw2ProfileDigest, stamped) <
                      0,
                  "the word-blind check reproduced a fingerprint stamped over the second word");
    }

    // ---- (6) The freeze, the KEEP set, the rollback and the retraction ---------------
    {
        Xw2FreezeWorld(kXw2SoaringPair);
        XW2_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == kXw2SoaringPair &&
                      Combo_ComboCrossItemFlags2() == kXw2SoaringPair &&
                      Combo_ComboCrossItemRule2(RSBS_XITEM2_SHARED_SONG_SOARING) &&
                      !Combo_ComboCrossItemRule2(RSBS_XITEM2_SONG_MINUET_MM),
                  "FreezeComboSettingsEx2 must store the word and the frozen accessors read it");
        XW2_CHECK(gComboCtx.comboSettingsHash == 0xE59A4747u && Combo_ComboSettingsWorldHash() == 0xC5D0FBFBu,
                  "the stamp is %08X and the world hash %08X; expected E59A4747 (v4) and C5D0FBFB (v0, the bag's)",
                  (unsigned)gComboCtx.comboSettingsHash, (unsigned)Combo_ComboSettingsWorldHash());
        XW2_CHECK((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) == 0u,
                  "a freshly frozen word does not reproduce its own fingerprint");
        Context_InvalidateSessionState(RSBS_SEED_STAMP_KEEP);
        XW2_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == kXw2SoaringPair &&
                      gComboCtx.comboSettingsHash == 0xE59A4747u,
                  "the file-create invalidation dropped the second word while keeping the fingerprint over it");
        Context_InvalidateSessionState(RSBS_SEED_STAMP_DROP);
        XW2_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == 0u && Combo_ComboCrossItemFlags2() == 0u,
                  "a DROP invalidation must drop the word with the record");

        // The plain Ex freeze is every second-word rule off, whatever was resident.
        Xw2FreezeWorld(kXw2SoaringPair);
        ComboSettingsRecord rec;
        Combo_ComboSettingsDefaults(&rec);
        Combo_FreezeComboSettingsEx(&rec, NULL);
        XW2_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == 0u,
                  "Combo_FreezeComboSettingsEx kept a stale second word");

        // The creation's two failure exits.
        Xw2FreezeWorld(0u);
        ComboCreationRecords prior;
        memset(&prior, 0xEE, sizeof(prior));
        Combo_SnapshotCreationRecords(&prior);
        XW2_CHECK(Combo_ComboItemRules2Word(&prior.comboItemRules2) == 0u, "the snapshot did not take the word");
        Combo_FreezeComboSettingsEx2(&rec, NULL, kXw2SoaringPair);
        Combo_RestoreCreationRecords(&prior);
        XW2_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == 0u &&
                      gComboCtx.comboSettingsHash == prior.comboSettingsHash &&
                      (Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) == 0u,
                  "the creation rollback left the failed generation's second word behind");
        Xw2FreezeWorld(kXw2SoaringPair);
        Combo_RetractCreationRecords();
        XW2_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == 0u,
                  "the creation retraction left the second word behind an ABSENT record");
        ComboContext_Init();
    }

    // ---- (7) Divergence and the frozen-record restore ---------------------------------
    {
        Xw2ClearKeys();
        Xw2FreezeWorld(kXw2SoaringPair);
        const uint32_t bits = Combo_ComboSettingsDivergence();
        char fields[192];
        Combo_ComboSettingsDivergenceDescribe(bits, fields, sizeof(fields));
        printf("[TEST] (7) a Soaring world under a session with no key: %08X (%s)\n", (unsigned)bits, fields);
        XW2_CHECK(bits == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS && !Combo_ComboSettingsDivergenceIsDamage(bits),
                  "the session diff is %08X; a frozen word this session would not freeze is named, not damage",
                  (unsigned)bits);
        const uint32_t frozen2 = Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2);
        // Hidden: another build's world, refused, nothing written.
        XW2_CHECK(Combo_ComboSettingsUnrestorableBitsEx2(&gComboCtx.comboItemRules, frozen2, bits) ==
                          RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS &&
                      Combo_ComboSettingsCanRestoreLiveEx2(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, frozen2,
                                                           &gComboCtx.comboTriforce, bits) == 0,
                  "while hidden, a file with a warp-song rule on must be refused as another build's");
        // Revealed: the keys put the file's word back, by name, in OoTMM's order.
        Combo_ComboItemRules2RevealForTest(1);
        char names[192];
        ComboSettingsKeyUndo undo;
        const int ok = Combo_ComboSettingsRestoreLiveEx2(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, frozen2,
                                                         &gComboCtx.comboTriforce, bits, names, sizeof(names), &undo);
        printf("[TEST] (7) revealed restore: %d (%s)\n", ok, names);
        XW2_CHECK(ok == 1 && strcmp(names, "Song of Soaring (OoT), Shared Song of Soaring") == 0,
                  "the restore wrote '%s', not the two Soaring rows", names);
        XW2_CHECK(Combo_ComboSettingsDivergence() == 0u, "after the restore the pair still diverges by %08X",
                  (unsigned)Combo_ComboSettingsDivergence());
        Combo_ComboSettingsRestoreUndo(&undo);
        XW2_CHECK(!Combo_ComboSettingIsExplicit(COMBO_SETTING_SONG_SOARING_OOT) &&
                      !Combo_ComboSettingIsExplicit(COMBO_SETTING_SHARED_SONG_SOARING),
                  "the undo did not put the keys back to unset");
        Combo_ComboItemRules2RevealForTest(0);

        // A word flipped after the stamp: the record and the hash name different worlds.
        Combo_ComboItemRules2Store(&gComboCtx.comboItemRules2, kXw2Soaring);
        XW2_CHECK((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) != 0u,
                  "a second word changed after the stamp must be FINGERPRINT damage");

        // A session's stray key against a file with every rule off: put back to
        // the file's "off", hidden or not (frozen wins at load, #781).
        Xw2FreezeWorld(0u);
        XW2_CHECK(Combo_ComboSettingsDivergence() == 0u, "a rules-off world under an unset session diverges");
        ComboContext_Init();
        XW2_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SONG_MINUET_MM, 1) == 1, "the writer refused the stray key");
        Xw2FreezeWorld(0u);
        const uint32_t stray = Combo_ComboSettingsDivergence();
        XW2_CHECK(stray == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS &&
                      Combo_ComboSettingsUnrestorableBitsEx2(&gComboCtx.comboItemRules, 0u, stray) == 0u,
                  "a stray second-word key against a rules-off file must be a restorable divergence (%08X)",
                  (unsigned)stray);
        const int back = Combo_ComboSettingsRestoreLiveEx2(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, 0u,
                                                           &gComboCtx.comboTriforce, stray, names, sizeof(names), NULL);
        XW2_CHECK(back == 1 && strcmp(names, "Minuet of Forest (MM)") == 0 &&
                      Combo_ComboSettingResolved(COMBO_SETTING_SONG_MINUET_MM) == 0 &&
                      Combo_ComboSettingsDivergence() == 0u,
                  "the stray key was not put back to the file's off (%d, '%s')", back, names);

        // Hidden, the session's keys EQUAL to the file's word (set from the
        // console, or by a later build's config): the arrival still refuses it
        // as another build's. The keys alone must not admit a hidden-rule world.
        Xw2ClearKeys();
        ComboContext_Init();
        XW2_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SONG_SOARING_OOT, 1) == 1 &&
                      Combo_ComboSettingSet(COMBO_SETTING_SHARED_SONG_SOARING, 1) == 1,
                  "the writers refused the Soaring keys before the freeze");
        Xw2FreezeWorld(kXw2SoaringPair);
        XW2_CHECK(Combo_ResolveComboItemRules2() == kXw2SoaringPair,
                  "the session resolves %08X, not the file's word: the leg below would not test equal keys",
                  (unsigned)Combo_ResolveComboItemRules2());
        const uint32_t keyed = Combo_ComboSettingsDivergence();
        printf("[TEST] (7) a Soaring world under the same keys, hidden: %08X\n", (unsigned)keyed);
        XW2_CHECK(keyed == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS &&
                      Combo_ComboSettingsUnrestorableBitsEx2(&gComboCtx.comboItemRules,
                                                             Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2),
                                                             keyed) == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                  "HIDDEN UNDER EQUAL KEYS: the arrival diff is %08X; a hidden-rule world must be refused as another "
                  "build's even when the session's keys match it",
                  (unsigned)keyed);
        // Revealed, the same keys and the same world: no divergence at all.
        Combo_ComboItemRules2RevealForTest(1);
        XW2_CHECK(Combo_ComboSettingsDivergence() == 0u,
                  "revealed, a Soaring world under its own keys diverges by %08X",
                  (unsigned)Combo_ComboSettingsDivergence());
        Combo_ComboItemRules2RevealForTest(0);
        Xw2ClearKeys();
        ComboContext_Init();
    }

    // ---- (8) THE SEED PACKAGE: the world record carries the word ---------------------
    {
        Xw2FreezeWorld(kXw2SoaringPair);
        gComboCtx.sharedFamilyOwned = 0x5u; // session state the record must NOT carry
        ComboContext live;
        memcpy(&live, &gComboCtx, sizeof(live));
        ComboContext_Init();
        ComboContext* carried = new ComboContext;
        memset(carried, 0xEE, sizeof(*carried));
        const int rt = OoT_SeedPackage_WorldRecordRoundTripForTest(&live, carried);
        XW2_CHECK(rt == 1 && Combo_ComboItemRules2Word(&carried->comboItemRules2) == kXw2SoaringPair &&
                      carried->sharedFamilyOwned == 0u,
                  "placement/combo-context.bin carries word %08X (family bits %08X); expected the world's %08X and "
                  "no session state",
                  (unsigned)Combo_ComboItemRules2Word(&carried->comboItemRules2), (unsigned)carried->sharedFamilyOwned,
                  (unsigned)kXw2SoaringPair);
        XW2_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == kXw2SoaringPair &&
                      gComboCtx.comboSettingsHash == 0xE59A4747u &&
                      Combo_ComboSettingsHashGeneratorVersionEx2(
                          &gComboCtx.comboSettings, &gComboCtx.comboItemRules,
                          Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2), gComboCtx.sharedRandoSettingsHash,
                          gComboCtx.mmProfileDigest, gComboCtx.comboSettingsHash) == (int)RSBS_GENERATOR_VERSION,
                  "ApplyComboWorld did not bind the package's second word with the fingerprint over it");
        delete carried;
        ComboContext_Init();
    }

    // ---- (9) THE .redsave: carried, refused while hidden, restored once revealed ----
    {
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        const std::string priorSaveDir = std::filesystem::path(mgr.SlotPath(0)).parent_path().string();
        mgr.SetSaveDirectory(rsbs::test::ScratchDir(kXw2LoadDir).string());

        // Hidden: a later build's warp-song world is refused, not quarantined.
        Xw2ClearKeys();
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        ComboContext_Init();
        Xw2SeedShadows(0x69u);
        Xw2FreezeWorld(kXw2SoaringPair);
        Xw2SeedShadows(0x69u);
        XW2_CHECK(mgr.Save(0), "Save(0) failed for the Soaring file");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        const bool hiddenLoaded = mgr.Load(0);
        printf("[TEST] (9) hidden: loaded %d, quarantine %d, file present %d (expected 0, 0, 1)\n",
               hiddenLoaded ? 1 : 0, RsbsSave_HasQuarantine(0), RsbsSave_HasSave(0));
        XW2_CHECK(!hiddenLoaded, "a file with a hidden warp-song rule on LOADED: this build cannot play it");
        XW2_CHECK(RsbsSave_HasQuarantine(0) == 0 && RsbsSave_HasSave(0) == 1,
                  "a rule this build does not offer is not damage: the .redsave must stay where it is");

        // Hidden, the session's keys the file's (the console, or a later build's
        // config): still refused as another build's, not quarantined, and the
        // word never becomes resident.
        XW2_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SONG_SOARING_OOT, 1) == 1 &&
                      Combo_ComboSettingSet(COMBO_SETTING_SHARED_SONG_SOARING, 1) == 1,
                  "the writers refused the keys before the hidden keyed load");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        const bool hiddenKeyedLoaded = mgr.Load(0);
        printf("[TEST] (9) hidden, keys equal to the file's: loaded %d, quarantine %d, file present %d, resident word "
               "%08X (expected 0, 0, 1, 00000000)\n",
               hiddenKeyedLoaded ? 1 : 0, RsbsSave_HasQuarantine(0), RsbsSave_HasSave(0),
               (unsigned)Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2));
        XW2_CHECK(!hiddenKeyedLoaded && Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == 0u,
                  "a file with a hidden warp-song rule on LOADED under keys equal to it: the keys alone must not "
                  "admit a world this build cannot play");
        XW2_CHECK(RsbsSave_HasQuarantine(0) == 0 && RsbsSave_HasSave(0) == 1,
                  "a hidden rule under equal keys is not damage: the .redsave must stay where it is");

        // Revealed, the session's keys the file's: the word round-trips exactly.
        Combo_ComboItemRules2RevealForTest(1);
        XW2_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SONG_SOARING_OOT, 1) == 1 &&
                      Combo_ComboSettingSet(COMBO_SETTING_SHARED_SONG_SOARING, 1) == 1,
                  "the writers refused the keys before the load");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        const bool keyedLoaded = mgr.Load(0);
        XW2_CHECK(keyedLoaded && Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == kXw2SoaringPair &&
                      gComboCtx.comboSettingsHash == 0xE59A4747u && Combo_ComboSettingsDivergence() == 0u,
                  "THE .redsave ROUND TRIP: loaded %d, word %08X, fingerprint %08X", keyedLoaded ? 1 : 0,
                  (unsigned)Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2),
                  (unsigned)gComboCtx.comboSettingsHash);

        // Revealed, the session's keys unset: frozen wins at load, the keys come back.
        Xw2ClearKeys();
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        const bool restoredLoaded = mgr.Load(0);
        XW2_CHECK(restoredLoaded && Combo_ComboSettingResolved(COMBO_SETTING_SONG_SOARING_OOT) == 1 &&
                      Combo_ComboSettingResolved(COMBO_SETTING_SHARED_SONG_SOARING) == 1 &&
                      Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == kXw2SoaringPair,
                  "the load did not restore the file's warp-song keys (loaded %d)", restoredLoaded ? 1 : 0);
        Combo_ComboItemRules2RevealForTest(0);

        Xw2ClearKeys();
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        mgr.SetSaveDirectory(priorSaveDir);
        ComboContext_Init();
    }

    Combo_ComboItemRules2RevealForTest(0);
    Xw2ClearKeys();
    ComboContext_Init();
    printf("[TEST] combo-item-rules-second-word: %s\n", sXw2Failures == 0 ? "PASS" : "FAIL");
    return sXw2Failures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// rules-off-goldens-unchanged
// ============================================================================

TestResult Test_RulesOffGoldensUnchanged(void) {
    printf("[TEST] rules-off-goldens-unchanged: with every second-word rule off, the fingerprint, the world hash, "
           "the single bag's seeds, the divergence diff and a seed package's record are main's (#1169 XW2)\n");
    sXw2Failures = 0;
    Combo_ComboItemRules2RevealForTest(0);

    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    ComboItemRulesRecord on;
    memset(&on, 0, sizeof(on));
    on.crossItemFlags = 0x00000825u;
    on.blastMaskCooldown = (uint8_t)RSBS_BLAST_COOLDOWN_SHORT;

    // ---- (1) The formula: a 0 word is no term, at every version ----------------------
    {
        const ComboItemRulesRecord* rules[2] = { NULL, &on };
        for (int r = 0; r < 2; r++) {
            for (uint32_t v = 0; v <= RSBS_GENERATOR_VERSION; v++) {
                const uint32_t before = Combo_ComputeComboSettingsHashForGeneratorEx(
                    &defaults, rules[r], kXw2SettingsHash, kXw2ProfileDigest, v);
                const uint32_t after = Combo_ComputeComboSettingsHashForGeneratorEx2(
                    &defaults, rules[r], 0u, kXw2SettingsHash, kXw2ProfileDigest, v);
                XW2_CHECK(before == after, "rules %d at generator %u: %08X with a 0 second word, %08X without", r,
                          (unsigned)v, (unsigned)after, (unsigned)before);
            }
        }
        // Main's vectors, computed outside the build with main's formula (the
        // defaults' v0 and v1 are test_combo_settings.c's pins, the ON record's
        // v0 and v4 test_cross_item_rules.c's).
        XW2_CHECK(Combo_ComputeComboSettingsHashForGeneratorEx2(&defaults, NULL, 0u, kXw2SettingsHash,
                                                                kXw2ProfileDigest, 0u) == 0xCEA3786Bu &&
                      Combo_ComputeComboSettingsHashForGeneratorEx2(&defaults, NULL, 0u, kXw2SettingsHash,
                                                                    kXw2ProfileDigest, 1u) == 0x0F8DDDA2u &&
                      Combo_ComputeComboSettingsHashForGeneratorEx2(&defaults, NULL, 0u, kXw2SettingsHash,
                                                                    kXw2ProfileDigest, 4u) == 0xA06B18F7u,
                  "a rules-off world's fingerprint moved off main's vectors");
        XW2_CHECK(Combo_ComputeComboSettingsHashForGeneratorEx2(&defaults, &on, 0u, kXw2SettingsHash, kXw2ProfileDigest,
                                                                0u) == 0x4D8B1306u &&
                      Combo_ComputeComboSettingsHashForGeneratorEx2(&defaults, &on, 0u, kXw2SettingsHash,
                                                                    kXw2ProfileDigest, 4u) == 0x56B84E70u,
                  "a first-word world's fingerprint moved off main's vectors when the second word is off");
        XW2_CHECK(Combo_ComputeComboSettingsHashEx2(&defaults, &on, 0u, kXw2SettingsHash, kXw2ProfileDigest) ==
                      Combo_ComputeComboSettingsHashEx(&defaults, &on, kXw2SettingsHash, kXw2ProfileDigest),
                  "the production stamp differs with the second word off");
        for (uint32_t v = 0; v <= RSBS_GENERATOR_VERSION; v++) {
            const uint32_t stored =
                Combo_ComputeComboSettingsHashForGeneratorEx(&defaults, &on, kXw2SettingsHash, kXw2ProfileDigest, v);
            XW2_CHECK(Combo_ComboSettingsHashGeneratorVersionEx2(&defaults, &on, 0u, kXw2SettingsHash,
                                                                 kXw2ProfileDigest, stored) == (int)v,
                      "a file stamped by generator %u no longer verifies as that generator", (unsigned)v);
        }
    }

    // ---- (2) A frozen rules-off world: stamp, world hash and the bag's seeds ---------
    {
        Xw2FreezeWorld(0u);
        const uint32_t viaEx2 = gComboCtx.comboSettingsHash;
        XW2_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == 0u && Combo_ComboCrossItemFlags2() == 0u,
                  "a rules-off freeze left a second-word bit");
        XW2_CHECK(viaEx2 == 0xA06B18F7u && Combo_ComboSettingsWorldHash() == 0xCEA3786Bu,
                  "a rules-off world stamps %08X with world hash %08X; main's are A06B18F7 and CEA3786B",
                  (unsigned)viaEx2, (unsigned)Combo_ComboSettingsWorldHash());
        // The single bag's three seeds, computed outside the build from main's
        // SingleBagIdentityHash over (seed 0169A0FF, DEADBEEF, 0BADF00D, CEA3786B).
        static const uint32_t kBagSeeds[3] = { 0x7ED4B1E3u, 0x2EA872D2u, 0xDE7C33C1u };
        for (int a = 0; a < 3; a++) {
            XW2_CHECK(Combo_SingleBag_SeedFor(a) == kBagSeeds[a], "the bag seed for attempt %d is %08X; main's is %08X",
                      a, (unsigned)Combo_SingleBag_SeedFor(a), (unsigned)kBagSeeds[a]);
        }
        XW2_CHECK(Combo_SingleBag_TrimSeed() == 0x6F81C454u && Combo_SingleBag_FoolishSeed() == 0x4D84BC5Bu,
                  "the trim seed %08X or the foolish seed %08X moved off main's (6F81C454, 4D84BC5B)",
                  (unsigned)Combo_SingleBag_TrimSeed(), (unsigned)Combo_SingleBag_FoolishSeed());
        // The pre-XW2 freeze stamps the same world.
        ComboContext_Init();
        gComboCtx.sourceIsRando = true;
        gComboCtx.sharedRandoSeed = kXw2Seed;
        gComboCtx.sharedRandoSettingsHash = kXw2SettingsHash;
        gComboCtx.mmProfileDigest = kXw2ProfileDigest;
        Combo_FreezeComboSettingsEx(&defaults, NULL);
        XW2_CHECK(gComboCtx.comboSettingsHash == viaEx2 && Combo_SingleBag_SeedFor(0) == kBagSeeds[0],
                  "the Ex and Ex2 freezes stamp different worlds with every second-word rule off");
        // The divergence diff over the resident identity is the Ex form's.
        XW2_CHECK(Combo_ComboSettingsDivergenceForEx2(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, 0u,
                                                      gComboCtx.comboSettingsHash, gComboCtx.sharedRandoSettingsHash,
                                                      gComboCtx.mmProfileDigest) ==
                      Combo_ComboSettingsDivergenceForEx(&gComboCtx.comboSettings, &gComboCtx.comboItemRules,
                                                         gComboCtx.comboSettingsHash, gComboCtx.sharedRandoSettingsHash,
                                                         gComboCtx.mmProfileDigest),
                  "the load's divergence check differs for a rules-off file");
        XW2_CHECK((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) == 0u,
                  "a rules-off world is FINGERPRINT damage");
        XW2_CHECK(Combo_ComboItemRules2Refused(0u, NULL, 0) == 0u,
                  "Generate, a package and a load must take a rules-off world");
    }

    // ---- (3) The seed package's record of a rules-off world --------------------------
    {
        ComboContext live;
        memcpy(&live, &gComboCtx, sizeof(live));
        ComboContext* carried = new ComboContext;
        memset(carried, 0xEE, sizeof(*carried));
        const int rt = OoT_SeedPackage_WorldRecordRoundTripForTest(&live, carried);
        static const uint8_t kZero[4] = { 0, 0, 0, 0 };
        XW2_CHECK(rt == 1 && memcmp(carried->comboItemRules2.crossItemFlags2, kZero, sizeof(kZero)) == 0 &&
                      carried->comboSettingsHash == 0xA06B18F7u,
                  "a rules-off world's package record carries second-word bytes, or another fingerprint");
        XW2_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == 0u &&
                      gComboCtx.comboSettingsHash == 0xA06B18F7u && Combo_SingleBag_SeedFor(0) == 0x7ED4B1E3u,
                  "applying a rules-off package moved the world");
        delete carried;
    }

    ComboContext_Init();
    printf("[TEST] rules-off-goldens-unchanged: %s\n", sXw2Failures == 0 ? "PASS" : "FAIL");
    return sXw2Failures == 0 ? TEST_PASS : TEST_FAIL;
}
