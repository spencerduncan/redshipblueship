#pragma once

#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>

#ifdef RSBS_SINGLE_EXECUTABLE
// OoT's soh/Enhancements/randomizer/randomizer_item_tracker.h defines an
// identically-named class ItemTrackerSettingsWindow (same collision story as
// AudioEditor.h: InitElement/DrawElement/vtable mangle identically), so MM's
// copy moves into S2H.
namespace S2H {
#endif

class ItemTrackerSettingsWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

  protected:
    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override;
};

#ifdef RSBS_SINGLE_EXECUTABLE
} // namespace S2H
using S2H::ItemTrackerSettingsWindow;
#endif
