/**
 * @file oot_scrub_text_hint_test.cpp
 * #855 lock: the Business Scrub's offer textbox follows Scrub Hint Text
 * (RSK_SCRUB_TEXT_HINT), the setting whose own description says "Business scrubs
 * will reveal the identity of what they're selling", and not Merchant Hint Text.
 *
 * THE DEFECT. BuildScrubMessage read RSK_MERCHANT_TEXT_HINT, while the Check
 * Tracker's reveal (hook_handlers.cpp, TEXT_SCRUB_RANDOM) and its row name read
 * RSK_SCRUB_TEXT_HINT, so the textbox and the tracker disagreed whenever the two
 * settings differed: the tracker marked a scrub's check identified after a
 * textbox that never named the item, and a textbox that did name it left the
 * check unmarked.
 *
 * WHAT THIS ROW ASSERTS (ScrubTextHintSetting, rando tier: it generates for the
 * context). A scrub check is given a known item (Bombchu (5)) and the scrub's
 * English offer text is read through the real builder for all four settings:
 *   S1. Scrub Hint Text on, Merchant Hint Text off: the offer names the item.
 *   S2. Scrub Hint Text off, Merchant Hint Text on: the offer does not.
 *   S3. both on: names it. S4. both off: does not.
 *   S5. the revealed and the hidden offers differ (the item name is the only
 *       difference the builder puts between them).
 *
 * RED: with the builder reading Merchant Hint Text, S1 and S2 fail.
 */

#include <cstdint>
#include <cstdio>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

// 3drando/menu.cpp: the headless generation bridge every rando row uses.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
// Messages/MerchantMessages.cpp: the scrub offer's English text under the live settings.
extern "C" int OoT_Rando_ScrubOfferText(uint16_t rc, char* out, size_t cap);

namespace {

const char* const kStshItemWord = "Bombchu"; // RG_BOMBCHU_5's name, whichever way the box wraps it

std::string StshOffer(RandomizerCheck rc, uint8_t scrubHint, uint8_t merchantHint) {
    const auto ctx = Rando::Context::GetInstance();
    ctx->GetOption(RSK_SCRUB_TEXT_HINT).Set(scrubHint);
    ctx->GetOption(RSK_MERCHANT_TEXT_HINT).Set(merchantHint);
    char text[1024] = {};
    if (OoT_Rando_ScrubOfferText(static_cast<uint16_t>(rc), text, sizeof(text)) < 0) {
        return std::string();
    }
    return text;
}

bool StshNamesItem(const std::string& text) {
    return text.find(kStshItemWord) != std::string::npos;
}

} // namespace

extern "C" int RandoTest_ScrubTextHintSetting(void) {
    printf("[TEST] scrub-text-hint-setting: a scrub's offer names its item exactly when Scrub Hint Text is on "
           "(#855)\n");

    if (Rando_HeadlessSeedTest("RSBSSCRUBTEXT1") != 0) {
        printf("[TEST] FAIL: headless generation failed, so there is no context to read a scrub from\n");
        return 1;
    }
    const auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        printf("[TEST] FAIL: no Rando::Context after generation\n");
        return 1;
    }

    // The unrevealing branch is the Mysterious cover; Mysterious Shuffle would hide the item
    // under every setting and make S1/S3 unreachable.
    CVarClear(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"));

    const RandomizerCheck rc = RC_LW_DEKU_SCRUB_NEAR_BRIDGE;
    ctx->GetItemLocation(rc)->SetPlacedItem(RG_BOMBCHU_5);
    ctx->GetItemLocation(rc)->SetPrice(10);

    const std::string scrubOnly = StshOffer(rc, RO_GENERIC_ON, RO_GENERIC_OFF);
    const std::string merchantOnly = StshOffer(rc, RO_GENERIC_OFF, RO_GENERIC_ON);
    const std::string both = StshOffer(rc, RO_GENERIC_ON, RO_GENERIC_ON);
    const std::string neither = StshOffer(rc, RO_GENERIC_OFF, RO_GENERIC_OFF);

    int failures = 0;
    if (!StshNamesItem(scrubOnly)) {
        printf("[TEST] FAIL S1: Scrub Hint Text on, Merchant Hint Text off: the offer does not name the item "
               "(\"%s\")\n",
               scrubOnly.c_str());
        failures++;
    }
    if (StshNamesItem(merchantOnly)) {
        printf("[TEST] FAIL S2: Scrub Hint Text off, Merchant Hint Text on: the offer names the item anyway "
               "(\"%s\")\n",
               merchantOnly.c_str());
        failures++;
    }
    if (!StshNamesItem(both)) {
        printf("[TEST] FAIL S3: both on: the offer does not name the item (\"%s\")\n", both.c_str());
        failures++;
    }
    if (StshNamesItem(neither)) {
        printf("[TEST] FAIL S4: both off: the offer names the item (\"%s\")\n", neither.c_str());
        failures++;
    }
    if (both.empty() || neither.empty() || both == neither) {
        printf("[TEST] FAIL S5: the revealed and the hidden offers are not two different texts\n");
        failures++;
    }

    if (failures != 0) {
        printf("[TEST] FAIL: scrub-text-hint-setting: %d assertion(s) failed\n", failures);
        return 1;
    }
    printf("[TEST] PASS: the scrub offer names its item with Scrub Hint Text on and hides it with it off, "
           "whatever Merchant Hint Text says\n");
    return 0;
}
