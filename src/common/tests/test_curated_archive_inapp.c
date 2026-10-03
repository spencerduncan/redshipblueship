/**
 * @file test_curated_archive_inapp.c
 * @brief Lock for the IN-APP curated-archive generator (#806).
 *
 * redship-oot.o2r / redship-mm.o2r are ROM-derived, so a package cannot ship
 * them; before #806 only the dev-time CMake target GenerateRedshipOtr made
 * them, and every packaged build ran without them. The game now generates both
 * halves itself at boot (src/common/curated_archives.cpp, a C++ port of
 * scripts/make_redship_otr.py's rules over the embedded manifest).
 *
 * Legs:
 *
 *   A. Parity, ROM-free. Synthetic oot.o2r / mm.o2r / soh.o2r / 2ship.o2r and a
 *      manifest whose display lists name textures and vertex arrays by path
 *      hash (one across directories, curated as a single path). The C++
 *      generator must write both halves; when a Python interpreter was
 *      configured, make_redship_otr.py runs on the same inputs and the two
 *      outputs must hold the same path set, the same payload bytes and the same
 *      stamp (the zip archive comment).
 *   B. Refusals, ROM-free. One manifest per content rule (collision with the
 *      other game's archive, collision with a host's port archive, a dispatched
 *      Room resource, a raw segmented G_SETTIMG, a display list truncated
 *      before its G_ENDDL, an X8 scalar array, a vertex array shorter than its
 *      count, a reference escaping its half, a prefix matching nothing, an
 *      empty half, the pre-split form, an unknown direction), plus one input
 *      per input rule (both halves named the same file, a required host
 *      archive absent, a source archive absent). The C++ generator must refuse
 *      each, name the rule, and write neither half; make_redship_otr.py must
 *      refuse each too.
 *   C. Staleness, ROM-free: what the boot path does. A packaged layout with no
 *      halves generates both; a second run is up to date and rewrites nothing;
 *      a pre-#806 (unstamped) half, a changed manifest and a re-extracted
 *      source whose curated entry changed each regenerate, while a source
 *      whose only change is an entry the manifest does not select stays up to
 *      date; a refused manifest leaves the stale halves byte-identical (boot
 *      is never refused); an absent mm.o2r skips without writing; CRLF and LF
 *      manifests stamp the same.
 *   D. Staged: the real extracted archives and the shipped manifest through
 *      both generators, compared like A -- the issue's "byte-equivalent to
 *      GenerateRedshipOtr" lock. Also checks the embedded manifest is the
 *      shipped one. Reports itself skipped (the row still runs A-C) when the
 *      archives, the source tree or Python are absent.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++).
 */

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <zip.h>

#include <ship/utils/StrHash64.h>

#include "../curated_archives.h"
#include "../zapd_subprocess.h"

