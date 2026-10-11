
#include "Notification.h"
#include <libultraship/libultraship.h>
#include "soh/OTRGlobals.h"
#include "context.h" // src/common: Context_GetCurrentGame (the [TOAST] line, #878)

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
}

namespace Notification {

static uint32_t nextId = 0;
static std::vector<Options> notifications = {};
// Test-only record of every toast queued while recording is on, as
// "<prefix> <message> <suffix>", the empty fields and their gaps left out
// (OoT_Notification_EmittedAtForTest). OFF unless a test
// row turns it on (OoT_Notification_RecordForTest), so a shipping process keeps
// nothing and Emit behaves exactly as upstream.
static bool recordForTest = false;
static std::vector<std::string> emittedForTest = {};

#ifdef RSBS_SINGLE_EXECUTABLE
// #906: the width a toast lays out at the current window's font scale, the way
// Draw lays it out: the icon, the prefix, the message and the suffix on one line,
// SameLine's item spacing between them, and the window padding on both sides of
// an AlwaysAutoResize window.
static float ToastWidth(const Options& notification) {
    const ImGuiStyle& style = ImGui::GetStyle();
    float width = 2.0f * style.WindowPadding.x;
    bool first = true;
    auto item = [&](float itemWidth) {
        width += (first ? 0.0f : style.ItemSpacing.x) + itemWidth;
        first = false;
    };
    if (notification.itemIcon != nullptr) {
        item(24.0f);
    }
    if (!notification.prefix.empty()) {
        item(ImGui::CalcTextSize(notification.prefix.c_str()).x);
    }
    item(ImGui::CalcTextSize(notification.message.c_str()).x);
    if (!notification.suffix.empty()) {
        item(ImGui::CalcTextSize(notification.suffix.c_str()).x);
    }
    return width;
}

// #906: a toast wider than the window it is drawn in (libultraship's default
// 640 x 480 window opens 580 px between the overlay's margins, and the refusal,
// load-refusal and pickup toasts reach 757 px at the default size 1.8) is drawn
// at the largest font scale, below the player's @p size and no smaller than 1.0
// (the menus' own text size), at which it keeps the margin on both sides. The
// words stay whole; a toast that fits is drawn exactly as before.
static void FitToastToWindow(const Options& notification, float size, float available) {
    if (available <= 0.0f) {
        return;
    }
    const float wide = ToastWidth(notification);
    if (wide <= available) {
        return;
    }
    const float smallest = size < 1.0f ? size : 1.0f;
    // The text scales with the font and the padding does not, so this first
    // guess is never too small; step down from it to the first that fits.
    float scale = size * available / wide;
    for (;; scale -= 0.01f) {
        if (scale <= smallest) {
            scale = smallest;
        }
        ImGui::SetWindowFontScale(scale);
        if (scale <= smallest || ToastWidth(notification) <= available) {
            return;
        }
    }
}
#endif

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
#ifdef RSBS_SINGLE_EXECUTABLE
        FitToastToWindow(notification, CVarGetFloat(CVAR_SETTING("Notifications.Size"), 1.8f),
                         vp->Size.x - 2.0f * margin);
#endif

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
    if (recordForTest) {
        // The suffix is recorded too (#878): an item toast keeps its verb in
        // `.message` and the item in `.suffix`, so without it two different
        // items' toasts would read as one text twice.
        std::string line;
        for (const std::string* field : { &notification.prefix, &notification.message, &notification.suffix }) {
            if (field->empty()) {
                continue;
            }
            if (!line.empty() && line.back() != ' ') {
                line += ' ';
            }
            line += *field;
        }
        emittedForTest.push_back(line);
        // Every recorded toast also lands in stderr, in order, with the game
        // that was running, so a run's log reads as the list of what the
        // overlay was asked to show (#878's reproduction).
        const GameId game = Context_GetCurrentGame();
        fprintf(stderr, "[TOAST] %d (%s): \"%s\"\n", (int)emittedForTest.size(),
                game == GAME_MM ? "MM" : (game == GAME_OOT ? "OoT" : "none"), line.c_str());
        fflush(stderr);
    }
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

extern "C" void OoT_Notification_EmitDefault(const char* prefix, const char* message, int mute) {
    Notification::Options options;
    options.prefix = prefix != nullptr ? prefix : "";
    options.message = message != nullptr ? message : "";
    options.mute = mute != 0;
    // remainingTime 0 is "the player's configured duration", which Emit reads
    // from the CVar store. A display-free row may have none, and the bridge
    // dereferences the store unconditionally, so fall back to that setting's
    // own default there.
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        options.remainingTime = 10.0f;
    }
    Notification::Emit(options);
}

