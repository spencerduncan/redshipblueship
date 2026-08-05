/**
 * @file test_curated_archive_generator.c
 * @brief Generator-level lock for the raw-segmented-texture admission guard (#605).
 *
 * scripts/make_redship_otr.py had two hard-fail admission guards before this
 * (#602/#603): a path collision with the other game's archive, and an
 * archive-dispatched resource type (Room/Cutscene/Path). It did NOT check the
 * property the whole #577 risk analysis turns on -- whether a curated display
 * list carries a RAW segmented `gsDPSetTextureImage` reference, which resolves
 * against the HOST game's segment table at draw time rather than the
 * exporting game's. #605 adds that as a third guard, walking each curated
 * ODLT resource's own command stream (constraint 5 in the generator's module
 * docstring).
 *
 * Unlike the runtime crossgame-model row (test_crossgame_model.c), this row
 * does not drive a live ResourceManager: it runs the REAL generator script as
 * a subprocess against the REAL extracted archives, so what is under test is
 * the generator's admission decision itself, not a re-implementation of it.
 *
 * Two legs, mirroring the counterfactual/control pattern the neighboring
 * #595/#593 lock uses (test_curated_archive_order.c):
 *
 *   - Negative control: a manifest curating `objects/object_slime/gChuchuEyesDL`
 *     -- the exact MM-exclusive display list the #577 spike's counterfactual
 *     2 named as carrying 2 raw segmented references at segment 0x09. The
 *     generator must exit non-zero and must NOT write an output archive.
 *   - Positive control: the REAL shipped manifest (assets/crossgame/manifest.txt,
 *     object_mask_truth -- zero raw segmented references) must still build
 *     successfully. Without this leg, a generator that refused EVERYTHING
 *     would pass the negative control vacuously.
 *
 * SKIPs (not fails) when the extracted archives or a Python interpreter are
 * not available, the same ZipContention/CuratedArchiveOrder policy: the
 * netplay-relay job re-runs this label archive-less on purpose.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++): it uses
 * std::filesystem and the shared ZapdSubprocess_Run helper.
 */

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <ship/Context.h>

#include "../zapd_subprocess.h"

namespace {

// Same resolution convention as CaoResolveArchive (test_curated_archive_order.c)
// and CrossGameModel's redship.o2r lookup: try the app-dir search first, then
// the app-bundle-relative path, and treat anything that does not exist on disk
// as absent.
std::string CagResolveArchive(const char* filename) {
    std::string path = Ship::Context::LocateFileAcrossAppDirs(filename);
    if (!path.empty() && std::filesystem::exists(path)) {
        return path;
    }
    path = Ship::Context::GetPathRelativeToAppBundle(filename);
    if (!path.empty() && std::filesystem::exists(path)) {
        return path;
    }
    return "";
}

// Runs `pythonExe script --oot-archive ootArchive --mm-archive mmArchive
// --manifest manifest --out outArchive` and returns its exit code (or -1 if
// it could not be spawned). outArchive is removed first so a leftover from a
// previous run cannot be mistaken for this run's output.
int CagRunGenerator(const std::string& pythonExe, const std::string& script, const std::string& ootArchive,
                    const std::string& mmArchive, const std::string& manifest, const std::string& outArchive) {
    std::error_code ec;
    std::filesystem::remove(outArchive, ec);

    std::vector<std::string> argStorage = { pythonExe, script,     "--oot-archive", ootArchive, "--mm-archive",
                                            mmArchive,  "--manifest", manifest,      "--out",     outArchive };
    std::vector<const char*> argv;
    argv.reserve(argStorage.size());
    for (const auto& arg : argStorage) {
        argv.push_back(arg.c_str());
    }
    return ZapdSubprocess_Run(pythonExe, argv.data(), (int)argv.size());
}

bool CagWriteFile(const std::string& path, const std::string& contents) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << contents;
    out.close();
    return !out.fail();
}

} // namespace

extern "C" int CuratedArchiveGenerator_RunHeadless(const char* pythonExe, const char* generatorScript,
                                                    const char* shippedManifest, const char* ootArchive,
                                                    const char* mmArchive) {
    const std::string workDir = std::filesystem::current_path().generic_string();
    const std::string badManifestPath = workDir + "/rsbs_test_605_bad_manifest.txt";
    const std::string badOutPath = workDir + "/rsbs_test_605_bad_out.o2r";
    const std::string goodOutPath = workDir + "/rsbs_test_605_good_out.o2r";

    int failures = 0;

    // ---- Negative control: a known raw-segmented model must be refused -----
    // objects/object_slime/gChuchuEyesDL is the exact counterfactual named in
    // #605/#577: 2 raw segmented texture references at segment 0x09. Curating
    // the single display list (not the whole object_slime/ directory) keeps
    // this row from also tripping the #602 collision guard over an unrelated
    // path, which would pass for the wrong reason.
    if (!CagWriteFile(badManifestPath, "mm objects/object_slime/gChuchuEyesDL\n")) {
        fprintf(stderr, "[curated-archive-generator] FAIL: could not write %s\n", badManifestPath.c_str());
        failures++;
    } else {
        int rc = CagRunGenerator(pythonExe, generatorScript, ootArchive, mmArchive, badManifestPath, badOutPath);
        printf("[curated-archive-generator] negative control (object_slime/gChuchuEyesDL) rc=%d\n", rc);
        if (rc == 0) {
            fprintf(stderr,
                    "[curated-archive-generator] FAIL: the generator accepted a display list known to carry raw "
                    "segmented texture references (#605) -- the admission guard is not wired up\n");
            failures++;
        }
        std::error_code ec;
        if (std::filesystem::exists(badOutPath, ec)) {
            fprintf(stderr,
                    "[curated-archive-generator] FAIL: the generator wrote %s despite refusing -- a refusal must "
                    "never leave a half-written archive behind\n",
                    badOutPath.c_str());
            failures++;
        }
    }

    // ---- Positive control: the real shipped manifest must still succeed ----
    // Without this leg, a generator that refused every manifest unconditionally
    // would pass the negative control above for the wrong reason.
    {
        int rc = CagRunGenerator(pythonExe, generatorScript, ootArchive, mmArchive, shippedManifest, goodOutPath);
        printf("[curated-archive-generator] positive control (shipped manifest) rc=%d\n", rc);
        if (rc != 0) {
            fprintf(stderr,
                    "[curated-archive-generator] FAIL: the generator refused the REAL shipped manifest (%s), which "
                    "is known to carry zero raw segmented references -- the guard has a false positive\n",
                    shippedManifest);
            failures++;
        }
        std::error_code ec;
        if (!std::filesystem::exists(goodOutPath, ec) || std::filesystem::file_size(goodOutPath, ec) == 0) {
            fprintf(stderr, "[curated-archive-generator] FAIL: the positive control did not produce a non-empty %s\n",
                    goodOutPath.c_str());
            failures++;
        }
    }

    // Leave no fixtures behind for a later row (or a later run) to trip over.
    std::error_code ec;
    std::filesystem::remove(badManifestPath, ec);
    std::filesystem::remove(badOutPath, ec);
    std::filesystem::remove(goodOutPath, ec);

    if (failures == 0) {
        printf("[curated-archive-generator] PASS: the raw-segmented-texture guard refuses a known-bad model and "
               "still accepts the real shipped manifest\n");
    }
    return failures == 0 ? 0 : 1;
}