namespace {

using CaiEntries = std::map<std::string, std::string>;

struct CaiArchive {
    bool ok = false;
    CaiEntries entries;
    std::string comment;
};

// Writes a zip holding exactly @p entries (deflated, like the extractor's).
bool CaiWriteZip(const std::string& path, const std::vector<std::pair<std::string, std::string>>& entries,
                 const std::string& comment = "") {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    int err = 0;
    zip_t* za = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (za == nullptr) {
        return false;
    }
    for (const auto& [name, data] : entries) {
        zip_source_t* src = zip_source_buffer(za, data.data(), data.size(), 0);
        if (src == nullptr || zip_file_add(za, name.c_str(), src, ZIP_FL_ENC_UTF_8) < 0) {
            if (src != nullptr) {
                zip_source_free(src);
            }
            zip_discard(za);
            return false;
        }
    }
    if (!comment.empty()) {
        zip_set_archive_comment(za, comment.data(), (zip_uint16_t)comment.size());
    }
    return zip_close(za) == 0;
}

CaiArchive CaiReadZip(const std::string& path) {
    CaiArchive out;
    int err = 0;
    zip_t* za = zip_open(path.c_str(), ZIP_RDONLY, &err);
    if (za == nullptr) {
        return out;
    }
    const zip_int64_t n = zip_get_num_entries(za, 0);
    for (zip_int64_t i = 0; i < n; i++) {
        zip_stat_t st;
        zip_stat_init(&st);
        if (zip_stat_index(za, (zip_uint64_t)i, 0, &st) != 0) {
            zip_discard(za);
            return out;
        }
        std::string data((size_t)st.size, '\0');
        zip_file_t* f = zip_fopen_index(za, (zip_uint64_t)i, 0);
        if (f == nullptr) {
            zip_discard(za);
            return out;
        }
        const zip_int64_t got = st.size > 0 ? zip_fread(f, data.data(), st.size) : 0;
        zip_fclose(f);
        if (got != (zip_int64_t)st.size) {
            zip_discard(za);
            return out;
        }
        out.entries[st.name] = std::move(data);
    }
    int len = 0;
    const char* comment = zip_get_archive_comment(za, &len, ZIP_FL_ENC_RAW);
    if (comment != nullptr) {
        out.comment.assign(comment, (size_t)len);
    }
    zip_discard(za);
    out.ok = true;
    return out;
}

std::string CaiReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool CaiExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

void CaiRemove(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// ---- Synthetic resources (same shapes as tools/tests/test_make_redship_otr.py) ----

void CaiPut32(std::string& out, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        out.push_back((char)((v >> (8 * i)) & 0xFF));
    }
}

// A little-endian 64-byte OTR header whose Type field holds @p tag as the
// exporter writes it (the big-endian constant byte-reversed).
std::string CaiHeader(const char* tag) {
    std::string h(4, '\0');
    for (int i = 3; i >= 0; i--) {
        h.push_back(tag[i]);
    }
    h.append(56, '\0');
    return h;
}

std::string CaiTexture(const std::string& label) {
    return CaiHeader("OTEX") + label;
}

// An F3DEX2 display list naming each path with expanded command @p opcode,
// optionally carrying a raw segmented G_SETTIMG, then G_ENDDL.
std::string CaiDisplayList(const std::vector<std::string>& refs, uint32_t opcode = 0x20, bool rawSetTimg = false) {
    std::string d = CaiHeader("ODLT");
    d.push_back(4); // ucode_f3dex2
    d.append(7, '\0');
    for (const std::string& ref : refs) {
        const uint64_t h = CRC64(ref.c_str());
        CaiPut32(d, opcode << 24);
        CaiPut32(d, 0);
        CaiPut32(d, (uint32_t)(h >> 32));
        CaiPut32(d, (uint32_t)(h & 0xFFFFFFFFu));
    }
    if (rawSetTimg) {
        CaiPut32(d, 0xFDu << 24);
        CaiPut32(d, 0x09000000u);
    }
    CaiPut32(d, 0xDFu << 24);
    CaiPut32(d, 0);
    return d;
}

std::string CaiVertexArray() {
    std::string a = CaiHeader("OARR");
    CaiPut32(a, 25); // ArrayResourceType::Vertex
    CaiPut32(a, 2);
    for (int i = 0; i < 32; i++) {
        a.push_back((char)i);
    }
    return a;
}

// A scalar array of one ZSCALAR_X8: MM's factory reads 1 byte, OoT's reads 0.
std::string CaiX8Array() {
    std::string a = CaiHeader("OARR");
    CaiPut32(a, 0); // ArrayResourceType::Scalar
    CaiPut32(a, 1);
    CaiPut32(a, 3); // ZSCALAR_X8
    a.push_back('\x7F');
    return a;
}

constexpr const char* kCaiMmDL = "objects/object_gi_mm_only/gGiMmDL";
constexpr const char* kCaiMmTex = "objects/object_gi_mm_only/gGiMmTex";
constexpr const char* kCaiMmVtx = "objects/object_gi_mm_only/gGiMmVtx";
constexpr const char* kCaiOotDL = "objects/object_gi_oot_only/gGiOotDL";
constexpr const char* kCaiOotTex = "objects/object_gi_oot_only/gGiOotTex";
constexpr const char* kCaiOotVtx = "objects/object_gi_oot_only/gGiOotVtx";
constexpr const char* kCaiKeepTex = "objects/gameplay_keep/gSharedTex";
constexpr const char* kCaiShared = "objects/object_shared/gSharedDL";

const std::string kCaiGoodManifest = "# synthetic #806 manifest\n"
                                     "mm->oot objects/object_gi_mm_only/\n"
                                     "oot->mm objects/object_gi_oot_only/\n"
                                     "oot->mm objects/gameplay_keep/gSharedTex   # rides with its model\n";

struct CaiFixture {
    std::string dir;
    std::string oot, mm, soh, twoShip;
};

// A display list whose stream ends before its G_ENDDL (one G_NOOP, then nothing).
std::string CaiTruncatedDisplayList() {
    std::string d = CaiHeader("ODLT");
    d.push_back(4); // ucode_f3dex2
    d.append(7, '\0');
    CaiPut32(d, 0);
    CaiPut32(d, 0);
    return d;
}

// A vertex array declaring two elements over four bytes of payload.
std::string CaiShortVertexArray() {
    std::string a = CaiHeader("OARR");
    CaiPut32(a, 25); // ArrayResourceType::Vertex
    CaiPut32(a, 2);
    CaiPut32(a, 0);
    return a;
}

// mm.o2r; @p texLabel is a curated entry's payload, @p sharedLabel one the
// manifest never selects (leg C re-extracts with each changed in turn).
bool CaiWriteMM(const std::string& path, const std::string& texLabel = "mm:tex",
                const std::string& sharedLabel = "mm:shared") {
    return CaiWriteZip(path, {
                                 { kCaiMmDL, CaiDisplayList({ kCaiMmTex, kCaiMmVtx }, 0x20) },
                                 { kCaiMmTex, CaiTexture(texLabel) },
                                 { kCaiMmVtx, CaiVertexArray() },
                                 { kCaiShared, CaiTexture(sharedLabel) },
                                 { "objects/object_mm_bad/gX8Array", CaiX8Array() },
                                 { "objects/object_mm_bad/gShortVtx", CaiShortVertexArray() },
                                 { "objects/object_mm_bad/gEscDL", CaiDisplayList({ "objects/object_mm_bad/gLoose" }) },
                                 { "objects/object_mm_bad/gLoose", CaiTexture("mm:loose") },
                                 { "objects/object_mm_collide/gTex", CaiTexture("mm:collide") },
                             });
}

bool CaiWriteFixture(const std::string& dir, CaiFixture* fx) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    fx->dir = dir;
    fx->oot = dir + "/oot.o2r";
    fx->mm = dir + "/mm.o2r";
    fx->soh = dir + "/soh.o2r";
    fx->twoShip = dir + "/2ship.o2r";
    return CaiWriteZip(fx->oot,
                       {
                           { kCaiOotDL, CaiDisplayList({ kCaiOotTex, kCaiOotVtx, kCaiKeepTex }) },
                           { kCaiOotTex, CaiTexture("oot:tex") },
                           { kCaiOotVtx, CaiVertexArray() },
                           { kCaiKeepTex, CaiTexture("oot:keep") },
                           { kCaiShared, CaiTexture("oot:shared") },
                           { "objects/object_oot_bad/gRoom", CaiHeader("OROM") + "room" },
                           { "objects/object_oot_bad/gRawDL", CaiDisplayList({}, 0x20, true) },
                           { "objects/object_oot_bad/gTruncDL", CaiTruncatedDisplayList() },
                       }) &&
           CaiWriteMM(fx->mm) &&
           CaiWriteZip(fx->soh, { { "textures/soh_only/gPortTex", CaiTexture("soh:port") },
                                  { "objects/object_mm_collide/gTex", CaiTexture("soh:collide") } }) &&
           CaiWriteZip(fx->twoShip, { { "textures/2ship_only/gPortTex", CaiTexture("2ship:port") } });
}

