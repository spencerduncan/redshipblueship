/**
 * @file curated_archives.cpp
 * @brief In-app generation of the curated cross-game archives (#806) -- see curated_archives.h
 *
 * A rule-for-rule port of scripts/make_redship_otr.py. Every guard below names
 * the constraint of that script's module docstring it implements; the reasons
 * live there and are not repeated here. Keep the two in step: the
 * curated-archive-inapp CTest row runs both over the same inputs and fails on
 * any difference in what they admit, refuse or write.
 */

#include "curated_archives.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <zip.h>

#include <ship/Context.h>
#include <ship/utils/StrHash64.h>

#include "rsbs_curated_manifest.h"

namespace rsbs::curated {

namespace {

constexpr int kGameCount = 2;
constexpr int kOoT = 0;
constexpr int kMM = 1;
const char* const kGameNames[kGameCount] = { "oot", "mm" };
const char* const kOutputNames[kGameCount] = { "redship-oot.o2r", "redship-mm.o2r" };

int Other(int game) {
    return game == kOoT ? kMM : kOoT;
}

int GameIndex(const std::string& name) {
    if (name == "oot") {
        return kOoT;
    }
    if (name == "mm") {
        return kMM;
    }
    return -1;
}

std::string Format(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}

bool FileExists(const std::string& path) {
    std::error_code ec;
    return !path.empty() && std::filesystem::is_regular_file(path, ec);
}

// ---------------------------------------------------------------------------
// Archive access (libzip).
// ---------------------------------------------------------------------------

class ZipReader {
  public:
    ZipReader() = default;
    ZipReader(const ZipReader&) = delete;
    ZipReader& operator=(const ZipReader&) = delete;
    ~ZipReader() {
        if (mZip != nullptr) {
            zip_discard(mZip);
        }
    }
    bool Open(const std::string& path) {
        int err = 0;
        mZip = zip_open(path.c_str(), ZIP_RDONLY, &err);
        return mZip != nullptr;
    }
    std::vector<std::string> Names() const {
        std::vector<std::string> names;
        const zip_int64_t n = zip_get_num_entries(mZip, 0);
        names.reserve(n > 0 ? (size_t)n : 0);
        for (zip_int64_t i = 0; i < n; i++) {
            const char* name = zip_get_name(mZip, (zip_uint64_t)i, ZIP_FL_ENC_RAW);
            if (name != nullptr) {
                names.emplace_back(name);
            }
        }
        return names;
    }
    bool Read(const std::string& name, std::string* out) const {
        const zip_int64_t index = zip_name_locate(mZip, name.c_str(), ZIP_FL_ENC_RAW);
        if (index < 0) {
            return false;
        }
        zip_stat_t st;
        zip_stat_init(&st);
        if (zip_stat_index(mZip, (zip_uint64_t)index, 0, &st) != 0 || (st.valid & ZIP_STAT_SIZE) == 0) {
            return false;
        }
        out->assign((size_t)st.size, '\0');
        if (st.size == 0) {
            return true;
        }
        zip_file_t* file = zip_fopen_index(mZip, (zip_uint64_t)index, 0);
        if (file == nullptr) {
            return false;
        }
        const zip_int64_t got = zip_fread(file, out->data(), st.size);
        zip_fclose(file);
        return got == (zip_int64_t)st.size;
    }

