/**
 * In-app Majora's Mask extraction seams (#944).
 *
 * A default launch boots Ocarina of Time, and its first-launch flow (RunExtract,
 * games/oot/soh/OTRGlobals.cpp) now also offers to generate mm.o2r. It reaches
 * MM's extractor through src/common/mm_extract.h, because OoT's and MM's
 * Extractor/Extract.h define the same global names and cannot share a
 * translation unit. Before the flow shows anything it scans the application's
 * directories for an MM ROM, so the scan and the validation must be SILENT: an
 * error box for a file that merely sits next to the executable (the OoT ROM, a
 * zipped download, a readme) would interrupt every launch. A timeout cannot
 * watch for a native box either (on the Linux CI tier, with no display, one
 * returns at once), so MMExtractor counts every box it shows and this row reads
 * that count around everything it calls.
 *
 * Leg A, ROM-free, never skips:
 *   1. A scan of a directory holding five things a ROM scan can meet finds no
 *      ROM, and ValidateRomFile rejects each of them and a missing path:
 *        zeros.z64   a supported size (32 MiB), uncompressed, no known header
 *        zipped.v64  a supported size behind a ZIP signature
 *        small.n64   a ROM extension at a size no MM ROM has
 *        notes.txt   no ROM extension
 *        dir.z64     a directory with a ROM extension
 *      Nothing throws and the native box count does not move. The two 32 MiB
 *      files are sized with resize_file, never written: this row also runs
 *      inside AllTests under the same 60 s budget.
 *   2. RomSearchDirs: MM's app directory "." and the install path spelled
 *      absolutely are one directory, so they are one entry; $APPIMAGE adds the
 *      directory its file sits in (on AppImage runs the ROM sits next to the
 *      .AppImage, the bundle path being a read-only mount); an $APPIMAGE whose
 *      directory does not exist adds nothing, and neither does a missing install
 *      path.
 *   3. ToolsAvailable needs BOTH halves under <install>/assets:
 *      Config_N64_US.xml and the bundled ZAPD_MM executable. ZapdSubprocess_Locate
 *      probes existence only, so empty files stand in for both.
 *
 * Leg B, ROM-staged: with <checkout>/OTRExporter/mm.z64 staged, ValidateRomFile
 * accepts it, a scan of that directory lists it (compared with
 * std::filesystem::equivalent: the scanner builds Windows paths with '\') and
 * not the OoT ROM staged beside it, still without a box. That directory holds
 * other ROMs too (the OoT ones and a descriptively named MM copy), so the
 * assertion is containment, not the whole list. Without the ROM the leg prints
 * its own SKIP line and the row still PASSes: a whole-row skip would hide leg A
 * on CI. A full Generate is minutes of ZAPD and stays in the manual recipe.
 *
 * Nothing here needs a Ship::Context: MM's app directory comes from the static
 * Ship::Context::GetAppDirectoryPath, and no bridge function touches the
 * context. Included at FILE SCOPE by test_runner.cpp (compiled as C++).
 */

#include "../mm_extract.h"
#include "../test_scratch.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace {

// The smallest size MM's extractor accepts (MB32 in its Extract.h).
constexpr std::uintmax_t kMmxRomSize = 32ull * 1024 * 1024;

// Calls one bridge function and turns an escaped exception into a named
// failure. Every bridge entry promises to catch everything itself: its callers
// run on the extraction worker thread or inside the boot flow, where an escaped
// exception ends the app.
template <typename F> bool MmxCall(const std::string& what, F&& call) {
    try {
        call();
        return true;
    } catch (const std::exception& e) {
        printf("[TEST] FAIL: mm-extract: %s threw (%s); the bridge must catch every exception\n", what.c_str(),
               e.what());
    } catch (...) {
        printf("[TEST] FAIL: mm-extract: %s threw a non-standard exception; the bridge must catch every exception\n",
               what.c_str());
    }
    return false;
}

