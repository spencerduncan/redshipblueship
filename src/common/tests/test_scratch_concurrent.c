/**
 * Concurrent save-writing rows lock (#926).
 *
 * Every worktree on this machine shares one temp directory, and lanes and gates
 * run their tiers at the same time. The save-writing rows below used to keep
 * their .redsave files under a FIXED directory in it
 * (`<temp>/rsbs_<name>_test`), so two runs of the same row on one machine
 * emptied and renamed into the same directory: in the #922 gate the AllTests
 * row failed on `rename ... redship_slot0.redsave.tmp -> ... "Access is
 * denied."` while another worktree ran the same rows.
 *
 * This row is that overlap on purpose: for each save-writing row it starts two
 * processes of `<this executable> --test <row>` at once, each in a working
 * directory of its own as two worktrees' runs are, and requires both to pass.
 * The temp directory is then the only one the copies share; with per-process
 * scratch directories there (src/common/test_scratch.h) they never see each
 * other's files. A child that fails has the lines of
 * its log that say why printed here; the full logs stay in this row's own
 * scratch directory when the row fails.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++). No display, no
 * ROM archives: every child row is display-free and ROM-free itself.
 */

#include "test_scratch.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace {

// The rows that write save files into the temp directory. Each must be
// display-free and ROM-free (it runs in the redship tier on its own).
const char* const kTscRows[] = {
    "mm-unified-save-capture", "mm-save-failure-surface",   "mm-capture-harvest-gate",
    "oot-exit-harvest-gate",   "mm-death-decline-autosave", "oot-file-select-refusal",
};

// Prints the lines of a failed child's log that say why it failed.
void TscPrintFailureLines(const std::filesystem::path& log) {
    std::ifstream in(log);
    std::string line;
    int printed = 0;
    while (std::getline(in, line) && printed < 12) {
        if (line.find("FAIL") != std::string::npos || line.find("NOT saved") != std::string::npos ||
            line.find("ERROR") != std::string::npos) {
            printf("[TEST]     | %s\n", line.c_str());
            printed++;
        }
    }
}

} // namespace

static TestResult Test_TestScratchConcurrent(void) {
    const std::filesystem::path logDir = rsbs::test::ScratchDir("rsbs_test_scratch_concurrent_test");
    int failures = 0;
    for (const char* row : kTscRows) {
        const std::vector<int> codes = rsbs::test::RunRowConcurrently(row, 2, logDir);
        for (size_t i = 0; i < codes.size(); i++) {
            const bool ok = codes[i] == 0;
            printf("[TEST] %s: two concurrent copies of '%s', copy %zu exited %d\n", ok ? "ok" : "FAIL", row, i,
                   codes[i]);
            if (!ok) {
                failures++;
                TscPrintFailureLines(logDir / (std::string(row) + "." + std::to_string(i) + ".log"));
            }
        }
    }
    printf("[TEST] %s: test-scratch-concurrent, %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? TEST_PASS : TEST_FAIL;
}