rsbs::curated::GenerateInputs CaiInputs(const CaiFixture& fx, const std::string& manifest, const std::string& outDir) {
    rsbs::curated::GenerateInputs in;
    in.ootArchive = fx.oot;
    in.mmArchive = fx.mm;
    in.hostArchives = { { "oot", fx.soh, true }, { "oot", fx.dir + "/oot-mq.o2r", false }, { "mm", fx.twoShip, true } };
    in.manifestText = manifest;
    in.outOoT = outDir + "/redship-oot.o2r";
    in.outMM = outDir + "/redship-mm.o2r";
    return in;
}

// The stamp the in-app generator would write for @p in right now.
std::string CaiWant(const rsbs::curated::GenerateInputs& in) {
    std::string stamp;
    std::string err;
    return rsbs::curated::CurrentStamp(in, &stamp, &err) ? stamp : "<no current stamp: " + err + ">";
}

bool CaiWriteText(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    out.close();
    return !out.fail();
}

// make_redship_otr.py over the same inputs; returns its exit code (-1: not run).
int CaiRunPython(const std::string& pythonExe, const std::string& script, const rsbs::curated::GenerateInputs& in,
                 const std::string& manifestPath) {
    std::vector<std::string> args = { pythonExe,     script,       "--oot-archive", in.ootArchive, "--mm-archive",
                                      in.mmArchive,  "--manifest", manifestPath,    "--out-oot",   in.outOoT,
                                      "--out-mm",    in.outMM };
    for (const auto& host : in.hostArchives) {
        args.push_back(host.required ? "--host-archive" : "--optional-host-archive");
        args.push_back(host.game + "=" + host.path);
    }
    std::vector<const char*> argv;
    for (const auto& a : args) {
        argv.push_back(a.c_str());
    }
    return ZapdSubprocess_Run(pythonExe, argv.data(), (int)argv.size());
}