// Sets or unsets one environment variable for a scope and puts the prior value
// back on every exit (a failed assertion returns early), so nothing leaks into
// a later row of `--test all`. An empty prior value is restored as unset:
// Windows cannot hold an empty variable.
class MmxEnvOverride {
  public:
    explicit MmxEnvOverride(const char* name) : mName(name) {
        const char* old = std::getenv(name);
        mHadValue = old != nullptr && old[0] != '\0';
        if (mHadValue) {
            mOldValue = old;
        }
    }
    ~MmxEnvOverride() {
        Set(mHadValue ? mOldValue.c_str() : nullptr);
    }
    MmxEnvOverride(const MmxEnvOverride&) = delete;
    MmxEnvOverride& operator=(const MmxEnvOverride&) = delete;

    // nullptr or "" unsets the variable.
    void Set(const char* value) {
        const bool unset = value == nullptr || value[0] == '\0';
#ifdef _WIN32
        _putenv_s(mName, unset ? "" : value);
#else
        if (unset) {
            unsetenv(mName);
        } else {
            setenv(mName, value, 1);
        }
#endif
    }

  private:
    const char* mName;
    bool mHadValue = false;
    std::string mOldValue;
};

bool MmxWriteFile(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    return !out.fail();
}

// Extends a file to `size` without writing the bytes (sparse where the
// filesystem allows it).
bool MmxResize(const std::filesystem::path& path, std::uintmax_t size) {
    std::error_code ec;
    std::filesystem::resize_file(path, size, ec);
    if (ec) {
        return false;
    }
    const std::uintmax_t actual = std::filesystem::file_size(path, ec);
    return !ec && actual == size;
}

// True when `paths` holds an entry naming the same file or directory as
// `target`. Not string equality: the scanner builds Windows paths with '\', and
// "." and its absolute spelling are one directory.
bool MmxContainsEquivalent(const std::vector<std::string>& paths, const std::filesystem::path& target) {
    for (const std::string& path : paths) {
        std::error_code ec;
        if (std::filesystem::equivalent(path, target, ec) && !ec) {
            return true;
        }
    }
    return false;
}

void MmxPrintList(const char* label, const std::vector<std::string>& paths) {
    printf("[TEST]     %s: %zu entr%s\n", label, paths.size(), paths.size() == 1 ? "y" : "ies");
    for (const std::string& path : paths) {
        printf("[TEST]       %s\n", path.c_str());
    }
}

// MMExtractor's native box count has not moved since `before`.
bool MmxNoBoxes(int before, const char* during) {
    int now = before;
    if (!MmxCall("DebugNativeBoxCount", [&] { now = MM_Extract_DebugNativeBoxCount(); })) {
        return false;
    }
    if (now != before) {
        printf("[TEST] FAIL: mm-extract: %d native message box(es) shown %s; the scan and the validation must be "
               "silent\n",
               now - before, during);
        return false;
    }
    return true;
}