  private:
    zip_t* mZip = nullptr;
};

// ---------------------------------------------------------------------------
// Resource inspection: make_redship_otr.py's dispatched_type_of, the display
// list walk, find_array_reader_disagreements.
// ---------------------------------------------------------------------------

constexpr size_t kOtrHeaderSize = 64;
constexpr size_t kTypeOffset = 4;

// The type tag in either byte order (constraint 4's matching convention).
bool HasTag(const std::string& data, const char* tag) {
    if (data.size() < kTypeOffset + 4) {
        return false;
    }
    const char* t = data.data() + kTypeOffset;
    const bool forward = t[0] == tag[0] && t[1] == tag[1] && t[2] == tag[2] && t[3] == tag[3];
    const bool reversed = t[0] == tag[3] && t[1] == tag[2] && t[2] == tag[1] && t[3] == tag[0];
    return forward || reversed;
}

// Constraint 4: Room/scene, Cutscene and Path.
const char* DispatchedTypeOf(const std::string& data) {
    if (HasTag(data, "OROM")) {
        return "Room/scene";
    }
    if (HasTag(data, "OCUT")) {
        return "Cutscene";
    }
    if (HasTag(data, "OPTH")) {
        return "Path";
    }
    return nullptr;
}

uint32_t ReadU32(const std::string& body, size_t pos, bool little) {
    const auto* b = reinterpret_cast<const unsigned char*>(body.data() + pos);
    if (little) {
        return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    }
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

constexpr uint32_t kOtrSetTimgHash = 0x20;
constexpr uint32_t kOtrDlHash = 0x31;
constexpr uint32_t kOtrVtxHash = 0x32;
constexpr uint32_t kOtrMarker = 0x33;
constexpr uint32_t kOtrBranchZ = 0x35;
constexpr uint32_t kOtrMtx = 0x36;
constexpr uint32_t kOtrMovemem = 0x42;
constexpr uint32_t kRawSetTimg = 0xFD;

bool IsExpanded(uint32_t op) {
    return op == kOtrSetTimgHash || op == kOtrDlHash || op == kOtrVtxHash || op == kOtrMarker || op == kOtrBranchZ ||
           op == kOtrMtx || op == kOtrMovemem;
}

const char* HashReferenceName(uint32_t op) {
    switch (op) {
        case kOtrSetTimgHash:
            return "G_SETTIMG_OTR_HASH";
        case kOtrDlHash:
            return "G_DL_OTR_HASH";
        case kOtrVtxHash:
            return "G_VTX_OTR_HASH";
        case kOtrBranchZ:
            return "G_BRANCH_Z_OTR";
        case kOtrMtx:
            return "G_MTX_OTR";
        case kOtrMovemem:
            return "G_MOVEMEM_OTR";
        default:
            return nullptr;
    }
}

struct RawSegmentedRef {
    int index;
    uint32_t segment;
};

struct HashRef {
    int index;
    const char* command;
    uint64_t hash;
};

// _walk_display_list + find_raw_segmented_texture_refs + find_hash_references
// in one pass. Returns "" on success, else the MalformedDisplayList reason.
std::string WalkDisplayList(const std::string& data, std::vector<RawSegmentedRef>* raw, std::vector<HashRef>* refs) {
    if (data.size() < kOtrHeaderSize + 1) {
        return "resource is smaller than the OTR header plus a 1-byte ucode field";
    }
    const bool little = data[0] == 0;
    const std::string body = data.substr(kOtrHeaderSize);
    const unsigned char ucode = (unsigned char)body[0];
    uint32_t endOpcode;
    if (ucode <= 3) {
        endOpcode = 0xB8; // f3db, f3d, f3dex, f3dexb
    } else if (ucode <= 5) {
        endOpcode = 0xDF; // f3dex2, s2dex
    } else {
        return Format("unrecognized ucode byte 0x%02X", ucode);
    }

    size_t pos = 8; // the 1-byte ucode field, aligned to 8
    int index = 0;
    const size_t n = body.size();
    while (true) {
        if (pos + 8 > n) {
            return Format("command stream truncated before G_ENDDL (at body offset %zu)", pos);
        }
        const uint32_t w0 = ReadU32(body, pos, little);
        const uint32_t w1 = ReadU32(body, pos + 4, little);
        pos += 8;
        const uint32_t opcode = (w0 >> 24) & 0xFF;

        if (!IsExpanded(opcode)) {
            if (opcode == kRawSetTimg) {
                const uint32_t segment = (w1 >> 24) & 0x0F;
                if (w1 != 0 && segment != 0) {
                    raw->push_back({ index, segment });
                }
            }
            index += 1;
            if (opcode == endOpcode) {
                break;
            }
            continue;
        }

        if (pos + 8 > n) {
            return Format("expanded command 0x%02X truncated before its payload (at body offset %zu)", opcode, pos);
        }
        const uint64_t hi = ReadU32(body, pos, little);
        const uint64_t lo = ReadU32(body, pos + 4, little);
        pos += 8;
        if (const char* name = HashReferenceName(opcode)) {
            refs->push_back({ index, name, (hi << 32) + lo });
        }
        index += 2;
        if (opcode == endOpcode) {
            break;
        }
    }
    return "";
}

constexpr uint32_t kArrayVector = 24;
constexpr uint32_t kArrayVertex = 25;
constexpr size_t kVertexElementSize = 16;

const char* ScalarTypeName(uint32_t type) {
    static const char* const kNames[] = { "ZSCALAR_NONE", "ZSCALAR_S8",  "ZSCALAR_U8",  "ZSCALAR_X8",  "ZSCALAR_S16",
                                          "ZSCALAR_U16",  "ZSCALAR_X16", "ZSCALAR_S32", "ZSCALAR_U32", "ZSCALAR_X32",
                                          "ZSCALAR_S64",  "ZSCALAR_U64", "ZSCALAR_X64", "ZSCALAR_F32", "ZSCALAR_F64" };
    return type < sizeof(kNames) / sizeof(kNames[0]) ? kNames[type] : nullptr;
}

uint32_t OoTScalarWidth(uint32_t type) {
    return (type == 4 || type == 5) ? 2 : 0;
}

uint32_t MMScalarWidth(uint32_t type) {
    switch (type) {
        case 1:
        case 2:
        case 3:
            return 1;
        case 4:
        case 5:
        case 6:
            return 2;
        case 7:
        case 8:
        case 9:
            return 4;
        case 10:
        case 11:
        case 12:
            return 8;
        default:
            return 0;
    }
}

struct ArrayDisagreement {
    bool found = false;
    uint32_t index = 0;
    uint32_t scalarType = 0;
    uint32_t ootWidth = 0;
    uint32_t mmWidth = 0;
};

// find_array_reader_disagreements. Returns "" on success, else the MalformedArray reason.
std::string WalkArray(const std::string& data, ArrayDisagreement* out) {
    if (data.size() < kOtrHeaderSize + 8) {
        return "resource is smaller than the OTR header plus the type/count fields";
    }
    const bool little = data[0] == 0;
    const std::string body = data.substr(kOtrHeaderSize);
    const size_t n = body.size();
    const uint32_t arrayType = ReadU32(body, 0, little);
    const uint32_t count = ReadU32(body, 4, little);
    if (count > n) {
        return Format("element count %u exceeds the %zu-byte payload", count, n);
    }
    if (arrayType == kArrayVertex) {
        const size_t need = 8 + (size_t)count * kVertexElementSize;
        if (need > n) {
            return Format("vertex array declares %u element(s) but the payload holds %zu byte(s), not %zu", count, n,
                          need);
        }
        return "";
    }
    size_t pos = 8;
    for (uint32_t index = 0; index < count; index++) {
        if (pos + 4 > n) {
            return Format("scalar element %u's type field runs past the payload (at body offset %zu)", index, pos);
        }
        const uint32_t scalarType = ReadU32(body, pos, little);
        pos += 4;
        uint64_t repeat = 1;
        if (arrayType == kArrayVector) {
            if (pos + 4 > n) {
                return Format("vector element %u's repeat count runs past the payload (at body offset %zu)", index,
                              pos);
            }
            repeat = ReadU32(body, pos, little);
            pos += 4;
        }
        const uint32_t ootWidth = OoTScalarWidth(scalarType);
        const uint32_t mmWidth = MMScalarWidth(scalarType);
        if (ootWidth != mmWidth) {
            *out = { true, index, scalarType, ootWidth, mmWidth };
            return "";
        }
        const uint64_t next = (uint64_t)pos + repeat * mmWidth;
        if (next > n) {
            return Format("scalar element %u runs past the payload (needs body offset %llu of %zu)", index,
                          (unsigned long long)next, n);
        }
        pos = (size_t)next;
    }
    return "";
}

// ---------------------------------------------------------------------------
// Manifest (read_manifest).
// ---------------------------------------------------------------------------

struct ManifestEntry {
    int source;
    int host;
    std::string prefix;
    int lineno;
};

bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

std::string Strip(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && IsSpace(s[b])) {
        b++;
    }
    while (e > b && IsSpace(s[e - 1])) {
        e--;
    }
    return s.substr(b, e - b);
}

std::string Lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
    }
    return s;
}

