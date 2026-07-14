/**
 * @file zapd_subprocess.h
 * @brief Bundled standalone ZAPD subprocess driver for in-app extraction (issue #325)
 *
 * In the single executable zapd_main cannot be called in-process: both
 * ZAPDLib_OoT and ZAPDLib_MM define it (same sources, different GAME_*
 * defines), so the link can only ever resolve the symbol to one variant.
 * In-app extraction instead spawns the bundled standalone ZAPD executable
 * (ZAPD_OoT / ZAPD_MM, packaged under <exe dir>/assets/extractor) as a
 * subprocess — the same way CI's extract_assets.py drives it. Both games'
 * extractors share this driver.
 */

#ifndef RSBS_COMMON_ZAPD_SUBPROCESS_H
#define RSBS_COMMON_ZAPD_SUBPROCESS_H

#include <string>

/**
 * Locate the bundled standalone ZAPD executable `variant` ("ZAPD_OoT" or
 * "ZAPD_MM") under assetsPath + "/extractor". Probes the platform name
 * variants: "<variant>.exe" on Windows; "<variant>.out" (ZAPDTR's
 * OUTPUT_NAME for Linux/macOS) and the bare "<variant>" elsewhere.
 *
 * @return the path of the executable that exists, or "" when none does
 */
std::string ZapdSubprocess_Locate(const std::string& assetsPath, const char* variant);

/**
 * Run a ZAPD executable as a subprocess and wait for it to finish. The child
 * inherits the process working directory, which the extraction flow has
 * already set to the temp extraction dir, so ZAPD's relative paths
 * (assets/..., the output o2r) resolve unchanged. argv[0] is replaced with
 * exePath.
 *
 * @return the child's exit code, or -1 if the process could not be spawned
 *         or terminated abnormally
 */
int ZapdSubprocess_Run(const std::string& exePath, const char* const* argv, int argc);

#endif // RSBS_COMMON_ZAPD_SUBPROCESS_H
