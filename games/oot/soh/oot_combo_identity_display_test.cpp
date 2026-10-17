/**
 * @file oot_combo_identity_display_test.cpp
 * #952 lock over REAL generations: the five seed icons cover only OoT's seed and
 * settings, and the fingerprint the file select prints beside them is the one
 * term that tells two paired worlds apart when only an MM option differs.
 *
 * WHAT THIS ROW ASSERTS (ComboIdentityDisplayWorld, rando tier: it generates):
 *   W1. A paired generation of the pinned seed prints its own fingerprint: the
 *       generated world's text (Combo_IdentityDisplay_GeneratedText, what the
 *       name-entry screen draws for OoT's staged seed) is
 *       gComboCtx.comboSettingsHash over the build.
 *   W2. The panes print the seed STRING the world was generated from, through
 *       OoT's real provider (Rando::Context's seed string, checked against the
 *       paired seed's hash), not its hash.
 *   W3. The same seed and OoT settings with ONE MM option moved
 *       (gRando.Options.RO_SHUFFLE_COWS): the five icons
 *       (Rando::Context::hashIconIndexes, which the file select draws) are the
 *       SAME, and the printed fingerprint is NOT, and is the new world's.
 *   W4. A blank seed field: the panes print the ten digits generation made up
 *       (3drando/menu.cpp), which hash to the paired seed.
 *
 * RED: with the generated text reading OoT's settings digest instead of
 * comboSettingsHash, W1 and W3's "is not the same" fail.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/util.h"

#include "combo_identity_display.h" // src/common
#include "context.h"                // src/common — gComboCtx

// 3drando/menu.cpp: the headless generation bridge every rando row uses.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);

namespace {

const char* const kCidwSeed = "RSBSSINGLEBAG1"; // ComboSingleBag's pinned paired seed
const char* const kCidwMmOption = "gRando.Options.RO_SHUFFLE_COWS";

struct CidwWorld {
    bool generated = false;
    uint32_t seed = 0;
    uint32_t comboSettingsHash = 0;
    uint8_t icons[5] = {};
    std::string text;
    std::string pairedSeed;
};

std::string CidwIcons(const uint8_t (&icons)[5]) {
    char out[32];
    std::snprintf(out, sizeof(out), "%u %u %u %u %u", icons[0], icons[1], icons[2], icons[3], icons[4]);
    return out;
}

std::string CidwOneLine(std::string text) {
    for (char& c : text) {
        if (c == '\n') {
            c = '|';
        }
    }
    return text;
}

CidwWorld CidwGenerate(const char* seedStr) {
    CidwWorld world;
    if (Rando_HeadlessSeedTest(seedStr) != 0) {
        return world;
    }
    const auto ctx = Rando::Context::GetInstance();
    world.generated = ctx != nullptr;
    if (!world.generated) {
        return world;
    }
    world.seed = ctx->GetSeed();
    world.comboSettingsHash = gComboCtx.comboSettingsHash;
    for (int i = 0; i < 5; i++) {
        world.icons[i] = ctx->hashIconIndexes[i];
    }
    char text[RSBS_COMBO_IDENTITY_TEXT_CAP];
    Combo_IdentityDisplay_GeneratedText(world.seed, text, sizeof(text));
    world.text = text;
    char seed[256];
    Combo_IdentityDisplay_PairedSeed(gComboCtx.sharedRandoSeed, seed, sizeof(seed));
    world.pairedSeed = seed;
    printf("[TEST] generated '%s': seed %u, icons %s, comboSettingsHash %08X, text '%s', paired seed '%s'\n", seedStr,
           (unsigned)world.seed, CidwIcons(world.icons).c_str(), (unsigned)world.comboSettingsHash,
           CidwOneLine(world.text).c_str(), world.pairedSeed.c_str());
    return world;
}

std::string CidwExpected(uint32_t hash) {
    char text[RSBS_COMBO_IDENTITY_TEXT_CAP];
    Combo_IdentityDisplay_Format(hash, text, sizeof(text));
    return text;
}

} // namespace

extern "C" int RandoTest_ComboIdentityDisplayWorld(void) {
    printf("[TEST] combo-identity-display-world: the icons cover OoT's seed and settings; the fingerprint beside "
           "them moves with an MM option (#952)\n");
    int failures = 0;
    CVarClear(kCidwMmOption);

    // ---- W1, W2: the pinned paired world ---------------------------------------
    const CidwWorld first = CidwGenerate(kCidwSeed);
    if (!first.generated) {
        printf("[TEST] FAIL: the pinned paired world did not generate\n");
        return 1;
    }
    if (first.comboSettingsHash == 0 || first.text.empty() || first.text != CidwExpected(first.comboSettingsHash)) {
        printf("[TEST] FAIL W1: the generated world printed '%s', not its comboSettingsHash %08X\n",
               CidwOneLine(first.text).c_str(), (unsigned)first.comboSettingsHash);
        failures++;
    }
    if (first.pairedSeed != kCidwSeed) {
        printf("[TEST] FAIL W2: the panes print the paired seed as '%s', not the seed string '%s'\n",
               first.pairedSeed.c_str(), kCidwSeed);
        failures++;
    }

    // ---- W3: one MM option moves the world, not the icons ----------------------
    const int32_t cows = CVarGetInteger(kCidwMmOption, 0);
    CVarSetInteger(kCidwMmOption, cows != 0 ? 0 : 1);
    const CidwWorld second = CidwGenerate(kCidwSeed);
    CVarClear(kCidwMmOption);
    if (!second.generated) {
        printf("[TEST] FAIL: the world with %s moved did not generate\n", kCidwMmOption);
        return 1;
    }
    if (second.seed != first.seed) {
        printf("[TEST] FAIL W3: the same seed string staged two seeds (%u, %u)\n", (unsigned)first.seed,
               (unsigned)second.seed);
        failures++;
    }
    if (std::memcmp(first.icons, second.icons, sizeof(first.icons)) != 0) {
        printf("[TEST] FAIL W3: the icons moved with an MM option (%s -> %s): the premise that they cover only "
               "OoT's seed and settings no longer holds\n",
               CidwIcons(first.icons).c_str(), CidwIcons(second.icons).c_str());
        failures++;
    }
    if (second.comboSettingsHash == first.comboSettingsHash) {
        printf("[TEST] FAIL W3: %s did not move comboSettingsHash (%08X)\n", kCidwMmOption,
               (unsigned)first.comboSettingsHash);
        failures++;
    }
    if (second.text == first.text) {
        printf("[TEST] FAIL W3: two different paired worlds print the same icons AND the same text beside them "
               "('%s')\n",
               CidwOneLine(second.text).c_str());
        failures++;
    }
    if (second.text != CidwExpected(second.comboSettingsHash)) {
        printf("[TEST] FAIL W3: the second world printed '%s', not its comboSettingsHash %08X\n",
               CidwOneLine(second.text).c_str(), (unsigned)second.comboSettingsHash);
        failures++;
    }

    // ---- W4: a blank seed field prints the ten digits generation made ----------
    const CidwWorld blank = CidwGenerate("");
    if (!blank.generated) {
        printf("[TEST] FAIL: the blank-seed world did not generate\n");
        return 1;
    }
    bool tenDigits = blank.pairedSeed.size() == 10;
    for (char c : blank.pairedSeed) {
        tenDigits = tenDigits && c >= '0' && c <= '9';
    }
    const auto ctx = Rando::Context::GetInstance();
    if (!tenDigits || blank.pairedSeed != ctx->GetSeedString() ||
        SohUtils::Hash(blank.pairedSeed) != gComboCtx.sharedRandoSeed) {
        printf("[TEST] FAIL W4: a blank seed field's world prints its paired seed as '%s' (context string '%s', "
               "paired seed %u)\n",
               blank.pairedSeed.c_str(), ctx->GetSeedString().c_str(), (unsigned)gComboCtx.sharedRandoSeed);
        failures++;
    }

    if (failures != 0) {
        printf("[TEST] FAIL: combo-identity-display-world: %d assertion(s) failed\n", failures);
        return 1;
    }
    printf("[TEST] PASS: same icons %s for two paired worlds, told apart by '%s' and '%s'; blank seed prints '%s'\n",
           CidwIcons(first.icons).c_str(), CidwOneLine(first.text).c_str(), CidwOneLine(second.text).c_str(),
           blank.pairedSeed.c_str());
    return 0;
}
