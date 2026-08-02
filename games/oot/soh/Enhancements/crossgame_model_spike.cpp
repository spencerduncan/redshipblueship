/**
 * @file crossgame_model_spike.cpp
 * @brief #577 spike: draw an MM-exclusive model from inside OoT.
 *
 * The standing verdict on cross-game asset rendering was "permanently blocked
 * by the 151-name object-namespace collision". That framing does not survive
 * contact with the substrate: RedShipBlueShip does not reach models through the
 * N64 object bank at all. The Fast3D interpreter takes a
 * `__OTR__objects/<obj>/<sym>` STRING out of the display-list command word and
 * hands it to ResourceManager::GetResourceRawPointer
 * (libultraship/src/fast/interpreter.cpp, gfx_dl_otr_filepath_handler_custom).
 * No object id, no bank slot, no segment, no DMA. SoH's own randomizer already
 * relies on this — Roc's Feather and the fishing pole are drawn from paths that
 * are not in OoT_gObjectTable at all.
 *
 * So the only real question left was the one the archives could not answer:
 * MM's display lists are ROM-extracted binary command streams, and a stream
 * that reaches into a shared segment (0x04 / gameplay_keep) by segment ADDRESS
 * would sample OoT's keep when executed under OoT. That is exactly what OoTMM
 * needed ~107 hand-authored kObjectPatches[] entries for.
 *
 * This spike answers it empirically. `object_mask_truth` was chosen because the
 * command-stream scan says it is fully self-contained: every G_SETTIMG / G_VTX
 * reference in object_mask_truth_DL_0001A0 is an OTR-hash reference that
 * resolves to a path inside that same object directory, and there is no raw
 * segmented SETTIMG anywhere in it. If the model draws with its own textures,
 * the divergence risk is absent for this class; if it garbles, the scan is
 * wrong and the patch table carries over.
 *
 * OFF unless RSBS_CROSSGAME_MODEL_SPIKE is set in the environment. This is a
 * spike surface, not a feature: nothing in normal play should draw a floating
 * Mask of Truth over Hyrule Field.
 */

#include <libultraship/bridge.h>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <string>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
// OPEN_DISPS/CLOSE_DISPS expand to FrameInterpolation_Record{Open,Close}Child;
// without this header those calls get C++ mangling and fail to link against the
// C definitions (same include nametag.cpp needs for the same reason).
#include "soh/frame_interpolation.h"
#include "soh_assets.h" // gFishingPoleGiDL — the native-model control

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "align_asset_macro.h"
extern PlayState* OoT_gPlayState;
}

// The MM-exclusive model, addressed exactly the way SoH addresses its own
// custom models (games/oot/assets/soh_assets.h). This path exists in mm.o2r and
// in the curated redship.o2r; it exists in NO OoT archive, which is the whole
// point — all 24 `object_mask_*` directories are collision-free.
#define dMMMaskOfTruthDL "__OTR__objects/object_mask_truth/object_mask_truth_DL_0001A0"
static const ALIGN_ASSET(2) char MMMaskOfTruthDL[] = dMMMaskOfTruthDL;

// The paths the display list itself pulls in. Probed (not drawn) so a failure
// to draw can be told apart from a failure to RESOLVE — a null here means the
// archive is not mounted; a non-null here plus a wrong-looking model means the
// segment divergence is real.
static const char* const kMaskResources[] = {
    "objects/object_mask_truth/object_mask_truth_DL_0001A0",
    "objects/object_mask_truth/object_mask_truthVtx_000000",
    "objects/object_mask_truth/object_mask_truth_Tex_000298",
};

static bool sSpikeEnabled = false;
static bool sProbeReported = false;

// One-shot resolution probe, reported on the first frame the spike draws. This
// is the headful counterpart of the crossgame-model ctest row: it prints
// whether each resource resolved and which archive served it, so a screenshot
// of a garbled model is never ambiguous about whether the right bytes were
// found.
static void ReportResolutionOnce() {
    if (sProbeReported) {
        return;
    }
    sProbeReported = true;

    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr) {
        printf("[#577] no resource manager\n");
        return;
    }
    auto archiveManager = ctx->GetResourceManager()->GetArchiveManager();

    for (const char* path : kMaskResources) {
        const void* raw = ctx->GetResourceManager()->GetResourceRawPointer(path);
        std::string owner = "<unmounted>";
        if (archiveManager != nullptr) {
            auto archive = archiveManager->GetArchiveFromFile(path);
            if (archive != nullptr) {
                owner = archive->GetPath();
            }
        }
        printf("[#577] %-58s raw=%p archive=%s\n", path, raw, owner.c_str());
    }
}