// Returns "" and fills @p entries, or the refusal.
std::string ReadManifest(const std::string& text, std::vector<ManifestEntry>* entries) {
    size_t start = 0;
    int lineno = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        std::string raw = text.substr(start, end - start);
        start = end + 1;
        lineno++;
        if (end == text.size() && raw.empty()) {
            break;
        }
        const std::string line = Strip(raw.substr(0, raw.find('#')));
        if (line.empty()) {
            continue;
        }
        size_t split = 0;
        while (split < line.size() && !IsSpace(line[split])) {
            split++;
        }
        if (split == line.size()) {
            return Format("manifest:%d: expected '<source>-><host> <path-prefix>', got '%s'", lineno,
                          Strip(raw).c_str());
        }
        const std::string word = line.substr(0, split);
        const std::string direction = Lower(word);
        const std::string prefix = Strip(line.substr(split));
        if (direction == "oot" || direction == "mm") {
            return Format("manifest:%d: '%s' is the pre-split '<game> <path-prefix>' form; the manifest now takes a "
                          "direction column, '<source>-><host>' (mm->oot or oot->mm)",
                          lineno, word.c_str());
        }
        if (direction == "mm->oot") {
            entries->push_back({ kMM, kOoT, prefix, lineno });
        } else if (direction == "oot->mm") {
            entries->push_back({ kOoT, kMM, prefix, lineno });
        } else {
            return Format("manifest:%d: unknown direction '%s' (expected one of mm->oot, oot->mm)", lineno,
                          word.c_str());
        }
        if (end == text.size()) {
            break;
        }
    }
    if (entries->empty()) {
        return "manifest is empty -- refusing to build an empty curated archive";
    }
    for (int host = 0; host < kGameCount; host++) {
        bool any = false;
        for (const ManifestEntry& e : *entries) {
            any = any || e.host == host;
        }
        if (!any) {
            return Format("no entry lands in %s (direction '%s->%s') -- refusing to build an empty curated half; the "
                          "runtime mounts both",
                          kOutputNames[host], kGameNames[Other(host)], kGameNames[host]);
        }
    }
    return "";
}

