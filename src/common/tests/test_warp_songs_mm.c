/**
 * @file test_warp_songs_mm.c
 * @brief Locks for Ocarina of Time's six warp songs as Majora's Mask items and
 *        their ownership byte (#1169 lane XW5; OoTMM songMinuetMm ..
 *        songPreludeMm; the items are games/mm/2s2h/Rando/WarpSongsMM.cpp, the
 *        rules and the byte src/common/warp_songs_mm.c, the carve context.h's
 *        mmWarpSongsOwned, ADR 0009 claim 20).
 *
 * Three ROM-free rows here, split by what each one needs, and one rando row in
 * games/mm/2s2h/mm_warp_songs_mm_test.cpp:
 *
 *   mm-warp-songs-items (CTest MMWarpSongsItems, redship tier; MM's pool reads
 *   the CVar store, so the runner brings up the display-free shared context
 *   first) -- THE ITEMS. The carve (1003, the tail of reserved[], which keeps 74
 *   bytes at 929, over ADR 0009's floor) and the ownership bits; the six ids
 *   appended after the masks with every saved id unchanged; their rows
 *   (RITYPE_MAJOR, OoTMM's names, the text and hint name, no ItemId, progression
 *   to the bag, MM's song note as icon, OoT's own song as model, the tooltips
 *   without the Song Locations sentence); MM's REAL give (an owned bit and no byte
 *   of MM's save) and conversion (a second copy is junk), ownership gated by the
 *   frozen rule; the copies rule over every combination; MM's REAL pool
 *   (GeneratePools) with the rules off, on and shared, and a starting copy left
 *   out; MM's REAL logic macro over every button combination against OoTMM's
 *   can_play_tp_* sets; and the snapshot brackets (the bag's MM engine gives a
 *   song and puts the byte back; from source, MM's crawl, its glitchless fill and
 *   the attempt ladder do too).
 *
 *   mm-warp-songs-ownership (CTest MMWarpSongsOwnershipSurvives, redship tier, the
 *   same bring-up: a load restores keys) -- OWNERSHIP SURVIVES. A starting song
 *   granted by the creation's real grant is dropped by the file-create KEEP
 *   invalidation (the trap) and put back by the creation's bind of the seed
 *   package's world record; it survives an MM in-game save (the production
 *   capture funnel) and the .redsave load after it; a pickup after that survives
 *   a departure's freeze and restore, the crossing's commit and a reload; the
 *   fingerprint never moves with it.
 *
 *   mm-warp-songs-rules-off-unchanged (CTest MMWarpSongsRulesOffUnchanged,
 *   redship tier, ROM-free, no CVar store) -- INERT. Every MM warp-song bit is
 *   refused at Generate while hidden; a rules-off world pools no copy and credits
 *   none; with every rule off and the byte at 0 or at every song owned, the
 *   fingerprint, the world hash, the bag's seeds and a seed package's record are
 *   main's (XW2's vectors); from source, the pool adds a song only through the
 *   copies rule and before the starting items leave it.
 *
 *   mm-warp-songs-pooled-only-when-rule-on (CTest MMWarpSongsPooledOnlyWhenRuleOn,
 *   rando tier) -- THE POOL over real paired creations: rules off, all six on
 *   through the test seam, and the Shared Minuet of Forest too (held back).
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Helpers carry the Wsm
 * prefix because every included test file shares one translation unit.
 */

#include "../combo_logic.h"
#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../foreign_model.h"
#include "../save.h"
#include "../shared_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../test_scratch.h"
#include "../warp_songs_mm.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>

#ifdef __cplusplus
extern "C" {
#endif
// games/mm/2s2h/mm_warp_songs_mm_test.cpp
int MM_WarpSongsTest_Id(int which);
void MM_WarpSongsTest_ArmLive(void);
void MM_WarpSongsTest_Give(uint16_t ri);
int MM_WarpSongsTest_Convert(uint16_t ri);
int MM_WarpSongsTest_GiveTouchesSave(uint16_t ri);
int MM_WarpSongsTest_RowFact(uint16_t ri, int fact);
const char* MM_WarpSongsTest_Name(uint16_t ri);
const char* MM_WarpSongsTest_TextName(uint16_t ri);
int MM_WarpSongsTest_IsJunk(int ri);
int MM_WarpSongsTest_CanPlay(uint16_t ri, int ocarina, uint32_t buttons);
int MM_WarpSongsTest_PoolCopies(uint16_t ri, uint16_t startingItem);
int MM_WarpSongsTest_StartingGrant(uint16_t ri);
int MM_WarpSongsTest_FreezeAndRestore(void);
// games/mm/2s2h/Rando/WarpSongsMM.cpp
uint32_t MM_WarpSongMM_RuleBit(uint16_t riId);
uint16_t MM_WarpSongMM_ItemId(uint32_t ruleBit);
bool MM_WarpSongMM_RuleOn(uint16_t riId);
int MM_WarpSongMM_PooledCopies(uint16_t riId);
bool MM_WarpSongMM_Owned(uint16_t riId);
bool MM_WarpSongMM_Usable(uint16_t riId);
// games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp
int MM_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
int MM_ComboLogic_ProbeGives(const uint16_t* ids, int count, int startIndex);
// games/mm/2s2h/Rando/ForeignModelSingleExe.cpp
const char* MM_ComboModel_TestItemReason(uint16_t id);
// games/mm/2s2h/GameExports_SingleExe.cpp: the commit an MM in-game save makes.
int MM_Combo_CaptureSaveToUnifiedSlot(void);
// games/oot/soh/Enhancements/randomizer/SeedPackageSingleExe.cpp: a package's
// world-record copy and its apply, end to end.
int OoT_SeedPackage_WorldRecordRoundTripForTest(const ComboContext* live, ComboContext* outRecordBytes);
#ifdef __cplusplus
}
#endif

