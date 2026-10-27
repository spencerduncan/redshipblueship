/**
 * @file oot_log_noise_test.cpp
 * ROM-free, display-free lock for #1204: two per-frame paths filled the log.
 * CTest label "redship", row OoTLogNoise in CMake/SingleExecutable.cmake,
 * dispatch "oot-log-noise" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. The operator's v0.3.1 log (114,410 lines) held:
 *   1. 113,123 lines of "[SaveManager.cpp] File[255] - does not exist". SoH's
 *      interval autosave (Enhancements/QoL/Autosave.cpp) runs on every
 *      OnGameFrameUpdate; once its three-minute interval has elapsed it re-checks
 *      Autosave_CanSave every frame until a save can be taken, and the first
 *      thing that check did was SaveManager::SaveFile_Exist(gSaveContext.fileNum),
 *      which logs its answer at info level on every call. A cross-game session's
 *      OoT half runs with fileNum pinned to the 0xFF sentinel (and the title
 *      screen with 0xFF too), so the probe asked for slot 255 every frame,
 *      forever. A real slot logged "exists" every frame the same way whenever
 *      the save was held back (the game paused, a cutscene, the first 60 frames).
 *   2. 120 lines of "[hook_handlers.cpp] Attempting to give Item mod 1 item 135
 *      from RC 721": the randomizer's item queue handler offers the queued give
 *      on every player update until the player takes it, and logged every offer.
 *      RC 721 is RC_GANONS_CASTLE_DEKU_SCRUB_RIGHT, a business scrub (EnDns):
 *      the purchase sets the check's flag inside the sale textbox, and the give
 *      waits until Link leaves that conversation. The wait is intended; the
 *      per-frame line was not.
 *
 * THE LEGS (every log line is captured through a synchronous spdlog sink that
 * replaces the default logger for the row; SPDLOG_INFO resolves the default
 * logger at each call):
 *   1. ten interval-autosave frames in a cross-game OoT session (fileNum 0xFF,
 *      GAMEMODE_NORMAL, a live PlayState and Player): "File[255]" is logged at
 *      most once;
 *   2. ten interval-autosave frames on a real slot whose file exists while the
 *      game is paused: "File[0]" is logged at most once;
 *   3. SaveFile_Exist on a real slot with no file, ten times: the miss is logged
 *      exactly once (a real miss is never silenced); then the file appears and
 *      "exists" is logged exactly once; then it goes and the miss is logged once
 *      more (a change is always logged);
 *   4. a queued give the player does not take for ten frames: one "Attempting
 *      to give" line, not ten, and the slot's release logs one line counting the
 *      ten attempts; control: a give taken at the first attempt logs no count.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "SaveManager.h"
#include "Enhancements/game-interactor/GameInteractor.h"
#include <libultraship/libultraship.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

#include "test_scratch.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

extern "C" {
#include "z64.h"
#include "macros.h"
extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
// Enhancements/QoL/Autosave.cpp: one frame of the interval autosave with its
// interval elapsed (the per-frame re-check).
void OoT_Autosave_TestIntervalFrame(void);
// Enhancements/randomizer/hook_handlers.cpp: occupy the give slot.
void OoT_Rando_TestQueueGive(uint16_t rc, const GetItemEntry* entry);
void OoT_SaveManager_SetSavDirectoryForTest(const char* dir);
}

// Enhancements/randomizer/hook_handlers.cpp: the item queue handler (an
// OnPlayerUpdate hook) and the show-only give's slot release.
void RandomizerOnPlayerUpdateForItemQueueHandler();
void Randomizer_ReleaseQueuedShowOnly(uint16_t rc);

namespace {

#define OLN_ASSERT(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            failures++;                               \
        }                                             \
    } while (0)

class CaptureSink : public spdlog::sinks::base_sink<std::mutex> {
  public:
    std::vector<std::string> lines;

  protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        lines.emplace_back(msg.payload.data(), msg.payload.size());
    }
    void flush_() override {
    }
};

int CountContaining(const std::vector<std::string>& lines, const std::string& needle) {
    int n = 0;
    for (const std::string& line : lines) {
        if (line.find(needle) != std::string::npos) {
            n++;
        }
    }
    return n;
}

void Dump(const std::vector<std::string>& lines) {
    for (const std::string& line : lines) {
        printf("[TEST]   log: %s\n", line.c_str());
    }
}

constexpr uint16_t kScrubRight = 721; // RC_GANONS_CASTLE_DEKU_SCRUB_RIGHT, the check in the report

} // namespace

extern "C" int OoT_LogNoise_RunHeadless(void) {
    printf("[TEST] oot-log-noise: the interval autosave's slot probe and the item queue's give retries log once, not "
           "once per frame (#1204)\n");
    int failures = 0;

    const std::filesystem::path dir = rsbs::test::ScratchDir("rsbs_oot_log_noise_test");
    const std::filesystem::path savDir = dir / "sav";
    std::error_code ec;
    std::filesystem::create_directories(savDir, ec);
    OoT_SaveManager_SetSavDirectoryForTest(savDir.string().c_str());

    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    if (SaveManager::Instance == nullptr) {
        SaveManager::Instance = new SaveManager(); // kept alive, like every other *::Instance singleton
    }

    auto sink = std::make_shared<CaptureSink>();
    auto capture = std::make_shared<spdlog::logger>("rsbs-oot-log-noise", sink);
    capture->set_level(spdlog::level::trace);
    const std::shared_ptr<spdlog::logger> loggerBefore = spdlog::default_logger();
    spdlog::set_default_logger(capture);

    auto saveBefore = std::make_unique<SaveContext>();
    memcpy(saveBefore.get(), &gSaveContext, sizeof(SaveContext));
    PlayState* const playBefore = OoT_gPlayState;

    // A live PlayState whose only actor is a Player standing still.
    std::unique_ptr<uint8_t[]> playBytes(new uint8_t[sizeof(PlayState)]());
    std::unique_ptr<uint8_t[]> playerBytes(new uint8_t[sizeof(Player)]());
    PlayState* play = reinterpret_cast<PlayState*>(playBytes.get());
    Player* player = reinterpret_cast<Player*>(playerBytes.get());
    player->actor.id = ACTOR_PLAYER;
    player->actor.category = ACTORCAT_PLAYER;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].head = &player->actor;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].length = 1;
    play->sceneNum = SCENE_HYRULE_FIELD;
    play->gameplayFrames = 1000;
    OoT_gPlayState = play;

    const int kFrames = 10;

    // Leg 1: a cross-game OoT session, slot sentinel 0xFF.
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    sink->lines.clear();
    for (int i = 0; i < kFrames; i++) {
        OoT_Autosave_TestIntervalFrame();
    }
    {
        const int n = CountContaining(sink->lines, "File[255]");
        printf("[TEST] leg 1: %d interval-autosave frames at fileNum 0xFF logged %d File[255] line(s)\n", kFrames, n);
        OLN_ASSERT(n <= 1, "leg 1: the autosave's slot probe logged File[255] %d times in %d frames (want at most 1)",
                   n, kFrames);
        if (n > 1) {
            Dump(std::vector<std::string>(sink->lines.begin(), sink->lines.begin() + 2));
        }
    }

    // Leg 2: a real slot whose file exists, held back by the pause menu.
    const std::filesystem::path slot0 = savDir / "file1.sav";
    {
        std::ofstream out(slot0, std::ios::binary);
        out << "{}";
    }
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.inventory.questItems = (1 << QUEST_SONG_TIME);
    play->pauseCtx.state = 1;
    sink->lines.clear();
    for (int i = 0; i < kFrames; i++) {
        OoT_Autosave_TestIntervalFrame();
    }
    play->pauseCtx.state = 0;
    {
        const int n = CountContaining(sink->lines, "File[0]");
        printf("[TEST] leg 2: %d paused interval-autosave frames on slot 0 logged %d File[0] line(s)\n", kFrames, n);
        OLN_ASSERT(n <= 1,
                   "leg 2: the autosave's slot probe logged File[0] %d times in %d paused frames (want at most 1)", n,
                   kFrames);
    }
    std::filesystem::remove(slot0, ec);

    // Leg 3: a real miss on a real slot is logged, once per change.
    sink->lines.clear();
    for (int i = 0; i < kFrames; i++) {
        OLN_ASSERT(!SaveManager::Instance->SaveFile_Exist(1),
                   "leg 3: slot 1 has no file but SaveFile_Exist says it does");
    }
    const std::filesystem::path slot1 = savDir / "file2.sav";
    {
        std::ofstream out(slot1, std::ios::binary);
        out << "{}";
    }
    for (int i = 0; i < kFrames; i++) {
        OLN_ASSERT(SaveManager::Instance->SaveFile_Exist(1), "leg 3: slot 1 has a file but SaveFile_Exist says not");
    }
    std::filesystem::remove(slot1, ec);
    for (int i = 0; i < kFrames; i++) {
        OLN_ASSERT(!SaveManager::Instance->SaveFile_Exist(1),
                   "leg 3: slot 1's file is gone but SaveFile_Exist says not");
    }
    {
        const int misses = CountContaining(sink->lines, "File[1] - does not exist");
        const int hits = CountContaining(sink->lines, "File[1] - exists");
        printf("[TEST] leg 3: 3 x %d probes of slot 1 (missing, present, missing) logged %d miss and %d hit line(s)\n",
               kFrames, misses, hits);
        OLN_ASSERT(misses == 2, "leg 3: the miss was logged %d times (want once per change: 2)", misses);
        OLN_ASSERT(hits == 1, "leg 3: the hit was logged %d times (want once: 1)", hits);
    }

    // Leg 4: a queued give the player does not take for ten frames.
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    GetItemEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.modIndex = 1;
    entry.tableId = 1;
    entry.itemId = 135;
    entry.getItemId = 135;
    sink->lines.clear();
    OoT_Rando_TestQueueGive(kScrubRight, &entry);
    for (int i = 0; i < kFrames; i++) {
        RandomizerOnPlayerUpdateForItemQueueHandler();
    }
    Randomizer_ReleaseQueuedShowOnly(kScrubRight);
    {
        const int attempts = CountContaining(sink->lines, "Attempting to give Item mod 1 item 135 from RC 721");
        const int totals = CountContaining(sink->lines, "after 10 attempts");
        printf("[TEST] leg 4: %d offers of one blocked give logged %d attempt line(s) and %d count line(s)\n", kFrames,
               attempts, totals);
        OLN_ASSERT(attempts == 1, "leg 4: the blocked give logged %d attempt lines in %d frames (want 1)", attempts,
                   kFrames);
        OLN_ASSERT(totals == 1, "leg 4: the release logged %d lines counting the 10 attempts (want 1)", totals);
        if (attempts != 1 || totals != 1) {
            Dump(sink->lines);
        }
    }
    // Control: a give taken at once logs its attempt and no count.
    sink->lines.clear();
    OoT_Rando_TestQueueGive(kScrubRight, &entry);
    RandomizerOnPlayerUpdateForItemQueueHandler();
    Randomizer_ReleaseQueuedShowOnly(kScrubRight);
    {
        const int attempts = CountContaining(sink->lines, "Attempting to give Item mod 1 item 135 from RC 721");
        const int totals = CountContaining(sink->lines, "attempts");
        OLN_ASSERT(attempts == 1, "leg 4 control: a give taken at once logged %d attempt lines (want 1)", attempts);
        OLN_ASSERT(totals == 0, "leg 4 control: a give taken at once logged %d count lines (want 0)", totals);
    }
    OoT_Rando_TestQueueGive(0, nullptr);

    OoT_gPlayState = playBefore;
    memcpy(&gSaveContext, saveBefore.get(), sizeof(SaveContext));
    spdlog::set_default_logger(loggerBefore);
    OoT_SaveManager_SetSavDirectoryForTest(nullptr);

    printf("[TEST] oot-log-noise: %s (%d failure(s))\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures;
}

#endif // RSBS_SINGLE_EXECUTABLE
