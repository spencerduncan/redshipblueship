/**
 * @file seed_package.cpp
 * @brief The seed package's container (#1044): a zip of named entries, written
 *        and read through libzip. See seed_package.h for what the entries are and
 *        who writes and reads them; this file knows nothing about either game.
 *
 * DETERMINISTIC BYTES. Every entry is stored in the order the caller gives, with
 * one fixed DOS timestamp (1980-01-01 00:00) and no archive comment, so the same
 * entries make the same archive: a package's bytes are a function of its world,
 * not of the moment it was written. (Deflate's output is a function of the zlib
 * build too, so two platforms may store different bytes for the same entries;
 * nothing compares package bytes across platforms, only the entries.)
 */
#include "seed_package.h"
#include "foreign_items.h" // RSBS_GENERATOR_VERSION

#include <zip.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <system_error>

namespace {

// 1980-01-01 00:00:00, the DOS epoch: date = ((1980 - 1980) << 9) | (1 << 5) | 1.
constexpr zip_uint16_t kFixedDosTime = 0;
constexpr zip_uint16_t kFixedDosDate = (1u << 5) | 1u;

int StatusFromZipOpenError(int err) {
    switch (err) {
        case ZIP_ER_NOENT:
        case ZIP_ER_OPEN:
        case ZIP_ER_READ:
            return RSBS_SEEDPKG_ERR_IO;
        default:
            return RSBS_SEEDPKG_ERR_NOT_A_PACKAGE;
    }
}

} // namespace

extern "C" const char* SeedPackage_StatusName(int status) {
    switch (status) {
        case RSBS_SEEDPKG_OK:
            return "ok";
        case RSBS_SEEDPKG_ERR_UNSUPPORTED:
            return "unsupported";
        case RSBS_SEEDPKG_ERR_IO:
            return "io";
        case RSBS_SEEDPKG_ERR_NOT_A_PACKAGE:
            return "notAPackage";
        case RSBS_SEEDPKG_ERR_MISSING_ENTRY:
            return "missingEntry";
        case RSBS_SEEDPKG_ERR_IDENTITY:
            return "identity";
        case RSBS_SEEDPKG_ERR_FORMAT:
            return "packageFormat";
        case RSBS_SEEDPKG_ERR_GENERATOR_VERSION:
            return "generatorVersion";
        case RSBS_SEEDPKG_ERR_INTEGRITY:
            return "integrity";
        case RSBS_SEEDPKG_ERR_LAYOUT:
            return "layout";
        case RSBS_SEEDPKG_ERR_NO_WORLD:
            return "noWorld";
        case RSBS_SEEDPKG_ERR_APPLY:
            return "apply";
        case RSBS_SEEDPKG_ERR_GENERATION:
            return "generation";
        default:
            return "unknown";
    }
}

extern "C" const char* SeedPackage_RefusalWords(int status) {
    switch (status) {
        case RSBS_SEEDPKG_ERR_GENERATOR_VERSION:
            return "Seed package from another generator";
        case RSBS_SEEDPKG_ERR_FORMAT:
            return "Seed package from a newer build";
        case RSBS_SEEDPKG_ERR_IO:
            return "Seed package could not be read";
        case RSBS_SEEDPKG_ERR_APPLY:
            return "Seed package could not be loaded";
        default:
            return "Seed package damaged or incomplete";
    }
}

extern "C" int SeedPackage_CheckGeneratorVersion(uint32_t packageGeneratorVersion) {
    // Equality, not "at most": a package carries a finished world, so an older
    // generator's world is not this build's world either, and the load cannot tell
    // which of its terms the generator change moved. Version 0 is the label-free
    // formula of files written before #1003; no package was ever written by it.
    return packageGeneratorVersion == RSBS_GENERATOR_VERSION ? RSBS_SEEDPKG_OK : RSBS_SEEDPKG_ERR_GENERATOR_VERSION;
}

extern "C" uint32_t SeedPackage_Fnv1a(const void* data, size_t len) {
    uint32_t h = 0x811C9DC5u;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 0x01000193u;
    }
    return h;
}