// Same path set, same payloads, same stamp. Returns the number of differences.
int CaiCompareHalves(const char* label, const std::string& cppPath, const std::string& pyPath) {
    const CaiArchive cpp = CaiReadZip(cppPath);
    const CaiArchive py = CaiReadZip(pyPath);
    if (!cpp.ok || !py.ok) {
        fprintf(stderr, "[curated-archive-inapp] FAIL: %s: could not read %s (%d) or %s (%d)\n", label,
                cppPath.c_str(), cpp.ok, pyPath.c_str(), py.ok);
        return 1;
    }
    int diffs = 0;
    for (const auto& [name, data] : py.entries) {
        auto it = cpp.entries.find(name);
        if (it == cpp.entries.end()) {
            fprintf(stderr, "[curated-archive-inapp] FAIL: %s: %s is in make_redship_otr.py's half only\n", label,
                    name.c_str());
            diffs++;
        } else if (it->second != data) {
            fprintf(stderr, "[curated-archive-inapp] FAIL: %s: %s differs (%zu vs %zu bytes)\n", label, name.c_str(),
                    it->second.size(), data.size());
            diffs++;
        }
    }
    for (const auto& [name, data] : cpp.entries) {
        if (py.entries.find(name) == py.entries.end()) {
            fprintf(stderr, "[curated-archive-inapp] FAIL: %s: %s is in the in-app half only\n", label, name.c_str());
            diffs++;
        }
    }
    if (cpp.comment != py.comment) {
        fprintf(stderr, "[curated-archive-inapp] FAIL: %s: stamps differ: in-app '%s', make_redship_otr.py '%s'\n",
                label, cpp.comment.c_str(), py.comment.c_str());
        diffs++;
    }
    printf("[curated-archive-inapp] %s: %zu in-app entries vs %zu from make_redship_otr.py, %d difference(s), "
           "stamp '%s'\n",
           label, cpp.entries.size(), py.entries.size(), diffs, cpp.comment.c_str());
    return diffs;
}

} // namespace

