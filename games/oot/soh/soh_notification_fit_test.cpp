/**
 * @file soh_notification_fit_test.cpp
 * @brief Display-free lock: every cross-game refusal toast keeps the overlay's
 *        margin on both sides of the smallest window the ui tier renders, drawn
 *        by SoH's OWN notification overlay in the real menu font, at the default
 *        Notifications.Size (1.8) and at 1.0; and at the X-Large menu scale in a
 *        4K window.
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
 * the display to a profile, emits each toast through its PRODUCTION emitter, and
 * calls SoH's Notification::Window::Draw itself for a few frames. The toast
 * window's position and size are then read back exactly as the ui tier's TOAST
 * oracle reads them (every active "notification#" window). Draw right-aligns a
 * toast a fixed margin in from the right edge and grows it leftwards, so the
 * right margin is MEASURED (display width less the window's right edge), and the
 * toast must keep the same margin on the left: a toast that does not crowds the
 * left edge, which no SoH toast does ("Game autosaved"), and one wider still runs
 * off it, which is the defect.
 *
 * PROFILES:
 *   - 832 x 600 at FontGlobalScale 1.0: the ui tier's smallest profile
 *     (soh_ui_snapshot.cpp, "min-832x600") at SoH's default menu scale;
 *   - 3840 x 2160 at FontGlobalScale 2.0: a 4K window at SoH's X-Large menu
 *     scale, applied the way OTRGlobals::ScaleImGui applies it (style sizes
 *     scaled, FontGlobalScale set), which multiplies every toast's font.
 * Each at Notifications.Size 1.8 (the default and the playtest scale) and 1.0.
 *
 * WHAT IT COVERS, in every profile and size:
 *   - the arrival's and the MM spoiler's refusals through
 *     MM_Rando_EmitPairingRefusalToast: the RULES refusal over EVERY non-empty
 *     combination of the divergence bits (their count is DERIVED from
 *     Combo_ComboSettingsDivergenceFieldName, so a new bit is covered the day it
 *     is named), and the SPOILER refusal over every route the MM loader reports
 *     plus an unknown one;
 *   - the refused file's toasts (#836): the file select's refusal through
 *     RsbsSave_EmitFileSelectRefusalToast over every entry of the words table
 *     (Combo_RefusalWords), and the load's refusals through RsbsSave_EmitLoadToast;
 *   - that every one of those refusals opens with the class's ONE prefix,
 *     RSBS_REFUSAL_TOAST_PREFIX, and that its message is capitalized and has no
 *     trailing period (#836, operator ruling 2026-10-01);
 *   - OoT's paired-spoiler refusal through OoT_EmitPairedSpoilerRefusalToast
 *     (outside the class: "Spoiler not loaded:", width only);
 *   - that a one-field RULES refusal names its field, and that the two-field
 *     refusal mm-combo-settings-gate leg 4 depends on names both;
 *   - the creation-failure toast (PR #749) as the calibration: #749 measured it
 *     at 800 px in the ui tier's 832-px window, and this row prints its own width.
 * That the production refusal SITES call these emitters is locked where each
 * site is driven: mm-combo-settings-gate (MM options, rules, missing half),
 * mm-spoiler-identity (spoiler) and combo-creation-event (OoT spoiler) compare
 * the toast each site queued with the emitter's copy, exactly.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/Notification/Notification.h"
#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
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

#include "foreign_items.h" // Combo_ComboSettingsDivergenceDescribe / FieldName, RSBS_COMBO_DIVERGE_*
#include "notification_bridge.h"
#include "pairing_refusal_toast.h"
#include "save.h" // RsbsSave_EmitLoadToast, RsbsSave_EmitFileSelectRefusalToast

#include <cctype>
#include <utility>

extern "C" void OoT_Creation_ReportFailureAtFileSelect(int slot, int reason);

namespace {

// SoH's default font (OTRGlobals.cpp: fontStandardLarger, ImGui::GetIO().FontDefault).
constexpr float kDefaultFontPixels = 20.0f;

struct Profile {
    const char* name;
    float width;
    float height;
    // OTRGlobals.cpp imguiScaleOptionToValue: 1.0 is the default, 2.0 X-Large.
    float menuScale;
};

constexpr Profile kProfiles[] = {
    // The ui tier's smallest profile (soh_ui_snapshot.cpp, ChooseProfile: "min-832x600").
    { "832x600 at menu scale 1.0", 832.0f, 600.0f, 1.0f },
    // A 4K window at SoH's X-Large menu scale.
    { "3840x2160 at menu scale 2.0", 3840.0f, 2160.0f, 2.0f },
};

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
    const Profile* profile = nullptr;
    int checked = 0;
    float widest = 0.0f;
    std::string widestText;
};

/** Whether a toast belongs to the refusal class, whose prefix and copy shape are checked too. */
enum class Class { REFUSAL, OTHER };

