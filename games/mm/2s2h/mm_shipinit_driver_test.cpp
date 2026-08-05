/**
 * @file mm_shipinit_driver_test.cpp
 * @brief MM-side half of the #539 lock: probes planted in MM's OWN registrar
 *        map, so the OoT-side driver can be observed crossing the seam.
 *
 * The other half is games/oot/soh/soh_shipinit_driver_test.cpp, which drives
 * OoT's `ShipInit::Init(path)` — the exact call every `SohMenu` widget makes.
 * The split is forced, not stylistic: under `RSBS_SINGLE_EXECUTABLE` MM's
 * `2s2h/ShipInit.hpp` ends with `using S2H::ShipInit;` at global scope, which
 * collides with OoT's global `struct ShipInit`, so NO translation unit can see
 * both headers. That is also exactly why the bug existed — the two maps cannot
 * see each other either.
 *
 * The probes are registered through MM's real `RegisterShipInitFunc`, so they
 * sit in the same map, under the same keys, with the same semantics as MM's
 * production registrars. They are inert: they increment a counter.
 *
 * Keys chosen on purpose:
 *  - `kProbeCVarPath` is SYNTHETIC. The driver leg must not depend on which
 *    real MM enhancement TUs survived plain-archive linking this week (the
 *    #516 elision class), and it must not re-run a production registrar with
 *    live side effects inside a display-free harness.
 *  - `"IS_RANDO"` is real, and the probe there is what makes the exclusion leg
 *    non-vacuous: without a registrant under that key, "IS_RANDO did not
 *    forward" would pass whether or not the filter existed.
 *  - The `"*"` exclusion needs no probe of its own: `RegisterShipInitFunc`
 *    pushes every registrant into `"*"` as well, so the CVar probe is already
 *    under it.
 *
 * The converged-key reality check (does MM actually keep registrars under a
 * shared enhancement key?) is asserted on the OoT side through
 * `MM_ShipInit_RegistrarCountForPath`, which is production code in
 * 2s2h/ShipInitBridge_SingleExe.cpp.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/ShipInit.hpp"

#include <cstdio>
#include <string>

namespace {

int gCVarProbeRuns = 0;
int gRandoProbeRuns = 0;

void CVarProbe() {
    gCVarProbeRuns++;
}

void RandoProbe() {
    gRandoProbeRuns++;
}

} // namespace

// Kept in sync with the same literal on the OoT side; a typo on either side
// shows up as FAIL(2) rather than a silent pass, because the probe count would
// stay zero.
#define MM_SHIPINIT_TEST_PROBE_PATH "gTest.ShipInitDriverProbe"

static RegisterShipInitFunc sCVarProbe(CVarProbe, { MM_SHIPINIT_TEST_PROBE_PATH });
static RegisterShipInitFunc sRandoProbe(RandoProbe, { "IS_RANDO" });

extern "C" {

void MMShipInitTest_ResetProbes(void) {
    gCVarProbeRuns = 0;
    gRandoProbeRuns = 0;
}

int MMShipInitTest_CVarProbeRuns(void) {
    return gCVarProbeRuns;
}

int MMShipInitTest_RandoProbeRuns(void) {
    return gRandoProbeRuns;
}

const char* MMShipInitTest_ProbePath(void) {
    return MM_SHIPINIT_TEST_PROBE_PATH;
}

/**
 * Print every CVar-shaped key MM's map holds, with its registrar count.
 * Called only on a failure path: when the converged-key leg goes red the two
 * plausible causes (a #516-class elision vs. the two games' spellings having
 * drifted) look identical from the OoT side, and this tells them apart in the
 * same run instead of costing a rebuild.
 */
void MMShipInitTest_DumpCVarKeys(void) {
    auto& shipInitFuncs = S2H::ShipInit::GetAll();
    printf("[TEST] MM S2H::ShipInit map: %d paths\n", (int)shipInitFuncs.size());
    for (const auto& [path, funcs] : shipInitFuncs) {
        if (path.find('.') == std::string::npos) {
            continue; // "*", "IS_RANDO" and other pseudo-paths
        }
        printf("[TEST]   %s -> %d\n", path.c_str(), (int)funcs.size());
    }
}

} // extern "C"

#endif // RSBS_SINGLE_EXECUTABLE
