/**
 * @file SeedPackageSingleExe.cpp
 * @brief OoT's side of the seed package (#1044, src/common/seed_package.h).
 *
 * THE LOCK COMMIT'S SEAM: every entry point is declared and behaves as today's
 * build does -- Generate completes nothing (the paired half is still authored at
 * file creation), no package is written, loaded or staged. The fix fills them in.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "seed_package.h"

extern "C" int OoT_SeedPackage_CompletePendingGeneration(void) {
    return 0;
}

extern "C" int OoT_SeedPackage_FileSelectTick(void) {
    return 0;
}

extern "C" int OoT_SeedPackage_LoadFile(const char* path) {
    (void)path;
    return RSBS_SEEDPKG_ERR_UNSUPPORTED;
}

extern "C" int OoT_SeedPackage_Staged(void) {
    return 0;
}

extern "C" const char* OoT_SeedPackage_StagedPath(void) {
    return "";
}

extern "C" int OoT_SeedPackage_StagedDigest(char* out, size_t cap) {
    if (out != nullptr && cap > 0) {
        out[0] = '\0';
    }
    return -1;
}

extern "C" int OoT_SeedPackage_ExportStaged(const char* destinationPath) {
    (void)destinationPath;
    return RSBS_SEEDPKG_ERR_UNSUPPORTED;
}

extern "C" void OoT_SeedPackage_Unstage(void) {
}

extern "C" int OoT_SeedPackage_LastStatus(void) {
    return RSBS_SEEDPKG_ERR_UNSUPPORTED;
}

extern "C" int OoT_SeedPackage_TestCompleteWorld(void) {
    return RSBS_SEEDPKG_ERR_UNSUPPORTED;
}

extern "C" int OoT_SeedPackage_TestWritePackage(const char* path) {
    (void)path;
    return RSBS_SEEDPKG_ERR_UNSUPPORTED;
}

extern "C" const uint8_t* OoT_SeedPackage_TestFinishedMMHalf(size_t* outSize) {
    if (outSize != nullptr) {
        *outSize = 0;
    }
    return nullptr;
}

extern "C" const uint8_t* OoT_SeedPackage_TestStagedMMHalf(size_t* outSize) {
    if (outSize != nullptr) {
        *outSize = 0;
    }
    return nullptr;
}

#endif // RSBS_SINGLE_EXECUTABLE
