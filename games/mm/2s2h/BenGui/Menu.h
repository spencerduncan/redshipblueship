#ifndef S2H_MENU_H
#define S2H_MENU_H
// Guard renamed from MENU_H: OoT's soh/SohGui/Menu.h uses that exact guard,
// so a TU including both headers would silently drop whichever came second
// (#446).

#include <ship/window/gui/GuiWindow.h>
#include "UIWidgets.hpp"
#include "MenuTypes.h"
#include <string>
#include <unordered_map>
#include <vector>

#ifdef RSBS_SINGLE_EXECUTABLE
// Single-exe symbol split for MM's Ship::Menu (#446).
//
// OoT (games/oot/soh/SohGui/Menu.h) defines an identically-named Ship::Menu
// with a DIVERGENT layout: its window-backends map is a std::map where MM's
// is a std::unordered_map (different size, shifting every later member), and
// every WidgetInfo/SearchWidget-typed member and parameter diverges per the
// MenuTypes.h header comment. OoT's implementation (SohGui/Menu.cpp) is
// compiled into the single-exe link while MM's used to be excluded, so any
// resurrected MM menu TU calling AddMenuEntry/AddSearchWidget/MenuDrawItem
// silently bound OoT's code against MM's layout — the #383/FlagTable class.
//
// Fix, per the ShipInit.hpp / UIWidgets.hpp (#434) recipe: nest MM's
// namespace Ship inside S2H so the two ports' menu symbols stop sharing
// mangled names. The using-directive re-exposes libultraship's real Ship::
// names (GuiWindow, WindowBackend, Context, ...) inside S2H::Ship, and the
// using-declarations after the class inject MM's names back into the real
// Ship namespace so existing MM callers (`Ship::Menu`,
// `class BenMenu : public Ship::Menu`) compile unchanged. A deliberate side
// effect: re-opening a bare `namespace Ship` to declare Menu in any MM TU
// that includes this header is a compile error in single-exe builds, so the
// split cannot silently regress. BenGui/Menu.cpp carries the matching wrap
// and is compiled into 2ship_rando_ui (plain archive — elided until
// referenced), so MM menu TUs that port later resolve against MM's
// layout-correct implementation; an un-ported reference now fails the link
// loudly instead of cross-binding.
namespace S2H {
#endif

namespace Ship {
#ifdef RSBS_SINGLE_EXECUTABLE
using namespace ::Ship;
#endif
uint32_t GetVectorIndexOf(std::vector<std::string>& vector, std::string value);
class Menu : public GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    Menu(const std::string& cVar, const std::string& name, uint8_t searchSidebarIndex_ = 0,
         UIWidgets::Colors menuThemeIndex_ = UIWidgets::Colors::LightBlue);

    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override;
    void Draw() override;
    void InsertSidebarSearch();
    void RemoveSidebarSearch();
    void UpdateWindowBackendObjects();
    bool IsMenuPopped();
    UIWidgets::Colors GetMenuThemeColor();

    void MenuDrawItem(WidgetInfo& widget, uint32_t width, UIWidgets::Colors menuThemeIndex);
    void AddMenuEntry(std::string entryName, const char* entryCvar);
    void AddSearchWidget(SearchWidget widget);
    std::unordered_map<uint32_t, disabledInfo>& GetDisabledMap();

  protected:
    ImVec2 mOriginalSize;
    std::string mName;
    uint32_t mWindowFlags;
    std::unordered_map<std::string, MainMenuEntry> menuEntries;
    std::vector<std::string> menuOrder;
    uint32_t DrawSearchResults(std::string& menuSearchText);
    ImGuiTextFilter menuSearch;
    uint8_t searchSidebarIndex;
    UIWidgets::Colors defaultThemeIndex;
    std::shared_ptr<std::vector<Ship::WindowBackend>> availableWindowBackends;
    std::unordered_map<Ship::WindowBackend, const char*> availableWindowBackendsMap;
    Ship::WindowBackend configWindowBackend;

    std::unordered_map<uint32_t, disabledInfo> disabledMap;
    std::vector<disabledInfo> disabledVector;
    const SidebarEntry searchSidebarEntry = {
        .columnCount = 1,
        .columnWidgets = { { { .name = "Sidebar Search",
                               .type = WIDGET_SEARCH,
                               .options = std::make_shared<UIWidgets::WidgetOptions>(UIWidgets::WidgetOptions{}.Tooltip(
                                   "Searches all menus for the given text, including tooltips.")) } } }
    };

  private:
    bool allowPopout = true; // PortNote: should be set to false on small screen ports
    bool popped;
    ImVec2 poppedSize;
    ImVec2 poppedPos;
    float windowHeight;
    float windowWidth;
    UIWidgets::Colors menuThemeIndex;
};
} // namespace Ship

#ifdef RSBS_SINGLE_EXECUTABLE
} // namespace S2H

// Inject MM's menu names back into the real Ship namespace so unqualified
// upstream MM callers resolve to the S2H versions unchanged (and a bare
// re-declaration of either name in Ship becomes a compile error).
namespace Ship {
using S2H::Ship::GetVectorIndexOf;
using S2H::Ship::Menu;
} // namespace Ship
#endif

#endif // S2H_MENU_H
