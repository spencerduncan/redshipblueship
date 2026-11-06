/**
 * @file test_mm_hint_sources.c
 * @brief MmHintSources (redship tier) and MmHintSourcesWorld (rando tier): the
 *        combined Hint Tracker's Majora's Mask side (#1475, lane H1).
 *
 * WHAT IS UNDER TEST. MM's hints become sources the combined Hint Tracker can
 * list (src/common/combo_hint_view.h): a pure resolver that says what each
 * source says in a world (Rando/HintSources.h, EnGs.cpp), the textbox hooks
 * that now build their text with that resolver, a read hook that records each
 * source in the combo save record (ComboContext.mmHintsRead, ADR 0009 claim 20)
 * and marks the MM checks it names Seen, and the pane's model. All of it is
 * inert for players until lane H2 draws the window: no hint text, pick,
 * generation, golden or MM save layout moves.
 *
 * mm-hint-sources (ROM-free, redship tier):
 *   S1. THE SOURCES: the MM adapter comes up with the check tracker's MM adapter
 *       (MM_TrackerAdapter_Register), 74 sources: the 58 stone placements (EnGs.cpp's
 *       table, pinned at 58 there) and the 16 fixed hints, each named uniquely and
 *       listed under a real MM area; the two scene versions of one stone (the
 *       Southern Swamp poisoned and cleared, Mountain Village in winter and
 *       spring) are two sources with two stone seeds.
 *   S2. THE STREAM: the resolver's caller-held draw (MM_Ship_RandomOnStream) is
 *       Ship_Random's draw and leaves the stream where Ship_Random leaves the
 *       global one, over seeds and bounds, without moving the global stream.
 *   S3. THE RECORD: ten bytes at .redsave offset 929, reserved[] 65 at 939, one
 *       bit per source; GAME_OOT has none; a new file (the KEEP invalidation) and
 *       a session drop clear it.
 *   S4. THE ROUND TRIPS: the record survives a .redsave save and load, and an
 *       OoT -> MM -> OoT crossing pair through the real GameRunner_SwitchTo and
 *       its whole-file commits (a bit set in MM survives both crossings and a
 *       kill and reload).
 *   S5. THE PANE over an authored MM half while OoT runs, with MM's live save
 *       another world (as after a .redsave load with MM not booted), so nothing
 *       passes by reading gSaveContext: STALE, every source listed and masked
 *       ("???", nothing resolved) until read, grouped by area; a read stone shows
 *       the resolver's answer worked out while the live save was the world, plain,
 *       and reads found (SoH's Journal state) once its check is collected; an off
 *       option drops its sources; an unmarked half is UNAVAILABLE; MM played reads
 *       LIVE.
 *   S6. Combo_HintPlainText: MM's colour codes and control bytes out.
 *   S8. THE BOMB SHOP'S JUNK (Rando/HintSources.h, JunkName): with MM's own junk
 *       padding (RI_JUNK) in the line's slot and Rando::CurrentJunkItem's cache
 *       cold, neither the resolver nor the read hook draws from MM's global
 *       stream; the hook's text is main's, "Junk (<junk item>)", and the
 *       resolver's is the same less that one suffix; another live save leaves the
 *       answer alone. With an OoT item crossed there (RI_JUNK as its cover) the
 *       resolver and the read hook still draw nothing, and main's text, the
 *       hook's and the resolver's all name the OoT item.
 *   S7. THE WIRING, from source (RSBS_SOURCE_DIR), each with a counterfactual:
 *       every textbox hook builds its text with the resolver's function and calls
 *       the read hook (the Bomb Shop's with main's junk naming, the resolver with
 *       the plain one), the stone's pick and text go through the resolver's
 *       functions, the free-stone digest goes through the resolver, and the hint
 *       adapter is registered by MM_TrackerAdapter_Register, which
 *       Combo_TrackerWindow_Init calls (main.cpp calls that; locked by
 *       mm-creation-new-file's entry-point scan).
 *
 * mm-hint-sources-world (rando tier, a real paired world: the ComboSingleBag
 * pinned seed through the production creation event, MM's world in the frozen
 * half, MM never booted):
 *   W1. For every source, the resolver over the frozen MM half (what the pane
 *       reads while OoT runs) equals the resolver over MM's live save (what the
 *       hooks read), field by field; then, with MM's live save another world (as
 *       after a .redsave load with MM not booted), every source over the frozen
 *       half still says exactly what it said, so the resolver reads the image it
 *       is handed and never gSaveContext.
 *   W2. For every stone, main's pick (MM's GLOBAL stream seeded with the world's
 *       seed plus the stone's, then Ship_Random) equals the talk-time pick
 *       (FreeStonePick) and both leave the next global draw equal; the talk-time
 *       text for that pick is the resolver's text, and an item hint's target is
 *       that pick. For every fixed hint, main's text (each hook's upstream
 *       expression, MM_HintSources_TestLegacyFixedText) is the hook's, and the
 *       resolver's, but for the one deliberate difference S8 pins: the Bomb
 *       Shop's line over MM's own junk padding, one junk suffix shorter.
 *   W3. Neither the read hook nor the resolver (over the live save and over the
 *       frozen half) moves MM's global stream, probed with
 *       Rando::CurrentJunkItem's cache cold.
 *   W4. THE SEEN MARK: reading a stone whose item hint names an MM check marks
 *       that check Seen in MM's live save, so the combined Check Tracker's MM
 *       pane shows it SEEN while MM is played; the frozen half is untouched; a
 *       fixed hint (the Hookshot's) marks its check too.
 *   W5. The pane over the world, MM's live save another world: every listed
 *       source masked until read, then the resolver's answer from W1's first pass.
 * The free-stone digest and the goldens are the rest of the proof: the digest now
 * goes through the resolver (S7), and every Golden* row (GoldenSeedDigestDefault,
 * ...ProfileV1, ...Entrances, ...ExcludesTricks, ...Shopsanity, ...DirectionForward,
 * ...BeatableOnly, ...NoLogic, ...ArmedCaps, GoldenPairedAttemptDigest, and the
 * PlayerPath / PackagePath / HostScale300 / MMNearlyNoLogic variants) folds
 * mmFreeStoneDigest and mmHintDigest unchanged, as does mm-gossip-stone-seed.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * after test_crossing_commit.c and test_combo_crossing_views.c, whose extern "C"
 * declarations it repeats verbatim where it shares them.
 */

#include "../combo_area.h"
#include "../combo_hint_view.h"
#include "../combo_tracker_view.h"
#include "../context.h"
#include "../crossing_store.h"
#include "../foreign_items.h"
#include "../game.h"
#include "../game_lifecycle.h"
#include "../save.h"
#include "../shared_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../test_scratch.h"
#include "test_named_items.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

extern "C" {
// MM: the gossip stones (games/mm/2s2h/Rando/ActorBehavior/EnGs.cpp)
int MM_Rando_Hints_TestGossipStone(int index, int16_t* outSceneId, int16_t* outX, int16_t* outZ, uint32_t* outSeed);
int MM_Rando_Hints_TestLegacyFreeStonePick(int index, uint8_t* outGame, uint16_t* outCheck, int32_t* outNext);
int MM_Rando_Hints_TestTalkTimeFreeStonePick(int index, uint8_t* outGame, uint16_t* outCheck, int32_t* outNext);
int MM_Rando_Hints_TestTalkTimeFreeStoneText(int index, uint8_t hostGame, uint16_t check, char* out, int cap);
int MM_Rando_Hints_TestMarkStoneRead(int index);
void MM_Rando_Hints_TestSetCheck(uint16_t randoCheckId, uint16_t randoItemId, int shuffled, int obtained,
                                 uint16_t* priorItemId, int* priorShuffled, int* priorObtained);
// MM: the sources, the resolver, the read hook (games/mm/2s2h/Rando/HintSourcesSingleExe.cpp)
int MM_HintSources_TestSaveSize(void);
void MM_HintSources_TestCopyLive(void* out);
void MM_HintSources_TestSetLive(const void* in);
void MM_HintSources_TestAuthorSave(void* image, uint32_t finalSeed, int hintsOn);
int MM_HintSources_TestResolve(const void* save, uint16_t source, ComboHintResolution* out);
void MM_HintSources_TestMarkRead(uint16_t source);
int MM_HintSources_TestKeepsStream(int mark, const void* save, uint16_t source, uint64_t streamSeed);
int MM_HintSources_TestSeen(const void* save, uint16_t check);
int MM_HintSources_TestSetSeen(uint16_t check, int seen);
void MM_HintSources_TestStreamReplica(uint64_t seed, int32_t max, int32_t* outGlobal, int32_t* outGlobalNext,
                                      int32_t* outLocal, int32_t* outLocalNext, int* outUntouched);
int MM_HintSources_TestLegacyFixedText(uint16_t source, char* out, int cap);
int MM_HintSources_TestTextboxText(uint16_t source, char* out, int cap);
void MM_HintSources_TestBombShopIds(uint16_t* outCheck, uint16_t* outJunk);
// MM: the check table and the tracker adapter's live seam
int MM_HintArea_TestCheckRow(uint16_t id, int* outIsEnemyDrop);
int MM_HintArea_TestCheckIdMax(void);
void MM_TrackerAdapter_TestLiveSeam(int live);
// OoT: a real paired world (3drando/menu.cpp, ForeignItemsSingleExe.cpp)
int Rando_HeadlessSeedTest(const char* seedStr);
int OoT_Creation_AuthorRandoFile(int slot);
void Randomizer_TestClearOoTSave(void);
// src/common: a departure's freeze (switch.cpp)
int Switch_PrepareHotSwap(GameId departing, const void* saveContext, size_t size);
}