int CuratedArchiveInApp_RunHeadless(const std::string& pythonExe, const std::string& generatorScript,
                                    const std::string& shippedManifest) {
    namespace cur = rsbs::curated;
    const std::string root = std::filesystem::current_path().generic_string() + "/rsbs_test_806";
    {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
    const bool havePython = !pythonExe.empty() && !generatorScript.empty();
    if (!havePython) {
        printf("[curated-archive-inapp] note: no Python interpreter or generator script; the parity legs compare "
               "nothing (python='%s', script='%s')\n",
               pythonExe.c_str(), generatorScript.c_str());
    }
    int failures = 0;

    CaiFixture fx;
    if (!CaiWriteFixture(root + "/src", &fx)) {
        fprintf(stderr, "[curated-archive-inapp] FAIL: could not write the synthetic archives under %s\n",
                root.c_str());
        return 1;
    }
    const std::string manifestPath = root + "/manifest.txt";

    // ---- A. Parity over synthetic archives --------------------------------------
    {
        const cur::GenerateInputs in = CaiInputs(fx, kCaiGoodManifest, root + "/cpp");
        const cur::GenerateResult res = cur::Generate(in);
        printf("[curated-archive-inapp] A: in-app generator ok=%d (%zu -> redship-oot.o2r, %zu -> redship-mm.o2r)%s%s\n",
               res.ok, res.resourcesOoT, res.resourcesMM, res.ok ? "" : ": ", res.error.c_str());
        if (!res.ok || !CaiExists(in.outOoT) || !CaiExists(in.outMM)) {
            fprintf(stderr,
                    "[curated-archive-inapp] FAIL: A: the in-app generator did not write both halves from a packaged "
                    "layout (oot.o2r + mm.o2r + soh.o2r + 2ship.o2r) and a valid manifest\n");
            failures++;
        } else {
            const CaiArchive oot = CaiReadZip(in.outOoT);
            const CaiArchive mm = CaiReadZip(in.outMM);
            const CaiEntries wantOoT = { { kCaiMmDL, "" }, { kCaiMmTex, "" }, { kCaiMmVtx, "" } };
            const CaiEntries wantMM = { { kCaiOotDL, "" }, { kCaiOotTex, "" }, { kCaiOotVtx, "" }, { kCaiKeepTex, "" } };
            auto sameKeys = [](const CaiEntries& a, const CaiEntries& b) {
                if (a.size() != b.size()) {
                    return false;
                }
                for (const auto& kv : a) {
                    if (b.find(kv.first) == b.end()) {
                        return false;
                    }
                }
                return true;
            };
            if (!sameKeys(oot.entries, wantOoT) || !sameKeys(mm.entries, wantMM)) {
                fprintf(stderr, "[curated-archive-inapp] FAIL: A: wrong path sets (%zu in OoT's half, %zu in MM's)\n",
                        oot.entries.size(), mm.entries.size());
                failures++;
            }
            if (oot.entries.count(kCaiMmTex) && oot.entries.at(kCaiMmTex) != CaiTexture("mm:tex")) {
                fprintf(stderr, "[curated-archive-inapp] FAIL: A: a curated payload is not the source's bytes\n");
                failures++;
            }
            const std::string stamp = CaiWant(in);
            if (oot.comment != stamp || mm.comment != stamp) {
                fprintf(stderr, "[curated-archive-inapp] FAIL: A: stamps '%s' / '%s', want '%s'\n",
                        oot.comment.c_str(), mm.comment.c_str(), stamp.c_str());
                failures++;
            }
        }
        if (havePython) {
            CaiWriteText(manifestPath, kCaiGoodManifest);
            const cur::GenerateInputs py = CaiInputs(fx, kCaiGoodManifest, root + "/py");
            const int rc = CaiRunPython(pythonExe, generatorScript, py, manifestPath);
            printf("[curated-archive-inapp] A: make_redship_otr.py rc=%d\n", rc);
            if (rc != 0) {
                fprintf(stderr, "[curated-archive-inapp] FAIL: A: make_redship_otr.py refused the synthetic manifest\n");
                failures++;
            } else {
                failures += CaiCompareHalves("A redship-oot.o2r", in.outOoT, py.outOoT) != 0;
                failures += CaiCompareHalves("A redship-mm.o2r", in.outMM, py.outMM) != 0;
            }
        }
    }

    // ---- B. Every admission rule refuses, in both generators ---------------------
    {
        struct Refusal {
            const char* rule;
            std::string manifest;
        };
        const std::string mmLine = "mm->oot objects/object_gi_mm_only/\n";
        const std::string ootLines = "oot->mm objects/object_gi_oot_only/\noot->mm objects/gameplay_keep/gSharedTex\n";
        const Refusal refusals[] = {
            { "COLLISION", kCaiGoodManifest + "mm->oot objects/object_shared/gSharedDL\n" },
            { "COLLISION", kCaiGoodManifest + "mm->oot objects/object_mm_collide/\n" },
            { "DISPATCHED TYPE", kCaiGoodManifest + "oot->mm objects/object_oot_bad/gRoom\n" },
            { "RAW SEGMENTED TEXTURE", kCaiGoodManifest + "oot->mm objects/object_oot_bad/gRawDL\n" },
            { "UNPARSABLE DISPLAY LIST", kCaiGoodManifest + "oot->mm objects/object_oot_bad/gTruncDL\n" },
            { "ARRAY READER DISAGREEMENT", kCaiGoodManifest + "mm->oot objects/object_mm_bad/gX8Array\n" },
            { "UNPARSABLE ARRAY", kCaiGoodManifest + "mm->oot objects/object_mm_bad/gShortVtx\n" },
            { "ESCAPING REFERENCE", kCaiGoodManifest + "mm->oot objects/object_mm_bad/gEscDL\n" },
            { "ESCAPING REFERENCE", mmLine + "oot->mm objects/object_gi_oot_only/\n" },
            { "matched nothing", kCaiGoodManifest + "mm->oot objects/object_missing/\n" },
            { "empty curated half", mmLine },
            { "pre-split", "mm objects/object_gi_mm_only/\n" + ootLines },
            { "unknown direction", "mm->mm objects/object_gi_mm_only/\n" + ootLines },
        };
        for (const Refusal& r : refusals) {
            const cur::GenerateInputs in = CaiInputs(fx, r.manifest, root + "/refused");
            CaiRemove(in.outOoT);
            CaiRemove(in.outMM);
            const cur::GenerateResult res = cur::Generate(in);
            std::string all = res.error;
            for (const std::string& f : res.findings) {
                all += "\n" + f;
            }
            const bool named = all.find(r.rule) != std::string::npos;
            const bool wrote = CaiExists(in.outOoT) || CaiExists(in.outMM);
            printf("[curated-archive-inapp] B (%s): in-app ok=%d named=%d wrote=%d -- %s\n", r.rule, res.ok, named,
                   wrote, res.error.c_str());
            if (res.ok || !named || wrote) {
                fprintf(stderr,
                        "[curated-archive-inapp] FAIL: B: the in-app generator must refuse (%s), say so, and write "
                        "neither half\n",
                        r.rule);
                failures++;
            }
            if (havePython) {
                CaiWriteText(manifestPath, r.manifest);
                const cur::GenerateInputs py = CaiInputs(fx, r.manifest, root + "/refused-py");
                CaiRemove(py.outOoT);
                CaiRemove(py.outMM);
                const int rc = CaiRunPython(pythonExe, generatorScript, py, manifestPath);
                if (rc == 0 || CaiExists(py.outOoT) || CaiExists(py.outMM)) {
                    fprintf(stderr,
                            "[curated-archive-inapp] FAIL: B: make_redship_otr.py accepted (%s) -- the two generators "
                            "disagree\n",
                            r.rule);
                    failures++;
                }
            }
        }

        // The input rules: a valid manifest, an input the generator must refuse.
        struct InputRefusal {
            const char* rule;
            std::function<void(cur::GenerateInputs&)> mutate;
        };
        const InputRefusal inputRefusals[] = {
            { "same output file", [](cur::GenerateInputs& in) { in.outMM = in.outOoT; } },
            { "host archive", [&](cur::GenerateInputs& in) { in.hostArchives[0].path = fx.dir + "/absent-soh.o2r"; } },
            { "needs the mm archive", [&](cur::GenerateInputs& in) { in.mmArchive = fx.dir + "/absent-mm.o2r"; } },
        };
        for (const InputRefusal& r : inputRefusals) {
            cur::GenerateInputs in = CaiInputs(fx, kCaiGoodManifest, root + "/refused");
            CaiRemove(in.outOoT);
            CaiRemove(in.outMM);
            r.mutate(in);
            const cur::GenerateResult res = cur::Generate(in);
            const bool named = res.error.find(r.rule) != std::string::npos;
            const bool wrote = CaiExists(in.outOoT) || CaiExists(in.outMM);
            printf("[curated-archive-inapp] B (%s): in-app ok=%d named=%d wrote=%d -- %s\n", r.rule, res.ok, named,
                   wrote, res.error.c_str());
            if (res.ok || !named || wrote) {
                fprintf(stderr,
                        "[curated-archive-inapp] FAIL: B: the in-app generator must refuse (%s), say so, and write "
                        "neither half\n",
                        r.rule);
                failures++;
            }
            if (havePython) {
                CaiWriteText(manifestPath, kCaiGoodManifest);
                cur::GenerateInputs py = CaiInputs(fx, kCaiGoodManifest, root + "/refused-py");
                CaiRemove(py.outOoT);
                CaiRemove(py.outMM);
                r.mutate(py);
                const int rc = CaiRunPython(pythonExe, generatorScript, py, manifestPath);
                if (rc == 0 || CaiExists(py.outOoT) || CaiExists(py.outMM)) {
                    fprintf(stderr,
                            "[curated-archive-inapp] FAIL: B: make_redship_otr.py accepted (%s) -- the two generators "
                            "disagree\n",
                            r.rule);
                    failures++;
                }
            }
        }
    }

    // ---- C. What the boot path does: missing, current, stale, refused, skipped ----
    {
        const std::string dir = root + "/packaged";
        CaiFixture pk;
        CaiWriteFixture(dir, &pk);
        cur::GenerateInputs in = CaiInputs(pk, kCaiGoodManifest, dir);
        std::string log;

        auto expect = [&](const char* step, cur::EnsureOutcome got, cur::EnsureOutcome want) {
            printf("[curated-archive-inapp] C (%s): %s -- %s\n", step, cur::EnsureOutcomeName(got), log.c_str());
            if (got != want) {
                fprintf(stderr, "[curated-archive-inapp] FAIL: C (%s): Ensure returned %s, want %s\n", step,
                        cur::EnsureOutcomeName(got), cur::EnsureOutcomeName(want));
                failures++;
            }
        };
        auto stampOf = [](const std::string& path) {
            std::string s;
            return cur::ReadStamp(path, &s) ? s : std::string("<unreadable>");
        };

        // 1. A packaged build after extraction: both sources, no halves.
        expect("no halves yet", cur::Ensure(in, &log), cur::EnsureOutcome::Generated);
        if (stampOf(in.outOoT) != CaiWant(in) || stampOf(in.outMM) != CaiWant(in)) {
            fprintf(stderr, "[curated-archive-inapp] FAIL: C: the generated halves do not carry the manifest's stamp\n");
            failures++;
        }

        // 2. The next boot: current, so nothing is rewritten.
        const std::string beforeOoT = CaiReadFile(in.outOoT);
        const std::string beforeMM = CaiReadFile(in.outMM);
        expect("current", cur::Ensure(in, &log), cur::EnsureOutcome::UpToDate);
        if (CaiReadFile(in.outOoT) != beforeOoT || CaiReadFile(in.outMM) != beforeMM) {
            fprintf(stderr, "[curated-archive-inapp] FAIL: C: an up-to-date half was rewritten\n");
            failures++;
        }

        // 3. A pre-#806 half (no stamp) is stale.
        CaiWriteZip(in.outMM, { { kCaiOotDL, "old" } });
        expect("unstamped half", cur::Ensure(in, &log), cur::EnsureOutcome::Generated);
        if (stampOf(in.outMM) != CaiWant(in) || CaiReadZip(in.outMM).entries.size() != 4) {
            fprintf(stderr, "[curated-archive-inapp] FAIL: C: the unstamped half was not regenerated\n");
            failures++;
        }

        // 4. A new build's manifest differs from the stamped one.
        const std::string newManifest = kCaiGoodManifest + "oot->mm objects/object_gi_oot_only/gGiOotTex\n";
        in.manifestText = newManifest;
        expect("manifest changed", cur::Ensure(in, &log), cur::EnsureOutcome::Generated);
        if (stampOf(in.outOoT) != CaiWant(in) || stampOf(in.outMM) != CaiWant(in)) {
            fprintf(stderr, "[curated-archive-inapp] FAIL: C: a changed manifest did not restamp both halves\n");
            failures++;
        }

        // 4b. mm.o2r re-extracted (SoH deletes an archive an incompatible
        // version made and re-extracts it; a player may re-extract from another
        // ROM revision) and a curated entry changed: the halves carved out of
        // the old archive are stale, though the manifest is the same.
        CaiWriteMM(pk.mm, "mm:tex:re-extracted");
        expect("curated source entry changed", cur::Ensure(in, &log), cur::EnsureOutcome::Generated);
        {
            const CaiArchive oot = CaiReadZip(in.outOoT);
            if (oot.entries.count(kCaiMmTex) == 0 || oot.entries.at(kCaiMmTex) != CaiTexture("mm:tex:re-extracted") ||
                oot.comment != CaiWant(in)) {
                fprintf(stderr, "[curated-archive-inapp] FAIL: C: a re-extracted source's changed curated entry was "
                                "not carried into the regenerated half\n");
                failures++;
            }
        }

        // 4c. Re-extracted again, and only an entry the manifest does not
        // select changed: still current, nothing rewritten.
        const std::string currentOoT = CaiReadFile(in.outOoT);
        CaiWriteMM(pk.mm, "mm:tex:re-extracted", "mm:shared:re-extracted");
        expect("uncurated source entry changed", cur::Ensure(in, &log), cur::EnsureOutcome::UpToDate);
        if (CaiReadFile(in.outOoT) != currentOoT) {
            fprintf(stderr, "[curated-archive-inapp] FAIL: C: an uncurated source change rewrote a half\n");
            failures++;
        }

        // 5. A manifest the guards refuse: nothing on disk changes, nothing throws.
        const std::string staleOoT = CaiReadFile(in.outOoT);
        const std::string staleMM = CaiReadFile(in.outMM);
        in.manifestText = kCaiGoodManifest + "oot->mm objects/object_oot_bad/gRawDL\n";
        expect("refused manifest", cur::Ensure(in, &log), cur::EnsureOutcome::Failed);
        if (CaiReadFile(in.outOoT) != staleOoT || CaiReadFile(in.outMM) != staleMM) {
            fprintf(stderr, "[curated-archive-inapp] FAIL: C: a refused regeneration touched the halves on disk\n");
            failures++;
        }

        // 6. MM not extracted yet: skip, write nothing.
        CaiRemove(in.outOoT);
        CaiRemove(in.outMM);
        in.manifestText = kCaiGoodManifest;
        in.mmArchive = dir + "/absent-mm.o2r";
        expect("mm.o2r absent", cur::Ensure(in, &log), cur::EnsureOutcome::Skipped);
        if (CaiExists(in.outOoT) || CaiExists(in.outMM)) {
            fprintf(stderr, "[curated-archive-inapp] FAIL: C: a skipped run wrote a half\n");
            failures++;
        }

        // 7. Line endings do not change the stamp.
        std::string crlf;
        for (char c : kCaiGoodManifest) {
            if (c == '\n') {
                crlf.push_back('\r');
            }
            crlf.push_back(c);
        }
        if (cur::Stamp(crlf, 0) != cur::Stamp(kCaiGoodManifest, 0)) {
            fprintf(stderr, "[curated-archive-inapp] FAIL: C: a CRLF manifest stamps differently from its LF form\n");
            failures++;
        }
    }

    // ---- D. Staged: the real archives through both generators ---------------------
    {
        const std::string oot = CaoResolveArchive("oot.o2r");
        const std::string mm = CaoResolveArchive("mm.o2r");
        const std::string soh = CaoResolveArchive("soh.o2r");
        const std::string twoShip = CaoResolveArchive("2ship.o2r");
        const std::string mq = CaoResolveArchive("oot-mq.o2r");

        if (!shippedManifest.empty()) {
            const std::string fileText = CaiReadFile(shippedManifest);
            if (cur::ManifestHash(fileText) != cur::ManifestHash(cur::EmbeddedManifest())) {
                fprintf(stderr,
                        "[curated-archive-inapp] FAIL: D: the manifest compiled into this binary is not %s -- "
                        "reconfigure\n",
                        shippedManifest.c_str());
                failures++;
            }
        }
        if (oot.empty() || mm.empty() || soh.empty() || twoShip.empty() || !havePython || shippedManifest.empty()) {
            printf("[curated-archive-inapp] D: staged leg SKIPPED (oot.o2r '%s', mm.o2r '%s', soh.o2r '%s', "
                   "2ship.o2r '%s', python %d, manifest '%s')\n",
                   oot.c_str(), mm.c_str(), soh.c_str(), twoShip.c_str(), havePython, shippedManifest.c_str());
        } else {
            cur::GenerateInputs in;
            in.ootArchive = oot;
            in.mmArchive = mm;
            in.hostArchives = { { "oot", soh, true },
                                { "oot", mq.empty() ? root + "/absent-oot-mq.o2r" : mq, false },
                                { "mm", twoShip, true } };
            in.manifestText = std::string(cur::EmbeddedManifest());
            in.outOoT = root + "/staged-cpp/redship-oot.o2r";
            in.outMM = root + "/staged-cpp/redship-mm.o2r";
            const cur::GenerateResult res = cur::Generate(in);
            printf("[curated-archive-inapp] D: in-app over the real archives ok=%d (%zu + %zu resources)%s%s\n", res.ok,
                   res.resourcesOoT, res.resourcesMM, res.ok ? "" : ": ", res.error.c_str());
            for (const std::string& f : res.findings) {
                fprintf(stderr, "[curated-archive-inapp]   %s\n", f.c_str());
            }
            cur::GenerateInputs py = in;
            py.outOoT = root + "/staged-py/redship-oot.o2r";
            py.outMM = root + "/staged-py/redship-mm.o2r";
            const int rc = CaiRunPython(pythonExe, generatorScript, py, shippedManifest);
            printf("[curated-archive-inapp] D: make_redship_otr.py over the real archives rc=%d\n", rc);
            if (!res.ok || rc != 0) {
                fprintf(stderr, "[curated-archive-inapp] FAIL: D: a generator refused the shipped manifest\n");
                failures++;
            } else {
                failures += CaiCompareHalves("D redship-oot.o2r", in.outOoT, py.outOoT) != 0;
                failures += CaiCompareHalves("D redship-mm.o2r", in.outMM, py.outMM) != 0;
            }
        }
    }

    {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
    if (failures == 0) {
        printf("[curated-archive-inapp] PASS: the in-app generator writes what make_redship_otr.py writes, refuses "
               "what it refuses (every rule above), and regenerates a missing half, an unstamped half, a changed "
               "manifest and a changed curated source entry\n");
    }
    return failures == 0 ? 0 : 1;
}