// Leg A, part 1: nothing that is not an MM ROM survives the silent scan or the
// silent validation, and neither of them throws or shows a box.
bool MmxLegARejects(const std::filesystem::path& scratch, int boxesBefore) {
    std::error_code ec;
    std::filesystem::create_directories(scratch / "dir.z64", ec);
    const bool staged = MmxWriteFile(scratch / "zeros.z64", "") && MmxResize(scratch / "zeros.z64", kMmxRomSize) &&
                        MmxWriteFile(scratch / "zipped.v64", std::string("PK\x03\x04", 4)) &&
                        MmxResize(scratch / "zipped.v64", kMmxRomSize) &&
                        MmxWriteFile(scratch / "small.n64", std::string(4096, '\0')) &&
                        MmxWriteFile(scratch / "notes.txt", "not a ROM\n") &&
                        std::filesystem::is_directory(scratch / "dir.z64", ec);
    if (!staged) {
        printf("[TEST] FAIL: mm-extract: could not stage the non-ROM candidates in %s\n", scratch.string().c_str());
        return false;
    }
    printf("[TEST] mm-extract: staged zeros.z64 and zipped.v64 (32 MiB), small.n64 (4 KiB), notes.txt and a "
           "directory dir.z64 in %s\n",
           scratch.string().c_str());

    // A directory that does not exist must add nothing and must not end the
    // scan of the others.
    const std::vector<std::string> scanDirs = { scratch.string(), (scratch / "no-such-dir").string() };
    std::vector<std::string> found;
    if (!MmxCall("FindValidRoms(scratch)", [&] { found = MM_Extract_FindValidRoms(scanDirs); })) {
        return false;
    }
    if (!found.empty()) {
        printf("[TEST] FAIL: mm-extract: FindValidRoms listed a file that is not an MM ROM\n");
        MmxPrintList("FindValidRoms", found);
        return false;
    }
    printf("[TEST] ok: FindValidRoms over the scratch dir and a missing dir found no ROM\n");

    const char* const candidates[] = { "zeros.z64", "zipped.v64", "small.n64", "notes.txt", "dir.z64", "missing.z64" };
    for (const char* name : candidates) {
        const std::string path = (scratch / name).string();
        bool valid = true;
        if (!MmxCall(std::string("ValidateRomFile(") + name + ")", [&] { valid = MM_Extract_ValidateRomFile(path); })) {
            return false;
        }
        if (valid) {
            printf("[TEST] FAIL: mm-extract: ValidateRomFile accepted %s, which is not an MM ROM\n", name);
            return false;
        }
    }
    printf("[TEST] ok: ValidateRomFile rejected the five candidates and a missing path, and none threw\n");

    if (!MmxNoBoxes(boxesBefore, "while scanning and validating files that are not MM ROMs")) {
        return false;
    }
    printf("[TEST] ok: no native message box shown by the scan or the validation (count %d)\n", boxesBefore);
    return true;
}

