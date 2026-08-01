#ifndef NOTIFICATION_H
#define NOTIFICATION_H
#ifdef __cplusplus

#include <string>
#include <cstdint>
#include <ship/window/gui/GuiWindow.h>
namespace Notification {

struct Options {
    uint32_t id = 0;
    const char* itemIcon = nullptr;
    std::string prefix = "";
    ImVec4 prefixColor = ImVec4(0.5f, 0.5f, 1.0f, 1.0f);
    std::string message = "";
    ImVec4 messageColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    std::string suffix = "";
    ImVec4 suffixColor = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
    float remainingTime = 0.0f; // Seconds
    bool mute = false;
};

class Window : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override{};
    void DrawElement() override{};
    void Draw() override;
    void UpdateElement() override;
};

#ifdef RSBS_SINGLE_EXECUTABLE
// `Emit` is deliberately NOT declared in single-exe builds (#427 item 1).
// 2s2h/BenGui/Notification.cpp is link-excluded there, so MM has no Emit body
// of its own, and `Notification::Emit(Notification::Options)` mangles
// identically to OoT's soh/Notification definition — every MM call site bound
// OoT's body and handed it MM's own view of Options, safe only for as long as
// the two structs stayed field-identical, with no link error possible when
// they stop. Dropping the declaration turns that silent re-arming into a
// compile error pointing here.
//
// MM reaches the one shared overlay through the explicit bridge instead:
// this forwards to OoT's Emit as plain-C ComboNotification data
// (src/common/notification_bridge.h), so no Options ever reaches a body
// compiled against the other port's view. The two layouts must still agree,
// because both trees declare this type and their implicit ctor/dtor
// COMDAT-fold; that half is locked by src/common/notification_layout_probe.h.
// Defined in games/mm/2s2h/mm_notification_bridge.cpp.
void MM_Notify_Emit(const Options& notification);
#else
void Emit(Options notification);

// Same call spelling in standalone 2Ship builds, where MM owns its own Emit.
inline void MM_Notify_Emit(const Options& notification) {
    Emit(notification);
}
#endif

} // namespace Notification

#endif // __cplusplus
#endif // NOTIFICATION_H