// ---------------------------------------------------------------------------
// Writing.
// ---------------------------------------------------------------------------

// Writes @p path holding @p entries (deflated) with @p comment. Returns "" or the failure.
using HalfEntries = std::vector<std::pair<const std::string*, const std::string*>>;

std::string WriteArchive(const std::string& path, const HalfEntries& entries, const std::string& comment) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    int err = 0;
    zip_t* za = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (za == nullptr) {
        return Format("could not create %s (libzip error %d)", path.c_str(), err);
    }
    for (const auto& [name, data] : entries) {
        const bool isDir = !name->empty() && name->back() == '/' && data->empty();
        zip_int64_t added;
        if (isDir) {
            added = zip_dir_add(za, name->c_str(), ZIP_FL_ENC_UTF_8);
        } else {
            zip_source_t* src = zip_source_buffer(za, data->data(), data->size(), 0);
            if (src == nullptr) {
                zip_discard(za);
                return Format("could not stage %s for %s", name->c_str(), path.c_str());
            }
            added = zip_file_add(za, name->c_str(), src, ZIP_FL_ENC_UTF_8);
            if (added < 0) {
                zip_source_free(src);
            }
        }
        if (added < 0) {
            const std::string why = zip_strerror(za);
            zip_discard(za);
            return Format("could not add %s to %s: %s", name->c_str(), path.c_str(), why.c_str());
        }
    }
    if (zip_set_archive_comment(za, comment.data(), (zip_uint16_t)comment.size()) != 0) {
        const std::string why = zip_strerror(za);
        zip_discard(za);
        return Format("could not stamp %s: %s", path.c_str(), why.c_str());
    }
    if (zip_close(za) != 0) {
        const std::string why = zip_strerror(za);
        zip_discard(za);
        return Format("could not write %s: %s", path.c_str(), why.c_str());
    }
    return "";
}

GenerateResult Refuse(GenerateResult result, std::string error) {
    result.ok = false;
    result.error = std::move(error);
    return result;
}

} // namespace

uint64_t ManifestHash(std::string_view manifestText) {
    std::string normalized;
    normalized.reserve(manifestText.size());
    for (char c : manifestText) {
        if (c != '\r') {
            normalized.push_back(c);
        }
    }
    return CRC64(normalized.c_str());
}

std::string Stamp(std::string_view manifestText) {
    return Format("redship-curated v%d manifest-crc64=%016llx", kStampVersion,
                  (unsigned long long)ManifestHash(manifestText));
}

std::string_view EmbeddedManifest() {
    return std::string_view(reinterpret_cast<const char*>(kRsbsCuratedManifest), sizeof(kRsbsCuratedManifest));
}