extern "C" int SeedPackage_WriteEntries(const char* path, const char* const* names, const void* const* data,
                                        const size_t* sizes, int count) {
    if (path == nullptr || path[0] == '\0' || count < 0 || (count > 0 && (names == nullptr || sizes == nullptr))) {
        return RSBS_SEEDPKG_ERR_IO;
    }
    std::error_code ec;
    const std::filesystem::path target(path);
    if (target.has_parent_path()) {
        std::filesystem::create_directories(target.parent_path(), ec);
    }
    // Written beside the target under a name of its own and renamed over it, so a
    // reader never meets a half-written package, a failed write leaves the old one
    // in place, and two processes writing the same package never share a file.
    std::random_device entropy;
    char suffix[32];
    snprintf(suffix, sizeof(suffix), ".%08X.partial", (unsigned)entropy());
    const std::string temp = std::string(path) + suffix;
    std::filesystem::remove(temp, ec);
    int err = 0;
    zip_t* za = zip_open(temp.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (za == nullptr) {
        fprintf(stderr, "[SeedPackage] could not create '%s' (libzip error %d)\n", temp.c_str(), err);
        return RSBS_SEEDPKG_ERR_IO;
    }
    for (int i = 0; i < count; i++) {
        const void* bytes = (data != nullptr && data[i] != nullptr) ? data[i] : "";
        zip_source_t* src = zip_source_buffer(za, bytes, sizes[i], 0);
        if (src == nullptr) {
            zip_discard(za);
            std::filesystem::remove(temp, ec);
            return RSBS_SEEDPKG_ERR_IO;
        }
        const zip_int64_t index = zip_file_add(za, names[i], src, ZIP_FL_ENC_UTF_8 | ZIP_FL_OVERWRITE);
        if (index < 0) {
            fprintf(stderr, "[SeedPackage] could not add '%s' to '%s': %s\n", names[i], temp.c_str(), zip_strerror(za));
            zip_source_free(src);
            zip_discard(za);
            std::filesystem::remove(temp, ec);
            return RSBS_SEEDPKG_ERR_IO;
        }
        if (zip_file_set_dostime(za, (zip_uint64_t)index, kFixedDosTime, kFixedDosDate, 0) != 0) {
            zip_discard(za);
            std::filesystem::remove(temp, ec);
            return RSBS_SEEDPKG_ERR_IO;
        }
    }
    if (zip_close(za) != 0) {
        fprintf(stderr, "[SeedPackage] could not write '%s': %s\n", temp.c_str(), zip_strerror(za));
        zip_discard(za);
        std::filesystem::remove(temp, ec);
        return RSBS_SEEDPKG_ERR_IO;
    }
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        // A rename over an existing file can be refused (another process holding
        // it open on Windows); fall back to replacing it.
        std::error_code ec2;
        std::filesystem::remove(target, ec2);
        std::filesystem::rename(temp, target, ec2);
        if (ec2) {
            std::filesystem::remove(temp, ec2);
            return RSBS_SEEDPKG_ERR_IO;
        }
    }
    return RSBS_SEEDPKG_OK;
}

extern "C" int SeedPackage_ReadEntry(const char* path, const char* name, void** outData, size_t* outSize) {
    if (outData != nullptr) {
        *outData = nullptr;
    }
    if (outSize != nullptr) {
        *outSize = 0;
    }
    if (path == nullptr || name == nullptr || outData == nullptr || outSize == nullptr) {
        return RSBS_SEEDPKG_ERR_IO;
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(std::filesystem::path(path), ec)) {
        return RSBS_SEEDPKG_ERR_IO;
    }
    int err = 0;
    zip_t* za = zip_open(path, ZIP_RDONLY, &err);
    if (za == nullptr) {
        return StatusFromZipOpenError(err);
    }
    const zip_int64_t index = zip_name_locate(za, name, ZIP_FL_ENC_RAW);
    if (index < 0) {
        zip_discard(za);
        return RSBS_SEEDPKG_ERR_MISSING_ENTRY;
    }
    zip_stat_t st;
    zip_stat_init(&st);
    if (zip_stat_index(za, (zip_uint64_t)index, 0, &st) != 0 || (st.valid & ZIP_STAT_SIZE) == 0) {
        zip_discard(za);
        return RSBS_SEEDPKG_ERR_NOT_A_PACKAGE;
    }
    char* buffer = static_cast<char*>(malloc((size_t)st.size + 1));
    if (buffer == nullptr) {
        zip_discard(za);
        return RSBS_SEEDPKG_ERR_IO;
    }
    zip_int64_t got = 0;
    if (st.size > 0) {
        zip_file_t* file = zip_fopen_index(za, (zip_uint64_t)index, 0);
        if (file == nullptr) {
            free(buffer);
            zip_discard(za);
            return RSBS_SEEDPKG_ERR_NOT_A_PACKAGE;
        }
        got = zip_fread(file, buffer, st.size);
        zip_fclose(file);
    }
    zip_discard(za);
    if (got != (zip_int64_t)st.size) {
        free(buffer);
        return RSBS_SEEDPKG_ERR_NOT_A_PACKAGE;
    }
    buffer[st.size] = '\0';
    *outData = buffer;
    *outSize = (size_t)st.size;
    return RSBS_SEEDPKG_OK;
}

