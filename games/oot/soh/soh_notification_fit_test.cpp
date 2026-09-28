/**
 * @file soh_notification_fit_test.cpp
 * @brief Display-free lock: every cross-game refusal toast fits the smallest
 *        window the ui tier renders, drawn by SoH's OWN notification overlay in
 *        the real menu font, at the default Notifications.Size (1.8) and at 1.0.
 *
 * CTest label "redship", row PairingRefusalToastFit in CMake/SingleExecutable.cmake,
 * dispatch "pairing-refusal-toast-fit" in src/common/test_runner.cpp.
 *
 * WHY. The "Cross-game pairing REFUSED:" toasts ran off the screen (the #749
 * playtest notes): SoH's overlay (Notification::Window::Draw,
 * soh/Notification/Notification.cpp) draws the prefix and the message on ONE line
 * and never wraps, and the MM-options refusal's sentence pair drew a toast about
 * 2,480 px wide. The copy is now one short line (src/common/pairing_refusal_toast.h);
 * this row is what keeps it one.
 *
 * HOW THE WIDTH IS DERIVED, not guessed: nothing here restates a pixel of the
 * overlay's layout. The row builds a private ImGui context whose default font is
 * the one SoH makes default (OTRGlobals.cpp: fontStandardLarger =
 * Montserrat-Regular.ttf at 20 px, loaded from the same file soh.o2r packs), sets
 * the display to the ui tier's smallest profile (832 x 600, soh_ui_snapshot.cpp
 * "min-832x600"), emits each toast through its PRODUCTION emitter, and calls SoH's
 * Notification::Window::Draw itself for a few frames. The toast window's position
 * and size are then read back exactly as the ui tier's TOAST oracle reads them
 * (every active "notification#" window) and must lie inside [0, 832]. Draw
 * right-aligns a toast 30 px in from the edge and grows it leftwards, so a toast
 * wider than 802 px starts left of 0: off-screen, which is the defect.
 *
 * WHAT IT COVERS:
 *   - the four "Not saved:" refusals through MM_Rando_EmitPairingRefusalToast,
 *     the RULES refusal over EVERY one of the 8,191 non-empty combinations of the
 *     13 divergence bits (Combo_ComboSettingsDivergenceDescribe's own field list),
 *     and the SPOILER refusal over every identity term the MM loader reports;
 *   - OoT's paired-spoiler refusal (SeedContext.cpp), with its prefix and copy;
 *   - each at Notifications.Size 1.8 (the default and the playtest scale) and 1.0;
 *   - that a one-field RULES refusal names its field, and that the two-field
 *     refusal mm-combo-settings-gate leg 4 depends on names both;
 *   - the creation-failure toast (PR #749) as the calibration: #749 measured it
 *     at 800 px in the ui tier's 832-px window, and this row prints its own width.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/Notification/Notification.h"
#include "soh/OTRGlobals.h"
#include <libultraship/libultraship.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

#include "foreign_items.h" // Combo_ComboSettingsDivergenceDescribe, RSBS_COMBO_DIVERGE_*
#include "notification_bridge.h"
#include "pairing_refusal_toast.h"

extern "C" void OoT_Creation_ReportFailureAtFileSelect(int slot, int reason);

namespace {

// The ui tier's smallest profile (soh_ui_snapshot.cpp, ChooseProfile: "min-832x600").
constexpr float kWindowWidth = 832.0f;
constexpr float kWindowHeight = 600.0f;
// SoH's default font (OTRGlobals.cpp: fontStandardLarger, ImGui::GetIO().FontDefault).
constexpr float kDefaultFontPixels = 20.0f;
// Every divergence bit Combo_ComboSettingsDivergenceDescribe names (foreign_items.h).
constexpr int kDivergenceBits = 13;

int Fail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    return 1;
}

struct Measured {
    int windows = 0;
    float x0 = 0.0f;
    float x1 = 0.0f;
    std::string prefix;
    std::string message;
};

/** Draw the queued toasts through SoH's own overlay and read the windows back. */
Measured DrawAndMeasure(Notification::Window& overlay) {
    // Three frames: an AlwaysAutoResize window measures its content on the frame
    // it first appears and Draw positions it from GetWindowSize(), so the settled
    // rectangle is the one after the first frame (the ui tier's Settle does the same).
    for (int frame = 0; frame < 3; frame++) {
        ImGui::NewFrame();
        overlay.Draw();
        ImGui::EndFrame();
    }
    Measured m;
    m.x0 = 1e9f;
    m.x1 = -1e9f;
    for (ImGuiWindow* w : GImGui->Windows) {
        if (w == nullptr || !w->Active || std::string(w->Name).rfind("notification#", 0) != 0) {
            continue;
        }
        m.windows++;
        m.x0 = std::min(m.x0, w->Pos.x);
        m.x1 = std::max(m.x1, w->Pos.x + w->Size.x);
    }
    ComboNotification last;
    memset(&last, 0, sizeof(last));
    if (OoT_Notification_PeekLastForTest(&last) == 1) {
        m.prefix = last.prefix != nullptr ? last.prefix : "";
        m.message = last.message != nullptr ? last.message : "";
    }
    return m;
}

