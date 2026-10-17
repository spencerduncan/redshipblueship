/**
 * @file test_combo_identity_display.c
 * @brief ROM-free lock for what the file select prints beside the seed icons and
 *        what the panes print as the paired seed (#952,
 *        src/common/combo_identity_display.cpp).
 *
 * CTest row ComboIdentityDisplay ("combo-identity-display", label "redship").
 * Display-free; its .redsave writes go to this process's own scratch directory.
 * The wrapper (Test_ComboIdentityDisplay, test_runner.cpp) brings up the CVar
 * store first: MM's profile stamp and the MM option read it.
 *
 * WHAT IT ASSERTS:
 *   A. The build the file select prints is the window title's (#880): the part
 *      of RsbsVersion_Display() after "<release>+", or the release for a build
 *      without git, and the title carries it.
 *   B. The text is the fingerprint as 8 hex digits over the build; 0 (no frozen
 *      pair) prints nothing.
 *   C. A GENERATED world: after the real creation freeze (MM's real profile
 *      stamp, Combo_FreezeComboSettings), the text's fingerprint IS
 *      gComboCtx.comboSettingsHash. An identity whose seed is not the staged
 *      world's, and an unpaired session, print nothing.
 *   D. ONE MM OPTION MOVES (gRando.Options.RO_SHUFFLE_COWS, through MM's real
 *      profile stamp and the real refreeze): the text moves and is the new
 *      comboSettingsHash, while sharedRandoSeed and sharedRandoSettingsHash, the
 *      only identity terms OoT's icons are a function of, do not. (The icons
 *      themselves are compared over two real generations by the rando-tier
 *      ComboIdentityDisplayWorld row.)
 *   E. A FILE: the text for a slot is the fingerprint its .redsave record froze,
 *      read back from disk by the real ReadMeta; a slot with no record prints
 *      nothing; one read serves every frame until SlotStateEpoch moves, and the
 *      rewrite after D's option change is read as the new fingerprint.
 *   F. The paired seed the panes print: the provider's string when it has one
 *      for this world, the number when it has none or declines.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++).
 */

#include <cstdio>
#include <cstring>
#include <string>

#include "../combo_identity_display.h"
#include "../combo_mm_options_view.h" // MM_Rando_ComputeProfileStamp: the creation freeze's MM half
#include "../context.h"
#include "../foreign_items.h"
#include "../rsbs_version.h"
#include "../save.h"
#include "../test_scratch.h"

namespace {

#define CID_ASSERT(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            failures++;                               \
        }                                             \
    } while (0)

constexpr uint32_t kCidSeed = 0x0952C0DEu;
constexpr uint32_t kCidSettingsHash = 0x5E770952u;
const char* const kCidMmOption = "gRando.Options.RO_SHUFFLE_COWS";

std::string CidExpected(uint32_t hash) {
    char text[RSBS_COMBO_IDENTITY_TEXT_CAP];
    std::snprintf(text, sizeof(text), "%08X\n%s", (unsigned)hash, Combo_IdentityDisplay_Build());
    return text;
}

std::string CidGenerated(uint32_t stagedSeed) {
    char text[RSBS_COMBO_IDENTITY_TEXT_CAP];
    Combo_IdentityDisplay_GeneratedText(stagedSeed, text, sizeof(text));
    return text;
}

std::string CidSlot(int slot) {
    char text[RSBS_COMBO_IDENTITY_TEXT_CAP];
    Combo_IdentityDisplay_SlotText(slot, text, sizeof(text));
    return text;
}

/** The creation freeze's order (playthrough.cpp): seed and OoT settings digest,
 *  MM's profile digest from the live MM options, then the combo record, whose
 *  freeze stamps the fingerprint over all three. */
void CidFreezePairedIdentity() {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kCidSeed;
    gComboCtx.sharedRandoSettingsHash = kCidSettingsHash;
    gComboCtx.mmProfileDigest = MM_Rando_ComputeProfileStamp();
    Combo_FreezeComboSettings(nullptr);
}

const char* const kCidProviderSeed = "RSBS952SEED";
int CidProvider(uint32_t sharedRandoSeed, char* out, size_t cap) {
    if (sharedRandoSeed != kCidSeed) {
        return 0;
    }
    std::snprintf(out, cap, "%s", kCidProviderSeed);
    return 1;
}

} // namespace

