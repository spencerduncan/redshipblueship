/**
 * MM tracker windows on the shared Gui, bypassing the BenMenu shell (#392).
 *
 * Upstream 2S2H wires its trackers in BenGui.cpp::SetupGuiElements — an
 * excluded TU whose only caller (BenPort.cpp) is also excluded — so the whole
 * MM tracker surface was link-elided. This TU is the MVP registration surface
 * from docs/unified-surface-findings.md §3:
 *
 *  - It defines the four BenGui::m*Window globals the vendored tracker TUs
 *    reference (CheckTracker.cpp needs mRandoCheckTrackerWindow,
 *    ItemTrackerSettings.cpp needs mItemTrackerWindow), which is what lets
 *    the linker pull CheckTracker.obj / UIWidgets.obj out of the plain
 *    2ship_rando_ui archive and ItemTracker(.Settings).obj out of 2ship_enh
 *    WITHOUT dragging in Rando/Menu.cpp's BenMenu externals — Menu.obj stays
 *    elided because nothing references its symbols.
 *
 *  - It registers the windows on the shared Ship::Context Gui under
 *    "MM "-prefixed names: SoH already owns "Check Tracker" / "Item Tracker"
 *    (games/oot/soh/SohGui/SohGui.cpp) and Gui::AddGuiWindow refuses
 *    duplicate names. Visibility CVars stay MM's upstream "gWindows.*" — OoT
 *    windows use "gOpenWindows.*", so there is no store collision to rename
 *    around.
 *
 *  - Every window is wrapped in an active-game gate. The shared Gui draws
 *    every registered window every frame regardless of which game runs, and
 *    gSaveContext storage is unified: an MM tracker drawing while OoT is
 *    active would read OoT bytes through MM's SaveContext layout, and MM's
 *    tracker Draw bodies hardcode ImGui ids that would land in the same
 *    frame as OoT's trackers. The gate makes MM tracker windows a strict
 *    no-op unless Context_GetCurrentGame() == GAME_MM.
 *
 * Teardown, deliberately absent: windows registered here persist across game
 * switches (dormant behind the gate, exactly like SoH's windows persist over
 * MM). Do NOT reach for BenGui::Destroy as a teardown — it calls
 * Gui::RemoveAllGuiWindows, which would drop SoH's windows too.
 *
 * Locked ROM-free by the mm-trackers-gui CTest (mm_trackers_gui_test.cpp):
 * registration list + name de-collision + gate flip.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "TrackersGuiSingleExe.h"

#include <memory>

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>

#include "2s2h/Rando/CheckTracker/CheckTracker.h"
#include "2s2h/Enhancements/Trackers/ItemTracker/ItemTracker.h"
#include "2s2h/Enhancements/Trackers/ItemTracker/ItemTrackerSettings.h"
#include "2s2h/DeveloperTools/SaveEditor.h"
#include "2s2h/ShipUtils.h"

#include "context.h" // src/common — Context_GetCurrentGame / GameId

// games/mm/2s2h/GameExports_SingleExe.cpp — true once LoadMMArchives
// registered mm.o2r. Tracker icon loading needs the archive; in the ROM-free
// harness Gui::LoadGuiTexture null-derefs on the missing resource.
extern "C" bool MM_Rando_AssetsReady(void);

namespace BenGui {
// The globals the vendored tracker TUs extern-declare (BenGui.cpp, which
// upstream defines them in, is excluded from single-exe builds — no ODR
// overlap). Types must match those extern declarations exactly:
// CheckTracker.cpp / Rando/Menu.cpp / ItemTracker(.Settings).cpp.
std::shared_ptr<Rando::CheckTracker::CheckTrackerWindow> mRandoCheckTrackerWindow;
std::shared_ptr<Rando::CheckTracker::SettingsWindow> mRandoCheckTrackerSettingsWindow;
std::shared_ptr<ItemTrackerWindow> mItemTrackerWindow;
std::shared_ptr<ItemTrackerSettingsWindow> mItemTrackerSettingsWindow;
} // namespace BenGui

/**
 * Single-exe home for SaveEditor.cpp's safe-item table (the
 * SaveEditorTimeSingleExe.cpp pattern). Upstream defines
 * safeItemsForInventorySlot and populates it from
 * SaveEditorWindow::InitElement — both in the excluded DeveloperTools TU —
 * and the item tracker reads slot [0] as the vanilla-icon fallback for empty
 * inventory slots. Declaration comes from DeveloperTools/SaveEditor.h (the
 * S2H-wrapped header the tracker TUs reach through UIWidgets.hpp), so the
 * definition below is compiler-checked against the extern the trackers bind.
 * If upstream 2S2H changes initSafeItemsForInventorySlot, re-sync this copy
 * (provenance: SaveEditor.cpp, upstream shape as of this port).
 */
namespace S2H {
std::vector<ItemId> safeItemsForInventorySlot[SLOT_MASK_FIERCE_DEITY + 1] = {};
} // namespace S2H

