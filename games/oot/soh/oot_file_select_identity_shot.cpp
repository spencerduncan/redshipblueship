/**
 * @file oot_file_select_identity_shot.cpp
 * #952's picture: the REAL file select showing a just-created paired file, its
 * five seed icons and, beside them, the pair's fingerprint and the build.
 *
 * The IntPairedFileSelectIdentity row runs int-paired-first-crossing with
 * RSBS_PFC_FILE_SELECT_SHOT=1: the drive generates the pinned paired world on
 * the shipped defaults and creates file 3 through OoT's own new-file seam (the
 * production creation event, GameExports_SingleExe.cpp), and then, instead of
 * loading the file, hands the file select to this module, which:
 *   1. presses A on file 3 the way a player opens it (the cursor on the file,
 *      A pressed in the main menu), so the file select runs its own path to the
 *      file's confirm screen, where SoH draws the file's seed icons;
 *   2. once the file's info has faded in, captures the frame
 *      (IntegrationTest_CaptureFrame, RSBS_CAPTURE_OUT) as
 *      file-select-paired-identity.png;
 *   3. asserts the text the file select DREW beside the icons
 *      (Combo_IdentityDisplay_LastDrawn, recorded by the draw itself) is the
 *      created file's fingerprint (gComboCtx.comboSettingsHash) over the
 *      build's short commit, that it was drawn on the confirm screen's frames,
 *      and that the capture is on disk and not a blank frame;
 *   4. on a pass, erases the row's own file and ends the run.
 * Needs the ROM archives and a display (the integration rows' needs).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "OTRGlobals.h"
#include "z64.h"

#include "combo_identity_display.h" // src/common
#include "context.h"                // src/common — gComboCtx
#include "integration_test_hooks.h" // src/common
#include "ui_snapshot_image.h"      // src/common — the capture's PNG reader

// Last: through global.h it pulls in functions.h, whose C++ half redefines `this`.
#include "src/overlays/gamestates/ovl_file_choose/file_choose.h"

extern "C" void Save_DeleteFile(int fileNum);

namespace {

enum class ShotStage { Off, Running, Done };

const char* const kShotName = "file-select-paired-identity";
// Frames to reach the file's confirm screen (the file select's own fades), then
// how long the confirm screen settles before the capture, and how long the
// capture has to reach the disk before the verdict.
constexpr int kShotBudgetFrames = 900;
constexpr int kShotSettleFrames = 10;
constexpr int kShotWriteFrames = 10;

ShotStage sStage = ShotStage::Off;
int sSlot = 0;
int sFrames = 0;
int sConfirmFrames = 0;
int sDrawsAtCapture = 0;
uint32_t sExpectedHash = 0;

void ShotFail(const char* why) {
    sStage = ShotStage::Done;
    IntegrationTest_GameplayFail(why);
}

std::string ShotCapturePath() {
    const char* dir = std::getenv("RSBS_CAPTURE_OUT");
    return (dir == nullptr || dir[0] == '\0') ? std::string() : std::string(dir) + "/" + kShotName + ".png";
}

bool ShotCaptureOnDisk(char* msg, size_t cap) {
    const std::string path = ShotCapturePath();
    if (path.empty()) {
        std::snprintf(msg, cap, "RSBS_CAPTURE_OUT is not set, so the file select was not captured");
        return false;
    }
    UiImage img = {};
    if (UiImage_ReadPng(&img, path.c_str()) != 0 || img.w <= 0 || img.h <= 0) {
        std::snprintf(msg, cap, "the capture %s is missing or unreadable", path.c_str());
        return false;
    }
    uint32_t modal = 0;
    double nonModal = 0.0;
    int distinct = 0;
    UiImage_Stats(&img, 64, &modal, &nonModal, &distinct);
    UiImage_Free(&img);
    if (distinct < 8) {
        std::snprintf(msg, cap, "the capture %s is a blank frame (%d distinct colours)", path.c_str(), distinct);
        return false;
    }
    std::snprintf(msg, cap, "%s (%d+ distinct colours)", path.c_str(), distinct);
    return true;
}

} // namespace

extern "C" int OoT_PfcFileSelectShot_Begin(FileChooseContext* fileChoose, int slot) {
    const char* raw = std::getenv("RSBS_PFC_FILE_SELECT_SHOT");
    if (raw == nullptr || std::strcmp(raw, "1") != 0 || fileChoose == nullptr) {
        return 0;
    }
    sStage = ShotStage::Running;
    sSlot = slot;
    sFrames = 0;
    sConfirmFrames = 0;
    sExpectedHash = gComboCtx.comboSettingsHash;
    // A capture from an earlier run must not pass for this one's.
    if (!ShotCapturePath().empty()) {
        std::remove(ShotCapturePath().c_str());
    }
    std::fprintf(stderr,
                 "[PFC] file-select shot (#952): staying at the file select to open file %d's confirm screen "
                 "(fingerprint %08X, build %s)\n",
                 slot + 1, (unsigned)sExpectedHash, Combo_IdentityDisplay_Build());
    std::fflush(stderr);
    return 1;
}

extern "C" int OoT_PfcFileSelectShot_Running(void) {
    return sStage != ShotStage::Off ? 1 : 0;
}

extern "C" void OoT_PfcFileSelectShot_Frame(FileChooseContext* fileChoose) {
    if (sStage != ShotStage::Running || fileChoose == nullptr) {
        return;
    }
    char msg[512];
    sFrames++;
    if (sFrames > kShotBudgetFrames) {
        std::snprintf(msg, sizeof(msg),
                      "the file select never settled on file %d's confirm screen in %d frames (menuMode %d, "
                      "configMode %d, selectMode %d, selectedFileIndex %d, fileInfoAlpha %d)",
                      sSlot + 1, kShotBudgetFrames, (int)fileChoose->menuMode, (int)fileChoose->configMode,
                      (int)fileChoose->selectMode, (int)fileChoose->selectedFileIndex,
                      (int)fileChoose->fileInfoAlpha[sSlot]);
        ShotFail(msg);
        return;
    }

    // 1. Open the file as a player does: the cursor on it, A in the main menu.
    if (fileChoose->menuMode == FS_MENU_MODE_CONFIG && fileChoose->configMode == CM_MAIN_MENU) {
        fileChoose->buttonIndex = sSlot;
        if (sFrames % 8 == 0) {
            fileChoose->state.input[0].press.button |= BTN_A;
        }
        return;
    }
    const bool confirming = fileChoose->menuMode == FS_MENU_MODE_SELECT && fileChoose->selectMode == SM_CONFIRM_FILE &&
                            fileChoose->selectedFileIndex == sSlot && fileChoose->fileInfoAlpha[sSlot] >= 200;
    if (!confirming) {
        return;
    }

    // 2. The confirm screen has settled: capture the next drawn frame.
    sConfirmFrames++;
    if (sConfirmFrames == kShotSettleFrames) {
        sDrawsAtCapture = Combo_IdentityDisplay_DrawCount();
        IntegrationTest_CaptureFrame(kShotName);
        return;
    }
    if (sConfirmFrames < kShotSettleFrames + kShotWriteFrames) {
        return;
    }

    // 3. The verdict.
    char expected[RSBS_COMBO_IDENTITY_TEXT_CAP];
    Combo_IdentityDisplay_Format(sExpectedHash, expected, sizeof(expected));
    const int draws = Combo_IdentityDisplay_DrawCount() - sDrawsAtCapture;
    const char* drawn = Combo_IdentityDisplay_LastDrawn();
    if (sExpectedHash == 0 || draws <= 0 || std::strcmp(drawn, expected) != 0) {
        std::snprintf(msg, sizeof(msg),
                      "file %d's confirm screen did not draw its fingerprint beside the icons: %d draw(s) since the "
                      "capture, last text \"%s\", expected \"%s\"",
                      sSlot + 1, draws, drawn, expected);
        ShotFail(msg);
        return;
    }
    if (!ShotCaptureOnDisk(msg, sizeof(msg))) {
        ShotFail(msg);
        return;
    }
    std::string oneLine = drawn;
    for (char& c : oneLine) {
        c = c == '\n' ? '|' : c;
    }
    std::fprintf(stderr,
                 "[PFC] PASS: file %d's confirm screen drew \"%s\" beside its seed icons (%d draws in %d frames); "
                 "captured %s\n",
                 sSlot + 1, oneLine.c_str(), draws, kShotWriteFrames, msg);
    std::fflush(stderr);
    sStage = ShotStage::Done;
    // A pass leaves no file behind (a failure keeps it for forensics; the next
    // run erases it by its name).
    Save_DeleteFile(sSlot);
    IntegrationTest_SetGameplayPhase(GP_PHASE_DONE);
    IntegrationTest_SignalBootComplete(GAME_OOT, "paired file shown at the file select");
}