// Leg A, part 2: which directories the boot flow scans.
bool MmxLegASearchDirs(const std::filesystem::path& scratch) {
    // libultraship's GetAppDirectoryPath returns $SHIP_HOME on Linux and macOS,
    // and CreateHarnessStyleContext points it at a sandbox for the rest of the
    // process under `--test all`. Unset it for this leg so MM's app directory is
    // ".", as in a portable launch. Both variables are restored on every exit.
    MmxEnvOverride shipHome("SHIP_HOME");
    shipHome.Set(nullptr);
    MmxEnvOverride appImage("APPIMAGE");
    appImage.Set(nullptr);

    const std::string install = std::filesystem::absolute(".").string();
    std::vector<std::string> dirs;
    if (!MmxCall("RomSearchDirs(absolute \".\")", [&] { dirs = MM_Extract_RomSearchDirs(install); })) {
        return false;
    }
    if (dirs.size() != 1 || !MmxContainsEquivalent(dirs, ".")) {
        printf("[TEST] FAIL: mm-extract: RomSearchDirs(%s) with $APPIMAGE unset must be ONE entry, the working "
               "directory: MM's app dir \".\" and that install path are the same directory\n",
               install.c_str());
        MmxPrintList("RomSearchDirs", dirs);
        return false;
    }
    printf("[TEST] ok: RomSearchDirs: MM's app dir \".\" and the install path %s are one entry\n", install.c_str());

    const std::string missingInstall = (scratch / "no-such-install").string();
    if (!MmxCall("RomSearchDirs(missing install)", [&] { dirs = MM_Extract_RomSearchDirs(missingInstall); })) {
        return false;
    }
    if (dirs.size() != 1 || !MmxContainsEquivalent(dirs, ".")) {
        printf("[TEST] FAIL: mm-extract: RomSearchDirs(%s) must list only existing directories (MM's app dir)\n",
               missingInstall.c_str());
        MmxPrintList("RomSearchDirs", dirs);
        return false;
    }
    printf("[TEST] ok: RomSearchDirs: a missing install path adds nothing\n");

    const std::filesystem::path appImageFile = scratch / "fake.AppImage";
    if (!MmxWriteFile(appImageFile, "")) {
        printf("[TEST] FAIL: mm-extract: could not create %s\n", appImageFile.string().c_str());
        return false;
    }
    appImage.Set(appImageFile.string().c_str());
    if (!MmxCall("RomSearchDirs($APPIMAGE set)", [&] { dirs = MM_Extract_RomSearchDirs(install); })) {
        return false;
    }
    if (dirs.size() != 2 || !MmxContainsEquivalent(dirs, ".") || !MmxContainsEquivalent(dirs, scratch)) {
        printf("[TEST] FAIL: mm-extract: with $APPIMAGE=%s, RomSearchDirs must list the working directory and %s\n",
               appImageFile.string().c_str(), scratch.string().c_str());
        MmxPrintList("RomSearchDirs", dirs);
        return false;
    }
    printf("[TEST] ok: RomSearchDirs: $APPIMAGE adds the directory of the .AppImage file\n");

    const std::string nowhere = (scratch / "no-such-dir" / "fake.AppImage").string();
    appImage.Set(nowhere.c_str());
    if (!MmxCall("RomSearchDirs($APPIMAGE nowhere)", [&] { dirs = MM_Extract_RomSearchDirs(install); })) {
        return false;
    }
    if (dirs.size() != 1 || !MmxContainsEquivalent(dirs, ".")) {
        printf("[TEST] FAIL: mm-extract: with $APPIMAGE=%s (no such directory), RomSearchDirs must add nothing\n",
               nowhere.c_str());
        MmxPrintList("RomSearchDirs", dirs);
        return false;
    }
    printf("[TEST] ok: RomSearchDirs: an $APPIMAGE in a missing directory adds nothing\n");
    return true;
}