struct FitRun {
    Notification::Window* overlay = nullptr;
    int checked = 0;
    float widest = 0.0f;
    std::string widestText;
};

/** One toast, already emitted, at the current Notifications.Size. */
int CheckOne(FitRun& run, const char* what, float scale) {
    const Measured m = DrawAndMeasure(*run.overlay);
    OoT_Notification_ClearForTest();
    if (m.windows != 1) {
        return Fail("%s at %.1fx: %d toast window(s) drawn, expected exactly one", what, scale, m.windows);
    }
    if (m.message.find('\n') != std::string::npos || m.prefix.find('\n') != std::string::npos) {
        return Fail("%s: the toast carries a line break ('%s %s'); SoH's toasts are one line", what, m.prefix.c_str(),
                    m.message.c_str());
    }
    const float width = m.x1 - m.x0;
    if (m.x0 < 0.0f || m.x1 > kWindowWidth) {
        return Fail("%s at %.1fx: the toast runs off the %d-px window (x %.0f to %.0f, %.0f px wide): '%s %s'", what,
                    scale, (int)kWindowWidth, m.x0, m.x1, width, m.prefix.c_str(), m.message.c_str());
    }
    run.checked++;
    if (scale > 1.5f && width > run.widest) {
        run.widest = width;
        run.widestText = m.prefix + " " + m.message;
    }
    return 0;
}

void SetScale(float scale) {
    CVarSetFloat(CVAR_SETTING("Notifications.Size"), scale);
}

} // namespace

