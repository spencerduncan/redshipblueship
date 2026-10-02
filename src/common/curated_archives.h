/**
 * @file curated_archives.h
 * @brief In-app generation of the curated cross-game archives (#806)
 *
 * redship-oot.o2r (the MM-origin models OoT draws) and redship-mm.o2r (the
 * OoT-origin models MM draws) are ROM-derived: they are carved out of the
 * extracted oot.o2r / mm.o2r, so no package can ship them. Before #806 only the
 * dev-time CMake target GenerateRedshipOtr (scripts/make_redship_otr.py) made
 * them, and every packaged build ran with both halves absent -- every #577
 * consumer fell back to its model-less stand-in.
 *
 * This is a C++ port of that generator's rules, run by the game on the
 * player's machine (rsbs/src/main.cpp, at boot, after both games' in-app
 * extraction has had its chance to run). It reads the SAME manifest,
 * assets/crossgame/manifest.txt, which the build embeds into the binary, and
 * applies the SAME admission guards (see the Python module docstring for why
 * each exists): verbatim paths; no collision with any base archive the host
 * mounts; no Room/Cutscene/Path resource; no raw segmented G_SETTIMG; no Array
 * resource the two games' readers consume differently; every path-hash
 * reference of a curated display list stays inside its half. Both halves are
 * written only after every guard passed, or neither is.
 *
 * Each half carries a STAMP in its zip archive comment naming the manifest it
 * was made from (Stamp below). The Python generator writes the same stamp, so
 * a dev tree's GenerateRedshipOtr output is current; a half whose stamp
 * differs (an older build's manifest, or a pre-#806 archive with no stamp) is
 * regenerated. The stamp lives in the comment, not in an entry, so the path
 * set stays exactly the manifest's.
 *
 * Nothing here ever refuses boot: a missing source archive, a guard refusal or
 * an I/O failure is logged, the halves on disk are left as they were, and
 * foreign models fall back to their stand-ins exactly as before.
 *
 * Game-header-free (ADR 0002): it reads archives through libzip and hashes
 * paths with libultraship's CRC64, nothing else.
 */

#ifndef RSBS_COMMON_CURATED_ARCHIVES_H
#define RSBS_COMMON_CURATED_ARCHIVES_H

#ifdef __cplusplus

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rsbs::curated {

/** A base archive the host mounts before its curated half (collision guard). */
struct HostArchive {
    /** "oot" or "mm": the host game that mounts it. */
    std::string game;
    std::string path;
    /** A required archive that is absent refuses generation (a check against
     *  an absent archive would pass vacuously); an optional one is skipped. */
    bool required = true;
};

struct GenerateInputs {
    /** The extracted base archives, the sources (and hosts) of both halves. */
    std::string ootArchive;
    std::string mmArchive;
    /** The hosts' OTHER base archives: soh.o2r / oot-mq.o2r for OoT, 2ship.o2r
     *  for MM (Combo_EnsureGameArchivesLoaded's list). */
    std::vector<HostArchive> hostArchives;
    /** The manifest's text, `<source>-><host> <path-prefix>` lines. */
    std::string manifestText;
    /** Where to write each half. Written via a sibling temp file and a rename. */
    std::string outOoT;
    std::string outMM;
};

struct GenerateResult {
    bool ok = false;
    /** One line naming why generation was refused (empty when ok). */
    std::string error;
    /** Every finding, one per line, in the Python generator's vocabulary
     *  (COLLISION, DISPATCHED TYPE, RAW SEGMENTED TEXTURE, ...). */
    std::vector<std::string> findings;
    size_t resourcesOoT = 0;
    size_t resourcesMM = 0;
};

/** The generator format revision. Bump together with STAMP_VERSION in
 *  scripts/make_redship_otr.py whenever an admission rule changes what a
 *  manifest produces, so archives made under the old rules are regenerated. */
inline constexpr int kStampVersion = 1;

/** libultraship's CRC64 of the manifest text with every CR byte removed, so a
 *  CRLF checkout and an LF one stamp the same manifest identically. */
uint64_t ManifestHash(std::string_view manifestText);

/** The archive comment each half carries: "redship-curated v<N> manifest-crc64=<16 hex>". */
std::string Stamp(std::string_view manifestText);

/** The manifest compiled into this binary (assets/crossgame/manifest.txt at
 *  build time). */
std::string_view EmbeddedManifest();

/** Read @p archivePath's zip archive comment into @p out. False when the file
 *  cannot be opened as a zip archive (an absent file included). */
bool ReadStamp(const std::string& archivePath, std::string* out);

/** Run every admission guard and, only if all pass, write both halves. */
GenerateResult Generate(const GenerateInputs& inputs);

enum class EnsureOutcome {
    /** Both halves exist and carry the current stamp; nothing was written. */
    UpToDate,
    /** At least one half was missing or stale, and both were (re)written. */
    Generated,
    /** A source archive is absent (a game not extracted yet): nothing to do. */
    Skipped,
    /** Generation was refused or failed; what was on disk is untouched. */
    Failed,
};

const char* EnsureOutcomeName(EnsureOutcome outcome);

/**
 * Regenerate both halves when either is absent or its stamp differs from
 * Stamp(inputs.manifestText). inputs.outOoT / inputs.outMM name where each half
 * lives (or will). Skipped when either source archive is absent; never throws.
 * @p log, when non-null, receives a one-line account of what happened.
 */
EnsureOutcome Ensure(const GenerateInputs& inputs, std::string* log = nullptr);

} // namespace rsbs::curated

extern "C" {
#endif

/**
 * Boot-time entry point (rsbs/src/main.cpp): resolve oot.o2r, mm.o2r and the
 * hosts' other base archives exactly the way Combo_EnsureGameArchivesLoaded
 * does, and Ensure the two curated halves next to them against the embedded
 * manifest. Call before the first curated mount. Never refuses boot.
 */
void Combo_EnsureCuratedArchives(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_CURATED_ARCHIVES_H