bool ReadStamp(const std::string& archivePath, std::string* out) {
    if (!FileExists(archivePath)) {
        return false;
    }
    int err = 0;
    zip_t* za = zip_open(archivePath.c_str(), ZIP_RDONLY, &err);
    if (za == nullptr) {
        return false;
    }
    int len = 0;
    const char* comment = zip_get_archive_comment(za, &len, ZIP_FL_ENC_RAW);
    if (out != nullptr) {
        out->assign(comment != nullptr ? comment : "", comment != nullptr ? (size_t)len : 0);
    }
    zip_discard(za);
    return true;
}

GenerateResult Generate(const GenerateInputs& inputs) {
    GenerateResult result;
    const std::string outputs[kGameCount] = { inputs.outOoT, inputs.outMM };
    {
        std::error_code ec;
        if (std::filesystem::absolute(outputs[kOoT], ec) == std::filesystem::absolute(outputs[kMM], ec)) {
            return Refuse(result, "the two halves name the same output file; they carry different game identities");
        }
    }

    // The source archives: both are required (each is a source AND the
    // other direction's host, whose collision check must not pass vacuously).
    const std::string sources[kGameCount] = { inputs.ootArchive, inputs.mmArchive };
    ZipReader zips[kGameCount];
    std::vector<std::string> names[kGameCount];
    for (int game = 0; game < kGameCount; game++) {
        if (!FileExists(sources[game]) || !zips[game].Open(sources[game])) {
            return Refuse(result, Format("the manifest needs the %s archive (as a source and as a host to check "
                                         "collisions against), which is not present at %s",
                                         kGameNames[game], sources[game].c_str()));
        }
        names[game] = zips[game].Names();
    }

    std::vector<ManifestEntry> entries;
    if (std::string err = ReadManifest(inputs.manifestText, &entries); !err.empty()) {
        return Refuse(result, err);
    }

    // Constraint 2's host path sets: the extracted archive plus every other
    // base archive the host mounts before its curated half.
    std::unordered_set<std::string> hostNames[kGameCount];
    std::string hostArchiveList[kGameCount];
    for (int game = 0; game < kGameCount; game++) {
        hostNames[game].insert(names[game].begin(), names[game].end());
        hostArchiveList[game] = sources[game];
    }
    for (const HostArchive& extra : inputs.hostArchives) {
        const int game = GameIndex(extra.game);
        if (game < 0 || extra.path.empty()) {
            return Refuse(result, Format("host archive '%s=%s': expected GAME=PATH with GAME one of oot, mm",
                                         extra.game.c_str(), extra.path.c_str()));
        }
        ZipReader zip;
        if (!FileExists(extra.path) || !zip.Open(extra.path)) {
            if (extra.required) {
                return Refuse(result, Format("%s host archive %s is not present; the collision guard cannot check a "
                                             "curated path against an archive that is not there",
                                             extra.game.c_str(), extra.path.c_str()));
            }
            continue;
        }
        for (std::string& n : zip.Names()) {
            hostNames[game].insert(std::move(n));
        }
        hostArchiveList[game] += ", " + extra.path;
    }

    // (source, path) in manifest order, sorted within a prefix, de-duplicated.
    std::vector<std::pair<int, std::string>> selected;
    std::set<std::pair<int, std::string>> seen;
    for (const ManifestEntry& e : entries) {
        std::vector<std::string> matches;
        for (const std::string& n : names[e.source]) {
            if (n.compare(0, e.prefix.size(), e.prefix) == 0) {
                matches.push_back(n);
            }
        }
        if (matches.empty()) {
            return Refuse(result, Format("manifest:%d: prefix '%s' matched nothing in %s", e.lineno, e.prefix.c_str(),
                                         sources[e.source].c_str()));
        }
        std::sort(matches.begin(), matches.end());
        for (std::string& n : matches) {
            if (seen.insert({ e.source, n }).second) {
                selected.emplace_back(e.source, std::move(n));
            }
        }
    }

    // Constraint 2: no curated path may exist in any base archive its host mounts.
    for (const auto& [game, path] : selected) {
        if (hostNames[Other(game)].count(path) != 0) {
            result.findings.push_back(Format("COLLISION: %s-owned '%s' also exists in a %s base archive, its host's (%s)",
                                             kGameNames[game], path.c_str(), kGameNames[Other(game)],
                                             hostArchiveList[Other(game)].c_str()));
        }
    }
    if (!result.findings.empty()) {
        return Refuse(result, Format("COLLISION: %zu curated path(s) collide with the host game's archives",
                                     result.findings.size()));
    }

    // Read every payload BEFORE anything is written. Constraint 4.
    std::vector<std::string> payloads(selected.size());
    for (size_t i = 0; i < selected.size(); i++) {
        const auto& [game, path] = selected[i];
        if (!zips[game].Read(path, &payloads[i])) {
            return Refuse(result, Format("could not read '%s' from %s", path.c_str(), sources[game].c_str()));
        }
        if (const char* kind = DispatchedTypeOf(payloads[i])) {
            result.findings.push_back(
                Format("DISPATCHED TYPE: %s-owned '%s' is a %s resource", kGameNames[game], path.c_str(), kind));
        }
    }
    if (!result.findings.empty()) {
        return Refuse(result, Format("DISPATCHED TYPE: %zu curated resource(s) use a loader slot single-exe builds "
                                     "resolve by owning archive",
                                     result.findings.size()));
    }

    // Constraints 5 and 7 walk the same stream; walk each display list once.
    std::vector<std::vector<HashRef>> hashRefs(selected.size());
    size_t malformed = 0;
    size_t rawCount = 0;
    for (size_t i = 0; i < selected.size(); i++) {
        if (!HasTag(payloads[i], "ODLT")) {
            continue;
        }
        const auto& [game, path] = selected[i];
        std::vector<RawSegmentedRef> raw;
        const std::string why = WalkDisplayList(payloads[i], &raw, &hashRefs[i]);
        if (!why.empty()) {
            result.findings.push_back(
                Format("UNPARSABLE DISPLAY LIST: %s-owned '%s': %s", kGameNames[game], path.c_str(), why.c_str()));
            malformed++;
            continue;
        }
        for (const RawSegmentedRef& r : raw) {
            result.findings.push_back(
                Format("RAW SEGMENTED TEXTURE: %s-owned '%s' carries a raw segmented G_SETTIMG at instruction %d, "
                       "segment 0x%02X",
                       kGameNames[game], path.c_str(), r.index, r.segment));
            rawCount++;
        }
    }
    if (malformed != 0) {
        return Refuse(result, Format("UNPARSABLE DISPLAY LIST: %zu curated display list(s) could not be walked",
                                     malformed));
    }
    if (rawCount != 0) {
        return Refuse(result, Format("RAW SEGMENTED TEXTURE: %zu raw segmented texture reference(s) in curated "
                                     "display lists resolve against the HOST game's segment table",
                                     rawCount));
    }

    // Constraint 6.
    size_t unwalkable = 0;
    size_t disagreements = 0;
    for (size_t i = 0; i < selected.size(); i++) {
        if (!HasTag(payloads[i], "OARR")) {
            continue;
        }
        const auto& [game, path] = selected[i];
        ArrayDisagreement d;
        const std::string why = WalkArray(payloads[i], &d);
        if (!why.empty()) {
            result.findings.push_back(
                Format("UNPARSABLE ARRAY: %s-owned '%s': %s", kGameNames[game], path.c_str(), why.c_str()));
            unwalkable++;
        } else if (d.found) {
            const char* typeName = ScalarTypeName(d.scalarType);
            result.findings.push_back(Format(
                "ARRAY READER DISAGREEMENT: %s-owned '%s' element %u is %s -- OoT's factory consumes %u byte(s) "
                "there, MM's consumes %u",
                kGameNames[game], path.c_str(), d.index,
                typeName != nullptr ? typeName : Format("scalar type %u", d.scalarType).c_str(), d.ootWidth,
                d.mmWidth));
            disagreements++;
        }
    }
    if (unwalkable != 0) {
        return Refuse(result, Format("UNPARSABLE ARRAY: %zu curated array resource(s) could not be walked", unwalkable));
    }
    if (disagreements != 0) {
        return Refuse(result, Format("ARRAY READER DISAGREEMENT: %zu curated array resource(s) are parsed differently "
                                     "by the two games' Array factories",
                                     disagreements));
    }

    // Constraint 7: every hashed reference names a path curated in the SAME half.
    std::unordered_set<uint64_t> curatedByHash[kGameCount];
    for (const auto& [game, path] : selected) {
        curatedByHash[Other(game)].insert(CRC64(path.c_str()));
    }
    std::unordered_map<uint64_t, const std::string*> sourceByHash[kGameCount];
    bool sourceHashed[kGameCount] = { false, false };
    size_t escaping = 0;
    for (size_t i = 0; i < selected.size(); i++) {
        const auto& [game, path] = selected[i];
        const int host = Other(game);
        for (const HashRef& ref : hashRefs[i]) {
            if (curatedByHash[host].count(ref.hash) != 0) {
                continue;
            }
            if (!sourceHashed[game]) {
                for (const std::string& n : names[game]) {
                    sourceByHash[game].emplace(CRC64(n.c_str()), &n);
                }
                sourceHashed[game] = true;
            }
            auto it = sourceByHash[game].find(ref.hash);
            std::string why;
            if (it == sourceByHash[game].end()) {
                why = Format("hash 0x%016llX, which names no path in the %s archive", (unsigned long long)ref.hash,
                             kGameNames[game]);
            } else if (hostNames[host].count(*it->second) != 0) {
                why = Format("'%s', which the host's own base archives also carry -- %s would draw ITS copy",
                             it->second->c_str(), kGameNames[host]);
            } else {
                why = Format("'%s', which is not curated in %s (the host does not carry it: curate it with the model)",
                             it->second->c_str(), kOutputNames[host]);
            }
            result.findings.push_back(Format("ESCAPING REFERENCE: %s-owned '%s' instruction %d (%s) names %s",
                                             kGameNames[game], path.c_str(), ref.index, ref.command, why.c_str()));
            escaping++;
        }
    }
    if (escaping != 0) {
        return Refuse(result, Format("ESCAPING REFERENCE: %zu reference(s) in curated display lists name resources "
                                     "outside their curated half",
                                     escaping));
    }

    // Every guard passed: write both halves to temp files, then move them into
    // place, so a failure part-way leaves what was on disk untouched.
    const std::string stamp = Stamp(inputs.manifestText);
    std::string temps[kGameCount];
    for (int host = 0; host < kGameCount; host++) {
        HalfEntries half;
        for (size_t i = 0; i < selected.size(); i++) {
            if (Other(selected[i].first) == host) {
                half.emplace_back(&selected[i].second, &payloads[i]);
            }
        }
        (host == kOoT ? result.resourcesOoT : result.resourcesMM) = half.size();
        std::error_code ec;
        const std::filesystem::path out(outputs[host]);
        if (out.has_parent_path()) {
            std::filesystem::create_directories(out.parent_path(), ec);
        }
        temps[host] = outputs[host] + ".tmp";
        if (std::string err = WriteArchive(temps[host], half, stamp); !err.empty()) {
            for (const std::string& t : temps) {
                std::filesystem::remove(t, ec);
            }
            return Refuse(result, err);
        }
    }
    for (int host = 0; host < kGameCount; host++) {
        std::error_code ec;
        std::filesystem::rename(temps[host], outputs[host], ec);
        if (ec) {
            for (const std::string& t : temps) {
                std::error_code ignored;
                std::filesystem::remove(t, ignored);
            }
            return Refuse(result, Format("could not move %s into place: %s", outputs[host].c_str(),
                                         ec.message().c_str()));
        }
    }
    result.ok = true;
    return result;
}