namespace {

// Verbatim port of SaveEditor.cpp's initSafeItemsForInventorySlot, plus an
// idempotence guard (upstream relies on GuiElement::Init's once-only; here
// RegisterWindows is the single caller, but a re-run must not duplicate).
void InitSafeItemsForInventorySlot() {
    static bool sInitialized = false;
    if (sInitialized) {
        return;
    }
    sInitialized = true;

    for (int i = 0; i < sizeof(MM_gItemSlots); i++) {
        InventorySlot slot = static_cast<InventorySlot>(MM_gItemSlots[i]);
        switch (slot) {
            case SLOT_BOTTLE_1:
                if (i != ITEM_LONGSHOT) { // No longshot in bottles
                    safeItemsForInventorySlot[SLOT_BOTTLE_1].push_back(static_cast<ItemId>(i));
                    safeItemsForInventorySlot[SLOT_BOTTLE_2].push_back(static_cast<ItemId>(i));
                    safeItemsForInventorySlot[SLOT_BOTTLE_3].push_back(static_cast<ItemId>(i));
                    safeItemsForInventorySlot[SLOT_BOTTLE_4].push_back(static_cast<ItemId>(i));
                    safeItemsForInventorySlot[SLOT_BOTTLE_5].push_back(static_cast<ItemId>(i));
                    safeItemsForInventorySlot[SLOT_BOTTLE_6].push_back(static_cast<ItemId>(i));
                }
                break;
            case SLOT_BOW:
                if (i == ITEM_BOW) { // No elemental bows here
                    safeItemsForInventorySlot[slot].push_back(static_cast<ItemId>(i));
                }
                break;
            case SLOT_TRADE_KEY_MAMA:
                if (i != ITEM_SLINGSHOT) { // No slingshot in trade items
                    safeItemsForInventorySlot[slot].push_back(static_cast<ItemId>(i));
                }
                break;
            case SLOT_TRADE_DEED:
                if (i != ITEM_OCARINA_FAIRY) { // No fairy ocarina in trade items
                    safeItemsForInventorySlot[slot].push_back(static_cast<ItemId>(i));
                }
                break;
            default:
                safeItemsForInventorySlot[slot].push_back(static_cast<ItemId>(i));
                break;
        }
    }
}

} // namespace

namespace {

/**
 * Active-game gate. Draw() covers the per-frame ImGui path (both the base
 * GuiWindow::Draw used by the settings windows and the full Draw overrides in
 * CheckTracker.cpp / ItemTracker.cpp); UpdateElement() covers Gui's
 * unconditional per-frame Update(). The bases' bodies run only while MM is
 * the active game. The mm-trackers-gui lock calls Draw() with OoT active and
 * no ImGui context, so an ungated path aborts the test process.
 */
template <typename BaseWindow> class MMActiveGated final : public BaseWindow {
  public:
    using BaseWindow::BaseWindow;

    void Draw() override {
        if (!MM_TrackersGui_ShouldDraw()) {
            return;
        }
        BaseWindow::Draw();
    }

  protected:
    void UpdateElement() override {
        if (!MM_TrackersGui_ShouldDraw()) {
            return;
        }
        BaseWindow::UpdateElement();
    }
};

} // namespace

namespace S2H {
namespace TrackersGui {

void RegisterWindows(std::shared_ptr<Ship::Gui> gui) {
    if (gui == nullptr) {
        return;
    }
    // Idempotence: MM_Rando_Init is once-only guarded, but the headless
    // harness may drive registration directly more than once.
    if (gui->GetGuiWindow(kCheckTrackerWindowName) != nullptr) {
        return;
    }

    // The item tracker's vanilla-icon fallback indexes [0] of each slot's
    // safe-item list; populate it before any window can draw (upstream did
    // this from the excluded SaveEditorWindow::InitElement).
    InitSafeItemsForInventorySlot();

    // CVar names and sizes mirror BenGui.cpp::SetupGuiElements; only the
    // registration names carry the "MM " prefix (SoH owns the unprefixed
    // ones). CheckTrackerWindow::Draw ignores the ctor CVar and reads
    // "gWindows.CheckTracker" itself, so the ctor CVar must stay in sync
    // with the vendored macro (CVAR_NAME_SHOW_CHECK_TRACKER).
    BenGui::mRandoCheckTrackerWindow = std::make_shared<MMActiveGated<Rando::CheckTracker::CheckTrackerWindow>>(
        "gWindows.CheckTracker", kCheckTrackerWindowName, ImVec2(375, 460));
    gui->AddGuiWindow(BenGui::mRandoCheckTrackerWindow);

    BenGui::mRandoCheckTrackerSettingsWindow = std::make_shared<MMActiveGated<Rando::CheckTracker::SettingsWindow>>(
        "gWindows.CheckTrackerSettings", kCheckTrackerSettingsWindowName);
    gui->AddGuiWindow(BenGui::mRandoCheckTrackerSettingsWindow);

    BenGui::mItemTrackerWindow =
        std::make_shared<MMActiveGated<ItemTrackerWindow>>("gWindows.ItemTracker", kItemTrackerWindowName);
    gui->AddGuiWindow(BenGui::mItemTrackerWindow);

    BenGui::mItemTrackerSettingsWindow = std::make_shared<MMActiveGated<ItemTrackerSettingsWindow>>(
        "gWindows.ItemTrackerSettings", kItemTrackerSettingsWindowName, ImVec2(800, 400));
    gui->AddGuiWindow(BenGui::mItemTrackerSettingsWindow);
}

} // namespace TrackersGui
} // namespace S2H

extern "C" bool MM_TrackersGui_ShouldDraw(void) {
    return Context_GetCurrentGame() == GAME_MM;
}

extern "C" void MM_TrackersGui_Init(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetWindow() == nullptr) {
        // ROM-free unit harness: shared subsystems without a window/Gui.
        return;
    }
    auto gui = ctx->GetWindow()->GetGui();
    if (gui == nullptr) {
        return;
    }

    S2H::TrackersGui::RegisterWindows(gui);

    // Tracker icons (check-type icons, item/quest icons). Upstream loads
    // these from BenPort.cpp's InitOTR; gated on the archive because the
    // string-path LoadGuiTexture crashes on a missing resource (#330 class),
    // and mm-rando-gen brings this path up windowed but archive-free.
    if (MM_Rando_AssetsReady()) {
        MM_LoadGuiTextures();
    }
}

#endif // RSBS_SINGLE_EXECUTABLE