static void DrawCrossGameModel() {
    PlayState* play = OoT_gPlayState;
    if (play == nullptr || play->state.gfxCtx == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(play);
    if (player == nullptr) {
        return;
    }

    ReportResolutionOnce();

    OPEN_DISPS(play->state.gfxCtx);

    // Screen-space marker. Needs no matrix, no lights, no texture and no
    // vertices, so it separates "this hook's commands are executed" from
    // "this hook's MODEL draw is set up correctly" — the two explanations for a
    // blank screenshot, which are otherwise indistinguishable.
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_FILL);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(POLY_OPA_DISP++, (GPACK_RGBA5551(255, 0, 0, 1) << 16) | GPACK_RGBA5551(255, 0, 0, 1));
    gDPFillRectangle(POLY_OPA_DISP++, 16, 16, 72, 48);
    gDPPipeSync(POLY_OPA_DISP++);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    // Park it above Link, slowly spinning, so a screenshot cannot be ambiguous
    // about whether it drew or whether its texture is the right one.
    //
    // The scale and the recentre are NOT decoration, and they are the one real
    // integration detail this spike turned up. A foreign model arrives in the
    // SOURCE game's object space: object_mask_truth's vertices span x 176..1009,
    // y -1417..203, z -781..781, centred at (595, -742, 0), because MM only ever
    // draws it through a Player limb matrix that supplies both the scale and the
    // offset. Drawn at OoT's actor scale it lands thousands of units off-camera —
    // which looks exactly like "it didn't render". Nothing is wrong with the
    // model; the caller simply has to supply what the source game's draw code
    // would have. kCgmModelScale/kCgmModelCentre are that.
    static constexpr float kCgmModelScale = 0.05f;
    // Parked a fixed distance straight down the camera's view axis, not at Link
    // and not at the look-at point. Both of those put the model wherever the
    // scene's camera happened to be — off the edge of the frame in Market, and
    // clipping through the near plane in Kakariko's overhead shot. Eye +
    // forward * kCgmProbeDistance is the only placement that is legible in every
    // scene, which is what a probe needs to be.
    static constexpr float kCgmProbeDistance = 220.0f;
    const Vec3f eye = play->view.eye;
    const Vec3f look = play->view.lookAt;
    float fx = look.x - eye.x, fy = look.y - eye.y, fz = look.z - eye.z;
    const float flen = sqrtf(fx * fx + fy * fy + fz * fz);
    if (flen > 0.001f) {
        fx /= flen;
        fy /= flen;
        fz /= flen;
    } else {
        fx = 0.0f;
        fy = 0.0f;
        fz = 1.0f;
    }
    const Vec3f at = { eye.x + fx * kCgmProbeDistance, eye.y + fy * kCgmProbeDistance,
                       eye.z + fz * kCgmProbeDistance };
    OoT_Matrix_Translate(at.x, at.y, at.z, MTXMODE_NEW);
    Matrix_RotateY((play->gameplayFrames & 0x3FF) * (2.0f * M_PIf / 1024.0f), MTXMODE_APPLY);
    OoT_Matrix_Scale(kCgmModelScale, kCgmModelScale, kCgmModelScale, MTXMODE_APPLY);
    OoT_Matrix_Translate(-595.0f, 742.0f, 0.0f, MTXMODE_APPLY);

    gDPPipeSync(POLY_OPA_DISP++);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)MMMaskOfTruthDL);

    // Control model, drawn beside it from OoT's OWN custom archive by the exact
    // same path-string mechanism. It is what makes a blank screenshot
    // interpretable: if the control appears and the foreign model does not, the
    // difference is the model; if neither appears, the difference is this draw
    // code. Without it "nothing rendered" says nothing at all.
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    OoT_Matrix_Translate(at.x + 60.0f, at.y, at.z, MTXMODE_NEW);
    Matrix_RotateY((play->gameplayFrames & 0x3FF) * (2.0f * M_PIf / 1024.0f), MTXMODE_APPLY);
    OoT_Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);
    gDPPipeSync(POLY_OPA_DISP++);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gFishingPoleGiDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// C linkage: declared inside mods.h's extern "C" block alongside InitMods().
extern "C" void RegisterCrossGameModelSpike() {
    const char* knob = std::getenv("RSBS_CROSSGAME_MODEL_SPIKE");
    sSpikeEnabled = knob != nullptr && knob[0] != '\0' && knob[0] != '0';
    if (!sSpikeEnabled) {
        return;
    }
    printf("[#577] cross-game model spike ARMED: drawing %s from OoT\n", dMMMaskOfTruthDL);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(DrawCrossGameModel);
}
