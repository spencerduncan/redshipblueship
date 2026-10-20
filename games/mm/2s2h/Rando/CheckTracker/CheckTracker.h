#ifndef RANDO_CHECK_TRACKER_H
#define RANDO_CHECK_TRACKER_H

#include "Rando/Rando.h"
#include <set>
#include <ship/window/gui/GuiWindow.h>

namespace Rando {

namespace CheckTracker {

void Init();
void OnFileLoad();

/** The set of checks the tracker draws as "in logic": reachable from `startEntrance` under the CURRENT save
 *  (CrawlReachableRegions + EvaluateReachableChecks), shuffled and not yet obtained. Gate-free (no CVar, no frame
 *  throttle) so a test reads exactly what the window draws. The crawl zeroes and re-fires RANDO_EVENTS, which lives
 *  inside the SaveContext the departure freeze copies and the .redsave persists, so this brackets those RE_MAX bytes
 *  and leaves the live save byte-identical (Logic.h's contract for a caller that must not perturb it). */
std::set<RandoCheckId> ComputeChecksInLogic(s32 startEntrance);

class CheckTrackerWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override{};
    void DrawElement() override{};
    void Draw() override;
    void UpdateElement() override{};
};

class SettingsWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override{};
    void DrawElement() override;
    void UpdateElement() override{};
};

} // namespace CheckTracker

} // namespace Rando

typedef enum { SCENE_LOAD, SCENE_UPDATE };

#endif // RANDO_CHECK_TRACKER_H
