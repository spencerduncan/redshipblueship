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
 * The same row also covers the generator's Array reader-agreement guard
 * (#604, constraint 6): curated `*Vtx_*`/'OARR' resources are parsed by
 * whichever game is RUNNING, and the two games' Array factories diverge on
 * scalar widths OoT never implemented.
 *
 * Three legs, mirroring the counterfactual/control pattern the neighboring
 * #595/#593 lock uses (test_curated_archive_order.c):
 *
 *   - Negative control A (#605): a manifest curating
 *     `objects/object_slime/gChuchuEyesDL` -- the exact MM-exclusive display
 *     list the #577 spike's counterfactual 2 named as carrying 2 raw segmented
 *     references at segment 0x09. The generator must exit non-zero and must
 *     NOT write an output archive.
 *   - Negative control B (#604): a manifest curating
 *     `objects/object_link_zora/object_link_zora_U8_011710`, an MM ZSCALAR_X8
 *     scalar array. MM's factory consumes one byte per element there; OoT's
 *     has no X8 case, reads zero, and desyncs for the rest of the resource.
 *
 *     This leg can only be failing for the #604 guard's reason, by
 *     construction: the resource is MM-exclusive (so it cannot trip the #602
 *     collision guard), it is an 'OARR' resource (so it is neither a
 *     dispatched Room/Cutscene/Path type nor an 'ODLT' the #605 walk even
 *     looks at), and it is curated as a single path rather than the whole
 *     `object_link_zora/` directory -- which WOULD also trip #605, because
 *     `gLinkZoraHeadDL` carries four raw segmented references of its own.
 *     Measured over both full archives, it is the ONLY one of 8,047 array
 *     resources on which the two factories disagree.
 *   - Positive control: the REAL shipped manifest (assets/crossgame/manifest.txt:
 *     `mm->oot` object_mask_truth and `oot->mm` object_gi_hammer -- zero raw
 *     segmented references, only Vertex arrays both factories read with
 *     identical code) must still build BOTH halves (#577 M1).
 *     Without this leg, a generator that refused EVERYTHING would pass both
 *     negative controls vacuously.
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
// and CrossGameModel's redship-oot.o2r lookup: try the app-dir search first, then
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
// --manifest manifest --out-oot <outPrefix>-oot.o2r --out-mm <outPrefix>-mm.o2r`
// (#577 M1: the generator writes the two per-direction halves) and returns its
// exit code (or -1 if it could not be spawned). Both outputs are removed first
// so a leftover from a previous run cannot be mistaken for this run's output.
std::string CagOutPath(const std::string& outPrefix, const char* host) {
    return outPrefix + "-" + host + ".o2r";
}

int CagRunGenerator(const std::string& pythonExe, const std::string& script, const std::string& ootArchive,
                    const std::string& mmArchive, const std::string& manifest, const std::string& outPrefix) {
    std::error_code ec;
    std::filesystem::remove(CagOutPath(outPrefix, "oot"), ec);
    std::filesystem::remove(CagOutPath(outPrefix, "mm"), ec);

    std::vector<std::string> argStorage = { pythonExe,
                                            script,
                                            "--oot-archive",
                                            ootArchive,
                                            "--mm-archive",
                                            mmArchive,
                                            "--manifest",
                                            manifest,
                                            "--out-oot",
                                            CagOutPath(outPrefix, "oot"),
                                            "--out-mm",
                                            CagOutPath(outPrefix, "mm") };
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
    const std::string badOutPrefix = workDir + "/rsbs_test_605_bad_out";
    const std::string goodOutPrefix = workDir + "/rsbs_test_605_good_out";

    // #577 M1: every manifest must fill BOTH halves or the generator refuses it
    // as empty. Each negative control therefore carries this known-good
    // `oot->mm` line (the shipped seed) next to its bad `mm->oot` one, so the
    // only thing that can make it refuse is the guard under test -- a
    // single-line manifest would be refused for the empty half and pass for
    // the wrong reason.
    static const char kCagGoodOoTToMMLine[] = "oot->mm objects/object_gi_hammer/\n";

    int failures = 0;

    // One negative control: write `manifestLine` as a whole manifest, run the
    // generator on it, and require BOTH that it refuses and that it leaves no
    // output archive behind. `whatItProves` names the guard for the log.
    auto expectRefusal = [&](const char* label, const char* manifestLine, const char* whatItProves) {
        if (!CagWriteFile(badManifestPath, std::string(manifestLine) + kCagGoodOoTToMMLine)) {
            fprintf(stderr, "[curated-archive-generator] FAIL: could not write %s\n", badManifestPath.c_str());
            failures++;
            return;
        }
        int rc = CagRunGenerator(pythonExe, generatorScript, ootArchive, mmArchive, badManifestPath, badOutPrefix);
        printf("[curated-archive-generator] negative control (%s) rc=%d\n", label, rc);
        if (rc == 0) {
            fprintf(stderr, "[curated-archive-generator] FAIL: the generator ACCEPTED %s -- %s\n", label,
                    whatItProves);
            failures++;
        }
        for (const char* host : { "oot", "mm" }) {
            const std::string badOutPath = CagOutPath(badOutPrefix, host);
            std::error_code ec;
            if (std::filesystem::exists(badOutPath, ec)) {
                fprintf(stderr,
                        "[curated-archive-generator] FAIL: the generator wrote %s despite refusing -- a refusal must "
                        "never leave a half-written archive behind\n",
                        badOutPath.c_str());
                failures++;
                std::filesystem::remove(badOutPath, ec);
            }
        }
    };

    // ---- Negative control A (#605): raw segmented texture reference --------
    // objects/object_slime/gChuchuEyesDL is the exact counterfactual named in
    // #605/#577: 2 raw segmented texture references at segment 0x09. Curating
    // the single display list (not the whole object_slime/ directory) keeps
    // this leg from also tripping the #602 collision guard over an unrelated
    // path, which would pass for the wrong reason.
    expectRefusal("object_slime/gChuchuEyesDL", "mm->oot objects/object_slime/gChuchuEyesDL\n",
                  "it carries raw segmented texture references, which resolve against the HOST game's segment table "
                  "at draw time (#605) -- the raw-segmented admission guard is not wired up");

    // ---- Negative control B (#604): Array reader disagreement --------------
    // objects/object_link_zora/object_link_zora_U8_011710 is a ZSCALAR_X8
    // scalar array: MM's Array factory consumes one byte per element, OoT's
    // has no X8 case and consumes zero. See the file comment for why a
    // refusal here can only be the #604 guard's doing.
    expectRefusal("object_link_zora/object_link_zora_U8_011710",
                  "mm->oot objects/object_link_zora/object_link_zora_U8_011710\n",
                  "the two games' Array factories do not consume it identically, and the curated half is parsed by "
                  "its HOST's factory, not the exporting game's (#604) -- the reader-agreement guard is not wired "
                  "up");

    // ---- Positive control: the real shipped manifest must still succeed ----
    // Without this leg, a generator that refused every manifest unconditionally
    // would pass both negative controls above for the wrong reason.
    {
        int rc = CagRunGenerator(pythonExe, generatorScript, ootArchive, mmArchive, shippedManifest, goodOutPrefix);
        printf("[curated-archive-generator] positive control (shipped manifest) rc=%d\n", rc);
        if (rc != 0) {
            fprintf(stderr,
                    "[curated-archive-generator] FAIL: the generator refused the REAL shipped manifest (%s), which "
                    "carries zero raw segmented references and only Vertex arrays both factories read with "
                    "identical code -- a guard has a false positive\n",
                    shippedManifest);
            failures++;
        }
        for (const char* host : { "oot", "mm" }) {
            const std::string goodOutPath = CagOutPath(goodOutPrefix, host);
            std::error_code ec;
            if (!std::filesystem::exists(goodOutPath, ec) || std::filesystem::file_size(goodOutPath, ec) == 0) {
                fprintf(stderr,
                        "[curated-archive-generator] FAIL: the positive control did not produce a non-empty %s\n",
                        goodOutPath.c_str());
                failures++;
            }
        }
    }

    // Leave no fixtures behind for a later row (or a later run) to trip over.
    std::error_code ec;
    std::filesystem::remove(badManifestPath, ec);
    for (const char* host : { "oot", "mm" }) {
        std::filesystem::remove(CagOutPath(badOutPrefix, host), ec);
        std::filesystem::remove(CagOutPath(goodOutPrefix, host), ec);
    }

    if (failures == 0) {
        printf("[curated-archive-generator] PASS: the raw-segmented-texture (#605) and Array reader-agreement (#604) "
               "guards each refuse a known-bad resource, and the real shipped manifest still builds\n");
    }
    return failures == 0 ? 0 : 1;
}