extern "C" int OoT_NotificationFit_RunHeadless(void) {
    printf("[TEST] pairing-refusal-toast-fit: every cross-game refusal toast fits an %dx%d window at "
           "Notifications.Size 1.8 and 1.0, drawn by SoH's own overlay in its default font\n",
           (int)kWindowWidth, (int)kWindowHeight);

    const char* fontPath = getenv("RSBS_NOTIFICATION_FONT");
    if (fontPath == nullptr || fontPath[0] == '\0') {
        return Fail("RSBS_NOTIFICATION_FONT is not set (CMake passes games/oot/assets/custom/fonts/"
                    "Montserrat-Regular.ttf, the file soh.o2r packs as SoH's default font)");
    }

    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* context = ImGui::CreateContext();
    ImGui::SetCurrentContext(context);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(kWindowWidth, kWindowHeight);
    io.DeltaTime = 1.0f / 60.0f;
    ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, kDefaultFontPixels);
    int rc = 0;
    if (font == nullptr) {
        rc = Fail("could not load the default font from '%s'", fontPath);
    } else {
        io.FontDefault = font;
        unsigned char* pixels = nullptr;
        int texW = 0;
        int texH = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &texW, &texH); // builds the atlas; no renderer needs it
    }

    // The overlay's own position and scale settings, pinned for the run and
    // cleared after it: bottom right (its default, 3) is where a toast grows
    // leftwards off the window.
    CVarSetInteger(CVAR_SETTING("Notifications.Position"), 3);
    Notification::Window overlay("", true, "Notifications Window");
    FitRun run;
    run.overlay = &overlay;
    OoT_Notification_ClearForTest();

    for (float scale : { 1.8f, 1.0f }) {
        if (rc != 0) {
            break;
        }
        SetScale(scale);

        // Calibration: PR #749's creation-failure toast, measured at 800 px in
        // the ui tier's 832-px window.
        OoT_Creation_ReportFailureAtFileSelect(0, 0);
        {
            const Measured m = DrawAndMeasure(overlay);
            OoT_Notification_ClearForTest();
            printf(
                "[TEST] calibration at %.1fx: PR #749's creation-failure toast draws %.0f px wide (x %.0f to %.0f)\n",
                scale, m.x1 - m.x0, m.x0, m.x1);
        }

        MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_MM_OPTIONS, nullptr);
        if ((rc = CheckOne(run, "MM-options refusal", scale)) != 0) {
            break;
        }
        MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_MISSING_HALF, nullptr);
        if ((rc = CheckOne(run, "missing-half refusal", scale)) != 0) {
            break;
        }
        for (const char* term : { "sourceIsRando", "rsbsPairing", "sharedRandoSeed", "sharedRandoSettingsHash",
                                  "mmProfileDigest", "anUnknownTermTheCopyHasNoWordsFor" }) {
            MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_SPOILER, term);
            const std::string what = std::string("spoiler refusal (") + term + ")";
            if ((rc = CheckOne(run, what.c_str(), scale)) != 0) {
                break;
            }
        }
        if (rc != 0) {
            break;
        }
        {
            char message[128];
            Combo_PairingRefusalToastMessage(RSBS_PAIRING_REFUSAL_OOT_SPOILER, nullptr, message, sizeof(message));
            Notification::Emit({
                .prefix = Combo_PairingRefusalToastPrefix(RSBS_PAIRING_REFUSAL_OOT_SPOILER),
                .message = message,
                .mute = true,
            });
            if ((rc = CheckOne(run, "OoT paired-spoiler refusal", scale)) != 0) {
                break;
            }
        }

        // RULES: every non-empty combination of the divergence bits, drawn once
        // per distinct message.
        std::set<std::string> drawn;
        for (uint32_t bits = 1; bits < (1u << kDivergenceBits) && rc == 0; bits++) {
            char fields[192];
            Combo_ComboSettingsDivergenceDescribe(bits, fields, sizeof(fields));
            char message[128];
            const int named =
                Combo_PairingRefusalToastMessage(RSBS_PAIRING_REFUSAL_RULES, fields, message, sizeof(message));
            if ((bits & (bits - 1)) == 0 && named != 1) {
                rc = Fail("the one-field rules refusal for '%s' does not name it: '%s'", fields, message);
                break;
            }
            if (!drawn.insert(message).second) {
                continue;
            }
            MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_RULES, fields);
            const std::string what = std::string("rules refusal (") + fields + ")";
            rc = CheckOne(run, what.c_str(), scale);
        }
        if (rc == 0) {
            printf("[TEST] %.1fx: %zu distinct rules messages over %u field combinations fit\n", scale, drawn.size(),
                   (1u << kDivergenceBits) - 1u);
        }
    }

    if (rc == 0) {
        // mm-combo-settings-gate leg 4 tells a moved triforce half from a matching
        // one by whether the toast names "triforceHunt" beside "goal".
        char fields[192];
        Combo_ComboSettingsDivergenceDescribe(RSBS_COMBO_DIVERGE_GOAL | RSBS_COMBO_DIVERGE_TRIFORCE, fields,
                                              sizeof(fields));
        char message[128];
        if (Combo_PairingRefusalToastMessage(RSBS_PAIRING_REFUSAL_RULES, fields, message, sizeof(message)) != 2) {
            rc = Fail("the goal + triforceHunt refusal does not name both fields: '%s'", message);
        }
    }

    CVarClear(CVAR_SETTING("Notifications.Size"));
    CVarClear(CVAR_SETTING("Notifications.Position"));
    OoT_Notification_ClearForTest();
    ImGui::DestroyContext(context);
    ImGui::SetCurrentContext(previous);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] widest refusal toast at 1.8x: %.0f px in the %d-px window ('%s')\n", run.widest, (int)kWindowWidth,
           run.widestText.c_str());
    printf("[TEST] PASS: %d refusal toasts drawn by SoH's overlay, every one inside the window\n", run.checked);
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