// #881: whether a toast in SoH's own shape (OoT_Notification_EmitDefault's: no
// icon, no suffix) keeps Window::Draw's 30-px margin on both sides of the window
// it is drawn in, measured the way Draw lays it out: the default font at the
// player's Notifications.Size (SetWindowFontScale), the prefix, SameLine's item
// spacing, the message, and the window padding on both sides of an
// AlwaysAutoResize window. -1 when there is nothing to measure against (no ImGui
// context, no built font, no display size: the display-free rows); the caller
// keeps its character budget then. The pixels are locked by
// pairing-refusal-toast-fit, which draws the toast through Draw itself.
extern "C" int OoT_Notification_FitsWindow(const char* prefix, const char* message) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return -1;
    }
    const ImGuiIO& io = ImGui::GetIO();
    ImFont* font = io.FontDefault;
    if (font == nullptr && io.Fonts != nullptr && !io.Fonts->Fonts.empty()) {
        font = io.Fonts->Fonts[0];
    }
    if (font == nullptr || io.Fonts == nullptr || !io.Fonts->IsBuilt() || io.DisplaySize.x <= 0.0f) {
        return -1;
    }
    auto ctx = Ship::Context::GetInstance();
    const float size = (ctx != nullptr && ctx->GetConsoleVariables() != nullptr)
                           ? CVarGetFloat(CVAR_SETTING("Notifications.Size"), 1.8f)
                           : 1.8f;
    // ImGui's FontBaseSize, times the window's font scale.
    const float base = io.FontGlobalScale * font->FontSize * font->Scale;
    const float pixels = (base > 1.0f ? base : 1.0f) * size;
    const ImGuiStyle& style = ImGui::GetStyle();
    // ImGui::CalcTextSize rounds each text item's width up to a whole pixel.
    auto textWidth = [&](const char* text) {
        const float w = font->CalcTextSizeA(pixels, FLT_MAX, 0.0f, text).x;
        return (float)(int)(w + 0.99999f);
    };
    float width = 2.0f * style.WindowPadding.x + textWidth(message != nullptr ? message : "");
    if (prefix != nullptr && prefix[0] != '\0') {
        width += textWidth(prefix) + style.ItemSpacing.x;
    }
    const float margin = 30.0f; // Window::Draw's
    return width <= io.DisplaySize.x - 2.0f * margin ? 1 : 0;
}

extern "C" void OoT_Notification_ClearForTest(void) {
    Notification::notifications.clear();
}

extern "C" void OoT_Notification_RecordForTest(int on) {
    Notification::recordForTest = on != 0;
    if (!Notification::recordForTest) {
        Notification::emittedForTest.clear();
        Notification::emittedForTest.shrink_to_fit();
    }
}

extern "C" int OoT_Notification_EmittedCountForTest(void) {
    return (int)Notification::emittedForTest.size();
}

extern "C" int OoT_Notification_EmittedAtForTest(int index, char* out, size_t cap) {
    if (index < 0 || index >= (int)Notification::emittedForTest.size() || out == nullptr || cap == 0) {
        return 0;
    }
    snprintf(out, cap, "%s", Notification::emittedForTest[index].c_str());
    return 1;
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