const char* EnsureOutcomeName(EnsureOutcome outcome) {
    switch (outcome) {
        case EnsureOutcome::UpToDate:
            return "up-to-date";
        case EnsureOutcome::Generated:
            return "generated";
        case EnsureOutcome::Skipped:
            return "skipped";
        case EnsureOutcome::Failed:
            return "failed";
    }
    return "?";
}

EnsureOutcome Ensure(const GenerateInputs& inputs, std::string* log) {
    std::string note;
    EnsureOutcome outcome = EnsureOutcome::Failed;
    try {
        if (!FileExists(inputs.ootArchive) || !FileExists(inputs.mmArchive)) {
            note = Format("both games must be extracted first (oot.o2r %s, mm.o2r %s)",
                          FileExists(inputs.ootArchive) ? "present" : "absent",
                          FileExists(inputs.mmArchive) ? "present" : "absent");
            outcome = EnsureOutcome::Skipped;
        } else {
            const std::string want = Stamp(inputs.manifestText);
            const std::string outs[kGameCount] = { inputs.outOoT, inputs.outMM };
            std::string why;
            for (int host = 0; host < kGameCount; host++) {
                std::string have;
                if (!ReadStamp(outs[host], &have)) {
                    why += Format("%s%s absent", why.empty() ? "" : "; ", kOutputNames[host]);
                } else if (have != want) {
                    why += Format("%s%s stale (stamp '%s')", why.empty() ? "" : "; ", kOutputNames[host],
                                  have.c_str());
                }
            }
            if (why.empty()) {
                note = want;
                outcome = EnsureOutcome::UpToDate;
            } else {
                const GenerateResult res = Generate(inputs);
                if (res.ok) {
                    note = Format("%s; wrote %s (%zu resources) and %s (%zu resources), %s", why.c_str(),
                                  inputs.outOoT.c_str(), res.resourcesOoT, inputs.outMM.c_str(), res.resourcesMM,
                                  want.c_str());
                    outcome = EnsureOutcome::Generated;
                } else {
                    note = why + "; refused: " + res.error;
                    for (const std::string& f : res.findings) {
                        note += "\n    " + f;
                    }
                    outcome = EnsureOutcome::Failed;
                }
            }
        }
    } catch (const std::exception& e) {
        note = std::string("threw: ") + e.what();
        outcome = EnsureOutcome::Failed;
    } catch (...) {
        note = "threw a non-standard exception";
        outcome = EnsureOutcome::Failed;
    }
    if (log != nullptr) {
        *log = note;
    }
    return outcome;
}

} // namespace rsbs::curated

