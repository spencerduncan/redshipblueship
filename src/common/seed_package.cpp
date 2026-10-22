/**
 * @file seed_package.cpp
 * @brief The seed package's container (#1044). See seed_package.h.
 *
 * THE LOCK COMMIT'S SEAM: every entry point is declared and answers "this build
 * cannot read or write seed packages", which is today's behaviour. The fix fills
 * them in.
 */
#include "seed_package.h"

extern "C" const char* SeedPackage_StatusName(int status) {
    (void)status;
    return "unsupported";
}

extern "C" int SeedPackage_CheckGeneratorVersion(uint32_t packageGeneratorVersion) {
    (void)packageGeneratorVersion;
    return RSBS_SEEDPKG_ERR_UNSUPPORTED;
}

extern "C" uint32_t SeedPackage_Fnv1a(const void* data, size_t len) {
    (void)data;
    (void)len;
    return 0;
}

extern "C" int SeedPackage_WriteEntries(const char* path, const char* const* names, const void* const* data,
                                        const size_t* sizes, int count) {
    (void)path;
    (void)names;
    (void)data;
    (void)sizes;
    (void)count;
    return RSBS_SEEDPKG_ERR_UNSUPPORTED;
}

extern "C" int SeedPackage_ReadEntry(const char* path, const char* name, void** outData, size_t* outSize) {
    (void)path;
    (void)name;
    if (outData != nullptr) {
        *outData = nullptr;
    }
    if (outSize != nullptr) {
        *outSize = 0;
    }
    return RSBS_SEEDPKG_ERR_UNSUPPORTED;
}

extern "C" size_t SeedPackage_FileName(const char* seedName, uint32_t fingerprint, char* out, size_t cap) {
    (void)seedName;
    (void)fingerprint;
    if (out != nullptr && cap > 0) {
        out[0] = '\0';
    }
    return 0;
}
