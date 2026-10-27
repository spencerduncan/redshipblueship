/**
 * @file test_hookshot_reticle_colour.c
 * @brief OoTHookshotReticleColour + MmHookshotReticleColour: SoH's "Targetable
 *        Hookshot Reticle" colours the hookshot reticle by the surface it
 *        points at, in both games, and nothing else turns it green (#1173).
 *
 * ROM-free, redship tier. The operator's ruling on #1173 is surface-only parity
 * with SoH: the reticle takes the target colour (default green) when the
 * option is on, the hookshot's probe hit a hookshottable surface, and the
 * held item is the Hookshot (or Longshot). No enemy colouring.
 *
 *   OoT: SoH 6340ad3d5 (#6279) added the held-item clause. Our inline copy in
 *        games/oot/src/code/z_player_lib.c still coloured the Bow, Fire Arrow,
 *        Slingshot and Boomerang reticles green on a hookshot surface, because
 *        the Bow/Boomerang reticle enhancements draw through the same
 *        function. The row feeds OoT_Player_HookshotReticleIsTarget each
 *        held item with a hookshottable and a plain probe result.
 *   MM:  2Ship drew the fixed-colour gHookshotReticleDL (red) whatever the
 *        probe hit. The row feeds MM_Player_HookshotReticleIsTarget the same
 *        matrix and reads the colour the draw sets: the target and non-target
 *        defaults, then SoH's two Cosmetics Editor colours, which Termina
 *        honours too (one game, one set of settings).
 *
 * Both rows clear the keys they touch before and after, and never save the
 * config. Linkage note: #included into test_runner.cpp at file scope
 * (compiled as C++).
 */

#include "../test_runner.h"

#include <cstdint>
#include <cstdio>

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
// games/oot/src/code/z_player_lib.c (#1173)
int32_t OoT_Player_HookshotReticle_TestIsTarget(int32_t sample, int32_t isHookshotSurface);
// games/mm/src/code/z_player_lib.c (#1173)
int32_t MM_Player_HookshotReticle_TestIsTarget(int32_t sample, int32_t isHookshotSurface);
uint32_t MM_Player_HookshotReticle_TestColor(int32_t isTarget);
}

namespace {

constexpr const char* kReticleOptionKey = "gEnhancements.HookshotableReticle";
constexpr const char* kReticleTargetColorKey = "gCosmetics.HookshotReticle.Target.Value";
constexpr const char* kReticleNonTargetColorKey = "gCosmetics.HookshotReticle.NonTarget.Value";

constexpr uint32_t kReticleGreen = 0x00FF00FFu; // SoH's default target colour
constexpr uint32_t kReticleRed = 0xFF0000FFu;   // SoH's default non-target colour, and the vanilla reticle's

void ReticleClearKeys(void) {
    CVarClear(kReticleOptionKey);
    CVarClear(kReticleTargetColorKey);
    CVarClear(kReticleNonTargetColorKey);
}

struct ReticleSample {
    const char* name;
    bool isHookshot; // only the Hookshot and Longshot colour their reticle by the surface
};

// Order matches each game's test export.
const ReticleSample kOotReticleSamples[] = {
    { "Hookshot", true },    { "Longshot", true },   { "Bow", false },
    { "Fire Arrow", false }, { "Slingshot", false }, { "Boomerang", false },
};
const ReticleSample kMmReticleSamples[] = {
    { "Hookshot", true },
    { "Bow", false },
};

// Every sample, both probe results, the option on and off.
template <size_t N>
bool ReticleCheckMatrix(const char* game, const ReticleSample (&samples)[N],
                        int32_t (*isTarget)(int32_t sample, int32_t isHookshotSurface)) {
    bool ok = true;
    for (int option = 0; option <= 1; option++) {
        CVarSetInteger(kReticleOptionKey, option);
        for (size_t s = 0; s < N; s++) {
            for (int surface = 0; surface <= 1; surface++) {
                const int32_t got = isTarget((int32_t)s, surface);
                const int32_t want = (option && surface && samples[s].isHookshot) ? 1 : 0;
                if (got != want) {
                    printf("[TEST] FAIL: %s, option %s, %s aimed at a %s surface: the reticle takes the %s colour, "
                           "expected the %s colour\n",
                           game, option ? "on" : "off", samples[s].name, surface ? "hookshottable" : "plain",
                           got == 1 ? "target" : (got == 0 ? "non-target" : "(fixture error)"),
                           want ? "target" : "non-target");
                    ok = false;
                }
            }
        }
    }
    return ok;
}

bool ReticleCheckColor(const char* what, uint32_t got, uint32_t want) {
    if (got != want) {
        printf("[TEST] FAIL: MM %s: the reticle draws in 0x%08X, expected 0x%08X\n", what, (unsigned)got,
               (unsigned)want);
        return false;
    }
    return true;
}

} // namespace

// The rows' bodies. Test_OotHookshotReticleColour / Test_MmHookshotReticleColour
// (test_runner.cpp) bring up the display-free Ship::Context first: the CVars
// live on it.
static TestResult OotHookshotReticleColour_Run(void) {
    printf("[TEST] oot-hookshot-reticle-colour: only the Hookshot and Longshot reticles turn green on a hookshot "
           "surface (#1173)\n");
    ReticleClearKeys();
    const bool ok = ReticleCheckMatrix("OoT", kOotReticleSamples, OoT_Player_HookshotReticle_TestIsTarget);
    ReticleClearKeys();
    if (!ok) {
        return TEST_FAIL;
    }
    printf("[TEST] PASS: oot-hookshot-reticle-colour\n");
    return TEST_PASS;
}

static TestResult MmHookshotReticleColour_Run(void) {
    printf("[TEST] mm-hookshot-reticle-colour: SoH's Targetable Hookshot Reticle turns Termina's hookshot reticle "
           "green on a hookshot surface (#1173)\n");
    ReticleClearKeys();
    bool ok = ReticleCheckMatrix("MM", kMmReticleSamples, MM_Player_HookshotReticle_TestIsTarget);

    // The colours the draw sets: SoH's defaults, then the Cosmetics Editor's.
    CVarClear(kReticleTargetColorKey);
    CVarClear(kReticleNonTargetColorKey);
    ok &= ReticleCheckColor("target, default colour", MM_Player_HookshotReticle_TestColor(1), kReticleGreen);
    ok &= ReticleCheckColor("non-target, default colour", MM_Player_HookshotReticle_TestColor(0), kReticleRed);
    const Color_RGBA8 customTarget = { 0x12, 0x34, 0x56, 0xFF };
    const Color_RGBA8 customNonTarget = { 0xAB, 0xCD, 0xEF, 0xFF };
    CVarSetColor(kReticleTargetColorKey, customTarget);
    CVarSetColor(kReticleNonTargetColorKey, customNonTarget);
    ok &= ReticleCheckColor("target, Cosmetics Editor colour", MM_Player_HookshotReticle_TestColor(1), 0x123456FFu);
    ok &= ReticleCheckColor("non-target, Cosmetics Editor colour", MM_Player_HookshotReticle_TestColor(0), 0xABCDEFFFu);
    ReticleClearKeys();

    if (!ok) {
        return TEST_FAIL;
    }
    printf("[TEST] PASS: mm-hookshot-reticle-colour\n");
    return TEST_PASS;
}