#define HS_CHECK(cond, ...)                                        \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (mm-hint-sources): ");             \
            printf(__VA_ARGS__);                                   \
            printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            return false;                                          \
        }                                                          \
    } while (0)

namespace {

bool HsSameResolution(const ComboHintResolution& a, const ComboHintResolution& b) {
    if (a.type != b.type || a.targetCount != b.targetCount || a.areaCount != b.areaCount ||
        strcmp(a.text, b.text) != 0) {
        return false;
    }
    for (int i = 0; i < (int)a.targetCount && i < RSBS_HINT_TARGET_CAP; i++) {
        const ComboHintTarget& x = a.targets[i];
        const ComboHintTarget& y = b.targets[i];
        if (x.checkGame != y.checkGame || x.check != y.check || x.itemGame != y.itemGame || x.item != y.item ||
            x.area.game != y.area.game || x.area.area != y.area.area) {
            return false;
        }
    }
    for (int i = 0; i < (int)a.areaCount && i < RSBS_HINT_TARGET_CAP; i++) {
        if (a.areas[i].game != b.areas[i].game || a.areas[i].area != b.areas[i].area) {
            return false;
        }
    }
    return true;
}

std::string HsPlain(const char* msg) {
    char buf[RSBS_HINT_TEXT_CAP];
    (void)Combo_HintPlainText(msg, buf, (int)sizeof(buf));
    return buf;
}

/** True when `textbox` is `plain` with one " (...)" put in where the two first
 *  differ: the junk suffix the Bomb Shop's textbox adds to MM's own junk padding
 *  and the resolver leaves out (Rando/HintSources.h, JunkName). The suffix's
 *  item name may hold parentheses of its own ("Arrows (10)"). */
bool HsOneJunkSuffixApart(const char* textbox, const char* plain) {
    size_t i = 0;
    while (textbox[i] != '\0' && textbox[i] == plain[i]) {
        i++;
    }
    const size_t textboxLen = strlen(textbox);
    const size_t tailLen = strlen(plain + i);
    if (textboxLen < i + tailLen + 4) { // " (" + a name + ")"
        return false;
    }
    const size_t insertEnd = textboxLen - tailLen;
    return strncmp(textbox + i, " (", 2) == 0 && textbox[insertEnd - 1] == ')' &&
           strcmp(textbox + insertEnd, plain + i) == 0;
}

/** A live MM save that is not the world under test: what MM's gSaveContext holds
 *  while OoT runs after a .redsave load (MM not booted) stands for any of them. A
 *  resolver that read it instead of the image it is handed changes its answer. */
void HsAuthorOtherLiveSave() {
    MM_HintSources_TestAuthorSave(nullptr, 0x5EEDF00Du, 0);
}

// ---- S1. the sources -------------------------------------------------------

bool HsCheckSources() {
    MM_TrackerAdapter_Register(); // the production registration (Combo_TrackerWindow_Init makes it)
    const ComboMMHintDesc* desc = Combo_Hint_GetMMDesc();
    HS_CHECK(desc != nullptr, "S1: MM_TrackerAdapter_Register did not register the hint adapter");
    HS_CHECK(desc->sourceCount == RSBS_MM_HINT_SRC_COUNT, "S1: %u sources, want %d", (unsigned)desc->sourceCount,
             (int)RSBS_MM_HINT_SRC_COUNT);
    int16_t scene = 0;
    int16_t x = 0;
    int16_t z = 0;
    uint32_t seed = 0;
    const int stones = MM_Rando_Hints_TestGossipStone(-1, &scene, &x, &z, &seed);
    HS_CHECK(stones == RSBS_MM_HINT_SRC_STONE_COUNT, "S1: the stone table has %d rows, the source list %d", stones,
             (int)RSBS_MM_HINT_SRC_STONE_COUNT);

    std::set<std::string> names;
    std::set<std::string> placements;
    int stoneRows = 0;
    int fixedRows = 0;
    for (uint16_t s = 0; s < desc->sourceCount; s++) {
        ComboHintSourceInfo info;
        memset(&info, 0, sizeof(info));
        HS_CHECK(desc->sourceInfo(s, &info), "S1: source %u has no facts", (unsigned)s);
        HS_CHECK(info.source == s && info.name != nullptr && info.name[0] != '\0', "S1: source %u is unnamed",
                 (unsigned)s);
        HS_CHECK(names.insert(info.name).second, "S1: two sources share the label \"%s\"", info.name);
        HS_CHECK(info.area.game == (uint8_t)GAME_MM && info.area.area != COMBO_AREA_NONE &&
                     info.area.area != COMBO_AREA_POCKET && Combo_Area_IsValid(info.area),
                 "S1: source %u (%s) is listed under no MM area (%u:%u)", (unsigned)s, info.name,
                 (unsigned)info.area.game, (unsigned)info.area.area);
        if (s < RSBS_MM_HINT_SRC_STONE_COUNT) {
            HS_CHECK(info.kind == (uint8_t)COMBO_HINT_SOURCE_GOSSIP_STONE, "S1: source %u is not a stone", (unsigned)s);
            MM_Rando_Hints_TestGossipStone((int)s, &scene, &x, &z, &seed);
            const std::string key = std::to_string(scene) + ":" + std::to_string(x) + ":" + std::to_string(z);
            HS_CHECK(placements.insert(key).second, "S1: two stone sources are one placement (%s)", key.c_str());
            stoneRows++;
        } else {
            HS_CHECK(info.kind == (uint8_t)COMBO_HINT_SOURCE_FIXED, "S1: source %u is not fixed", (unsigned)s);
            fixedRows++;
        }
        if (s < 3 || s == RSBS_MM_HINT_SRC_STONE_COUNT || s + 1 == desc->sourceCount) {
            printf("[TEST]   S1: source %u \"%s\" under %s\n", (unsigned)s, info.name, Combo_Area_Name(info.area));
        }
    }
    HS_CHECK(stoneRows == 58 && fixedRows == 16, "S1: %d stones and %d fixed hints, want 58 and 16", stoneRows,
             fixedRows);
    ComboHintSourceInfo past;
    HS_CHECK(!desc->sourceInfo(desc->sourceCount, &past), "S1: a source past the count answers");

    // One stone in two scene versions: two sources, two stone seeds, so two hints.
    const struct {
        int16_t x;
        int16_t z;
        const char* what;
    } kVersions[] = { { 4170, -1657, "the Southern Swamp stone (poisoned / cleared)" },
                      { -805, 1346, "the Mountain Village stone (winter / spring)" } };
    for (const auto& v : kVersions) {
        std::vector<int> rows;
        std::set<int16_t> scenes;
        std::set<uint32_t> seeds;
        for (int i = 0; i < stones; i++) {
            MM_Rando_Hints_TestGossipStone(i, &scene, &x, &z, &seed);
            if (x == v.x && z == v.z) {
                rows.push_back(i);
                scenes.insert(scene);
                seeds.insert(seed);
            }
        }
        printf("[TEST]   S1: %s: %zu source(s), %zu scene(s), %zu stone seed(s)\n", v.what, rows.size(), scenes.size(),
               seeds.size());
        HS_CHECK(rows.size() == 2 && scenes.size() == 2 && seeds.size() == 2,
                 "S1: %s is not two sources keyed by scene with two stone seeds", v.what);
    }
    return true;
}

// ---- S2. the stream ----------------------------------------------------------

bool HsCheckStream() {
    const int32_t bounds[] = { 1, 2, 3, 7, 57, 100, 1000, 65535, 0x40000000 };
    int compared = 0;
    for (uint64_t i = 0; i < 96; i++) {
        const uint64_t seed = (i * 0x9E3779B97F4A7C15ull) ^ (i << 40) ^ 0x1475ull;
        for (const int32_t max : bounds) {
            int32_t g = 0;
            int32_t gn = 0;
            int32_t l = 0;
            int32_t ln = 0;
            int untouched = 0;
            MM_HintSources_TestStreamReplica(seed, max, &g, &gn, &l, &ln, &untouched);
            HS_CHECK(g == l && gn == ln, "S2: seed %016llX over [0, %d): global %d then %d, caller-held %d then %d",
                     (unsigned long long)seed, (int)max, (int)g, (int)gn, (int)l, (int)ln);
            HS_CHECK(untouched == 1, "S2: a caller-held draw moved MM's global stream (seed %016llX)",
                     (unsigned long long)seed);
            compared++;
        }
    }
    printf("[TEST]   S2: %d draws: the caller-held stream is Ship_Random's, and leaves the global stream alone\n",
           compared);
    return true;
}

// ---- S3. the record ----------------------------------------------------------

bool HsCheckRecord() {
    HS_CHECK(offsetof(ComboContext, mmHintsRead) == 929u && sizeof(((ComboContext*)0)->mmHintsRead) == 10u &&
                 offsetof(ComboContext, reserved) == 939u && sizeof(((ComboContext*)0)->reserved) == 65u &&
                 sizeof(ComboContext) == 1844u,
             "S3: the read record is %u bytes at %u and reserved[] %u at %u (sizeof %u); want 10 at 929, 65 at 939, "
             "1844",
             (unsigned)sizeof(((ComboContext*)0)->mmHintsRead), (unsigned)offsetof(ComboContext, mmHintsRead),
             (unsigned)sizeof(((ComboContext*)0)->reserved), (unsigned)offsetof(ComboContext, reserved),
             (unsigned)sizeof(ComboContext));
    ComboContext_Init();
    HS_CHECK(Combo_HintReadCount((uint8_t)GAME_MM) == 0, "S3: a fresh record reads something read");
    HS_CHECK(Combo_HintMarkRead((uint8_t)GAME_MM, 0) && !Combo_HintMarkRead((uint8_t)GAME_MM, 0),
             "S3: the first mark does not set, or the second sets again");
    HS_CHECK(Combo_HintMarkRead((uint8_t)GAME_MM, (uint16_t)(RSBS_MM_HINT_SRC_COUNT - 1)),
             "S3: the last source has no bit");
    HS_CHECK(!Combo_HintMarkRead((uint8_t)GAME_MM, (uint16_t)RSBS_MM_HINT_READ_CAP),
             "S3: a source past the record's capacity was recorded");
    HS_CHECK(!Combo_HintMarkRead((uint8_t)GAME_OOT, 1) && !Combo_HintIsRead((uint8_t)GAME_OOT, 0),
             "S3: OoT answered from MM's record");
    HS_CHECK(Combo_HintReadCount((uint8_t)GAME_MM) == 2 && Combo_HintIsRead((uint8_t)GAME_MM, 0) &&
                 !Combo_HintIsRead((uint8_t)GAME_MM, 1),
             "S3: the record does not hold exactly the two marks");
    const uint16_t last = (uint16_t)(RSBS_MM_HINT_SRC_COUNT - 1);
    HS_CHECK(gComboCtx.mmHintsRead[0] == 0x01u && gComboCtx.mmHintsRead[last >> 3] == (uint8_t)(1u << (last & 7u)),
             "S3: a source's bit is not mmHintsRead[i >> 3] bit (i & 7)");

    Context_InvalidateSessionState(RSBS_SEED_STAMP_KEEP);
    const int afterCreate = Combo_HintReadCount((uint8_t)GAME_MM);
    (void)Combo_HintMarkRead((uint8_t)GAME_MM, 5);
    Context_InvalidateSessionState(RSBS_SEED_STAMP_DROP);
    const int afterDrop = Combo_HintReadCount((uint8_t)GAME_MM);
    printf("[TEST]   S3: read sources after a file creation %d, after a session drop %d (want 0 0)\n", afterCreate,
           afterDrop);
    HS_CHECK(afterCreate == 0, "S3: a new file kept the previous file's read hints (the KEEP invalidation)");
    HS_CHECK(afterDrop == 0, "S3: a session drop kept a dead file's read hints");
    return true;
}

// ---- S4. the round trips -----------------------------------------------------

inline const char* HsSaveDir() {
    return rsbs::test::ScratchDirCStr("rsbs_test_mm_hint_sources");
}

int HsGameInit(int, char**) {
    return 0;
}
void HsGameNoop(void) {
}
GameOps sHsOoT = { "oot", "Mock OoT", HsGameInit, HsGameNoop, HsGameNoop, HsGameNoop, HsGameNoop };
GameOps sHsMM = { "mm", "Mock MM", HsGameInit, HsGameNoop, HsGameNoop, HsGameNoop, HsGameNoop };

// The process dies: every RAM-only cross-game store goes (test_crossing_commit.c's XcKill).
void HsKill() {
    ComboContext_Init();
    Combo_Crossings_Clear();
    Context_ClearFrozenState(GAME_OOT);
    Context_ClearFrozenState(GAME_MM);
    Combo_ClearSharedItemOutbox();
    Combo_ResetSharedResourceWatermarks();
    rsbs::SaveManager::Instance().ResetSlotSessionState();
}

void HsArmSlot() {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.ResetSlotSessionState();
    mgr.ArmSlotOnCreate(0);
    mgr.SetActiveSlot(0);
}

void HsSetBit(uint8_t* bits, uint16_t source) {
    bits[source >> 3] |= (uint8_t)(1u << (source & 7u));
}

bool HsRoundTripLegs() {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(HsSaveDir());
    Context_InitFrozenStates();
    HsKill();
    mgr.DeleteSave(0);
    HsArmSlot();
    {
        std::vector<uint8_t> o1(OOT_SAVE_CONTEXT_SIZE, 0x11);
        std::vector<uint8_t> m1(MM_SAVE_CONTEXT_SIZE, 0x22);
        Context_UpdateShadowCopy(GAME_OOT, o1.data(), o1.size());
        Context_UpdateShadowCopy(GAME_MM, m1.data(), m1.size());
    }
    gComboCtx.sourceGame = GAME_OOT;

    // (a) A save and a load.
    uint8_t want[sizeof(gComboCtx.mmHintsRead)];
    memset(want, 0, sizeof(want));
    const uint16_t before[] = { 3, (uint16_t)(RSBS_MM_HINT_SRC_STONE_COUNT - 1), (uint16_t)RSBS_MM_HINT_SRC_OATH };
    for (const uint16_t s : before) {
        (void)Combo_HintMarkRead((uint8_t)GAME_MM, s);
        HsSetBit(want, s);
    }
    HS_CHECK(mgr.Save(0), "S4: the save did not commit");
    ComboContext_Init();
    HS_CHECK(Combo_HintReadCount((uint8_t)GAME_MM) == 0, "S4: setup: the record survived ComboContext_Init");
    HS_CHECK(mgr.Load(0), "S4: the load failed");
    HS_CHECK(memcmp(gComboCtx.mmHintsRead, want, sizeof(want)) == 0,
             "S4: the read record did not round-trip through a .redsave");
    printf("[TEST]   S4: %d read source(s) round-trip through a .redsave save and load\n",
           Combo_HintReadCount((uint8_t)GAME_MM));

    // (b) OoT -> MM -> OoT, through the real switch and its commits.
    HsArmSlot();
    gComboCtx.sourceGame = GAME_OOT;
    HS_CHECK(mgr.Save(0), "S4: setup: the last OoT save did not commit");
    const uint32_t g = gComboCtx.commitGeneration;
    GameRunner r;
    GameRunner_Init(&r);
    GameRunner_RegisterGame(&r, GAME_OOT, &sHsOoT);
    GameRunner_RegisterGame(&r, GAME_MM, &sHsMM);
    GameRunner_StartGame(&r, GAME_OOT, 0, nullptr);
    {
        std::vector<uint8_t> o2(OOT_SAVE_CONTEXT_SIZE, 0x33);
        HS_CHECK(Switch_PrepareHotSwap(GAME_OOT, o2.data(), o2.size()) == 1, "S4: setup: OoT's departure freeze");
    }
    HS_CHECK(GameRunner_SwitchTo(&r, GAME_MM, 0, nullptr) == 0, "S4: the switch to MM failed");
    HS_CHECK(memcmp(gComboCtx.mmHintsRead, want, sizeof(want)) == 0, "S4: the OoT -> MM crossing changed the record");
    // MM's hook records a hint during the visit.
    (void)Combo_HintMarkRead((uint8_t)GAME_MM, (uint16_t)RSBS_MM_HINT_SRC_HOOKSHOT);
    HsSetBit(want, (uint16_t)RSBS_MM_HINT_SRC_HOOKSHOT);
    {
        std::vector<uint8_t> m2(MM_SAVE_CONTEXT_SIZE, 0x44);
        HS_CHECK(Switch_PrepareHotSwap(GAME_MM, m2.data(), m2.size()) == 1, "S4: setup: MM's departure freeze");
    }
    HS_CHECK(GameRunner_SwitchTo(&r, GAME_OOT, 0, nullptr) == 0, "S4: the switch back to OoT failed");
    HS_CHECK(memcmp(gComboCtx.mmHintsRead, want, sizeof(want)) == 0,
             "S4: the record did not survive OoT -> MM -> OoT in memory");
    const uint32_t generationAfter = gComboCtx.commitGeneration;

    // (c) The commits carried it: the process dies and the slot is reloaded.
    HsKill();
    mgr.SetActiveSlot(0);
    HS_CHECK(RsbsSave_LoadSlotChecked(0, g) == RSBS_LOAD_OK, "S4: the slot does not load after the round trip");
    printf("[TEST]   S4: generation %u -> %u over the round trip; reloaded %u with %d read source(s)\n", (unsigned)g,
           (unsigned)generationAfter, (unsigned)gComboCtx.commitGeneration, Combo_HintReadCount((uint8_t)GAME_MM));
    HS_CHECK(memcmp(gComboCtx.mmHintsRead, want, sizeof(want)) == 0,
             "S4: the hint read during the MM visit is not in the file the crossing commits wrote");
    return true;
}

bool HsCheckRoundTrips() {
    const bool ok = HsRoundTripLegs();
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetActiveSlot(-1);
    mgr.SetSaveDirectory("Save");
    HsKill();
    return ok;
}

// ---- S5. the pane --------------------------------------------------------------

bool HsPaneLegs(std::vector<uint8_t>* shadow) {
    ComboContext_Init();
    Context_SetCurrentGame(GAME_OOT);

    // A small world on MM's live save (the bridges write it), frozen into the MM
    // half: eight checks with an area hold the Lens of Truth, so every stone has
    // candidates, no area is foolish (the Lens is progression) and no Way of the
    // Hero is frozen: every stone gives its item hint.
    SharedItem lens;
    HS_CHECK(TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &lens), "S5: MM's Lens of Truth was not found");
    MM_HintSources_TestAuthorSave(nullptr, 0x0C0FFEE5u, 1);
    std::vector<uint16_t> lensChecks;
    for (int id = 1; id < MM_HintArea_TestCheckIdMax() && lensChecks.size() < 8; id++) {
        if (MM_HintArea_TestCheckRow((uint16_t)id, nullptr)) {
            MM_Rando_Hints_TestSetCheck((uint16_t)id, lens.id, 1, 0, nullptr, nullptr, nullptr);
            lensChecks.push_back((uint16_t)id);
        }
    }
    HS_CHECK(lensChecks.size() == 8, "S5: fewer than eight MM checks have an area");
    shadow->assign((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    MM_HintSources_TestCopyLive(shadow->data());
    Context_UpdateShadowCopy(GAME_MM, shadow->data(), shadow->size());

    // What stone 0 says in that world, worked out while MM's live save is that
    // world. Then MM's live save becomes another one, as while OoT runs after a
    // .redsave load with MM not booted, so every leg below until MM is played
    // reads the half or fails: the same answer over the half proves the resolver
    // reads the image it is handed, not MM's gSaveContext.
    ComboHintResolution direct;
    HS_CHECK(MM_HintSources_TestResolve(shadow->data(), 0, &direct) == 1, "S5: the resolver says nothing for stone 0");
    HsAuthorOtherLiveSave();
    {
        ComboHintResolution overOther;
        HS_CHECK(MM_HintSources_TestResolve(shadow->data(), 0, &overOther) == 1 && HsSameResolution(direct, overOther),
                 "S5: stone 0 over the MM half changed with MM's live save: the resolver read the live save");
    }

    ComboHintGameSummary sum;
    Combo_HintGameSummary((uint8_t)GAME_MM, &sum);
    printf("[TEST]   S5: OoT plays: freshness %u, world %d, %d source(s), %d read\n", (unsigned)sum.freshness,
           sum.hasWorld ? 1 : 0, sum.sourceCount, sum.readCount);
    HS_CHECK(sum.freshness == (uint8_t)COMBO_TRACKER_FRESH_STALE && sum.hasWorld &&
                 sum.sourceCount == RSBS_MM_HINT_SRC_COUNT && sum.readCount == 0,
             "S5: the pane over the frozen MM half is not STALE with every source listed and none read");
    HS_CHECK(Combo_HintRowCount((uint8_t)GAME_MM) == RSBS_MM_HINT_SRC_COUNT,
             "S5: the row count is not the source count");
    uint8_t prevArea = 0;
    for (int i = 0; i < RSBS_MM_HINT_SRC_COUNT; i++) {
        ComboHintRow row;
        HS_CHECK(Combo_HintRowAt((uint8_t)GAME_MM, i, &row), "S5: row %d is missing", i);
        HS_CHECK(!row.read && !row.known && row.hint.type == (uint8_t)COMBO_HINT_TYPE_NONE &&
                     row.hint.text[0] == '\0' && strcmp(row.display, RSBS_HINT_MASKED_TEXT) == 0,
                 "S5: unread row %d (source %u) is not masked: \"%s\"", i, (unsigned)row.info.source, row.display);
        HS_CHECK(row.info.area.area >= prevArea && row.areaName != nullptr,
                 "S5: row %d is out of area order or has no area name", i);
        prevArea = row.info.area.area;
    }
    ComboHintRow pastRow;
    HS_CHECK(!Combo_HintRowAt((uint8_t)GAME_MM, RSBS_MM_HINT_SRC_COUNT, &pastRow), "S5: a row past the count answers");

    // A read stone: its row shows the resolver's text over the MM half, plain.
    (void)Combo_HintMarkRead((uint8_t)GAME_MM, 0);
    ComboHintRow read;
    bool found = false;
    for (int i = 0; i < Combo_HintRowCount((uint8_t)GAME_MM) && !found; i++) {
        found = Combo_HintRowAt((uint8_t)GAME_MM, i, &read) && read.info.source == 0;
    }
    HS_CHECK(found && read.read && read.known, "S5: the read stone's row is not known");
    printf("[TEST]   S5: stone 0 (\"%s\") reads \"%s\"\n", read.info.name, read.display);
    HS_CHECK(HsSameResolution(read.hint, direct),
             "S5: the row's hint is not the resolver's answer over the MM half (MM's live save is another world)");
    HS_CHECK(read.display == HsPlain(direct.text) && strchr(read.display, '%') == nullptr,
             "S5: the row's display is not the resolver's text, plain");
    HS_CHECK(read.hint.type == (uint8_t)COMBO_HINT_TYPE_ITEM && read.hint.targetCount == 1 &&
                 read.hint.targets[0].checkGame == (uint8_t)GAME_MM &&
                 read.hint.targets[0].itemGame == (uint8_t)GAME_MM && read.hint.targets[0].item == lens.id &&
                 read.hint.areaCount == 1 && strstr(read.display, "Lens of Truth") != nullptr,
             "S5: stone 0 does not name the Lens of Truth on an MM check in that check's area");
    bool onLensCheck = false;
    for (const uint16_t c : lensChecks) {
        onLensCheck = onLensCheck || read.hint.targets[0].check == c;
    }
    HS_CHECK(onLensCheck, "S5: stone 0's check holds no Lens");
    Combo_HintGameSummary((uint8_t)GAME_MM, &sum);
    HS_CHECK(sum.readCount == 1, "S5: the summary does not count the read stone");

    // SoH's Journal "found": once the hinted check is collected in the MM half.
    HS_CHECK(!read.found, "S5: a hint whose check is not collected reads found");
    {
        const ComboMMTrackerDesc* tdesc = Combo_Tracker_GetMMDesc();
        HS_CHECK(tdesc != nullptr, "S5: no MM tracker descriptor");
        std::vector<uint8_t> collected(*shadow);
        collected[tdesc->checkTableOffset + (size_t)read.hint.targets[0].check * tdesc->checkStride +
                  tdesc->obtainedOffset] = 1;
        Context_UpdateShadowCopy(GAME_MM, collected.data(), collected.size());
        ComboHintRow again;
        bool foundRow = false;
        for (int i = 0; i < Combo_HintRowCount((uint8_t)GAME_MM) && !foundRow; i++) {
            foundRow = Combo_HintRowAt((uint8_t)GAME_MM, i, &again) && again.info.source == 0;
        }
        Context_UpdateShadowCopy(GAME_MM, shadow->data(), shadow->size());
        HS_CHECK(foundRow && again.known && again.found, "S5: a hint whose check is collected does not read found");
    }
    HS_CHECK(MM_HintSources_TestKeepsStream(0, shadow->data(), 0, 0x1475000000000000ull) == 1,
             "S5: resolving a stone moved MM's global stream");

    // An off option drops its sources: only the always-on lines are listed.
    std::vector<uint8_t> quiet(*shadow);
    MM_HintSources_TestAuthorSave(quiet.data(), 0x0C0FFEE5u, 0);
    Context_UpdateShadowCopy(GAME_MM, quiet.data(), quiet.size());
    const int quietRows = Combo_HintRowCount((uint8_t)GAME_MM);
    printf("[TEST]   S5: with every hint option off, %d row(s) (the gold dust, Great Fairy and Bomb Shop lines)\n",
           quietRows);
    HS_CHECK(quietRows == 8, "S5: with every hint option off the pane lists %d sources, want 8", quietRows);

    // Nothing to read: an unmarked MM half.
    std::vector<uint8_t> zero((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    Context_UpdateShadowCopy(GAME_MM, zero.data(), zero.size());
    Combo_HintGameSummary((uint8_t)GAME_MM, &sum);
    HS_CHECK(sum.freshness == (uint8_t)COMBO_TRACKER_FRESH_UNAVAILABLE && sum.sourceCount == 0 &&
                 Combo_HintRowCount((uint8_t)GAME_MM) == 0,
             "S5: an unmarked MM half is not UNAVAILABLE");

    // MM played: the pane reads MM's live save (the authored world again), LIVE.
    MM_HintSources_TestSetLive(shadow->data());
    MM_TrackerAdapter_TestLiveSeam(1);
    Context_SetCurrentGame(GAME_MM);
    Combo_HintGameSummary((uint8_t)GAME_MM, &sum);
    MM_TrackerAdapter_TestLiveSeam(-1);
    Context_SetCurrentGame(GAME_OOT);
    HS_CHECK(sum.freshness == (uint8_t)COMBO_TRACKER_FRESH_LIVE && sum.sourceCount == RSBS_MM_HINT_SRC_COUNT &&
                 sum.readCount == 1,
             "S5: MM played does not read its live save LIVE");
    return true;
}

bool HsCheckPane() {
    MM_TrackerAdapter_Register();
    const GameId priorGame = Context_GetCurrentGame();
    std::vector<uint8_t> priorLive((size_t)MM_HintSources_TestSaveSize());
    MM_HintSources_TestCopyLive(priorLive.data());
    std::vector<uint8_t> priorShadow((size_t)MM_SAVE_CONTEXT_SIZE);
    memcpy(priorShadow.data(), Context_GetMMSaveContext(), priorShadow.size());
    std::vector<uint8_t> shadow;
    const bool ok = HsPaneLegs(&shadow);
    MM_TrackerAdapter_TestLiveSeam(-1);
    Context_UpdateShadowCopy(GAME_MM, priorShadow.data(), priorShadow.size());
    MM_HintSources_TestSetLive(priorLive.data());
    Context_SetCurrentGame(priorGame);
    ComboContext_Init();
    return ok;
}

// ---- S6. the plain text ----------------------------------------------------------

bool HsCheckPlainText() {
    const struct {
        const char* msg;
        const char* want;
    } kCases[] = {
        { "         %g Odolwa%w:\nLast seen in %ythe Southern Swamp%w.", "Odolwa: Last seen in the Southern Swamp." },
        { "Gold dust can be found at %pthe Goron Racetrack%w.\x10"
          "Bring me that, and my %rKokiri Sword%w is all yours.\xE0",
          "Gold dust can be found at the Goron Racetrack. Bring me that, and my Kokiri Sword is all yours." },
        { "They say %ga Lens of Truth (OoT)%w is hidden in %yKokiri Forest%w.",
          "They say a Lens of Truth (OoT) is hidden in Kokiri Forest." },
        { "100%", "100%" },
        { "", "" },
    };
    for (const auto& c : kCases) {
        const std::string got = HsPlain(c.msg);
        HS_CHECK(got == c.want, "S6: plain text of \"%s\" is \"%s\", want \"%s\"", c.msg, got.c_str(), c.want);
    }
    char tiny[6];
    const int full = Combo_HintPlainText("They say", tiny, (int)sizeof(tiny));
    HS_CHECK(full == 8 && strcmp(tiny, "They ") == 0, "S6: a short buffer does not truncate and report the length");
    return true;
}

// ---- S8. the Bomb Shop's junk ------------------------------------------------------

bool HsBombShopLegs(std::vector<uint8_t>* world) {
    const uint16_t src = (uint16_t)RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK;
    uint16_t shopCheck = 0;
    uint16_t junk = 0;
    MM_HintSources_TestBombShopIds(&shopCheck, &junk);
    HS_CHECK(shopCheck != 0 && junk != 0, "S8: no Bomb Shop check or junk item id");
    ComboContext_Init(); // no foreign placement anywhere
    MM_HintSources_TestAuthorSave(nullptr, 0x1475B0B5u, 1);
    MM_Rando_Hints_TestSetCheck(shopCheck, junk, 1, 0, nullptr, nullptr, nullptr);
    world->assign((size_t)MM_HintSources_TestSaveSize(), 0);
    MM_HintSources_TestCopyLive(world->data());

    // (a) MM's own junk padding in the slot (junk filled there, or the check
    // skipped or excluded). With Rando::CurrentJunkItem's cache cold (the stream
    // probe makes it cold), neither the resolver nor the read hook draws.
    HS_CHECK(MM_HintSources_TestKeepsStream(0, world->data(), src, 0xB0B5000000000001ull) == 1,
             "S8: resolving the Bomb Shop's line over MM's junk padding drew from MM's global stream "
             "(GetItemName -> Rando::CurrentJunkItem)");
    HS_CHECK(MM_HintSources_TestKeepsStream(1, nullptr, src, 0xB0B5000000000002ull) == 1,
             "S8: the Bomb Shop's read hook drew from MM's global stream over MM's junk padding");
    ComboHintResolution plain;
    HS_CHECK(MM_HintSources_TestResolve(world->data(), src, &plain) == 1, "S8: the Bomb Shop's line says nothing");
    char textbox[RSBS_HINT_TEXT_CAP];
    char legacy[RSBS_HINT_TEXT_CAP];
    HS_CHECK(MM_HintSources_TestTextboxText(src, textbox, (int)sizeof(textbox)) >= 0 &&
                 MM_HintSources_TestLegacyFixedText(src, legacy, (int)sizeof(legacy)) >= 0,
             "S8: no textbox text for the Bomb Shop's line");
    printf("[TEST]   S8: MM's junk padding: the textbox says \"%s\", the resolver \"%s\"\n", HsPlain(textbox).c_str(),
           HsPlain(plain.text).c_str());
    HS_CHECK(strcmp(legacy, textbox) == 0, "S8: the hook's text \"%s\" is not main's \"%s\"", textbox, legacy);
    HS_CHECK(strstr(plain.text, "%rJunk%w") != nullptr && HsOneJunkSuffixApart(textbox, plain.text),
             "S8: the resolver's \"%s\" is not the textbox's \"%s\" less its one junk suffix", plain.text, textbox);
    HS_CHECK(plain.type == (uint8_t)COMBO_HINT_TYPE_FIXED && plain.targetCount == 1 &&
                 plain.targets[0].checkGame == (uint8_t)GAME_MM && plain.targets[0].check == shopCheck &&
                 plain.targets[0].itemGame == (uint8_t)GAME_MM && plain.targets[0].item == junk,
             "S8: the line does not name MM's junk padding at the Bomb Shop's check");
    // The world is the image's: another live save leaves the answer alone.
    HsAuthorOtherLiveSave();
    {
        ComboHintResolution again;
        HS_CHECK(MM_HintSources_TestResolve(world->data(), src, &again) == 1 && HsSameResolution(plain, again),
                 "S8: the Bomb Shop's line over the image changed with MM's live save");
    }
    MM_HintSources_TestSetLive(world->data());

    // (b) An OoT item crossed there: the slot holds RI_JUNK as its cover. The
    // text names the OoT item in every form, and the resolver never asks
    // GetItemName about the cover.
    SharedItem sword;
    HS_CHECK(TestNamedItem((uint8_t)GAME_OOT, "Kokiri Sword", &sword), "S8: OoT's Kokiri Sword was not found");
    HS_CHECK(Combo_SetForeignPlacement(shopCheck, sword) >= 0, "S8: setup: the crossing at the Bomb Shop's check");
    HS_CHECK(MM_HintSources_TestKeepsStream(0, world->data(), src, 0xB0B5000000000003ull) == 1,
             "S8: resolving the Bomb Shop's line over a crossed slot drew from MM's global stream: GetItemName "
             "was asked about the cover");
    HS_CHECK(MM_HintSources_TestKeepsStream(1, nullptr, src, 0xB0B5000000000004ull) == 1,
             "S8: the Bomb Shop's read hook drew from MM's global stream over a crossed slot");
    ComboHintResolution crossed;
    HS_CHECK(MM_HintSources_TestResolve(world->data(), src, &crossed) == 1, "S8: the crossed line says nothing");
    HS_CHECK(MM_HintSources_TestTextboxText(src, textbox, (int)sizeof(textbox)) >= 0 &&
                 MM_HintSources_TestLegacyFixedText(src, legacy, (int)sizeof(legacy)) >= 0,
             "S8: no textbox text for the crossed line");
    printf("[TEST]   S8: a crossed slot: \"%s\"\n", HsPlain(crossed.text).c_str());
    HS_CHECK(strcmp(legacy, textbox) == 0 && strcmp(textbox, crossed.text) == 0,
             "S8: over a crossed slot main says \"%s\", the hook \"%s\", the resolver \"%s\"", legacy, textbox,
             crossed.text);
    HS_CHECK(strstr(crossed.text, "Kokiri Sword") != nullptr && crossed.targetCount == 1 &&
                 crossed.targets[0].itemGame == (uint8_t)GAME_OOT && crossed.targets[0].item == sword.id,
             "S8: the crossed line does not name the OoT item");
    return true;
}

bool HsCheckBombShopJunk() {
    const ComboContext savedCtx = gComboCtx;
    std::vector<uint8_t> priorLive((size_t)MM_HintSources_TestSaveSize());
    MM_HintSources_TestCopyLive(priorLive.data());
    std::vector<uint8_t> world;
    const bool ok = HsBombShopLegs(&world);
    MM_HintSources_TestSetLive(priorLive.data());
    gComboCtx = savedCtx;
    return ok;
}

// ---- S7. the wiring, from source -------------------------------------------------

std::vector<std::string> HsLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string cur;
    for (const char c : text) {
        if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur += c;
        }
    }
    lines.push_back(cur);
    return lines;
}

std::string HsTrim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) {
        return "";
    }
    const size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

/** One wiring fact: the block of `file` whose first line starts with `head`
 *  (trimmed) and ends at the first later line exactly `close` holds the trimmed
 *  line `line`. Removing that line must fail the fact (the counterfactual). */
struct HsSite {
    const char* file;
    const char* head;
    const char* close;
    const char* line;
    const char* what;
};

const HsSite kHsSites[] = {
    // The stone: the talk-time text records the read; the pick and the text are the resolver's.
    { "games/mm/2s2h/Rando/ActorBehavior/EnGs.cpp",
      "static std::string FreeStoneHintText(const GossipHintTarget& target) {", "}",
      "MarkGossipStoneRead(MM_gPlayState->sceneId, enGs->actor.home.pos);",
      "the free stone's textbox records its stone as read" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnGs.cpp",
      "static GossipHintTarget FreeStonePick(const std::vector<GossipHintTarget>& candidates, uint32_t stoneSeed) {",
      "}", "const GossipHintTarget target = FreeStonePickFrom(candidates, finalSeed, stoneSeed, &stream);",
      "the talk-time pick is the resolver's pick" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnGs.cpp", "static bool ResolveFreeStone(", "}",
      "out->target = FreeStonePickFrom(candidates, finalSeed, stoneSeed, &out->stream);",
      "the resolver picks with the stone's own pick" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnGs.cpp", "static bool ResolveFreeStone(", "}",
      "out->text = FreeStoneHintTextSeeded(checks, out->target, finalSeed + stoneSeed, &out->hint);",
      "the resolver's text is the stone's own text" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnGs.cpp", "extern \"C\" int MM_Rando_Hints_DigestFreeStones(", "}",
      "if (!ResolveFreeStone(RANDO_SAVE_CHECKS, finalSeed, seed, &answer)) {",
      "the free-stone digest (the goldens' mmFreeStoneDigest) goes through the resolver" },
    // The fixed hints: each hook's text is FixedText's, and it records the read.
    { "games/mm/2s2h/Rando/ActorBehavior/EnTalk.cpp", "void ApplyRemainsHint(", "}",
      "msg = Rando::HintSources::FixedText(source, RANDO_SAVE_CHECKS, nullptr);", "the remains posters' text" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnTalk.cpp", "void ApplyRemainsHint(", "}",
      "Rando::HintSources::MarkRead(source);", "the remains posters' read hook" },
    { "games/mm/2s2h/Rando/ActorBehavior/DmStk.cpp", "void ApplyOathHint(", "}",
      "msg = Rando::HintSources::FixedText(RSBS_MM_HINT_SRC_OATH, RANDO_SAVE_CHECKS, nullptr);", "the Oath's text" },
    { "games/mm/2s2h/Rando/ActorBehavior/DmStk.cpp", "void ApplyOathHint(", "}",
      "Rando::HintSources::MarkRead(RSBS_MM_HINT_SRC_OATH);", "the Oath's read hook" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnZow.cpp", "void ApplyHookshotHint(", "}",
      "std::string msg = Rando::HintSources::FixedText(RSBS_MM_HINT_SRC_HOOKSHOT, RANDO_SAVE_CHECKS, nullptr);",
      "the Hookshot's text" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnZow.cpp", "void ApplyHookshotHint(", "}",
      "Rando::HintSources::MarkRead(RSBS_MM_HINT_SRC_HOOKSHOT);", "the Hookshot's read hook" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnKgy.cpp", "static std::string GoldDustHintText() {", "}",
      "Rando::HintSources::MarkRead(RSBS_MM_HINT_SRC_GOLD_DUST);", "the gold dust's read hook" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnKgy.cpp", "static std::string GoldDustHintText() {", "}",
      "return Rando::HintSources::FixedText(RSBS_MM_HINT_SRC_GOLD_DUST, RANDO_SAVE_CHECKS, nullptr);",
      "the gold dust's text" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnKgy.cpp", "COND_ID_HOOK(OnOpenText, 0xc49, IS_RANDO,", "    });",
      "entry.msg = GoldDustHintText();", "the gold dust textbox shows the gold dust text" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnKgy.cpp", "static std::string GoldDustPrizeHintText() {", "}",
      "Rando::HintSources::MarkRead(RSBS_MM_HINT_SRC_GOLD_DUST_PRIZE);", "the gold dust prize's read hook" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnKgy.cpp", "COND_ID_HOOK(OnOpenText, 0xc4b, IS_RANDO,", "    });",
      "entry.msg = GoldDustPrizeHintText();", "the gold dust prize textbox shows the prize text" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnSsh.cpp", "void ApplySwampSpiderHouseHint(", "}",
      "Rando::HintSources::MarkRead(RSBS_MM_HINT_SRC_SPIDER_HOUSE_SWAMP);", "the swamp spider house's read hook" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnSsh.cpp", "void ApplySwampSpiderHouseHint(", "}",
      ".msg = Rando::HintSources::FixedText(RSBS_MM_HINT_SRC_SPIDER_HOUSE_SWAMP, RANDO_SAVE_CHECKS, nullptr),",
      "the swamp spider house's text" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnSsh.cpp", "void ApplyOceanSpiderHouseHint(", "}",
      "Rando::HintSources::MarkRead(RSBS_MM_HINT_SRC_SPIDER_HOUSE_OCEAN);", "the ocean spider house's read hook" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnSsh.cpp", "void ApplyOceanSpiderHouseHint(", "}",
      ".msg = Rando::HintSources::FixedText(RSBS_MM_HINT_SRC_SPIDER_HOUSE_OCEAN, RANDO_SAVE_CHECKS, nullptr),",
      "the ocean spider house's text" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnElfgrp.cpp", "void ApplyClockTownGreatFairyHint(", "}",
      "Rando::HintSources::MarkRead(RSBS_MM_HINT_SRC_FAIRY_CLOCK_TOWN);", "the Clock Town fairy's read hook" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnElfgrp.cpp", "void ApplyClockTownGreatFairyHint(", "}",
      ".msg = Rando::HintSources::FixedText(RSBS_MM_HINT_SRC_FAIRY_CLOCK_TOWN, RANDO_SAVE_CHECKS, nullptr),",
      "the Clock Town fairy's text" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnElfgrp.cpp", "void ApplyGreatFairyHint(", "}",
      "Rando::HintSources::MarkRead(source);", "the other fairies' read hook" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnElfgrp.cpp", "void ApplyGreatFairyHint(", "}",
      ".msg = Rando::HintSources::FixedText(source, RANDO_SAVE_CHECKS, nullptr),", "the other fairies' text" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnGirlA.cpp", "static std::string BombShopNextStockText() {", "}",
      "Rando::HintSources::MarkRead(RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK);", "the Bomb Shop's read hook" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnGirlA.cpp", "static std::string BombShopNextStockText() {", "}",
      "return Rando::HintSources::FixedText(RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK, RANDO_SAVE_CHECKS, nullptr, junk);",
      "the Bomb Shop's text" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnGirlA.cpp", "static std::string BombShopNextStockText() {", "}",
      "const Rando::HintSources::JunkName junk = Rando::HintSources::JunkName::Textbox;",
      "the Bomb Shop's textbox keeps main's junk naming (its drawn suffix)" },
    // The resolver names MM's junk padding without a draw (the Bomb Shop's line, HintSources.h).
    { "games/mm/2s2h/Rando/HintSourcesSingleExe.cpp", "bool Rando::HintSources::Resolve(", "}",
      "if (FixedText(source, checks, &resolved, JunkName::Plain).empty()) {",
      "the resolver's fixed texts name MM's junk padding plainly" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnGirlA.cpp", "COND_ID_HOOK(OnOpenText, 0x648, IS_RANDO,", "    });",
      "entry.msg = BombShopNextStockText();", "the Bomb Shop textbox shows the next-stock text" },
    // The registration: the hint adapter rides the check tracker's MM adapter.
    { "games/mm/2s2h/Rando/TrackerAdapterSingleExe.cpp", "extern \"C\" void MM_TrackerAdapter_Register(void) {", "}",
      "MM_HintAdapter_Register();", "MM_TrackerAdapter_Register registers the hint adapter" },
    { "src/common/ComboTrackerWindow.cpp", "extern \"C\" void Combo_TrackerWindow_Init(void) {", "}",
      "MM_TrackerAdapter_Register();", "Combo_TrackerWindow_Init brings both MM adapters up" },
};