// Leg A, part 3: the offer needs MM's extractor data and the bundled ZAPD_MM.
bool MmxLegATools(const std::filesystem::path& scratch) {
    const std::string install = scratch.string();
    const std::filesystem::path config = scratch / "assets" / "Config_N64_US.xml";
#ifdef _WIN32
    const std::filesystem::path zapd = scratch / "assets" / "extractor" / "ZAPD_MM.exe";
#else
    const std::filesystem::path zapd = scratch / "assets" / "extractor" / "ZAPD_MM.out";
#endif
    auto check = [&install](const char* state, bool expected) {
        bool available = !expected;
        if (!MmxCall(std::string("ToolsAvailable(") + state + ")",
                     [&] { available = MM_Extract_ToolsAvailable(install); })) {
            return false;
        }
        if (available != expected) {
            printf("[TEST] FAIL: mm-extract: ToolsAvailable is %s with %s, want %s\n", available ? "true" : "false",
                   state, expected ? "true" : "false");
            return false;
        }
        return true;
    };

    if (!check("no assets folder", false)) {
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(zapd.parent_path(), ec);
    if (ec || !MmxWriteFile(config, "")) {
        printf("[TEST] FAIL: mm-extract: could not create %s\n", config.string().c_str());
        return false;
    }
    if (!check("Config_N64_US.xml but no ZAPD_MM", false)) {
        return false;
    }
    if (!MmxWriteFile(zapd, "")) {
        printf("[TEST] FAIL: mm-extract: could not create %s\n", zapd.string().c_str());
        return false;
    }
    if (!check("Config_N64_US.xml and ZAPD_MM", true)) {
        return false;
    }
    std::filesystem::remove(config, ec);
    if (ec) {
        printf("[TEST] FAIL: mm-extract: could not remove %s\n", config.string().c_str());
        return false;
    }
    if (!check("ZAPD_MM but no Config_N64_US.xml", false)) {
        return false;
    }
    printf("[TEST] ok: ToolsAvailable needs both assets/Config_N64_US.xml and assets/extractor/%s\n",
           zapd.filename().string().c_str());
    return true;
}

// Leg B: the real MM ROM, when the checkout has one staged.
bool MmxLegB(void) {
#ifdef RSBS_SOURCE_DIR
    const std::filesystem::path romDir = std::filesystem::path(RSBS_SOURCE_DIR) / "OTRExporter";
    const std::filesystem::path mmRom = romDir / "mm.z64";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(mmRom, ec)) {
        printf("[TEST] SKIP: no OTRExporter/mm.z64 staged — leg B needs the MM ROM\n");
        return true;
    }
    bool valid = false;
    if (!MmxCall("ValidateRomFile(OTRExporter/mm.z64)", [&] { valid = MM_Extract_ValidateRomFile(mmRom.string()); })) {
        return false;
    }
    if (!valid) {
        printf("[TEST] FAIL: mm-extract: ValidateRomFile rejected the staged MM ROM %s\n", mmRom.string().c_str());
        return false;
    }
    printf("[TEST] ok: leg B: ValidateRomFile accepts %s\n", mmRom.string().c_str());

    std::vector<std::string> found;
    const std::vector<std::string> scanDirs = { romDir.string() };
    if (!MmxCall("FindValidRoms(OTRExporter)", [&] { found = MM_Extract_FindValidRoms(scanDirs); })) {
        return false;
    }
    if (!MmxContainsEquivalent(found, mmRom)) {
        printf("[TEST] FAIL: mm-extract: a scan of %s does not list mm.z64\n", romDir.string().c_str());
        MmxPrintList("FindValidRoms", found);
        return false;
    }
    // A launch with both ROMs next to the executable must offer only the MM one.
    const std::filesystem::path ootRom = romDir / "oot.z64";
    if (std::filesystem::is_regular_file(ootRom, ec) && MmxContainsEquivalent(found, ootRom)) {
        printf("[TEST] FAIL: mm-extract: a scan of %s lists the OoT ROM oot.z64 as an MM ROM\n",
               romDir.string().c_str());
        MmxPrintList("FindValidRoms", found);
        return false;
    }
    printf("[TEST] ok: leg B: a scan of OTRExporter lists mm.z64 and not oot.z64 (%zu MM ROM(s) there)\n",
           found.size());
    return true;
#else
    printf("[TEST] SKIP: RSBS_SOURCE_DIR is not defined — leg B needs the checkout's OTRExporter/mm.z64\n");
    return true;
#endif
}

} // namespace

static TestResult Test_MmExtract(void) {
    printf("[TEST] mm-extract: in-app Majora's Mask extraction seams: silent ROM scan and validation, search dirs, "
           "tools check (#944)\n");
    try {
        const std::filesystem::path scratch = rsbs::test::ScratchDir("rsbs_test_mm_extract");
        int boxesBefore = 0;
        if (!MmxCall("DebugNativeBoxCount", [&] { boxesBefore = MM_Extract_DebugNativeBoxCount(); })) {
            return TEST_FAIL;
        }
        if (!MmxLegARejects(scratch, boxesBefore) || !MmxLegASearchDirs(scratch) || !MmxLegATools(scratch) ||
            !MmxLegB() || !MmxNoBoxes(boxesBefore, "during the row")) {
            return TEST_FAIL;
        }
    } catch (const std::exception& e) {
        printf("[TEST] FAIL: mm-extract: the row's own setup threw (%s)\n", e.what());
        return TEST_FAIL;
    } catch (...) {
        printf("[TEST] FAIL: mm-extract: the row's own setup threw a non-standard exception\n");
        return TEST_FAIL;
    }
    printf("[TEST] ok: no native message box shown during the row\n");
    printf("[TEST] PASS: mm-extract\n");
    return TEST_PASS;
}
