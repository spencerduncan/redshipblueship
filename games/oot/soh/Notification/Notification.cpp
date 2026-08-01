
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
// OoT's half of the cross-game notification bridge (#427 item 1).
//
// MM's 2s2h/BenGui/Notification.cpp is excluded from single-exe builds, so MM
// has no Emit of its own and its toasts have to reach this one. They used to do
// that by ABI coincidence — `Notification::Emit(Notification::Options)` mangles
// identically in both ports, so MM's call bound THIS body and handed it MM's
// own view of Options. Nothing but the two structs happening to stay
// field-identical kept that safe, and no link error can ever catch a
// divergence (exactly one Emit definition survives).
//
// The bridge replaces the coincidence with a declared interface: MM packs its
// Options into the plain-C ComboNotification (src/common/notification_bridge.h)
// and calls here; this side unpacks into ITS OWN Options by field name. Neither
// port's layout is load-bearing for the other anymore, and a rename/removal/
// retype of a mapped field is a compile error in whichever port drifted — see
// the shared contract asserts below, which MM's side compiles too.
//
// The bridge does NOT retire the layout requirement, only the reason for it:
// both trees still declare `Notification::Options`, so the implicit
// constructor/destructor MM's TUs emit for their own view COMDAT-fold with
// these, and the linker keeps one. OoT_NotificationOptionsLayout below reports
// this port's view for the runtime equality half of the lock (see
// src/common/notification_layout_probe.h).
#include "notification_bridge.h"
#include "notification_layout_probe.h"
#include <new>

COMBO_NOTIFICATION_ASSERT_OPTIONS_CONTRACT(Notification::Options);

extern "C" void OoT_NotificationOptionsLayout(NotificationOptionsLayout* out) {
    // Slack buffer + placement new, never destroyed — see the twin in
    // games/mm/2s2h/mm_notification_binding_test.cpp: once the two views have
    // diverged, the folded implicit constructor may be the other port's and may
    // write past this port's sizeof, and a plain local would fail-fast before
    // the fingerprint could be reported.
    alignas(alignof(Notification::Options)) static unsigned char storage[sizeof(Notification::Options) * 4] = {};
    const Notification::Options* o = new (storage) Notification::Options();

    const char* base = reinterpret_cast<const char*>(o);
    out->structSize = static_cast<uint32_t>(sizeof(Notification::Options));
    out->offId = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->id) - base);
    out->offItemIcon = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->itemIcon) - base);
    out->offPrefix = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->prefix) - base);
    out->offPrefixColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->prefixColor) - base);
    out->offMessage = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->message) - base);
    out->offMessageColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->messageColor) - base);
    out->offSuffix = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->suffix) - base);
    out->offSuffixColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->suffixColor) - base);
    out->offRemainingTime = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->remainingTime) - base);
    out->offMute = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->mute) - base);
}

extern "C" void OoT_Notification_Emit(const ComboNotification* notification) {
    if (notification == nullptr) {
        return;
    }

    Notification::Options options;
    options.itemIcon = notification->itemIcon;
    options.prefix = notification->prefix != nullptr ? notification->prefix : "";
    options.prefixColor = ImVec4(notification->prefixColor[0], notification->prefixColor[1],
                                 notification->prefixColor[2], notification->prefixColor[3]);
    options.message = notification->message != nullptr ? notification->message : "";
    options.messageColor = ImVec4(notification->messageColor[0], notification->messageColor[1],
                                  notification->messageColor[2], notification->messageColor[3]);
    options.suffix = notification->suffix != nullptr ? notification->suffix : "";
    options.suffixColor = ImVec4(notification->suffixColor[0], notification->suffixColor[1],
                                 notification->suffixColor[2], notification->suffixColor[3]);
    options.remainingTime = notification->remainingTime;
    options.mute = notification->mute != 0;

    // id is assigned by Emit, which is why the wire form carries none.
    Notification::Emit(options);
}

extern "C" int OoT_Notification_PeekLastForTest(ComboNotification* out) {
    if (out == nullptr || Notification::notifications.empty()) {
        return 0;
    }

    const Notification::Options& last = Notification::notifications.back();
    out->itemIcon = last.itemIcon;
    out->prefix = last.prefix.c_str();
    out->prefixColor[0] = last.prefixColor.x;
    out->prefixColor[1] = last.prefixColor.y;
    out->prefixColor[2] = last.prefixColor.z;
    out->prefixColor[3] = last.prefixColor.w;
    out->message = last.message.c_str();
    out->messageColor[0] = last.messageColor.x;
    out->messageColor[1] = last.messageColor.y;
    out->messageColor[2] = last.messageColor.z;
    out->messageColor[3] = last.messageColor.w;
    out->suffix = last.suffix.c_str();
    out->suffixColor[0] = last.suffixColor.x;
    out->suffixColor[1] = last.suffixColor.y;
    out->suffixColor[2] = last.suffixColor.z;
    out->suffixColor[3] = last.suffixColor.w;
    out->remainingTime = last.remainingTime;
    out->mute = last.mute ? 1 : 0;
    return 1;
}
