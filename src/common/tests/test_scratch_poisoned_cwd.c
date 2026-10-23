/**
 * Save-writing rows in a poisoned working directory (#824, #925).
 *
 * Two CTest rows of one build directory run at the same time (`ctest -j`, and
 * `AllTests` beside the standalone rows it contains) and share that directory
 * as their working directory. #926 moved six rows' .redsave files to a scratch
 * directory per process, but every other save-writing row still spelled its
 * directory as a fixed name relative to the working directory
 * (`rsbs_test_saves_foreign`, `rsbs_test_saves`, ...), and SoH's own
 * `Save/file<N>.sav` is relative to it too. Two such rows overwrite each other's
 * files, and one of them reads the other's: a verdict that is wrong, or no
 * verdict at all when the loser dies mid-write. Neither failure reproduces on
 * demand, which is why #824 (ForeignItemGiveReverse ended with no verdict) and
 * #925 (three slot-0 subtests of AllTests failed together) stayed open.
 *
 * This row makes the working directory hostile instead of waiting for a second
 * process. It plants, in a fresh directory, a plain FILE under every fixed name a
 * row has used there (a row that still writes `<name>/redship_slot0.redsave`
 * cannot create its directory) and DIRECTORIES under `Save/file<N>.sav` and
 * `Save/file<N>.temp` (SoH's .sav of a slot cannot be written or read), then runs
 * two groups of rows in it, each group back to back in ONE process:
 *   - the three slot-0 subtests #925 names, in the order AllTests runs them;
 *   - the foreign-item rows #824 names and the .redsave rows that shared their
 *     working directory.
 * Every group must exit 0. A row that is self-contained never looks at the
 * poison; a row that still shares the working directory fails here, whatever
 * the timing of any other process.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++). No display, no
 * ROM archives: every child row is display-free and ROM-free itself.
 */

#include "test_scratch.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

// Every fixed name a save-writing row has taken relative to its working
// directory. Planted as plain files.
const char* const kSpcFixedNames[] = {
    "rsbs_test_saves_crossing_views",
    "rsbs_test_saves_item_view_new_file",
    "rsbs_test_combo_settings",
    "rsbs_test_combo_settings_load",
    "rsbs_test_saves_spoiler_view",
    "rsbs_test_commit_gen",
    "rsbs_test_crossing_commit",
    "rsbs_test_saves_crossings",
    "rsbs_test_saves_crossings_roundtrip",
    "rsbs_test_saves_crossings_capacity",
    "rsbs_test_saves_crossings_redsave",
    "rsbs_test_saves_crossings_spoiler",
    "rsbs_test_crossing_views_world",
    "rsbs_test_saves_foreign",
    "rsbs_test_saves_grants",
    "rsbs_test_refusal_saves",
    "rsbs_test_saves",
    "rsbs_test_session",
    "rsbs_test_triforce_hunt_load",
    "rsbs_test_whole_file_commit",
    "rsbs_test_saves_combo_settings",
    "rsbs_test_saves_creation_new_file",
    "rsbs_test_saves_paired_load_restore",
    "rsbs_test_saves_spoiler_identity",
    "rsbs_test_oot_check_state_load",
};

struct SpcGroup {
    const char* why;
    const char* rows; // comma-separated, run back to back in one process
};

const SpcGroup kSpcGroups[] = {
    { "the three slot-0 subtests of #925, in AllTests' order",
      "oot-exit-harvest-gate,mm-death-decline-autosave,oot-file-select-refusal" },
    { "the foreign-item rows of #824 and the rows that shared their working directory",
      "foreign-item-give,foreign-item-give-reverse,foreign-host-eligibility,foreign-host-gossip-hint,"
      "save-roundtrip-tiers,save-has-delete,save-refused-quarantine,save-write-latch,commit-generation-monotonic,"
      "commit-torn-write,whole-file-commit,whole-file-redeemed-item,session-invalidation,grant-idempotency,"
      "grant-persistence,crossing-commit,crossing-store-roundtrip,crossing-store-capacity,crossing-store-redsave,"
      "crossing-store-spoiler,combo-settings-format,combo-settings-legacy-freeze,"
      "combo-spoiler-view,combo-triforce-hunt" },
};

// Plants the poison in @p cwd; returns how many names it took.
int SpcPlantPoison(const std::filesystem::path& cwd) {
    std::error_code ec;
    int planted = 0;
    for (const char* name : kSpcFixedNames) {
        std::ofstream out(cwd / name, std::ios::binary | std::ios::trunc);
        out << "poison: a row that writes here is not self-contained";
        if (out) {
            planted++;
        }
    }
    const std::filesystem::path save = cwd / "Save";
    std::filesystem::create_directories(save, ec);
    for (int slot = 1; slot <= 3; slot++) {
        for (const char* ext : { ".sav", ".temp" }) {
            const std::filesystem::path trap = save / ("file" + std::to_string(slot) + ext);
            std::filesystem::create_directories(trap, ec);
            std::ofstream inside(trap / "poison.txt");
            inside << "poison";
            if (!ec && std::filesystem::is_directory(trap)) {
                planted++;
            }
        }
        // A dirty Save/ folder: a slot file of the .redsave format's name that no
        // row wrote and no reader can parse.
        std::ofstream dirty(save / ("redship_slot" + std::to_string(slot - 1) + ".redsave"), std::ios::binary);
        dirty << "poison: not a .redsave";
        if (dirty) {
            planted++;
        }
    }
    return planted;
}

// Prints the lines of a failed child's log that say why it failed.
void SpcPrintFailureLines(const std::filesystem::path& log) {
    std::ifstream in(log);
    std::string line;
    int printed = 0;
    while (std::getline(in, line) && printed < 16) {
        if (line.find("FAIL") != std::string::npos || line.find("NOT saved") != std::string::npos ||
            line.find("ERROR") != std::string::npos || line.find("CRASH") != std::string::npos) {
            printf("[TEST]     | %s\n", line.c_str());
            printed++;
        }
    }
}

} // namespace

static TestResult Test_TestScratchPoisonedCwd(void) {
    const std::filesystem::path root = rsbs::test::ScratchDir("rsbs_test_scratch_poisoned_cwd_test");
    int failures = 0;
    int group = 0;
    for (const SpcGroup& g : kSpcGroups) {
        const std::filesystem::path cwd = root / ("cwd" + std::to_string(group));
        const std::filesystem::path log = root / ("group" + std::to_string(group) + ".log");
        std::error_code ec;
        std::filesystem::create_directories(cwd, ec);
        const int planted = SpcPlantPoison(cwd);
        const int wanted = (int)(sizeof(kSpcFixedNames) / sizeof(kSpcFixedNames[0])) + 3 * 3;
        if (planted != wanted) {
            printf("[TEST] FAIL: group %d: planted %d of %d poisoned names (this row would pass vacuously)\n", group,
                   planted, wanted);
            failures++;
            group++;
            continue;
        }
        const int code = rsbs::test::RunRowsInDir(g.rows, cwd, log);
        const bool ok = code == 0;
        printf("[TEST] %s: %s, in a poisoned working directory, one process, exited %d\n", ok ? "ok" : "FAIL", g.why,
               code);
        if (!ok) {
            failures++;
            SpcPrintFailureLines(log);
        }
        group++;
    }
    printf("[TEST] %s: test-scratch-poisoned-cwd, %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? TEST_PASS : TEST_FAIL;
}