/** "" when `text` holds `site`, else what is wrong. */
std::string HsSiteProblem(const HsSite& site, const std::string& text) {
    const std::vector<std::string> lines = HsLines(text);
    for (size_t i = 0; i < lines.size(); i++) {
        if (HsTrim(lines[i]).rfind(site.head, 0) != 0) {
            continue;
        }
        for (size_t j = i + 1; j < lines.size(); j++) {
            if (lines[j] == site.close) {
                break;
            }
            if (HsTrim(lines[j]) == site.line) {
                return "";
            }
        }
        return std::string("\"") + site.line + "\" is not in " + site.head;
    }
    return std::string("\"") + site.head + "\" was not found";
}

bool HsCheckWiring() {
#ifndef RSBS_SOURCE_DIR
    printf("[TEST] FAIL: RSBS_SOURCE_DIR is undefined: the hint sources' wiring cannot be checked\n");
    return false;
#else
    for (const HsSite& site : kHsSites) {
        std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + site.file, std::ios::binary);
        HS_CHECK(in.good(), "S7: %s cannot be read", site.file);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const std::string problem = HsSiteProblem(site, text);
        HS_CHECK(problem.empty(), "S7: %s: %s (%s)", site.file, problem.c_str(), site.what);
        std::string mutated = text;
        const size_t at = mutated.find(site.line);
        HS_CHECK(at != std::string::npos, "S7: %s: the wired line vanished before its counterfactual", site.file);
        mutated.replace(at, strlen(site.line), "/* removed */");
        HS_CHECK(!HsSiteProblem(site, mutated).empty(), "S7: the check of %s is vacuous: it passes with \"%s\" gone",
                 site.file, site.line);
        printf("[TEST]   S7: %s\n", site.what);
    }
    return true;