extern "C" size_t SeedPackage_FileName(const char* seedName, uint32_t fingerprint, char* out, size_t cap) {
    if (out == nullptr || cap == 0) {
        return 0;
    }
    out[0] = '\0';
    if (seedName == nullptr) {
        return 0;
    }
    const int n = snprintf(out, cap, "%s-%08X%s", seedName, (unsigned)fingerprint, RSBS_SEED_PACKAGE_EXTENSION);
    if (n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

namespace rsbs::seedpkg {

int ReadAll(const std::string& path, Entries& out) {
    out.clear();
    std::error_code ec;
    if (!std::filesystem::is_regular_file(std::filesystem::path(path), ec)) {
        return RSBS_SEEDPKG_ERR_IO;
    }
    int err = 0;
    zip_t* za = zip_open(path.c_str(), ZIP_RDONLY, &err);
    if (za == nullptr) {
        return StatusFromZipOpenError(err);
    }
    const zip_int64_t n = zip_get_num_entries(za, 0);
    for (zip_int64_t i = 0; i < n; i++) {
        const char* name = zip_get_name(za, (zip_uint64_t)i, ZIP_FL_ENC_RAW);
        zip_stat_t st;
        zip_stat_init(&st);
        if (name == nullptr || zip_stat_index(za, (zip_uint64_t)i, 0, &st) != 0 || (st.valid & ZIP_STAT_SIZE) == 0) {
            zip_discard(za);
            out.clear();
            return RSBS_SEEDPKG_ERR_NOT_A_PACKAGE;
        }
        std::string bytes((size_t)st.size, '\0');
        if (st.size > 0) {
            zip_file_t* file = zip_fopen_index(za, (zip_uint64_t)i, 0);
            if (file == nullptr) {
                zip_discard(za);
                out.clear();
                return RSBS_SEEDPKG_ERR_NOT_A_PACKAGE;
            }
            const zip_int64_t got = zip_fread(file, bytes.data(), st.size);
            zip_fclose(file);
            if (got != (zip_int64_t)st.size) {
                zip_discard(za);
                out.clear();
                return RSBS_SEEDPKG_ERR_NOT_A_PACKAGE;
            }
        }
        out[name] = std::move(bytes);
    }
    zip_discard(za);
    return RSBS_SEEDPKG_OK;
}

int WriteAll(const std::string& path, const std::vector<std::pair<std::string, std::string>>& entries) {
    std::vector<const char*> names;
    std::vector<const void*> data;
    std::vector<size_t> sizes;
    for (const auto& [name, bytes] : entries) {
        names.push_back(name.c_str());
        data.push_back(bytes.data());
        sizes.push_back(bytes.size());
    }
    return SeedPackage_WriteEntries(path.c_str(), names.data(), data.data(), sizes.data(), (int)entries.size());
}

std::vector<std::string> ListDirectory(const std::string& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    const std::string ext = RSBS_SEED_PACKAGE_EXTENSION;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code fileEc;
        if (!it->is_regular_file(fileEc)) {
            continue;
        }
        const std::string name = it->path().filename().string();
        if (name.size() > ext.size() && name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

int CopyPackage(const std::string& source, const std::string& destination) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(std::filesystem::path(source), ec)) {
        return RSBS_SEEDPKG_ERR_IO;
    }
    if (std::filesystem::equivalent(std::filesystem::path(source), std::filesystem::path(destination), ec)) {
        return RSBS_SEEDPKG_OK;
    }
    const std::filesystem::path target(destination);
    if (target.has_parent_path()) {
        std::filesystem::create_directories(target.parent_path(), ec);
    }
    std::error_code copyEc;
    std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, copyEc);
    return copyEc ? RSBS_SEEDPKG_ERR_IO : RSBS_SEEDPKG_OK;
}

} // namespace rsbs::seedpkg
