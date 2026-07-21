
#include "Notification.h"
#include <libultraship/libultraship.h>
#include "soh/OTRGlobals.h"

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
}

namespace Notification {

static uint32_t nextId = 0;
static std::vector<Options> notifications = {};

void Window::Draw() {
    auto vp = ImGui::GetMainViewport();

    const float margin = 30.0f;
    const float padding = 10.0f;

    int position = CVarGetInteger(CVAR_SETTING("Notifications.Position"), 3);

    // Top Left
    ImVec2 basePosition;
    switch (position) {
        case 0: // Top Left
            basePosition = ImVec2(vp->Pos.x + margin, vp->Pos.y + margin);
            break;
        case 1: // Top Right
            basePosition = ImVec2(vp->Pos.x + vp->Size.x - margin, vp->Pos.y + margin);
            break;
        case 2: // Bottom Left
            basePosition = ImVec2(vp->Pos.x + margin, vp->Pos.y + vp->Size.y - margin);
            break;
        case 3: // Bottom Right
            basePosition = ImVec2(vp->Pos.x + vp->Size.x - margin, vp->Pos.y + vp->Size.y - margin);
            break;
        case 4: // Hidden
            return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(0, 0, 0, CVarGetFloat(CVAR_SETTING("Notifications.BgOpacity"), 0.5f)));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

    for (int index = 0; index < notifications.size(); ++index) {
        auto& notification = notifications[index];
        int inverseIndex = -ABS(index - (notifications.size() - 1));

        ImGui::SetNextWindowViewport(vp->ID);
        if (notification.remainingTime < 4.0f) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, (notification.remainingTime - 1) / 3.0f);
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
        }

        ImGui::Begin(("notification#" + std::to_string(notification.id)).c_str(), nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

        ImGui::SetWindowFontScale(CVarGetFloat(CVAR_SETTING("Notifications.Size"), 1.8f)); // Make this adjustable

        ImVec2 notificationPos;
        switch (position) {
            case 0: // Top Left
                notificationPos =
                    ImVec2(basePosition.x, basePosition.y + ((ImGui::GetWindowSize().y + padding) * inverseIndex));
                break;
            case 1: // Top Right
                notificationPos = ImVec2(basePosition.x - ImGui::GetWindowSize().x,
                                         basePosition.y + ((ImGui::GetWindowSize().y + padding) * inverseIndex));
                break;
            case 2: // Bottom Left
                notificationPos = ImVec2(basePosition.x,
                                         basePosition.y - ((ImGui::GetWindowSize().y + padding) * (inverseIndex + 1)));
                break;
            case 3: // Bottom Right
                notificationPos = ImVec2(basePosition.x - ImGui::GetWindowSize().x,
                                         basePosition.y - ((ImGui::GetWindowSize().y + padding) * (inverseIndex + 1)));
                break;
        }

        ImGui::SetWindowPos(notificationPos);

        if (notification.itemIcon != nullptr) {
            ImGui::Image(Ship::Context::GetInstance()->GetWindow()->GetGui()->GetTextureByName(notification.itemIcon),
                         ImVec2(24, 24));
            ImGui::SameLine();
        }
        if (!notification.prefix.empty()) {
            ImGui::TextColored(notification.prefixColor, "%s", notification.prefix.c_str());
            ImGui::SameLine();
        }
        ImGui::TextColored(notification.messageColor, "%s", notification.message.c_str());
        if (!notification.suffix.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(notification.suffixColor, "%s", notification.suffix.c_str());
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void Window::UpdateElement() {
    for (int index = 0; index < notifications.size(); ++index) {
        auto& notification = notifications[index];

        // decrement remainingTime
        notification.remainingTime -= ImGui::GetIO().DeltaTime;

        // remove notification if it has expired
        if (notification.remainingTime <= 0) {
            notifications.erase(notifications.begin() + index);
            --index;
        }
    }
}

void Emit(Options notification) {
    notification.id = nextId++;
    if (notification.remainingTime == 0.0f) {
        notification.remainingTime = CVarGetFloat(CVAR_SETTING("Notifications.Duration"), 10.0f);
    }
    notifications.push_back(notification);
    if (!notification.mute && !CVarGetInteger(CVAR_SETTING("Notifications.Mute"), 0)) {
        Audio_PlaySoundGeneral(NA_SE_SY_METRONOME, &OoT_gSfxDefaultPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                               &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
    }
}

} // namespace Notification

// ---------------------------------------------------------------------------
// Cross-game layout probe for the deliberate MM->OoT Notification::Emit bind
// (#427 item 1). MM's 2s2h/BenGui/Notification.cpp is excluded from single-exe
// builds, so MM's Rando pickup toast (2s2h/Rando/MiscBehavior/CheckQueue.cpp)
// binds THIS Emit against MM's own view of Notification::Options. The bind is
// safe only while the two views stay field-identical; the mm-notification-
// binding CTest compares this fingerprint against MM's and fails on drift.
//
// Reported with pointer arithmetic on a real instance rather than offsetof so
// it is well-defined without regard to standard-layout status, and no
// platform-specific byte offsets are hardcoded. The field-type static_asserts
// below turn a same-view retype (the half a cross-game offset compare cannot
// see if BOTH games retype in lockstep) into an OoT build break.
#include "notification_layout_probe.h"
#include <cstddef>
#include <type_traits>

static_assert(std::is_same_v<decltype(Notification::Options::id), uint32_t>,
              "Notification::Options::id retyped — the MM Emit bind assumes this layout (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::itemIcon), const char*>,
              "Notification::Options::itemIcon retyped — the MM Emit bind assumes this layout (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::prefix), std::string>,
              "Notification::Options::prefix retyped — the MM Emit bind assumes this layout (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::prefixColor), ImVec4>,
              "Notification::Options::prefixColor retyped — the MM Emit bind assumes this layout (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::message), std::string>,
              "Notification::Options::message retyped — the MM Emit bind assumes this layout (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::messageColor), ImVec4>,
              "Notification::Options::messageColor retyped — the MM Emit bind assumes this layout (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::suffix), std::string>,
              "Notification::Options::suffix retyped — the MM Emit bind assumes this layout (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::suffixColor), ImVec4>,
              "Notification::Options::suffixColor retyped — the MM Emit bind assumes this layout (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::remainingTime), float>,
              "Notification::Options::remainingTime retyped — the MM Emit bind assumes this layout (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::mute), bool>,
              "Notification::Options::mute retyped — the MM Emit bind assumes this layout (#427)");

extern "C" void OoT_NotificationOptionsLayout(NotificationOptionsLayout* out) {
    Notification::Options o;
    const char* base = reinterpret_cast<const char*>(&o);
    out->structSize = static_cast<uint32_t>(sizeof(Notification::Options));
    out->offId = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.id) - base);
    out->offItemIcon = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.itemIcon) - base);
    out->offPrefix = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.prefix) - base);
    out->offPrefixColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.prefixColor) - base);
    out->offMessage = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.message) - base);
    out->offMessageColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.messageColor) - base);
    out->offSuffix = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.suffix) - base);
    out->offSuffixColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.suffixColor) - base);
    out->offRemainingTime = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.remainingTime) - base);
    out->offMute = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.mute) - base);
}
