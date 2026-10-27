/**
 * @file soh_presets_folder_test.cpp
 * ROM-free, display-free lock for #1210: a file in presets/ that is not a
 * preset JSON killed the boot. CTest label "redship", row PresetsSkipNonJson in
 * CMake/SingleExecutable.cmake, dispatch "presets-skip-non-json" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. SoH's LoadPresets() (Enhancements/Presets/Presets.cpp) runs
 * at boot from RegisterPresetsWidgets, a RegisterMenuInitFunc static, and
 * parsed every entry of presets/ as JSON with no extension filter and no catch.
 * A stray index.html (saved into every folder by a recursive HTTP fetch of an
 * install), a .json.tmp left by an interrupted save, a Thumbs.db or a .DS_Store
 * threw nlohmann::json::parse_error out of the loader, nothing caught it, and
 * the process died about 110 ms after launch with exception 0xe06d7363.
 *
 * THE ROW. A per-process scratch folder stands in for presets/ and holds:
 *   - Valid.json, a preset ("Rsbs Test Preset");
 *   - index.html, HTML text (the operator's stray file);
 *   - Truncated.json, a preset cut off mid-object (a .json that is not JSON).
 * LoadPresets()'s user-folder pass runs over it (Presets_LoadFolderForTest)
 * and every log line is captured through a synchronous spdlog sink that
 * replaces the default logger for the row. It asserts:
 *   1. the pass returns (it does not throw: on origin/main the parse error
 *      escaped and, at boot, ended the process);
 *   2. exactly the valid preset is listed;
 *   3. exactly one line at warning level or above names index.html, and
 *      exactly one error line names Truncated.json (a failure is reported
 *      once, with the file it came from, and loading carries on past it).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "Enhancements/Presets/Presets.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

#include "test_scratch.h"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

#define SPF_ASSERT(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            failures++;                               \
        }                                             \
    } while (0)

struct CapturedLine {
    spdlog::level::level_enum level;
    std::string text;
};

class CaptureSink : public spdlog::sinks::base_sink<std::mutex> {
  public:
    std::vector<CapturedLine> lines;

  protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        lines.push_back({ msg.level, std::string(msg.payload.data(), msg.payload.size()) });
    }
    void flush_() override {
    }
};

// Lines naming @p needle, and how many of them are at @p minLevel or above.
int CountNaming(const std::vector<CapturedLine>& lines, const std::string& needle, spdlog::level::level_enum minLevel,
                int* atLevel) {
    int n = 0;
    *atLevel = 0;
    for (const CapturedLine& line : lines) {
        if (line.text.find(needle) != std::string::npos) {
            n++;
            if (line.level >= minLevel) {
                (*atLevel)++;
            }
        }
    }
    return n;
}

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}

} // namespace

extern "C" int Soh_PresetsSkipNonJson_RunHeadless(void) {
    printf("[TEST] presets-skip-non-json: a non-JSON file in presets/ is skipped with one log line, not thrown out "
           "of the boot (#1210)\n");
    int failures = 0;

    const std::filesystem::path dir = rsbs::test::ScratchDir("rsbs_soh_presets_folder_test");
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    WriteFile(dir / "Valid.json", "{\n    \"presetName\": \"Rsbs Test Preset\",\n    \"fileType\": 0,\n"
                                  "    \"blocks\": {}\n}\n");
    WriteFile(dir / "index.html", "<!DOCTYPE HTML>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
                                  "<title>Directory listing for /presets/</title>\n</head>\n<body>\n"
                                  "<h1>Directory listing for /presets/</h1>\n<hr>\n<ul>\n"
                                  "<li><a href=\"Valid.json\">Valid.json</a></li>\n</ul>\n<hr>\n</body>\n</html>\n");
    WriteFile(dir / "Truncated.json", "{\n    \"presetName\": \"Rsbs Truncated Preset\",\n    \"blocks\": {\n");

    auto sink = std::make_shared<CaptureSink>();
    auto capture = std::make_shared<spdlog::logger>("rsbs-soh-presets-folder", sink);
    capture->set_level(spdlog::level::trace);
    const std::shared_ptr<spdlog::logger> loggerBefore = spdlog::default_logger();
    spdlog::set_default_logger(capture);

    bool threw = false;
    std::string what;
    std::vector<std::string> listed;
    try {
        listed = Presets_LoadFolderForTest(dir.string());
    } catch (const std::exception& e) {
        threw = true;
        what = e.what();
    } catch (...) {
        threw = true;
        what = "(not a std::exception)";
    }

    spdlog::set_default_logger(loggerBefore);

    // Leg 1: the loader returns.
    SPF_ASSERT(!threw, "leg 1: LoadPresets threw out of the presets folder pass: %s (at boot this ends the process)",
               what.c_str());

    // Leg 2: exactly the valid preset is listed.
    printf("[TEST] leg 2: %zu preset(s) listed\n", listed.size());
    for (const std::string& name : listed) {
        printf("[TEST]   listed: %s\n", name.c_str());
    }
    SPF_ASSERT(listed.size() == 1 && listed[0] == "Rsbs Test Preset",
               "leg 2: want exactly the preset \"Rsbs Test Preset\" listed, got %zu preset(s)", listed.size());

    // Leg 3: one line names each bad file.
    {
        int warnOrAbove = 0;
        const int n = CountNaming(sink->lines, "index.html", spdlog::level::warn, &warnOrAbove);
        printf("[TEST] leg 3: %d line(s) name index.html, %d at warning level or above\n", n, warnOrAbove);
        SPF_ASSERT(n == 1 && warnOrAbove == 1,
                   "leg 3: want exactly one warning-or-above line naming index.html, got %d line(s) (%d at that "
                   "level)",
                   n, warnOrAbove);
    }
    {
        int errorOrAbove = 0;
        const int n = CountNaming(sink->lines, "Truncated.json", spdlog::level::err, &errorOrAbove);
        printf("[TEST] leg 3: %d line(s) name Truncated.json, %d at error level or above\n", n, errorOrAbove);
        SPF_ASSERT(n == 1 && errorOrAbove == 1,
                   "leg 3: want exactly one error line naming Truncated.json, got %d line(s) (%d at that level)", n,
                   errorOrAbove);
    }
    if (failures != 0) {
        for (const CapturedLine& line : sink->lines) {
            printf("[TEST]   log: %s\n", line.text.c_str());
        }
    }

    printf("[TEST] presets-skip-non-json: %s (%d failure(s))\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures;
}

#endif // RSBS_SINGLE_EXECUTABLE