#endif
}

// ---- the world row ---------------------------------------------------------------

bool HsWorldLegs(const std::vector<uint8_t>& shadow) {
    // W1-W2: every source, over the frozen half and the live save, against main's text and pick.
    int listed = 0;
    int stonesSaid = 0;
    int items = 0;
    int foolish = 0;
    int woth = 0;
    int crossGame = 0;
    int fixedSaid = 0;
    int seenStone = -1;
    uint16_t seenCheck = 0;
    // Every source's answer over the frozen half, kept for the second W1 pass and W5.
    std::vector<ComboHintResolution> firstPass((size_t)RSBS_MM_HINT_SRC_COUNT);
    std::vector<int> firstSaid((size_t)RSBS_MM_HINT_SRC_COUNT, 0);
    uint16_t shopCheck = 0;
    uint16_t junkItem = 0;
    MM_HintSources_TestBombShopIds(&shopCheck, &junkItem);
    const char* bombShopSlot = "not listed";
    for (uint16_t s = 0; s < (uint16_t)RSBS_MM_HINT_SRC_COUNT; s++) {
        ComboHintResolution live;
        ComboHintResolution frozen;
        const int saidLive = MM_HintSources_TestResolve(nullptr, s, &live);
        const int saidFrozen = MM_HintSources_TestResolve(shadow.data(), s, &frozen);
        HS_CHECK(saidLive == saidFrozen && HsSameResolution(live, frozen),
                 "W1: source %u says \"%s\" over MM's live save and \"%s\" over the frozen half", (unsigned)s,
                 live.text, frozen.text);
        firstPass[s] = frozen;
        firstSaid[s] = saidFrozen;
        // W3 with Rando::CurrentJunkItem's cache cold (the probe makes it cold), over
        // the live save and over the frozen half.
        HS_CHECK(MM_HintSources_TestKeepsStream(0, nullptr, s, 0x1475ull << 32 | s) == 1,
                 "W3: resolving source %u over MM's live save moved MM's global stream", (unsigned)s);
        HS_CHECK(MM_HintSources_TestKeepsStream(0, shadow.data(), s, 0x1476ull << 32 | s) == 1,
                 "W3: resolving source %u over the frozen half moved MM's global stream", (unsigned)s);
        if (saidLive == 0) {
            continue;
        }
        listed++;
        if (s < RSBS_MM_HINT_SRC_STONE_COUNT) {
            stonesSaid++;
            uint8_t legacyGame = 0;
            uint16_t legacyCheck = 0;
            int32_t legacyNext = 0;
            uint8_t talkGame = 0;
            uint16_t talkCheck = 0;
            int32_t talkNext = 0;
            HS_CHECK(MM_Rando_Hints_TestLegacyFreeStonePick((int)s, &legacyGame, &legacyCheck, &legacyNext) == 1 &&
                         MM_Rando_Hints_TestTalkTimeFreeStonePick((int)s, &talkGame, &talkCheck, &talkNext) == 1,
                     "W2: stone %u has a hint but no pick", (unsigned)s);
            HS_CHECK(legacyGame == talkGame && legacyCheck == talkCheck && legacyNext == talkNext,
                     "W2: stone %u: main's pick %u:%u (next draw %d) is not the talk-time pick %u:%u (%d)", (unsigned)s,
                     (unsigned)legacyGame, (unsigned)legacyCheck, (int)legacyNext, (unsigned)talkGame,
                     (unsigned)talkCheck, (int)talkNext);
            char talk[RSBS_HINT_TEXT_CAP];
            HS_CHECK(MM_Rando_Hints_TestTalkTimeFreeStoneText((int)s, talkGame, talkCheck, talk, (int)sizeof(talk)) >=
                             0 &&
                         strcmp(talk, live.text) == 0,
                     "W2: stone %u's textbox says \"%s\", the resolver \"%s\"", (unsigned)s, talk, live.text);
            if (live.type == (uint8_t)COMBO_HINT_TYPE_ITEM) {
                items++;
                HS_CHECK(live.targetCount == 1 && live.targets[0].checkGame == talkGame &&
                             live.targets[0].check == talkCheck && live.areaCount == 1,
                         "W2: stone %u's item hint does not name its pick", (unsigned)s);
                crossGame +=
                    (live.targets[0].checkGame != (uint8_t)GAME_MM || live.targets[0].itemGame != (uint8_t)GAME_MM) ? 1
                                                                                                                    : 0;
                if (seenStone < 0 && live.targets[0].checkGame == (uint8_t)GAME_MM) {
                    seenStone = (int)s;
                    seenCheck = live.targets[0].check;
                }
            } else if (live.type == (uint8_t)COMBO_HINT_TYPE_FOOLISH) {
                foolish++;
            } else if (live.type == (uint8_t)COMBO_HINT_TYPE_WOTH) {
                woth++;
            }
        } else {
            fixedSaid++;
            // Main's text (each hook's upstream expression) is the hook's, byte for
            // byte, and the resolver's, except where the Bomb Shop's line names MM's
            // own junk padding: the hook adds the junk item it draws, the resolver
            // does not (Rando/HintSources.h, JunkName).
            char legacy[RSBS_HINT_TEXT_CAP];
            char textbox[RSBS_HINT_TEXT_CAP];
            HS_CHECK(MM_HintSources_TestLegacyFixedText(s, legacy, (int)sizeof(legacy)) >= 0 &&
                         MM_HintSources_TestTextboxText(s, textbox, (int)sizeof(textbox)) >= 0 &&
                         strcmp(legacy, textbox) == 0,
                     "W2: fixed source %u: main's text \"%s\", the hook's \"%s\"", (unsigned)s, legacy, textbox);
            HS_CHECK(live.type == (uint8_t)COMBO_HINT_TYPE_FIXED && live.targetCount >= 1,
                     "W2: fixed source %u names nothing", (unsigned)s);
            const bool nativeJunk = s == (uint16_t)RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK &&
                                    live.targets[0].itemGame == (uint8_t)GAME_MM &&
                                    live.targets[0].check == shopCheck && live.targets[0].item == junkItem;
            if (s == (uint16_t)RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK) {
                bombShopSlot = nativeJunk
                                   ? "MM's junk padding"
                                   : (live.targets[0].itemGame == (uint8_t)GAME_MM ? "an MM item" : "an OoT item");
            }
            if (nativeJunk) {
                HS_CHECK(HsOneJunkSuffixApart(textbox, live.text),
                         "W2: the Bomb Shop's line over MM's junk padding: the hook's \"%s\" is not the resolver's "
                         "\"%s\" plus one junk suffix",
                         textbox, live.text);
            } else {
                HS_CHECK(strcmp(textbox, live.text) == 0,
                         "W2: fixed source %u: the hook's text \"%s\", the resolver's \"%s\"", (unsigned)s, textbox,
                         live.text);
            }
        }
        if (s < 2 || s == RSBS_MM_HINT_SRC_HOOKSHOT || s == RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK) {
            printf("[TEST]   W2: source %u: \"%s\"\n", (unsigned)s, HsPlain(live.text).c_str());
        }
    }
    printf("[TEST]   W1-W2: %d source(s) say something: %d stone(s) (%d item, %d foolish, %d Way of the Hero; %d "
           "cross-game), %d fixed; the Bomb Shop's slot holds %s\n",
           listed, stonesSaid, items, foolish, woth, crossGame, fixedSaid, bombShopSlot);
    HS_CHECK(stonesSaid == RSBS_MM_HINT_SRC_STONE_COUNT,
             "W1: %d of the 58 stones say something; the default world has gossip-stone hints on", stonesSaid);
    HS_CHECK(items > 0, "W2: no stone gives an item hint, so the pick legs proved nothing");
    HS_CHECK(fixedSaid >= 8, "W2: %d fixed hints say something; the always-on eight alone are more", fixedSaid);

    // W4: the Seen mark.
    HS_CHECK(seenStone >= 0, "W4: no stone's item hint names an MM check");
    const int priorSeen = MM_HintSources_TestSetSeen(seenCheck, 0);
    const int frozenSeen = MM_HintSources_TestSeen(shadow.data(), seenCheck);
    HS_CHECK(MM_HintSources_TestKeepsStream(1, nullptr, (uint16_t)seenStone, 0xC0FFEEull) == 1,
             "W3: the read hook moved MM's global stream");
    (void)MM_HintSources_TestSetSeen(seenCheck, 0);
    memset(gComboCtx.mmHintsRead, 0, sizeof(gComboCtx.mmHintsRead));
    HS_CHECK(MM_Rando_Hints_TestMarkStoneRead(seenStone) == seenStone, "W4: the stone's read hook found no stone");
    printf("[TEST]   W4: reading stone %d marks MM check %u seen %d (frozen half %d)\n", seenStone, (unsigned)seenCheck,
           MM_HintSources_TestSeen(nullptr, seenCheck), MM_HintSources_TestSeen(shadow.data(), seenCheck));
    HS_CHECK(Combo_HintIsRead((uint8_t)GAME_MM, (uint16_t)seenStone) && Combo_HintReadCount((uint8_t)GAME_MM) == 1,
             "W4: the stone's read hook did not record exactly that stone");
    HS_CHECK(MM_HintSources_TestSeen(nullptr, seenCheck) == 1,
             "W4: the stone's MM check is not Seen in MM's live save");
    HS_CHECK(MM_HintSources_TestSeen(shadow.data(), seenCheck) == frozenSeen,
             "W4: the read hook wrote the frozen half");
    MM_TrackerAdapter_TestLiveSeam(1);
    Context_SetCurrentGame(GAME_MM);
    ComboTrackerCheckRow crow;
    const bool rowOk = Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)seenCheck, &crow);
    ComboHintGameSummary liveSum;
    Combo_HintGameSummary((uint8_t)GAME_MM, &liveSum);
    MM_TrackerAdapter_TestLiveSeam(-1);
    Context_SetCurrentGame(GAME_OOT);
    HS_CHECK(rowOk && crow.status == (uint8_t)COMBO_TRACKER_CHECK_SEEN,
             "W4: the combined Check Tracker's MM pane does not show check %u SEEN (status %u)", (unsigned)seenCheck,
             (unsigned)crow.status);
    HS_CHECK(liveSum.freshness == (uint8_t)COMBO_TRACKER_FRESH_LIVE && liveSum.readCount == 1,
             "W4: the pane does not read MM's live save LIVE with the stone read");
    (void)MM_HintSources_TestSetSeen(seenCheck, priorSeen);

    // A fixed hint marks its check too (the Hookshot, when an MM check holds it).
    ComboHintResolution hook;
    if (MM_HintSources_TestResolve(nullptr, (uint16_t)RSBS_MM_HINT_SRC_HOOKSHOT, &hook) == 1 &&
        hook.targets[0].checkGame == (uint8_t)GAME_MM) {
        const uint16_t c = hook.targets[0].check;
        const int prior = MM_HintSources_TestSetSeen(c, 0);
        MM_HintSources_TestMarkRead((uint16_t)RSBS_MM_HINT_SRC_HOOKSHOT);
        const int seen = MM_HintSources_TestSeen(nullptr, c);
        (void)MM_HintSources_TestSetSeen(c, prior);
        printf("[TEST]   W4: reading the Hookshot hint marks MM check %u seen %d\n", (unsigned)c, seen);
        HS_CHECK(seen == 1, "W4: the Hookshot hint did not mark its MM check Seen");
    } else {
        printf(
            "[TEST]   W4: the Hookshot is not on an MM check in this world (crossed or nowhere); fixed leg skipped\n");
    }

    // W1 again, the frozen half on its own. Until now MM's live save held the
    // world too, so a resolver reading gSaveContext instead of the image it is
    // handed would have passed. MM's live save becomes another world (as while OoT
    // runs after a .redsave load, MM not booted): every source over the frozen
    // half must still say what it said, field for field, and so must the pane.
    HsAuthorOtherLiveSave();
    for (uint16_t s = 0; s < (uint16_t)RSBS_MM_HINT_SRC_COUNT; s++) {
        ComboHintResolution again;
        const int said = MM_HintSources_TestResolve(shadow.data(), s, &again);
        HS_CHECK(said == firstSaid[s] && HsSameResolution(again, firstPass[s]),
                 "W1: source %u over the frozen half says \"%s\" once MM's live save is another world, \"%s\" before: "
                 "the resolver read the live save",
                 (unsigned)s, again.text, firstPass[s].text);
    }
    printf("[TEST]   W1: with MM's live save another world, all %d source(s) over the frozen half say the same\n",
           (int)RSBS_MM_HINT_SRC_COUNT);

    // W5: the pane over the frozen half while OoT plays.
    memset(gComboCtx.mmHintsRead, 0, sizeof(gComboCtx.mmHintsRead));
    const int rows = Combo_HintRowCount((uint8_t)GAME_MM);
    HS_CHECK(rows > 0, "W5: the pane lists nothing over a real world");
    for (int i = 0; i < rows; i++) {
        ComboHintRow row;
        HS_CHECK(Combo_HintRowAt((uint8_t)GAME_MM, i, &row) && !row.known &&
                     strcmp(row.display, RSBS_HINT_MASKED_TEXT) == 0,
                 "W5: unread row %d is not masked", i);
        (void)Combo_HintMarkRead((uint8_t)GAME_MM, row.info.source);
    }
    int known = 0;
    for (int i = 0; i < rows; i++) {
        ComboHintRow row;
        HS_CHECK(Combo_HintRowAt((uint8_t)GAME_MM, i, &row) && row.info.source < RSBS_MM_HINT_SRC_COUNT,
                 "W5: row %d vanished", i);
        const ComboHintResolution& frozen = firstPass[row.info.source];
        const int said = firstSaid[row.info.source];
        HS_CHECK(row.read && row.known == (said == 1), "W5: read row %d (source %u) is not known as its resolution is",
                 i, (unsigned)row.info.source);
        if (row.known) {
            known++;
            HS_CHECK(HsSameResolution(row.hint, frozen) && row.display == HsPlain(frozen.text),
                     "W5: row %d's text is not the resolver's over the frozen half (MM's live save is another world)",
                     i);
        }
    }
    printf("[TEST]   W5: %d row(s), %d known once read\n", rows, known);
    return true;
}

} // namespace