static int sWsmFailures;

#define WSM_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sWsmFailures++;                           \
        }                                             \
    } while (0)

namespace {

// The identity terms of XW2's pinned vectors (test_combo_item_rules_word2.c), so a
// rules-off world here stamps main's fingerprint.
const uint32_t kWsmSettingsHash = 0xDEADBEEFu;
const uint32_t kWsmProfileDigest = 0x0BADF00Du;
const uint32_t kWsmSeed = 0x0169A0FFu;
const char* const kWsmSaveDir = "rsbs_test_warp_songs_mm";

/** One song as XW2 allocated its rule and this lane its ownership bit, spelled as
 *  literals: both are .redsave and seed-package format. */
struct WsmSong {
    uint32_t rule;   // RSBS_XITEM2_SONG_*_MM
    uint32_t shared; // RSBS_XITEM2_SHARED_SONG_*
    uint8_t owned;   // RSBS_MM_WARP_SONG_OWNED_*
    ComboSettingId key;
    const char* name;    // OoTMM's option name, the MM item's name
    const char* ootName; // Ocarina of Time's own item
    const char* ootmmKey;
    uint32_t buttons; // OoTMM's can_play_tp_*: bit 0 A, 1 C-up, 2 C-down, 3 C-left, 4 C-right
};
const uint32_t kWsmBtnA = 0x01u;
const uint32_t kWsmBtnUp = 0x02u;
const uint32_t kWsmBtnDown = 0x04u;
const uint32_t kWsmBtnLeft = 0x08u;
const uint32_t kWsmBtnRight = 0x10u;
const WsmSong kWsmSongs[6] = {
    { 0x0002u, 0x0100u, 0x01u, COMBO_SETTING_SONG_MINUET_MM, "Minuet of Forest (MM)", "Minuet of Forest",
      "songMinuetMm", kWsmBtnA | kWsmBtnUp | kWsmBtnRight | kWsmBtnLeft },
    { 0x0004u, 0x0200u, 0x02u, COMBO_SETTING_SONG_BOLERO_MM, "Bolero of Fire (MM)", "Bolero of Fire", "songBoleroMm",
      kWsmBtnA | kWsmBtnRight | kWsmBtnDown },
    { 0x0008u, 0x0400u, 0x04u, COMBO_SETTING_SONG_SERENADE_MM, "Serenade of Water (MM)", "Serenade of Water",
      "songSerenadeMm", kWsmBtnA | kWsmBtnRight | kWsmBtnDown | kWsmBtnLeft },
    { 0x0010u, 0x0800u, 0x08u, COMBO_SETTING_SONG_REQUIEM_MM, "Requiem of Spirit (MM)", "Requiem of Spirit",
      "songRequiemMm", kWsmBtnA | kWsmBtnRight | kWsmBtnDown },
    { 0x0020u, 0x1000u, 0x10u, COMBO_SETTING_SONG_NOCTURNE_MM, "Nocturne of Shadow (MM)", "Nocturne of Shadow",
      "songNocturneMm", kWsmBtnA | kWsmBtnRight | kWsmBtnDown | kWsmBtnLeft },
    { 0x0040u, 0x2000u, 0x20u, COMBO_SETTING_SONG_PRELUDE_MM, "Prelude of Light (MM)", "Prelude of Light",
      "songPreludeMm", kWsmBtnUp | kWsmBtnRight | kWsmBtnLeft },
};
const uint32_t kWsmAllRules = 0x0000007Eu;  // the six, not songSoaringOot
const uint32_t kWsmAllShared = 0x00003F00u; // their six shared songs

uint16_t WsmId(int i) {
    return (uint16_t)MM_WarpSongsTest_Id(i);
}

/** Publish a paired identity and freeze the shipped combo record with every
 *  first-word rule off and @p rules2 as the second word, the way Playthrough_Init
 *  stamps a world. Starts from a fresh record, so nothing is owned. */
void WsmFreezeWorld(uint32_t rules2) {
    ComboContext_Init();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kWsmSeed;
    gComboCtx.sharedRandoSettingsHash = kWsmSettingsHash;
    gComboCtx.mmProfileDigest = kWsmProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx2(&rec, NULL, rules2);
}

/** WsmFreezeWorld keeping the ownership byte as it was. */
void WsmRefreezeKeepingOwnership(uint32_t rules2) {
    const uint8_t owned = Combo_MMWarpSongsOwned();
    WsmFreezeWorld(rules2);
    Combo_MMWarpSongsSetOwned(owned);
}

void WsmClearKeys() {
    if (!Combo_ComboSettingStoreAvailable()) {
        return;
    }
    for (const WsmSong& s : kWsmSongs) {
        CVarClear(Combo_ComboSettingKey(s.key));
    }
    const ComboSettingId shared[6] = { COMBO_SETTING_SHARED_SONG_MINUET,   COMBO_SETTING_SHARED_SONG_BOLERO,
                                       COMBO_SETTING_SHARED_SONG_SERENADE, COMBO_SETTING_SHARED_SONG_REQUIEM,
                                       COMBO_SETTING_SHARED_SONG_NOCTURNE, COMBO_SETTING_SHARED_SONG_PRELUDE };
    for (const ComboSettingId id : shared) {
        CVarClear(Combo_ComboSettingKey(id));
    }
}

/** The text of @p rel under the checkout, or "" when it cannot be read. */
std::string WsmReadSource(const char* rel) {
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

int WsmCount(const std::string& text, const char* needle) {
    int n = 0;
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) {
        n++;
    }
    return n;
}

} // namespace

// ============================================================================
// mm-warp-songs-items
// ============================================================================

