/**
 * @file curated_archives.cpp
 * @brief In-app generation of the curated cross-game archives (#806) -- see curated_archives.h
 *
 * RED-half stub: the lock's surface with no generator behind it, which is the
 * state of every packaged build before #806.
 */

#include "curated_archives.h"

#include <cstdio>
#include <string>

#include <zip.h>

#include <ship/utils/StrHash64.h>

#include "rsbs_curated_manifest.h"

namespace rsbs::curated {

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
    char buf[96];
    snprintf(buf, sizeof(buf), "redship-curated v%d manifest-crc64=%016llx", kStampVersion,
             (unsigned long long)ManifestHash(manifestText));
    return buf;
}

std::string_view EmbeddedManifest() {
    return std::string_view(reinterpret_cast<const char*>(kRsbsCuratedManifest), sizeof(kRsbsCuratedManifest));
}

bool ReadStamp(const std::string& archivePath, std::string* out) {
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

GenerateResult Generate(const GenerateInputs& /*inputs*/) {
    GenerateResult result;
    result.error = "no in-app curated-archive generator (pre-#806)";
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

EnsureOutcome Ensure(const GenerateInputs& /*inputs*/, std::string* log) {
    if (log != nullptr) {
        *log = "no in-app curated-archive generator (pre-#806)";
    }
    return EnsureOutcome::Skipped;
}

} // namespace rsbs::curated

extern "C" void Combo_EnsureCuratedArchives(void) {
}