/** One toast, already emitted, at the current Notifications.Size. */
int CheckOne(FitRun& run, const char* what, float scale, Class cls = Class::REFUSAL) {
    const Measured m = DrawAndMeasure(*run.overlay);
    OoT_Notification_ClearForTest();
    const Profile& p = *run.profile;
    if (m.windows != 1) {
        return Fail("%s at %.1fx, %s: %d toast window(s) drawn, expected exactly one", what, scale, p.name, m.windows);
    }
    if (cls == Class::REFUSAL) {
        // #836 (operator ruling 2026-10-01): ONE prefix for the whole class, and
        // a message that is capitalized and has no trailing period, so the Combo
        // > Save Files status cell is the toast's own line.
        if (m.prefix != RSBS_REFUSAL_TOAST_PREFIX) {
            return Fail("%s: the toast's prefix is '%s', not the refusal class's one prefix '%s' (message '%s')", what,
                        m.prefix.c_str(), RSBS_REFUSAL_TOAST_PREFIX, m.message.c_str());
        }
        if (m.message.empty() || std::islower(static_cast<unsigned char>(m.message.front())) ||
            m.message.back() == '.') {
            return Fail("%s: the message '%s' is not capitalized without a trailing period", what, m.message.c_str());
        }
    }
    if (m.message.find('\n') != std::string::npos || m.prefix.find('\n') != std::string::npos) {
        return Fail("%s: the toast carries a line break ('%s %s'); SoH's toasts are one line", what, m.prefix.c_str(),
                    m.message.c_str());
    }
    const float width = m.x1 - m.x0;
    // The overlay's right margin, measured: Draw anchors the window's right edge
    // there. A toast must keep the same margin on the left.
    const float margin = p.width - m.x1;
    if (margin <= 0.0f) {
        return Fail("%s at %.1fx, %s: the toast's right edge is at %.0f, not inside the %.0f-px window", what, scale,
                    p.name, m.x1, p.width);
    }
    if (m.x0 < margin) {
        return Fail("%s at %.1fx, %s: the toast leaves %.0f px on the left against the overlay's %.0f-px right "
                    "margin (x %.0f to %.0f, %.0f px wide, at most %.0f): '%s %s'",
                    what, scale, p.name, m.x0, margin, m.x0, m.x1, width, p.width - 2.0f * margin, m.prefix.c_str(),
                    m.message.c_str());
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

/**
 * How many RSBS_COMBO_DIVERGE_* bits exist, DERIVED from the describer's own
 * name table: the bits are allocated from bit 0 upwards, so the count is the
 * first bit Combo_ComboSettingsDivergenceFieldName does not name. -1 when a named
 * bit sits above an unnamed one (a hole the loop below would skip).
 */
int CountDivergenceBits() {
    int count = 0;
    while (count < 32 && strcmp(Combo_ComboSettingsDivergenceFieldName(1u << count), "(unknown)") != 0) {
        count++;
    }
    for (int b = count; b < 32; b++) {
        if (strcmp(Combo_ComboSettingsDivergenceFieldName(1u << b), "(unknown)") != 0) {
            return -1;
        }
    }
    return count;
}

} // namespace

extern "C" int OoT_NotificationFit_RunHeadless(const char* fontPath) {
    printf("[TEST] pairing-refusal-toast-fit: every cross-game refusal toast keeps the overlay's margin on both "
           "sides, drawn by SoH's own overlay in its default font, at Notifications.Size 1.8 and 1.0, in an 832-px "
           "window and a 4K window at the X-Large menu scale\n");

    if (fontPath == nullptr || fontPath[0] == '\0') {
        return Fail("no font path: the runner resolves games/oot/assets/custom/fonts/Montserrat-Regular.ttf (the "
                    "file soh.o2r packs as SoH's default font) under RSBS_SOURCE_DIR");
    }
    const int divergenceBits = CountDivergenceBits();
    if (divergenceBits <= 0 || divergenceBits > 30) {
        return Fail("the divergence field names are not a contiguous run from bit 0 (count %d); the RULES sweep "
                    "cannot know which bits to cover",
                    divergenceBits);
    }
    const uint32_t combinations = (1u << divergenceBits) - 1u;

    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* context = ImGui::CreateContext();
    ImGui::SetCurrentContext(context);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
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
    const ImGuiStyle baseStyle = ImGui::GetStyle();

    // The overlay's own position and scale settings, pinned for the run and
    // cleared after it: bottom right (its default, 3) is where a toast grows
    // leftwards off the window.
    CVarSetInteger(CVAR_SETTING("Notifications.Position"), 3);
    Notification::Window overlay("", true, "Notifications Window");
    OoT_Notification_ClearForTest();

    // Every toast is checked even after a failure, so a red run lists all of
    // them (at most kMaxRulesFailures of the rules combinations per pass).
    constexpr int kMaxRulesFailures = 3;
    int failures = 0;
    int checked = 0;
    for (const Profile& profile : kProfiles) {
        if (rc != 0) {
            break;
        }
        // OTRGlobals::ScaleImGui: style sizes scaled by the menu scale, and the
        // scale set as FontGlobalScale (which the overlay's SetWindowFontScale
        // multiplies).
        ImGui::GetStyle() = baseStyle;
        ImGui::GetStyle().ScaleAllSizes(profile.menuScale);
        io.FontGlobalScale = profile.menuScale;
        io.DisplaySize = ImVec2(profile.width, profile.height);
        FitRun run;
        run.overlay = &overlay;
        run.profile = &profile;
        const int failuresBefore = failures;

        for (float scale : { 1.8f, 1.0f }) {
            SetScale(scale);

            // Calibration: PR #749's creation-failure toast, measured at 800 px in
            // the ui tier's 832-px window.
            OoT_Creation_ReportFailureAtFileSelect(0, 0);
            {
                const Measured m = DrawAndMeasure(overlay);
                OoT_Notification_ClearForTest();
                printf("[TEST] calibration, %s, %.1fx: PR #749's creation-failure toast draws %.0f px wide (x %.0f "
                       "to %.0f)\n",
                       profile.name, scale, m.x1 - m.x0, m.x0, m.x1);
            }

            MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_MM_OPTIONS, nullptr);
            failures += CheckOne(run, "MM-options refusal", scale);
            MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_MISSING_HALF, nullptr);
            failures += CheckOne(run, "missing-half refusal", scale);
            for (const char* route : { RSBS_SPOILER_REFUSAL_NOT_PAIRED, RSBS_SPOILER_REFUSAL_SESSION_UNSETTLED,
                                       RSBS_SPOILER_REFUSAL_NO_IDENTITY, RSBS_SPOILER_REFUSAL_IDENTITY_INCOMPLETE,
                                       RSBS_SPOILER_REFUSAL_OTHER_SEED, RSBS_SPOILER_REFUSAL_OTHER_SETTINGS,
                                       RSBS_SPOILER_REFUSAL_OTHER_MM_OPTIONS, "anUnknownRouteTheCopyHasNoWordsFor" }) {
                MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_SPOILER, route);
                const std::string what = std::string("spoiler refusal (") + route + ")";
                failures += CheckOne(run, what.c_str(), scale);
            }
            // Outside the refusal class (it refuses a document and latches
            // nothing): width only, under its own "Spoiler not loaded:".
            OoT_EmitPairedSpoilerRefusalToast(/*mute=*/1);
            failures += CheckOne(run, "OoT paired-spoiler refusal", scale, Class::OTHER);

            // The refused FILE (#836): the file select's toast over every words
            // entry, and the load's own refusal toasts, each through its
            // production emitter.
            for (int words = 0; words < RSBS_REFUSAL_WORDS_COUNT; words++) {
                RsbsSave_EmitFileSelectRefusalToast(Combo_RefusalWords(words));
                const std::string what = std::string("file-select refusal (") + Combo_RefusalWords(words) + ")";
                failures += CheckOne(run, what.c_str(), scale);
            }
            for (const auto& [kind, name] :
                 { std::pair<int, const char*>{ RSBS_LOAD_TOAST_MM_NOT_RESTORED, "load refusal (MM profile)" },
                   { RSBS_LOAD_TOAST_REFUSED_RULES, "load refusal (rules)" },
                   { RSBS_LOAD_TOAST_REFUSED_OTHER_BUILD, "load refusal (other build)" },
                   { RSBS_LOAD_TOAST_REFUSED_DAMAGED, "load refusal (damaged)" } }) {
                RsbsSave_EmitLoadToast(kind, nullptr, 0);
                failures += CheckOne(run, name, scale);
            }

            // RULES: every non-empty combination of the divergence bits, drawn once
            // per distinct message.
            std::set<std::string> drawn;
            int rulesFailures = 0;
            for (uint32_t bits = 1; bits <= combinations && rulesFailures < kMaxRulesFailures; bits++) {
                char fields[256];
                Combo_ComboSettingsDivergenceDescribe(bits, fields, sizeof(fields));
                char message[512];
                const int named =
                    Combo_PairingRefusalToastMessage(RSBS_PAIRING_REFUSAL_RULES, fields, message, sizeof(message));
                if ((bits & (bits - 1)) == 0 && named != 1) {
                    rulesFailures +=
                        Fail("the one-field rules refusal for '%s' does not name it: '%s'", fields, message);
                    continue;
                }
                if (!drawn.insert(message).second) {
                    continue;
                }
                MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_RULES, fields);
                const std::string what = std::string("rules refusal (") + fields + ")";
                rulesFailures += CheckOne(run, what.c_str(), scale);
            }
            failures += rulesFailures;
            if (rulesFailures == 0) {
                printf("[TEST] %s, %.1fx: %zu distinct rules messages over %u field combinations (%d bits) fit\n",
                       profile.name, scale, drawn.size(), combinations, divergenceBits);
            }
        }
        checked += run.checked;
        if (failures == failuresBefore) {
            printf("[TEST] %s: widest refusal toast at 1.8x is %.0f px ('%s')\n", profile.name, run.widest,
                   run.widestText.c_str());
        }
    }
    ImGui::GetStyle() = baseStyle;
    io.FontGlobalScale = 1.0f;
    if (rc == 0 && failures != 0) {
        rc = Fail("%d refusal toast check(s) failed (listed above)", failures);
    }

    if (rc == 0) {
        // mm-combo-settings-gate leg 4 tells a moved triforce half from a matching
        // one by whether the toast names "triforceHunt" beside "goal".
        char fields[256];
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
    printf("[TEST] PASS: %d refusal toasts drawn by SoH's overlay, every one keeping the overlay's margin on both "
           "sides\n",
           checked);
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
