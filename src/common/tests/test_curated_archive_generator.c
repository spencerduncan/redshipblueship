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
 * And, since #577 M6, the escaping-reference guard (constraint 7) and the
 * admission report: negative control C and the count check below.
 *
 * Legs, mirroring the counterfactual/control pattern the neighboring
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
 *   - Negative control C (#577 M6): `oot->mm objects/object_gi_hammer/`
 *     WITHOUT the two OoT gameplay_keep textures its display list names by
 *     path hash. It clears every other guard (it was M1's shipped seed), so
 *     only the escaping-reference guard can refuse it.
 *   - Positive control: the REAL shipped manifest (assets/crossgame/manifest.txt:
 *     every host-exclusive get-item directory in each direction, plus
 *     object_mask_truth and the gameplay_keep textures OoT's get-item lists
 *     name) must build BOTH halves (#577 M1), and its admission report must
 *     show the whole host-exclusive `object_gi_*` set admitted in each
 *     direction (#577 M6: 43 mm->oot, 36 oot->mm as measured 2026-09-30).
 *     Without this leg, a generator that refused EVERYTHING would pass the
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
#include <cstring>
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

// `reportPath`, when non-empty, is passed as --report (#577 M6) and removed
// first like the outputs.
int CagRunGenerator(const std::string& pythonExe, const std::string& script, const std::string& ootArchive,
                    const std::string& mmArchive, const std::string& manifest, const std::string& outPrefix,
                    const std::string& reportPath = "") {
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
    if (!reportPath.empty()) {
        std::filesystem::remove(reportPath, ec);
        argStorage.push_back("--report");
        argStorage.push_back(reportPath);
    }
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

// One `direction` line of the generator's admission report (#577 M6).
struct CagDirectionReport {
    bool seen = false;
    int entries = 0;
    int resources = 0;
    int giExclusive = 0;
    int giAdmitted = 0;
};

// Parses the report the generator writes with --report: one
// `direction <src>-><host> entries N resources N gi_exclusive N gi_admitted N`
// line per half and one `missing <src>-><host> <dir>` line per host-exclusive
// get-item directory the manifest does not cover. Returns false when the file
// cannot be read.
bool CagReadReport(const std::string& path, CagDirectionReport* mmToOoT, CagDirectionReport* ootToMM,
                   std::vector<std::string>* missing) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        char direction[16] = { 0 };
        char dir[256] = { 0 };
        CagDirectionReport row;
        if (sscanf(line.c_str(), "direction %15s entries %d resources %d gi_exclusive %d gi_admitted %d", direction,
                   &row.entries, &row.resources, &row.giExclusive, &row.giAdmitted) == 5) {
            row.seen = true;
            if (strcmp(direction, "mm->oot") == 0) {
                *mmToOoT = row;
            } else if (strcmp(direction, "oot->mm") == 0) {
                *ootToMM = row;
            }
        } else if (sscanf(line.c_str(), "missing %15s %255s", direction, dir) == 2) {
            missing->push_back(std::string(direction) + " " + dir);
        }
    }
    return true;
}

} // namespace

extern "C" int CuratedArchiveGenerator_RunHeadless(const char* pythonExe, const char* generatorScript,
                                                    const char* shippedManifest, const char* ootArchive,
                                                    const char* mmArchive) {
    const std::string workDir = std::filesystem::current_path().generic_string();
    const std::string badManifestPath = workDir + "/rsbs_test_605_bad_manifest.txt";
    const std::string badOutPrefix = workDir + "/rsbs_test_605_bad_out";
    const std::string goodOutPrefix = workDir + "/rsbs_test_605_good_out";
    const std::string goodReportPath = workDir + "/rsbs_test_577_m6_report.txt";

    // #577 M1: every manifest must fill BOTH halves or the generator refuses it
    // as empty. Each negative control therefore carries this known-good
    // `oot->mm` line next to its bad one, so the only thing that can make it
    // refuse is the guard under test -- a single-line manifest would be
    // refused for the empty half and pass for the wrong reason. #577 M6: the
    // Goron Tunic's directory, because every one of its display lists names
    // only its own directory (M1's seed, object_gi_hammer, names two
    // gameplay_keep textures and is now admitted only together with them).
    static const char kCagGoodOoTToMMLine[] = "oot->mm objects/object_gi_clothes/\n";

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

    // ---- Negative control C (#577 M6): a reference escaping its half -------
    // objects/object_gi_hammer/gGiHammerDL names two OoT gameplay_keep
    // environment-map textures (gEffUnknown10Tex, gEffUnknown12Tex) by path
    // hash. Curated without them, redship-mm.o2r would hand MM a model whose
    // textures resolve only if oot.o2r happens to be mounted. The hammer
    // passes the collision, dispatched-type, raw-segmented and Array guards
    // (it was M1's shipped seed), so only the escaping-reference guard can
    // refuse it. The leading `mm->oot` line is the shipped probe, known good.
    expectRefusal("object_gi_hammer without its gameplay_keep textures",
                  "mm->oot objects/object_mask_truth/\noot->mm objects/object_gi_hammer/\n",
                  "its display list names gameplay_keep textures the curated half does not carry, so MM would "
                  "resolve them against whatever it has mounted (#577 M6) -- the escaping-reference guard is not "
                  "wired up");

    // ---- Positive control: the real shipped manifest must still succeed ----
    // Without this leg, a generator that refused every manifest unconditionally
    // would pass the negative controls above for the wrong reason.
    {
        int rc = CagRunGenerator(pythonExe, generatorScript, ootArchive, mmArchive, shippedManifest, goodOutPrefix,
                                 goodReportPath);
        printf("[curated-archive-generator] positive control (shipped manifest) rc=%d\n", rc);
        if (rc != 0) {
            fprintf(stderr,
                    "[curated-archive-generator] FAIL: the generator refused the REAL shipped manifest (%s) -- its "
                    "reason is printed above: either the manifest carries an unsafe entry or a guard has a false "
                    "positive\n",
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

        // ---- Admitted-entry counts (#577 M6) --------------------------------
        // The shipped manifest must admit the WHOLE host-exclusive get-item
        // set in each direction: every `objects/object_gi_*` directory the
        // source archive carries and the host's does not, which the generator
        // derives from the archives themselves. Measured 2026-09-30: 43
        // MM-exclusive (mm->oot) and 36 OoT-exclusive (oot->mm).
        CagDirectionReport mmToOoT;
        CagDirectionReport ootToMM;
        std::vector<std::string> missing;
        if (!CagReadReport(goodReportPath, &mmToOoT, &ootToMM, &missing)) {
            fprintf(stderr,
                    "[curated-archive-generator] FAIL: the shipped manifest's run wrote no admission report at %s\n",
                    goodReportPath.c_str());
            failures++;
        } else {
            const struct {
                const char* name;
                const CagDirectionReport* row;
            } directions[] = { { "mm->oot", &mmToOoT }, { "oot->mm", &ootToMM } };
            for (const auto& d : directions) {
                if (!d.row->seen) {
                    fprintf(stderr, "[curated-archive-generator] FAIL: the admission report has no %s line\n",
                            d.name);
                    failures++;
                    continue;
                }
                printf("[curated-archive-generator] admitted %s: %d manifest entries, %d resources, %d of %d "
                       "host-exclusive object_gi_* directories\n",
                       d.name, d.row->entries, d.row->resources, d.row->giAdmitted, d.row->giExclusive);
                if (d.row->giExclusive <= 0) {
                    fprintf(stderr,
                            "[curated-archive-generator] FAIL: %s: the generator found no host-exclusive get-item "
                            "directory at all -- the coverage check below would be vacuous\n",
                            d.name);
                    failures++;
                } else if (d.row->giAdmitted != d.row->giExclusive) {
                    fprintf(stderr,
                            "[curated-archive-generator] FAIL: %s: the shipped manifest admits %d of the %d "
                            "host-exclusive object_gi_* directories (#577 M6 curates all of them)\n",
                            d.name, d.row->giAdmitted, d.row->giExclusive);
                    failures++;
                }
            }
            for (const auto& m : missing) {
                fprintf(stderr, "[curated-archive-generator]   not curated: %s\n", m.c_str());
            }
        }
    }

    // Leave no fixtures behind for a later row (or a later run) to trip over.
    std::error_code ec;
    std::filesystem::remove(goodReportPath, ec);
    std::filesystem::remove(badManifestPath, ec);
    for (const char* host : { "oot", "mm" }) {
        std::filesystem::remove(CagOutPath(badOutPrefix, host), ec);
        std::filesystem::remove(CagOutPath(goodOutPrefix, host), ec);
    }

    if (failures == 0) {
        printf("[curated-archive-generator] PASS: the raw-segmented-texture (#605), Array reader-agreement (#604) "
               "and escaping-reference (#577 M6) guards each refuse a known-bad resource, and the real shipped "
               "manifest builds and admits every host-exclusive get-item directory in both directions\n");
    }
    return failures == 0 ? 0 : 1;
}