TestResult MmHintSources_RunSynthetic(void) {
    printf("[TEST] mm-hint-sources: MM's hint sources, the resolver's stream, the read record and its round trips, "
           "the pane and the wiring (#1475)\n");
    const ComboContext savedCtx = gComboCtx;
    const bool ok = HsCheckSources() && HsCheckStream() && HsCheckRecord() && HsCheckRoundTrips() && HsCheckPane() &&
                    HsCheckPlainText() && HsCheckBombShopJunk() && HsCheckWiring();
    gComboCtx = savedCtx;
    if (!ok) {
        return TEST_FAIL;
    }
    printf("[TEST] mm-hint-sources: PASS\n");
    return TEST_PASS;
}

TestResult MmHintSources_RunWorld(void) {
    printf("[TEST] mm-hint-sources-world: over a real paired world, the resolver is the textbox, the read hook marks "
           "Seen, and the pane reads the frozen MM half (#1475)\n");
#define HS_WORLD_ASSERT(cond)                                                                        \
    do {                                                                                             \
        if (!(cond)) {                                                                               \
            printf("[TEST] FAIL (mm-hint-sources-world): %s  [%s:%d]\n", #cond, __FILE__, __LINE__); \
            return TEST_FAIL;                                                                        \
        }                                                                                            \
    } while (0)
    HS_WORLD_ASSERT(Rando_HeadlessSeedTest("RSBSSINGLEBAG1") == 0);
    Randomizer_TestClearOoTSave();
    Context_ClearFrozenState(GAME_MM);
    HS_WORLD_ASSERT(OoT_Creation_AuthorRandoFile(0) == 1);
    HS_WORLD_ASSERT(Combo_ForeignPairingActive());
    HS_WORLD_ASSERT(Context_HasFrozenState(GAME_MM) != 0); // MM's world is in the armed half; MM never booted
    MM_TrackerAdapter_Register();
    OoT_TrackerAdapter_Register();
    Context_SetCurrentGame(GAME_OOT);
    memset(gComboCtx.mmHintsRead, 0, sizeof(gComboCtx.mmHintsRead));

    // The world in both places the resolver reads: the frozen MM half (the pane
    // while OoT plays) and MM's live save (the hooks at talk time).
    std::vector<uint8_t> shadow((size_t)MM_SAVE_CONTEXT_SIZE);
    memcpy(shadow.data(), Context_GetMMSaveContext(), shadow.size());
    std::vector<uint8_t> priorLive((size_t)MM_HintSources_TestSaveSize());
    MM_HintSources_TestCopyLive(priorLive.data());
    MM_HintSources_TestSetLive(shadow.data());

    const bool ok = HsWorldLegs(shadow);

    MM_TrackerAdapter_TestLiveSeam(-1);
    MM_HintSources_TestSetLive(priorLive.data());
    memset(gComboCtx.mmHintsRead, 0, sizeof(gComboCtx.mmHintsRead));
#undef HS_WORLD_ASSERT
    if (!ok) {
        return TEST_FAIL;
    }
    printf("[TEST] mm-hint-sources-world: PASS\n");
    return TEST_PASS;
}