namespace {

std::string ExistingOr(const std::string& path, const std::string& fallback) {
    std::error_code ec;
    return (!path.empty() && std::filesystem::exists(path, ec)) ? path : fallback;
}

} // namespace

extern "C" void Combo_EnsureCuratedArchives(void) {
    namespace cur = rsbs::curated;
    // The same lookups Combo_EnsureGameArchivesLoaded (rsbs/src/main.cpp) mounts
    // through, so the archives checked and written are the ones mounted.
    const std::string oot = Ship::Context::LocateFileAcrossAppDirs("oot.o2r", "soh");
    const std::string mm = Ship::Context::LocateFileAcrossAppDirs("mm.o2r", "2s2h");

    cur::GenerateInputs in;
    in.ootArchive = oot;
    in.mmArchive = mm;
    in.hostArchives = {
        { "oot", ExistingOr(Ship::Context::GetPathRelativeToAppBundle("soh.o2r"),
                            Ship::Context::LocateFileAcrossAppDirs("soh.o2r", "soh")),
          true },
        { "oot", Ship::Context::LocateFileAcrossAppDirs("oot-mq.o2r", "soh"), false },
        { "mm", ExistingOr(Ship::Context::GetPathRelativeToAppBundle("2ship.o2r"),
                           Ship::Context::LocateFileAcrossAppDirs("2ship.o2r", "2s2h")),
          true },
    };
    in.manifestText = std::string(cur::EmbeddedManifest());

    // Regenerate a half where the mount will look for it; a half that does not
    // exist yet goes next to its host's own extracted archive, which the same
    // search then finds first.
    auto beside = [](const std::string& base, const char* name) {
        const std::filesystem::path dir = std::filesystem::path(base).parent_path();
        return (dir.empty() ? std::filesystem::path(name) : dir / name).generic_string();
    };
    in.outOoT = ExistingOr(Ship::Context::LocateFileAcrossAppDirs("redship-oot.o2r", "soh"),
                           beside(oot, "redship-oot.o2r"));
    in.outMM = ExistingOr(Ship::Context::LocateFileAcrossAppDirs("redship-mm.o2r", "2s2h"),
                          beside(mm, "redship-mm.o2r"));

    std::string log;
    const cur::EnsureOutcome outcome = cur::Ensure(in, &log);
    FILE* stream = outcome == cur::EnsureOutcome::Failed ? stderr : stdout;
    fprintf(stream, "[RSBS] Curated cross-game archives %s: %s\n", cur::EnsureOutcomeName(outcome), log.c_str());
    if (outcome == cur::EnsureOutcome::Failed) {
        fprintf(stream, "[RSBS] Foreign models keep their model-less stand-ins where no curated half is current\n");
    }
    fflush(stream);
}
