/**
 * ROM-free lock for the MM GameInteractor shim (#395 / #383 GameInteractor
 * item). CTest label "redship", row mm-gi-shim in src/common/test_runner.cpp.
 *
 * DIAGNOSTIC PHASE (this revision deliberately FAILS): before migrating MM's
 * four RegisterGameHook call sites off the shared C++ class, this test
 * measures — from real OoT and MM translation units compiled with their
 * production flags — which byte offsets each side's registration path
 * actually writes, by running registrations against oversized zeroed buffers:
 *
 *  - Direct probes measure the real call sites (inlining included).
 *  - OutOfLine probes (member-function-pointer calls) measure the
 *    linker-selected COMDAT copy of RegisterGameHook<OnGameStateMainStart> —
 *    the one every non-inlined call site in EITHER game binds.
 *
 * CI runs ctest with --output-on-failure, so the intentional failure is what
 * publishes the measurements to the build-linux log. The follow-up commit
 * replaces this body with the real regression lock.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// MM's 2s2h/GameInteractor/GameInteractor.h is force-included into every MM
// C++ TU (games/mm/CMakeLists.txt), so GameInteractor here is MM's layout.

extern "C" {
// games/oot/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp
size_t OoT_GI_InstanceSize(void);
size_t OoT_GI_NextHookIdOffset(void);
uint32_t OoT_GI_ProbeRegisterOnMainStartDirect(void* storage, void (*fn)(void));
uint32_t OoT_GI_ProbeRegisterOnMainStartOutOfLine(void* storage, void (*fn)(void));
void OoT_GI_ProbeUnregisterOnMainStart(uint32_t hookId);
void OoT_GI_ProbePumpOnMainStart(void);
// GameExports_SingleExe.cpp (TEMPORARY diagnostics, same TU as the live
// MM call sites)
uint32_t MM_GI_DiagRegisterOnMainStartDirect(void* storage, void (*fn)(void));
uint32_t MM_GI_DiagRegisterOnMainStartOutOfLine(void* storage, void (*fn)(void));
size_t MM_GI_DiagInstanceSize(void);
size_t MM_GI_DiagNextHookIdOffset(void);
}

namespace {

constexpr size_t kProbeBufSize = 256;

void GIShimNoopHook(void) {
}

// Prints every byte range [lo, hi) that is nonzero after a probe ran against
// a zeroed buffer. The registration path writes only nextHookId, so the
// ranges directly name the layout the executed code was compiled against.
void ReportWrites(const char* label, const unsigned char* buf, size_t n) {
    printf("[GI-DIAG] %-28s wrote:", label);
    bool any = false;
    size_t i = 0;
    while (i < n) {
        if (buf[i] != 0) {
            size_t lo = i;
            while (i < n && buf[i] != 0) {
                i++;
            }
            printf(" [%zu,%zu)", lo, i);
            any = true;
        } else {
            i++;
        }
    }
    if (!any) {
        printf(" nothing");
    }
    printf("\n");
}

} // namespace

extern "C" int MM_GIShim_RunHeadless(void) {
    printf("[GI-DIAG] layout: OoT sizeof=%zu nextHookId@%zu | MM sizeof=%zu nextHookId@%zu\n", OoT_GI_InstanceSize(),
           OoT_GI_NextHookIdOffset(), MM_GI_DiagInstanceSize(), MM_GI_DiagNextHookIdOffset());

    struct ProbeCase {
        const char* label;
        uint32_t (*probe)(void*, void (*)(void));
    };
    const ProbeCase cases[] = {
        { "OoT direct call", OoT_GI_ProbeRegisterOnMainStartDirect },
        { "OoT out-of-line (COMDAT)", OoT_GI_ProbeRegisterOnMainStartOutOfLine },
        { "MM direct call", MM_GI_DiagRegisterOnMainStartDirect },
        { "MM out-of-line (COMDAT)", MM_GI_DiagRegisterOnMainStartOutOfLine },
    };

    uint32_t ids[4] = { 0, 0, 0, 0 };
    int idCount = 0;

    for (const ProbeCase& c : cases) {
        unsigned char* buf = static_cast<unsigned char*>(calloc(1, kProbeBufSize));
        if (buf == nullptr) {
            printf("[TEST] FAIL: probe buffer allocation failed\n");
            return 1;
        }
        uint32_t id = c.probe(buf, GIShimNoopHook);
        ids[idCount++] = id;
        ReportWrites(c.label, buf, kProbeBufSize);
        free(buf);
    }

    // Clean the shared inline-static hook map back up: queue all probe hooks
    // for unregistration, then pump once (the flush happens at the head of
    // ExecuteHooks, so nothing actually executes).
    for (int i = 0; i < idCount; i++) {
        OoT_GI_ProbeUnregisterOnMainStart(ids[i]);
    }
    OoT_GI_ProbePumpOnMainStart();

    printf("[TEST] FAIL: mm-gi-shim is in DIAGNOSTIC mode — intentional failure so ctest "
           "--output-on-failure publishes the probe report above. The shim migration commit "
           "replaces this with the real regression lock.\n");
    return 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