TestResult ComboIdentityDisplay_RunHeadless(void) {
    printf("[TEST] combo-identity-display: the fingerprint and build beside the seed icons, and the paired seed "
           "string (#952)\n");
    int failures = 0;

    // ---- A. the build is the window title's -----------------------------------
    {
        const std::string display = RsbsVersion_Display();
        const std::string release = RSBS_VERSION_STRING;
        const std::string build = Combo_IdentityDisplay_Build();
        const std::string expected =
            (display.size() > release.size() + 1 && display.compare(0, release.size(), release) == 0 &&
             display[release.size()] == '+')
                ? display.substr(release.size() + 1)
                : display;
        printf("[TEST] A: display '%s' -> build '%s'\n", display.c_str(), build.c_str());
        CID_ASSERT(!build.empty() && build == expected, "A: build '%s' is not the display's commit '%s'", build.c_str(),
                   expected.c_str());
        CID_ASSERT(std::string(RsbsVersion_WindowTitle()).find(build) != std::string::npos,
                   "A: the window title '%s' does not carry the build '%s'", RsbsVersion_WindowTitle(), build.c_str());
    }

    // ---- B. the text's shape ------------------------------------------------
    {
        char text[RSBS_COMBO_IDENTITY_TEXT_CAP];
        std::memset(text, 'x', sizeof(text));
        CID_ASSERT(Combo_IdentityDisplay_Format(0, text, sizeof(text)) == 0 && text[0] == '\0',
                   "B: an unfrozen fingerprint (0) printed '%s'", text);
        CID_ASSERT(Combo_IdentityDisplay_Format(0x00ABCDEFu, text, sizeof(text)) == 1 &&
                       CidExpected(0x00ABCDEFu) == text && std::strncmp(text, "00ABCDEF\n", 9) == 0,
                   "B: 0x00ABCDEF printed '%s'", text);
    }

    // ---- C. a generated world prints its own fingerprint ----------------------
    CVarClear(kCidMmOption);
    ComboContext_Init();
    CID_ASSERT(CidGenerated(0).empty(), "C: an unpaired session printed '%s'", CidGenerated(0).c_str());
    CidFreezePairedIdentity();
    const uint32_t hashBefore = gComboCtx.comboSettingsHash;
    const std::string textBefore = CidGenerated(kCidSeed);
    printf("[TEST] C: comboSettingsHash %08X (settingsHash %08X, mmProfile %08X) -> '%s'\n", (unsigned)hashBefore,
           (unsigned)gComboCtx.sharedRandoSettingsHash, (unsigned)gComboCtx.mmProfileDigest, textBefore.c_str());
    CID_ASSERT(hashBefore != 0, "C: the freeze stamped no fingerprint");
    CID_ASSERT(textBefore == CidExpected(hashBefore),
               "C: the generated world's text '%s' is not its comboSettingsHash %08X", textBefore.c_str(),
               (unsigned)hashBefore);
    CID_ASSERT(CidGenerated(kCidSeed + 1u).empty(), "C: an identity for another staged seed printed '%s'",
               CidGenerated(kCidSeed + 1u).c_str());

    // ---- E (first half). the file the creation writes prints the same text ----
    Context_InitFrozenStates(); // the shadows a commit stages (both halves, blank)
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(rsbs::test::ScratchDir("rsbs_test_combo_identity_display").string());
    mgr.DeleteSave(0);
    mgr.DeleteSave(1);
    CID_ASSERT(mgr.Save(0), "E: Save(0) failed");
    const int readsAfterSave = Combo_IdentityDisplay_SlotReadCountForTest();
    const std::string slotBefore = CidSlot(0);
    for (int frame = 0; frame < 5; frame++) {
        CidSlot(0);
        CidSlot(1);
    }
    printf("[TEST] E: slot 0 -> '%s', slot 1 -> '%s'\n", slotBefore.c_str(), CidSlot(1).c_str());
    CID_ASSERT(slotBefore == CidExpected(hashBefore), "E: file 1's text '%s' is not its record's fingerprint %08X",
               slotBefore.c_str(), (unsigned)hashBefore);
    CID_ASSERT(CidSlot(1).empty(), "E: a slot with no record printed '%s'", CidSlot(1).c_str());
    CID_ASSERT(Combo_IdentityDisplay_SlotReadCountForTest() == readsAfterSave + 1,
               "E: eleven frames read the disk %d times, not once",
               Combo_IdentityDisplay_SlotReadCountForTest() - readsAfterSave);

    // ---- D. one MM option moves the fingerprint, not OoT's icon inputs --------
    const int32_t cows = CVarGetInteger(kCidMmOption, 0);
    CVarSetInteger(kCidMmOption, cows != 0 ? 0 : 1);
    const uint32_t seedBefore = gComboCtx.sharedRandoSeed;
    const uint32_t settingsBefore = gComboCtx.sharedRandoSettingsHash;
    const uint32_t profileBefore = gComboCtx.mmProfileDigest;
    CidFreezePairedIdentity();
    const uint32_t hashAfter = gComboCtx.comboSettingsHash;
    const std::string textAfter = CidGenerated(kCidSeed);
    printf("[TEST] D: %s %d -> %d: mmProfile %08X -> %08X, comboSettingsHash %08X -> %08X, text '%s' -> '%s'\n",
           kCidMmOption, cows, cows != 0 ? 0 : 1, (unsigned)profileBefore, (unsigned)gComboCtx.mmProfileDigest,
           (unsigned)hashBefore, (unsigned)hashAfter, textBefore.c_str(), textAfter.c_str());
    CID_ASSERT(gComboCtx.mmProfileDigest != profileBefore, "D: the MM option did not move MM's profile digest");
    CID_ASSERT(textAfter != textBefore, "D: an MM option moved the world but not the text beside the icons ('%s')",
               textAfter.c_str());
    CID_ASSERT(textAfter == CidExpected(hashAfter), "D: the text '%s' is not the new comboSettingsHash %08X",
               textAfter.c_str(), (unsigned)hashAfter);
    CID_ASSERT(gComboCtx.sharedRandoSeed == seedBefore && gComboCtx.sharedRandoSettingsHash == settingsBefore,
               "D: the MM option moved OoT's icon inputs (seed %08X -> %08X, settingsHash %08X -> %08X)",
               (unsigned)seedBefore, (unsigned)gComboCtx.sharedRandoSeed, (unsigned)settingsBefore,
               (unsigned)gComboCtx.sharedRandoSettingsHash);

    // ---- E (second half). a rewrite is read once, as the new fingerprint -------
    mgr.DeleteSave(0);
    CID_ASSERT(mgr.Save(0), "E: the second Save(0) failed");
    const std::string slotAfter = CidSlot(0);
    CID_ASSERT(slotAfter == CidExpected(hashAfter), "E: the rewritten file's text '%s' is not %08X", slotAfter.c_str(),
               (unsigned)hashAfter);
    mgr.DeleteSave(0);
    CID_ASSERT(CidSlot(0).empty(), "E: an erased slot still printed '%s'", CidSlot(0).c_str());
    CVarClear(kCidMmOption);

    // ---- F. the paired seed the panes print ----------------------------------
    {
        char seed[256];
        const ComboSeedStringProvider real = Combo_IdentityDisplay_SetSeedStringProvider(nullptr);
        Combo_IdentityDisplay_PairedSeed(kCidSeed, seed, sizeof(seed));
        CID_ASSERT(std::to_string(kCidSeed) == seed, "F: with no provider the seed printed '%s', not the number", seed);
        Combo_IdentityDisplay_SetSeedStringProvider(&CidProvider);
        Combo_IdentityDisplay_PairedSeed(kCidSeed, seed, sizeof(seed));
        CID_ASSERT(std::string(kCidProviderSeed) == seed, "F: the provider's seed string printed as '%s'", seed);
        Combo_IdentityDisplay_PairedSeed(kCidSeed + 1u, seed, sizeof(seed));
        CID_ASSERT(std::to_string(kCidSeed + 1u) == seed,
                   "F: a seed the provider has no string for printed '%s', not the number", seed);
        Combo_IdentityDisplay_SetSeedStringProvider(real);
        printf("[TEST] F: the OoT provider is %s\n", real != nullptr ? "registered" : "MISSING");
        CID_ASSERT(real != nullptr, "F: OoT registered no seed-string provider");
    }

    ComboContext_Init();
    if (failures != 0) {
        printf("[TEST] FAIL: combo-identity-display: %d assertion(s) failed\n", failures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: combo-identity-display\n");
    return TEST_PASS;
}
