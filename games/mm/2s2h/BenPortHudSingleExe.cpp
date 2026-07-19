/**
 * BenPort HUD-geometry implementations for single-executable builds.
 *
 * BenPort.cpp is excluded from single-exe builds (games/mm/CMakeLists.txt,
 * "2s2h/BenPort\.cpp$" filter), but MM's HUD draw path calls
 * OTRConvertHUDXToScreenX on every HUD frame and CONSUMES its return value
 * as a viewport/scissor X coordinate — e.g. z_parameter.c
 * Interface_SetPerspectiveView:
 *
 *     interfaceCtx->viewport.leftX  = OTRConvertHUDXToScreenX(leftX);
 *     interfaceCtx->viewport.rightX = OTRConvertHUDXToScreenX(rightX);
 *
 * The placeholder in src/common/mm_stubs.c declared this as
 * `float OTRConvertHUDXToScreenX(float)` while the real implementation and
 * every caller (via BenPort.h, included by z_parameter.c) use
 * `int32_t(int32_t)`. On the MSVC x64 ABI the caller passes the argument in
 * ECX and reads the result from EAX, while the float-typed stub reads and
 * returns XMM0 — so EAX was never written and the viewport received stale
 * register contents. The A button is the ONLY MM HUD element drawn through a
 * perspective viewport (Interface_SetPerspectiveView's only callers are the
 * three sites in Interface_DrawAButton), so it alone collapsed to a
 * degenerate rect and was clipped away entirely, while every other element —
 * which goes through Interface_SetOrthoView or a plain texture rect —
 * rendered normally. That is the operator-reported "MM is missing the A
 * button" (2026-07-19). The same stub also fed the gDPSetScissor calls for
 * the three-day clock, so those were silently wrong too.
 *
 * This is the same "MM stub signature drift" class as the CosmeticEditor
 * Gfx_Draw*_DropShadowOverride family — see the file header of
 * CosmeticGfxSingleExe.cpp for that instance. Like that TU, this one is
 * compiled against 2s2h/BenPort.h so every signature here is compiler-checked
 * against the declaration the real callers use: the failure mode being fixed
 * becomes a compile error instead of a silent ABI mismatch. The body is
 * ported verbatim from BenPort.cpp.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <fast/Fast3dWindow.h>
#include <ship/Context.h>

#include "BenPort.h"

#include <cassert>
#include <cstdint>

extern "C" int32_t OTRConvertHUDXToScreenX(int32_t v) {
    auto fastWnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());
    if (!fastWnd) {
        return v;
    }
    auto intP = fastWnd->GetInterpreterWeak().lock();

    if (!intP) {
        assert(false && "Lost reference to Fast::Interpreter");
        return v;
    }

    uint32_t gameHeight, gameWidth;
    intP->GetCurDimensions(&gameWidth, &gameHeight);
    float hudAspectRatio = 4.0f / 3.0f;
    int32_t hudHeight = gameHeight;
    int32_t hudWidth = hudHeight * hudAspectRatio;

    float hudScreenRatio = (hudWidth / 320.0f);
    float hudCoord = v * hudScreenRatio;
    float gameOffset = (gameWidth - hudWidth) / 2;
    float gameCoord = hudCoord + gameOffset;
    float gameScreenRatio = (320.0f / gameWidth);
    float screenScaledCoord = gameCoord * gameScreenRatio;
    int32_t screenScaledCoordInt = screenScaledCoord;

    return screenScaledCoordInt;
}

#endif // RSBS_SINGLE_EXECUTABLE