TestResult MMWarpSongsItems_Run(void) {
    printf("[TEST] mm-warp-songs-items: Ocarina of Time's six warp songs are MM items appended after the masks, "
           "named as OoTMM names them, owned through one byte of the combo record only under their frozen rules, "
           "pooled once each unless shared, played with OoTMM's buttons, and no simulated pickup owns one "
           "(#1169 XW5)\n");
    sWsmFailures = 0;
    Combo_ComboItemRules2RevealForTest(0);
    ComboContext_Init();

    // ---- (0) The carve and the ownership bits --------------------------------------
    {
        WSM_CHECK(offsetof(ComboContext, mmWarpSongsOwned) == 1003u && offsetof(ComboContext, reserved) == 929u &&
                      sizeof(((ComboContext*)0)->reserved) == 74u &&
                      offsetof(ComboContext, trackerAvailCountOoT) == 1004u && sizeof(ComboContext) == 1844u,
                  "the ownership byte is at %u and reserved[] is %u bytes at %u (sizeof %u); expected 1003, the tail "
                  "of reserved[], which keeps 74 bytes at 929, and nothing behind it moved",
                  (unsigned)offsetof(ComboContext, mmWarpSongsOwned), (unsigned)sizeof(((ComboContext*)0)->reserved),
                  (unsigned)offsetof(ComboContext, reserved), (unsigned)sizeof(ComboContext));
        WSM_CHECK(sizeof(((ComboContext*)0)->reserved) >= 64u, "the carve took reserved[] under ADR 0009's floor");
        WSM_CHECK(RSBS_MM_WARP_SONG_OWNED_ALL == 0x3Fu && RSBS_MM_WARP_SONG_COUNT == 6u,
                  "the six ownership bits are not 0x3F");
        for (const WsmSong& s : kWsmSongs) {
            WSM_CHECK(Combo_MMWarpSongOwnedBit(s.rule) == s.owned && Combo_MMWarpSongRuleBit(s.owned) == s.rule,
                      "%s's ownership bit is %02X, expected %02X (its rule bit %04X shifted down by one)", s.name,
                      (unsigned)Combo_MMWarpSongOwnedBit(s.rule), (unsigned)s.owned, (unsigned)s.rule);
        }
        WSM_CHECK(Combo_MMWarpSongOwnedBit(0x0001u) == 0u && Combo_MMWarpSongOwnedBit(0x0100u) == 0u &&
                      Combo_MMWarpSongOwnedBit(0x0006u) == 0u && Combo_MMWarpSongRuleBit(0x40u) == 0u,
                  "the Song of Soaring (OoT), a shared song, a two-bit mask or an unallocated ownership bit maps to a "
                  "song");
        ComboContext_Init();
        WSM_CHECK(Combo_MMWarpSongsOwned() == 0u, "a fresh (zero-extended) record owns a song");
        gComboCtx.mmWarpSongsOwned = 0xFFu;
        WSM_CHECK(Combo_MMWarpSongsOwned() == 0x3Fu, "bits 6-7 of the byte read as owned songs");
        Combo_MMWarpSongsSetOwned(0xC1u);
        WSM_CHECK(gComboCtx.mmWarpSongsOwned == 0x01u, "a restore stored bits 6-7 (%02X)",
                  (unsigned)gComboCtx.mmWarpSongsOwned);
        ComboContext_Init();
    }

    // ---- (1) The ids: appended after the masks, every saved id unchanged -----------
    {
        const int spooky = MM_WarpSongsTest_Id(6);
        const int maxTrap = MM_WarpSongsTest_Id(7);
        const int riMax = MM_WarpSongsTest_Id(8);
        printf("[TEST] (1) RI_MASK_SPOOKY %d, the songs %d..%d, RI_MAX_TRAP %d, RI_MAX %d\n", spooky,
               MM_WarpSongsTest_Id(0), MM_WarpSongsTest_Id(5), maxTrap, riMax);
        bool consecutive = true;
        for (int i = 0; i < 6; i++) {
            consecutive = consecutive && MM_WarpSongsTest_Id(i) == spooky + 1 + i;
        }
        WSM_CHECK(consecutive && maxTrap > MM_WarpSongsTest_Id(5) && riMax == maxTrap + 1,
                  "the six ids are not appended after the masks and before RI_MAX_TRAP");
        // Saved ids (randoSaveChecks[].randoItemId, the crossing store) pinned at
        // their pre-XW5 values: RI_WOODFALL_STRAY_FAIRY and the masks P5 appended.
        WSM_CHECK(MM_WarpSongsTest_Id(9) == 234 && MM_WarpSongsTest_Id(12) == 235 && spooky == 237 &&
                      MM_WarpSongsTest_Id(11) == 56 && MM_WarpSongsTest_Id(0) == 238 && MM_WarpSongsTest_Id(5) == 243,
                  "a saved RI_* value moved: RI_WOODFALL_STRAY_FAIRY %d (234), RI_MASK_GERUDO %d (235), "
                  "RI_MASK_SPOOKY %d (237), RI_JUNK %d (56), Minuet %d (238), Prelude %d (243)",
                  MM_WarpSongsTest_Id(9), MM_WarpSongsTest_Id(12), spooky, MM_WarpSongsTest_Id(11),
                  MM_WarpSongsTest_Id(0), MM_WarpSongsTest_Id(5));
        for (int i = 0; i < 6; i++) {
            WSM_CHECK(MM_WarpSongMM_RuleBit(WsmId(i)) == kWsmSongs[i].rule &&
                          MM_WarpSongMM_ItemId(kWsmSongs[i].rule) == WsmId(i),
                      "%s and its rule %04X do not map onto each other", kWsmSongs[i].name,
                      (unsigned)kWsmSongs[i].rule);
        }
        WSM_CHECK(MM_WarpSongMM_RuleBit((uint16_t)MM_WarpSongsTest_Id(10)) == 0u &&
                      MM_WarpSongMM_RuleBit((uint16_t)spooky) == 0u && MM_WarpSongMM_ItemId(0x0001u) == 0u,
                  "MM's own Song of Soaring, a mask or OoT's Soaring rule maps onto an MM warp song");
    }

    // ---- (2) The rows: names, type, model, icon, tooltips ------------------------------
    {
        WSM_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table is not available");
        for (int i = 0; i < 6; i++) {
            const WsmSong& s = kWsmSongs[i];
            const uint16_t ri = WsmId(i);
            const char* name = MM_WarpSongsTest_Name(ri);
            WSM_CHECK(name != NULL && strcmp(name, s.name) == 0 && strcmp(Combo_MMWarpSongName(s.rule), s.name) == 0,
                      "id %u is named '%s', not OoTMM's '%s'", (unsigned)ri, name != NULL ? name : "(none)", s.name);
            const std::string text = std::string("the ") + s.name;
            const char* textName = MM_WarpSongsTest_TextName(ri);
            WSM_CHECK(textName != NULL && text == textName, "MM's texts and hints name %s '%s', not '%s'", s.name,
                      textName != NULL ? textName : "(none)", text.c_str());
            static const char* const kFacts[5] = { "a RITYPE_MAJOR row", "an item with no MM ItemId",
                                                   "an item with no get-item row", "an item with no draw row",
                                                   "shown with MM's own song note" };
            for (int f = 0; f < 5; f++) {
                WSM_CHECK(MM_WarpSongsTest_RowFact(ri, f) == 1, "%s is not %s (%d)", s.name, kFacts[f],
                          MM_WarpSongsTest_RowFact(ri, f));
            }
            ComboItemClassRow row;
            memset(&row, 0, sizeof(row));
            WSM_CHECK(MM_ComboLogic_ClassifyItem(ri, &row) == 1 && row.fillClass == RSBS_FILL_CLASS_PROGRESSION &&
                          row.armedBy == 0u,
                      "%s is not an unarmed progression item to the bag", s.name);
            // Hosted by OoT it answers the masks' named no-model; in MM its model
            // is OoT's own song, which MM draws as its own note (host-native).
            const char* why = MM_ComboModel_TestItemReason(ri);
            WSM_CHECK(why != NULL && strstr(why, "custom draw") != NULL,
                      "%s's answer as a model in OoT is '%s', not the named custom-draw no-model", s.name,
                      why != NULL ? why : "a model");
            SharedItem oot;
            memset(&oot, 0, sizeof(oot));
            WSM_CHECK(TestNamedItem((uint8_t)GAME_OOT, s.ootName, &oot) &&
                          strcmp(Combo_MMWarpSongOoTName(s.rule), s.ootName) == 0,
                      "OoT has no item named \"%s\"", s.ootName);
            ComboModelAnswer answer;
            memset(&answer, 0, sizeof(answer));
            const uint8_t kind = Combo_GetForeignItemModel((uint8_t)GAME_MM, oot, &answer);
            WSM_CHECK(kind == (uint8_t)COMBO_MODEL_ANSWER_HOST_NATIVE,
                      "MM does not draw OoT's %s as its own note (answer kind %u): the song's get-item model would "
                      "fall back to the generic note",
                      s.ootName, (unsigned)kind);
            // The tooltip: OoTMM's first sentence, the Song Locations one dropped.
            const char* tip = Combo_MMWarpSongTooltip(s.rule);
            const std::string want = std::string("Add the ") + s.ootName + " in Majora's Mask.";
            WSM_CHECK(tip != NULL && want == tip && strstr(tip, "Song Locations") == NULL,
                      "%s's tooltip is '%s', not '%s'", s.name, tip != NULL ? tip : "(none)", want.c_str());
        }
        WSM_CHECK(Combo_MMWarpSongName(0x0001u) == NULL && Combo_MMWarpSongTooltip(0x0100u) == NULL,
                  "the Song of Soaring (OoT) or a shared song has an MM song's name");
    }

    // ---- (3) The give, the conversion and ownership ----------------------------------
    {
        WsmFreezeWorld(kWsmAllRules);
        MM_WarpSongsTest_ArmLive();
        uint8_t expect = 0u;
        for (int i = 0; i < 6; i++) {
            const WsmSong& s = kWsmSongs[i];
            const uint16_t ri = WsmId(i);
            WSM_CHECK(!MM_WarpSongMM_Owned(ri) && !MM_WarpSongMM_Usable(ri), "a fresh record owns %s", s.name);
            WSM_CHECK(MM_WarpSongsTest_Convert(ri) == (int)ri, "an unowned %s converted to %d", s.name,
                      MM_WarpSongsTest_Convert(ri));
            WSM_CHECK(MM_WarpSongsTest_GiveTouchesSave(ri) == 0,
                      "MM's give of %s changed a byte of MM's save: it must set its owned bit and nothing else",
                      s.name);
            expect = (uint8_t)(expect | s.owned);
            WSM_CHECK(Combo_MMWarpSongsOwned() == expect && MM_WarpSongMM_Owned(ri) && MM_WarpSongMM_Usable(ri),
                      "MM's give of %s left the byte at %02X, expected %02X", s.name,
                      (unsigned)Combo_MMWarpSongsOwned(), (unsigned)expect);
            const int second = MM_WarpSongsTest_Convert(ri);
            WSM_CHECK(second != (int)ri && MM_WarpSongsTest_IsJunk(second), "a second %s converted to %d, not junk",
                      s.name, second);
        }
        WSM_CHECK(gComboCtx.mmWarpSongsOwned == 0x3Fu, "after the six gives the byte is %02X, not 3F",
                  (unsigned)gComboCtx.mmWarpSongsOwned);
        // The rule gates use: the bit stays, the song is not usable.
        WsmRefreezeKeepingOwnership(kWsmAllRules & ~kWsmSongs[0].rule);
        WSM_CHECK(MM_WarpSongMM_Owned(WsmId(0)) && !MM_WarpSongMM_Usable(WsmId(0)) && MM_WarpSongMM_Usable(WsmId(1)),
                  "with the Minuet's rule off it is still usable (or the Bolero is not)");
        ComboContext_Init();
        WSM_CHECK(!MM_WarpSongMM_Usable(WsmId(1)) && !MM_WarpSongMM_RuleOn(WsmId(1)),
                  "with no world frozen the Bolero is usable");
    }

    // ---- (4) The copies rule over every combination ------------------------------------
    {
        for (const WsmSong& s : kWsmSongs) {
            WSM_CHECK(Combo_MMWarpSongCopies(0u, s.rule) == 0u && Combo_MMWarpSongCopies(s.rule, s.rule) == 1u &&
                          Combo_MMWarpSongCopies(s.rule | s.shared, s.rule) == 0u &&
                          Combo_MMWarpSongCopies(s.shared, s.rule) == 0u &&
                          Combo_MMWarpSongCopies(kWsmAllRules & ~s.rule, s.rule) == 0u &&
                          Combo_MMWarpSongCopies(kWsmAllRules | (kWsmAllShared & ~s.shared), s.rule) == 1u,
                      "%s: off 0, on 1, shared 0, shared alone 0, the other songs' rules 0 and the other songs' "
                      "shared rules 1 do not hold",
                      s.name);
        }
        WSM_CHECK(Combo_MMWarpSongCopies(0x3FFFu, 0x0001u) == 0u && Combo_MMWarpSongCopies(0x3FFFu, 0x0100u) == 0u &&
                      Combo_MMWarpSongCopies(0x3FFFu, 0x4000u) == 0u,
                  "the Song of Soaring (OoT), a shared bit or an unallocated bit is pooled as an MM warp song");
        // Through the frozen world: the same answer per song.
        WsmFreezeWorld(kWsmAllRules | kWsmSongs[2].shared);
        for (int i = 0; i < 6; i++) {
            WSM_CHECK(MM_WarpSongMM_PooledCopies(WsmId(i)) == (i == 2 ? 0 : 1),
                      "a frozen world with every rule on and the Shared Serenade pools %d copies of %s",
                      MM_WarpSongMM_PooledCopies(WsmId(i)), kWsmSongs[i].name);
        }
        ComboContext_Init();
    }

    // ---- (5) MM's REAL pool --------------------------------------------------------------
    {
        WSM_CHECK(Combo_ComboSettingStoreAvailable(), "no CVar store: MM's pool cannot be generated");
        struct Leg {
            uint32_t rules2;
            const char* name;
        };
        const Leg legs[3] = { { 0u, "rules off" },
                              { kWsmAllRules, "six rules on" },
                              { kWsmAllRules | kWsmAllShared, "six rules and their shared songs on" } };
        for (const Leg& leg : legs) {
            WsmFreezeWorld(leg.rules2);
            for (int i = 0; i < 6; i++) {
                const int copies = MM_WarpSongsTest_PoolCopies(WsmId(i), 0u);
                const int want = leg.rules2 == kWsmAllRules ? 1 : 0;
                printf("[TEST] (5) %s: %s x%d (expected %d)\n", leg.name, kWsmSongs[i].name, copies, want);
                WSM_CHECK(copies == want, "MM's pool with %s holds %d copies of %s, expected %d", leg.name, copies,
                          kWsmSongs[i].name, want);
            }
        }
        // A starting copy leaves the pool with the other starting items.
        WsmFreezeWorld(kWsmAllRules);
        WSM_CHECK(MM_WarpSongsTest_PoolCopies(WsmId(0), WsmId(0)) == 0 &&
                      MM_WarpSongsTest_PoolCopies(WsmId(1), WsmId(0)) == 1,
                  "a world starting with the Minuet still pools it, or its start took another song out");
        ComboContext_Init();
    }

    // ---- (6) MM's REAL logic macro against OoTMM's button sets ---------------------------
    {
        WsmFreezeWorld(kWsmAllRules);
        MM_WarpSongsTest_ArmLive();
        for (int i = 0; i < 6; i++) {
            MM_WarpSongsTest_Give(WsmId(i));
        }
        int wrong = 0;
        for (int i = 0; i < 6; i++) {
            const WsmSong& s = kWsmSongs[i];
            for (uint32_t buttons = 0; buttons < 32u; buttons++) {
                const int want = (buttons & s.buttons) == s.buttons ? 1 : 0;
                const int got = MM_WarpSongsTest_CanPlay(WsmId(i), 1, buttons);
                if (got != want) {
                    wrong++;
                    printf("[TEST]   (6) %s with buttons %02X: %d, OoTMM's %s says %d\n", s.name, (unsigned)buttons,
                           got, s.ootmmKey, want);
                }
            }
            WSM_CHECK(MM_WarpSongsTest_CanPlay(WsmId(i), 0, 0x1Fu) == 0,
                      "%s is played with every button and no Ocarina of Time", s.name);
        }
        WSM_CHECK(wrong == 0, "%d button combinations disagree with OoTMM's can_play_tp_* sets", wrong);
        // Not owned, or its rule off: never.
        WsmRefreezeKeepingOwnership(kWsmAllRules & ~kWsmSongs[5].rule);
        WSM_CHECK(MM_WarpSongsTest_CanPlay(WsmId(5), 1, 0x1Fu) == 0 &&
                      MM_WarpSongsTest_CanPlay(WsmId(4), 1, 0x1Fu) == 1,
                  "the Prelude is played with its rule off (or the Nocturne is not with its rule on)");
        WsmFreezeWorld(kWsmAllRules);
        WSM_CHECK(MM_WarpSongsTest_CanPlay(WsmId(0), 1, 0x1Fu) == 0, "an unowned Minuet is played");
        ComboContext_Init();
    }

    // ---- (7) The snapshot brackets: a simulated pickup never owns a song ------------------
    {
        WsmFreezeWorld(kWsmAllRules);
        MM_WarpSongsTest_ArmLive();
        uint16_t ids[6];
        for (int i = 0; i < 6; i++) {
            ids[i] = WsmId(i);
        }
        const int given = MM_ComboLogic_ProbeGives(ids, 6, 0);
        WSM_CHECK(given == 6 && Combo_MMWarpSongsOwned() == 0u,
                  "the bag's MM engine gave %d songs inside its snapshot and left the byte at %02X, expected 6 and 00",
                  given, (unsigned)Combo_MMWarpSongsOwned());
        MM_WarpSongsTest_Give(ids[3]); // a real pickup outside any bracket
        WSM_CHECK(MM_ComboLogic_ProbeGives(ids, 6, 0) == 6 && Combo_MMWarpSongsOwned() == kWsmSongs[3].owned,
                  "the engine's restore did not put back exactly the owned Requiem (%02X)",
                  (unsigned)Combo_MMWarpSongsOwned());
        // From source: MM's crawl, its glitchless fill (both restores) and the
        // attempt ladder put the byte back with the save; a new MM file and a
        // failed generation own nothing.
        const std::string crawl = WsmReadSource("games/mm/2s2h/Rando/Logic/Logic.cpp");
        const std::string fill = WsmReadSource("games/mm/2s2h/Rando/Logic/GlitchlessLogic.cpp");
        const std::string create = WsmReadSource("games/mm/2s2h/Rando/MiscBehavior/OnFileCreate.cpp");
        const std::string engine = WsmReadSource("games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp");
        WSM_CHECK(!crawl.empty() && !fill.empty() && !create.empty() && !engine.empty(),
                  "the checkout's MM logic sources could not be read (RSBS_SOURCE_DIR)");
        WSM_CHECK(WsmCount(crawl, "MM_WarpSongsMM_RestoreOwned(warpSongsOwned);") == 1 &&
                      WsmCount(fill, "MM_WarpSongsMM_RestoreOwned(rsbsWarpSongsOwned);") == 2 &&
                      WsmCount(create, "MM_WarpSongsMM_RestoreOwned(rsbsPreAttemptWarpSongs);") == 1 &&
                      WsmCount(create, "MM_WarpSongsMM_RestoreOwned(0);") == 2 &&
                      WsmCount(engine, "MM_WarpSongsMM_RestoreOwned(sSnapshotWarpSongs);") == 1,
                  "a snapshot bracket over MM's save no longer restores the warp songs' byte");
        ComboContext_Init();
    }

    ComboContext_Init();
    printf("[TEST] mm-warp-songs-items: %s\n", sWsmFailures == 0 ? "PASS" : "FAIL");
    return sWsmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// mm-warp-songs-ownership
// ============================================================================

TestResult MMWarpSongsOwnership_Run(void) {
    printf("[TEST] mm-warp-songs-ownership: a starting MM warp song is owned after creation (the package's world "
           "record puts back what the file-create invalidation drops), after an MM in-game save and its .redsave "
           "load, and a pickup after a freeze, the crossing's commit and a reload; the fingerprint never moves "
           "(#1169 XW5)\n");
    sWsmFailures = 0;
    WSM_CHECK(Combo_ComboSettingStoreAvailable(), "no CVar store: the load's key compare cannot run");
    Combo_ComboItemRules2RevealForTest(0);
    WsmClearKeys();
    const uint16_t minuet = WsmId(0);
    const uint16_t bolero = WsmId(1);
    const uint16_t prelude = WsmId(5);

    // ---- (C) Creation with a starting song -------------------------------------------
    {
        WsmFreezeWorld(kWsmAllRules);
        MM_WarpSongsTest_ArmLive();
        const uint32_t stamp = gComboCtx.comboSettingsHash;
        WSM_CHECK(MM_WarpSongsTest_StartingGrant(minuet) == 1, "the persisted start list did not read back as the "
                                                               "Minuet alone");
        WSM_CHECK(Combo_MMWarpSongsOwned() == 0x01u && MM_WarpSongMM_Usable(minuet),
                  "the creation's starting grant left the byte at %02X, expected the Minuet's 01",
                  (unsigned)Combo_MMWarpSongsOwned());
        WSM_CHECK(gComboCtx.comboSettingsHash == stamp &&
                      (Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) == 0u,
                  "owning a song moved the fingerprint or reads as FINGERPRINT damage: the byte must not be folded");
        // Generate stages the package from the live record (WorldRecordBytes).
        ComboContext* live = new ComboContext;
        memcpy(live, &gComboCtx, sizeof(ComboContext));
        // The file-create invalidation (z_sram.c) runs BEFORE the creation's bind:
        // the byte is session state, so it goes. This is the trap the doc names.
        Context_InvalidateSessionOnNewGame(1, kWsmSeed);
        WSM_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == kWsmAllRules && Combo_ComboSettingsFrozen(),
                  "the file-create invalidation dropped the frozen world (premise)");
        WSM_CHECK(
            Combo_MMWarpSongsOwned() == 0u,
            "the file-create invalidation KEPT the ownership byte: a failed Generate's starting songs would reach "
            "the next world");
        // The creation's bind applies the package's world record (ApplyComboWorld).
        ComboContext* carried = new ComboContext;
        memset(carried, 0xEE, sizeof(ComboContext));
        const int rt = OoT_SeedPackage_WorldRecordRoundTripForTest(live, carried);
        printf("[TEST] (C) package record byte %02X, resident after the bind %02X (expected 01, 01)\n",
               (unsigned)carried->mmWarpSongsOwned, (unsigned)Combo_MMWarpSongsOwned());
        WSM_CHECK(rt == 1 && carried->mmWarpSongsOwned == 0x01u,
                  "placement/combo-context.bin does not carry the world's starting Minuet");
        WSM_CHECK(Combo_MMWarpSongsOwned() == 0x01u && MM_WarpSongMM_Usable(minuet) &&
                      gComboCtx.comboSettingsHash == stamp,
                  "AFTER CREATION the starting Minuet is not owned (byte %02X)", (unsigned)Combo_MMWarpSongsOwned());
        delete carried;
        delete live;
    }

    // ---- (S) An MM in-game save, and the .redsave load after it ------------------------
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    const std::string priorSaveDir = std::filesystem::path(mgr.SlotPath(0)).parent_path().string();
    mgr.SetSaveDirectory(rsbs::test::ScratchDir(kWsmSaveDir).string());
    RsbsSave_ResetSlotSessionState();
    Context_InitFrozenStates();
    {
        RsbsSave_SetActiveSlot(0);
        mgr.DeleteSave(0);
        WSM_CHECK(RsbsSave_LoadSlot(0) == RSBS_LOAD_ABSENT, "an empty slot must open as ABSENT and arm");
        // The world, its keys (the session plays it) and the seam (the rules are
        // hidden until XW12; a load refuses them otherwise).
        Combo_ComboItemRules2RevealForTest(1);
        for (const WsmSong& s : kWsmSongs) {
            CVarSetInteger(Combo_ComboSettingKey(s.key), 1);
        }
        WsmFreezeWorld(kWsmAllRules);
        MM_WarpSongsTest_ArmLive();
        MM_WarpSongsTest_Give(minuet);
        MM_WarpSongsTest_Give(prelude);
        const uint8_t before = Combo_MMWarpSongsOwned();
        const int committed = MM_Combo_CaptureSaveToUnifiedSlot();
        printf("[TEST] (S) the MM in-game save committed %d; byte %02X -> %02X (expected 1, 21 -> 21)\n", committed,
               (unsigned)before, (unsigned)Combo_MMWarpSongsOwned());
        WSM_CHECK(committed == 1 && before == 0x21u && Combo_MMWarpSongsOwned() == 0x21u,
                  "the MM in-game save did not commit, or moved the ownership byte");
        // A new session: nothing resident, then the slot's own .redsave.
        RsbsSave_ResetSlotSessionState();
        Context_InvalidateSessionOnSlotLoad();
        WSM_CHECK(Combo_MMWarpSongsOwned() == 0u, "a slot load's DROP kept a dead session's songs");
        RsbsSave_ResetSlotSessionState();
        const bool loaded = mgr.Load(0);
        printf("[TEST] (S) the .redsave load: %d, byte %02X (expected 1, 21)\n", loaded ? 1 : 0,
               (unsigned)Combo_MMWarpSongsOwned());
        WSM_CHECK(loaded && Combo_MMWarpSongsOwned() == 0x21u && MM_WarpSongMM_Usable(prelude),
                  "AFTER AN MM IN-GAME SAVE AND A RELOAD the Minuet and the Prelude are not owned");
    }

    // ---- (F) A pickup, a departure's freeze, the crossing's commit, a reload -----------
    {
        MM_WarpSongsTest_ArmLive();
        MM_WarpSongsTest_Give(bolero);
        WSM_CHECK(MM_WarpSongsTest_FreezeAndRestore() == 1, "the freeze and restore of MM's save did not take");
        WSM_CHECK(Combo_MMWarpSongsOwned() == 0x23u, "a freeze and restore moved the byte to %02X, expected 23",
                  (unsigned)Combo_MMWarpSongsOwned());
        WSM_CHECK(mgr.Save(0), "the crossing's commit (Save(0)) failed");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        const bool loaded = mgr.Load(0);
        printf("[TEST] (F) after the freeze, the commit and a reload: %d, byte %02X (expected 1, 23)\n", loaded ? 1 : 0,
               (unsigned)Combo_MMWarpSongsOwned());
        WSM_CHECK(loaded && Combo_MMWarpSongsOwned() == 0x23u && MM_WarpSongMM_Usable(bolero) &&
                      (Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) == 0u,
                  "AFTER A FREEZE, THE COMMIT AND A .redsave RELOAD the songs are not owned (or the file reads as "
                  "FINGERPRINT damage)");
    }

    Combo_ComboItemRules2RevealForTest(0);
    WsmClearKeys();
    RsbsSave_ResetSlotSessionState();
    mgr.DeleteSave(0);
    mgr.SetSaveDirectory(priorSaveDir);
    RsbsSave_SetActiveSlot(-1);
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    printf("[TEST] mm-warp-songs-ownership: %s\n", sWsmFailures == 0 ? "PASS" : "FAIL");
    return sWsmFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// mm-warp-songs-rules-off-unchanged
// ============================================================================

TestResult Test_MMWarpSongsRulesOffUnchanged(void) {
    printf("[TEST] mm-warp-songs-rules-off-unchanged: while the warp-song rules are hidden no world a player can make "
           "pools or credits an MM warp song, and owning one moves no fingerprint, bag seed or package record "
           "(#1169 XW5)\n");
    sWsmFailures = 0;
    Combo_ComboItemRules2RevealForTest(0);

    // ---- (1) Hidden: Generate refuses every MM warp-song bit, by OoTMM's name ---------
    {
        char names[192];
        for (const WsmSong& s : kWsmSongs) {
            const uint32_t refused = Combo_ComboItemRules2Refused(s.rule, names, sizeof(names));
            WSM_CHECK(refused == s.rule && strcmp(names, s.ootmmKey) == 0,
                      "while hidden, %s is not refused by name "
                      "('%s')",
                      s.ootmmKey, names);
            WSM_CHECK((Combo_ComboItemRules2Refused(s.rule | s.shared, NULL, 0) & (s.rule | s.shared)) ==
                          (s.rule | s.shared),
                      "while hidden, %s and its shared song are not both refused", s.ootmmKey);
        }
    }

    // ---- (2) A rules-off world: no copy, no credit, whatever the byte says -------------
    {
        WsmFreezeWorld(0u);
        Combo_MMWarpSongsSetOwned(0x3Fu);
        for (int i = 0; i < 6; i++) {
            WSM_CHECK(MM_WarpSongMM_PooledCopies(WsmId(i)) == 0 && !MM_WarpSongMM_RuleOn(WsmId(i)) &&
                          !MM_WarpSongMM_Usable(WsmId(i)),
                      "a rules-off world pools or credits %s", kWsmSongs[i].name);
        }
        ComboContext_Init();
        for (int i = 0; i < 6; i++) {
            WSM_CHECK(MM_WarpSongMM_PooledCopies(WsmId(i)) == 0, "an unfrozen session pools %s", kWsmSongs[i].name);
        }
    }

    // ---- (3) Main's fingerprint, world hash, bag seeds and package record --------------
    {
        // XW2's vectors (test_combo_item_rules_word2.c, computed outside the build
        // with main's formula over these terms).
        static const uint32_t kBagSeeds[3] = { 0x7ED4B1E3u, 0x2EA872D2u, 0xDE7C33C1u };
        const uint8_t bytes[2] = { 0x00u, 0x3Fu };
        for (const uint8_t owned : bytes) {
            WsmFreezeWorld(0u);
            Combo_MMWarpSongsSetOwned(owned);
            const uint32_t restamped = Combo_StampComboSettingsHash();
            WSM_CHECK(gComboCtx.comboSettingsHash == 0xA06B18F7u && restamped == 0xA06B18F7u &&
                          Combo_ComboSettingsWorldHash() == 0xCEA3786Bu,
                      "with the byte at %02X a rules-off world stamps %08X (re-stamped %08X) with world hash %08X; "
                      "main's are A06B18F7 and CEA3786B",
                      (unsigned)owned, (unsigned)gComboCtx.comboSettingsHash, (unsigned)restamped,
                      (unsigned)Combo_ComboSettingsWorldHash());
            for (int a = 0; a < 3; a++) {
                WSM_CHECK(Combo_SingleBag_SeedFor(a) == kBagSeeds[a],
                          "with the byte at %02X the bag seed for attempt %d is %08X; main's is %08X", (unsigned)owned,
                          a, (unsigned)Combo_SingleBag_SeedFor(a), (unsigned)kBagSeeds[a]);
            }
            WSM_CHECK(Combo_SingleBag_TrimSeed() == 0x6F81C454u && Combo_SingleBag_FoolishSeed() == 0x4D84BC5Bu,
                      "with the byte at %02X the trim or foolish seed moved off main's", (unsigned)owned);
            WSM_CHECK((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) == 0u,
                      "with the byte at %02X a rules-off world reads as FINGERPRINT damage", (unsigned)owned);
        }
        // A rules-off world's package record: no ownership byte, main's fingerprint.
        WsmFreezeWorld(0u);
        ComboContext* live = new ComboContext;
        memcpy(live, &gComboCtx, sizeof(ComboContext));
        ComboContext* carried = new ComboContext;
        memset(carried, 0xEE, sizeof(ComboContext));
        const int rt = OoT_SeedPackage_WorldRecordRoundTripForTest(live, carried);
        WSM_CHECK(rt == 1 && carried->mmWarpSongsOwned == 0u && carried->comboSettingsHash == 0xA06B18F7u &&
                      Combo_MMWarpSongsOwned() == 0u,
                  "a rules-off world's package record carries an ownership byte (%02X) or another fingerprint",
                  (unsigned)carried->mmWarpSongsOwned);
        delete carried;
        delete live;
    }

    // ---- (4) From source: the pool adds a song only through the copies rule ------------
    {
        const std::string pools = WsmReadSource("games/mm/2s2h/Rando/Logic/GeneratePools.cpp");
        const size_t add = pools.find("copy < MM_WarpSongMM_PooledCopies(song)");
        const size_t removeStart = pools.find("// Remove starting items from the pool");
        WSM_CHECK(!pools.empty() && add != std::string::npos && removeStart != std::string::npos && add < removeStart &&
                      WsmCount(pools, "RI_SONG_MINUET") == 1,
                  "GeneratePools.cpp does not add the MM warp songs once, through the copies rule, before the "
                  "starting items leave the pool");
        const std::string give = WsmReadSource("games/mm/2s2h/Rando/GiveItem.cpp");
        WSM_CHECK(WsmCount(give, "MM_WarpSongMM_Give(randoItemId);") == 1,
                  "GiveItem.cpp does not give the MM warp songs through their owned bit");
    }

    ComboContext_Init();
    printf("[TEST] mm-warp-songs-rules-off-unchanged: %s\n", sWsmFailures == 0 ? "PASS" : "FAIL");
    return sWsmFailures == 0 ? TEST_PASS : TEST_FAIL;
}
