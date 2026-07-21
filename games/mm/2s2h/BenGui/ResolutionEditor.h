#ifndef S2H_RESOLUTIONEDITOR_H
#define S2H_RESOLUTIONEDITOR_H
// Guard renamed from RESOLUTIONEDITOR_H: OoT's soh/SohGui/ResolutionEditor.h
// uses that exact guard, so a TU including both headers would silently drop
// whichever came second (#446). The declarations themselves do not collide
// (namespace BenGui here vs SohGui there).

namespace BenGui {
bool IsDroppingFrames();
void RegisterResolutionWidgets();
void UpdateResolutionVars();
} // namespace BenGui

#endif // S2H_RESOLUTIONEDITOR_H
