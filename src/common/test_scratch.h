/**
 * @file test_scratch.h
 * @brief Per-process scratch directories for test rows that write files (#926)
 *
 * Every worktree on one machine shares the same temp directory, and every lane
 * and gate runs its tiers at the same time as the others. A row that writes its
 * .redsave files under a FIXED name in the temp directory therefore shares that
 * directory with every other run of the same row: one process empties it while
 * the other is renaming its slot file into place, and the rename fails with
 * "Access is denied". ScratchDir hands each process its own directory instead.
 *
 * Lifetime: the test runner calls ScratchDirs_EndRow after every row. A row that
 * passed (or skipped) has its directories removed; a row that failed keeps them
 * for forensics, and the runner prints where they are.
 */

#ifndef RSBS_COMMON_TEST_SCRATCH_H
#define RSBS_COMMON_TEST_SCRATCH_H

#include <filesystem>
#include <string>
#include <vector>

namespace rsbs::test {

/**
 * The calling process's own scratch directory for @p name:
 * `<temp>/<name>.<pid>`. The first call for a name in a row empties and creates
 * it and registers it for ScratchDirs_EndRow; a later call in the same row
 * returns the same directory unchanged.
 */
std::filesystem::path ScratchDir(const std::string& name);

/**
 * ScratchDir(@p name) as a C string, for the rows that spell their save
 * directory as a `const char*` (SaveManager::SetSaveDirectory, create_directories,
 * a path joined with `/`). The pointer stays valid for the life of the process.
 */
const char* ScratchDirCStr(const std::string& name);

/**
 * End of a test row: removes every directory ScratchDir handed out since the
 * previous call when @p passed, otherwise keeps them and prints their paths.
 */
void ScratchDirs_EndRow(const char* rowName, bool passed);

/**
 * Runs `<this executable> --test <row>` @p copies times AT ONCE (one process
 * each, this process's environment) and waits for all of them. Child i works in
 * its own new directory `<logDir>/<row>.<i>` (as two worktrees' runs do: the
 * only directory the copies share is the temp directory) and its stdout and
 * stderr go to `<logDir>/<row>.<i>.log`.
 *
 * @return one exit code per child, or -1 for a child that could not be spawned
 *         or did not exit normally
 */
std::vector<int> RunRowConcurrently(const char* row, int copies, const std::filesystem::path& logDir);

/**
 * Runs `<this executable> --test <rowArg>` ONCE with @p cwd as its working
 * directory and waits for it; its stdout and stderr go to @p log. @p rowArg is
 * a row name or a comma-separated list of row names, which the runner runs back
 * to back in that one process (`--test a,b,c`).
 *
 * @return the exit code, or -1 if the child could not be spawned or did not
 *         exit normally
 */
int RunRowsInDir(const std::string& rowArg, const std::filesystem::path& cwd, const std::filesystem::path& log);

} // namespace rsbs::test

#endif // RSBS_COMMON_TEST_SCRATCH_H
