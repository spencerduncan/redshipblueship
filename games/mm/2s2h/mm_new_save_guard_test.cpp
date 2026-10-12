/**
 * @file mm_new_save_guard_test.cpp
 * ROM-free, display-free lock for #564 V21: MM may not author a new save over
 * an open paired file. CTest label "redship", row MMNewSaveSessionGuard in
 * CMake/SingleExecutable.cmake, dispatch "mm-new-save-session-guard" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. MM_Sram_InitNewSave authors a fresh vanilla save into the
 * live gSaveContext, and nothing asked whether a combo session was resident.
 * #532 and #590 closed two exits into MM's TitleSetup (whose
 * TitleSetup_SetupTitleScreen calls it) one call site at a time, and
 * tools/tests/test_repo_invariants.py pins those sites, but the invariant was
 * per call site, not per class: MM's file-select new-file path
 * (MM_Sram_InitSave, which also dispatches OnSaveInit, MM's generation hook)
 * and any future caller still authored over the session. And MM had no twin of
 * OoT's Context_InvalidateSessionOnReturnToTitle,
 * so a TitleSetup entry from a live paired session left gComboCtx, the active
 * slot and OoT's frozen blob resident under a vanilla bootstrap, which the next
 * hop back to OoT freezes into the MM shadow and the next commit writes into
 * the slot (#532's mechanism).
 *
 * "A COMBO SESSION IS RESIDENT" means a paired file is open: a paired world
 * identity is stamped (Combo_ForeignPairingActive) AND a unified-save slot is
 * active (RsbsSave_GetActiveSlot() >= 0). Context_ComboSessionResident in
 * src/common/context.h states why both halves are needed.
 *
 * WHAT THIS LOCKS:
 *
 *   1. MM_Sram_InitNewSave over an open paired file leaves the live save
 *      byte-identical. RED before the fix: it authored a vanilla save.
 *   2. MM_Sram_InitSave (MM's file-select new-file path) over an open paired
 *      file leaves the live save and the flash buffer untouched and dispatches
 *      no OnSaveInit. RED before the fix: a vanilla file was authored, copied
 *      to the buffer, and OnSaveInit ran.
 *   3. The REAL TitleSetup_SetupTitleScreen from a live paired session, with no
 *      cross-game arrival in flight, retires the session first (MM's twin of
 *      the return-to-title invalidation): no pairing, no active slot, no
 *      frozen OoT blob; then authors its bootstrap over nothing. RED before the
 *      fix: the bootstrap was authored with the whole session still resident.
 *   4. The same TitleSetup with a cross-game arrival into MM in flight (the
 *      arrival's boot chain) keeps the session and authors the bootstrap: that
 *      bootstrap is the canvas the arrival's consume overwrites.
 *   5. No paired file open, nothing refused: a vanilla file's session (slot
 *      open, no pairing; the hot-swap first entry beside a suspended OoT) keeps
 *      OoT's frozen blob through MM's title, and a staged generation with no
 *      file (pairing, no slot) still lets MM author.
 *   6. The paired creation event's exemption, through the production bracket:
 *      MM_Creation_AuthorBootstrap (the step MM_Rando_AuthorHalfAtCreation
 *      authors its vanilla bootstrap with, ahead of the generation) authors
 *      even with a paired file open, and the creation scope it opens
 *      (MM_Combo_SetCreationAuthoring) is closed behind it, so the very next
 *      MM_Sram_InitNewSave is refused again. RED with the bracket removed from
 *      the seam (the creation is refused) and with only its closing call
 *      removed (the scope stays open).
 */

#include "global.h"

#include "context.h"
#include "entrance.h"
#include "foreign_items.h"
#include "game.h"
#include "save.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include "overlays/gamestates/ovl_file_choose/z_file_select.h"
#include "overlays/gamestates/ovl_opening/z_opening.h"
}

extern "C" SaveContext gSaveContext;

extern "C" void TitleSetup_SetupTitleScreen(TitleSetupState* thisx);
extern "C" uint32_t MM_Rando_OnSaveInitDispatchCount(void);
extern "C" void MM_Creation_AuthorBootstrap(void);

namespace {

#define NSG_ASSERT(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            fflush(stdout);                           \
            return 1;                                 \
        }                                             \
    } while (0)

constexpr int kSlot = 1;
constexpr uint32_t kSeed = 0x0C0FFEE5u;
constexpr uint32_t kSettingsHash = 0x5E551011u;
constexpr uint16_t kOoTReturnEntrance = 0x0530;
constexpr uint16_t kMMArrivalEntrance = 0xD800;

// Backing store for MM_Sram_InitSave's flash buffer (static: 128 KiB).
u8 sSaveBuf[SAVE_BUFFER_SIZE];
// FileSelectState is ~146 KiB: static, not on the stack.
FileSelectState sFileSelect;
// The snapshot a refused call must leave byte-identical (static: it is large).
SaveContext sBefore;

// Nothing resident: the state a fresh process, or a slot load's clear, leaves.
void ClearSession() {
    Entrance_ClearStartupEntrance();
    Context_InvalidateSessionOnSlotLoad();
}

// A paired file is open: the identity is stamped and the slot is active.
void OpenPairedFile() {
    gComboCtx.sourceIsRando = 1;
    gComboCtx.sharedRandoSeed = kSeed;
    gComboCtx.sharedRandoSettingsHash = kSettingsHash;
    RsbsSave_SetActiveSlot(kSlot);
}

// OoT's half suspended behind MM, as a crossing or an F10 leaves it.
void FreezeOoTBehindMM() {
    static uint8_t ootBlob[256];
    memset(ootBlob, 0x5A, sizeof(ootBlob));
    Context_FreezeState(GAME_OOT, kOoTReturnEntrance, ootBlob, sizeof(ootBlob));
}

// A recognisable live MM half: what an arrival leaves in gSaveContext.
void AuthorLiveHalf() {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.day = 2;
    gSaveContext.save.time = 0x8123;
    gSaveContext.save.saveInfo.playerData.rupees = 123;
    gSaveContext.save.saveInfo.playerData.health = 0x50;
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    gSaveContext.save.shipSaveInfo.rando.finalSeed = 0xA5A5F00Du;
}

bool LiveHalfIntact() {
    return memcmp(&gSaveContext, &sBefore, sizeof(SaveContext)) == 0;
}

// What MM_Sram_InitNewSave visibly authors over AuthorLiveHalf's bytes.
bool LooksLikeNewSave() {
    return gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_VANILLA &&
           gSaveContext.save.saveInfo.playerData.rupees == 0 && gSaveContext.save.shipSaveInfo.rando.finalSeed == 0;
}

// What MM_Sram_InitNewSave authors over a ZEROED SaveContext: the creation
// bootstrap memsets first, so LooksLikeNewSave's zero fields cannot tell an
// authored bootstrap from a refused one there. The new-file clock and the
// three-heart capacity can.
bool BootstrapAuthoredOverZero() {
    return LooksLikeNewSave() && gSaveContext.save.time == (u16)(CLOCK_TIME(6, 0) - 1) &&
           gSaveContext.save.saveInfo.playerData.healthCapacity == 0x30;
}

void RunTitleSetup() {
    static TitleSetupState state;
    memset(&state, 0, sizeof(state));
    state.state.running = true;
    TitleSetup_SetupTitleScreen(&state);
}

} // namespace

extern "C" int MM_NewSaveSessionGuard_RunHeadless(void) {
    printf("[TEST] mm-new-save-session-guard: MM may not author a new save over an open paired file (#564 V21)\n");

    // ---- 1. MM_Sram_InitNewSave over an open paired file is refused --------
    ClearSession();
    OpenPairedFile();
    FreezeOoTBehindMM();
    AuthorLiveHalf();
    sBefore = gSaveContext;
    MM_Sram_InitNewSave();
    NSG_ASSERT(LiveHalfIntact(),
               "leg 1: MM_Sram_InitNewSave rewrote the live MM half of an open paired file (rupees %d, saveType %d, "
               "finalSeed %08X); it must refuse over a resident combo session",
               (int)gSaveContext.save.saveInfo.playerData.rupees, (int)gSaveContext.save.shipSaveInfo.saveType,
               (unsigned)gSaveContext.save.shipSaveInfo.rando.finalSeed);
    NSG_ASSERT(Combo_ForeignPairingActive() && RsbsSave_GetActiveSlot() == kSlot && Context_HasFrozenState(GAME_OOT),
               "leg 1: a refused new save must leave the session as it was");
    printf("[TEST]   leg 1 PASS: MM_Sram_InitNewSave refused over an open paired file\n");

    // ---- 2. MM's file-select new-file path is refused, OnSaveInit included -
    ClearSession();
    OpenPairedFile();
    FreezeOoTBehindMM();
    AuthorLiveHalf();
    sBefore = gSaveContext;
    memset(&sFileSelect, 0, sizeof(sFileSelect));
    sFileSelect.buttonIndex = 0;
    memset(sSaveBuf, 0xEE, sizeof(sSaveBuf));
    SramContext sramCtx;
    memset(&sramCtx, 0, sizeof(sramCtx));
    sramCtx.saveBuf = sSaveBuf;
    const uint32_t dispatchesBefore = MM_Rando_OnSaveInitDispatchCount();
    MM_Sram_InitSave(&sFileSelect, &sramCtx);
    NSG_ASSERT(MM_Rando_OnSaveInitDispatchCount() == dispatchesBefore,
               "leg 2: MM_Sram_InitSave dispatched OnSaveInit (MM's generation hook) %u time(s) over an open paired "
               "file; MM's file select may not create a world while one is resident",
               (unsigned)(MM_Rando_OnSaveInitDispatchCount() - dispatchesBefore));
    NSG_ASSERT(LiveHalfIntact(), "leg 2: MM_Sram_InitSave rewrote the live MM half of an open paired file");
    NSG_ASSERT(sSaveBuf[0] == 0xEE && sSaveBuf[SAVE_BUFFER_SIZE_HALF] == 0xEE,
               "leg 2: MM_Sram_InitSave copied a new file into the flash buffer over an open paired file");
    printf("[TEST]   leg 2 PASS: MM's file-select new-file path refused, no OnSaveInit\n");

    // ---- 3. TitleSetup from a live paired session retires the session ------
    ClearSession();
    OpenPairedFile();
    FreezeOoTBehindMM();
    AuthorLiveHalf();
    RunTitleSetup();
    NSG_ASSERT(!Combo_ForeignPairingActive(),
               "leg 3: MM's TitleSetup left the paired identity resident (seed %u) under the bootstrap it authored; "
               "MM's return to title must retire the session as OoT's does",
               (unsigned)gComboCtx.sharedRandoSeed);
    NSG_ASSERT(RsbsSave_GetActiveSlot() == -1,
               "leg 3: MM's TitleSetup left slot %d active, so the bootstrap can be committed into the paired file",
               RsbsSave_GetActiveSlot());
    NSG_ASSERT(!Context_HasFrozenState(GAME_OOT),
               "leg 3: MM's TitleSetup left OoT's frozen blob resident for the bootstrap to be paired with");
    NSG_ASSERT(LooksLikeNewSave(), "leg 3: with the session retired, TitleSetup still authors its bootstrap");
    printf("[TEST]   leg 3 PASS: TitleSetup retired the paired session, then authored its bootstrap\n");

    // ---- 4. The arrival's boot chain keeps the session ---------------------
    ClearSession();
    OpenPairedFile();
    FreezeOoTBehindMM();
    AuthorLiveHalf();
    Entrance_SetStartupEntrance(kMMArrivalEntrance, GAME_MM);
    RunTitleSetup();
    NSG_ASSERT(Combo_ForeignPairingActive() && gComboCtx.sharedRandoSeed == kSeed,
               "leg 4: an arrival's TitleSetup pass dropped the paired identity it is on its way to hydrate");
    NSG_ASSERT(RsbsSave_GetActiveSlot() == kSlot, "leg 4: an arrival's TitleSetup pass closed the slot");
    NSG_ASSERT(Context_HasFrozenState(GAME_OOT), "leg 4: an arrival's TitleSetup pass dropped OoT's frozen blob");
    NSG_ASSERT(LooksLikeNewSave(),
               "leg 4: an arrival's TitleSetup pass must still author the bootstrap the arrival's consume overwrites");
    Entrance_ClearStartupEntrance();
    printf("[TEST]   leg 4 PASS: an arrival's boot chain keeps the session and authors its canvas\n");

    // ---- 5. No paired file open: nothing is refused or retired -------------
    // 5a. A vanilla file's session: slot open, no identity. MM's title on the
    // hot-swap first entry must not end the OoT session suspended behind it.
    ClearSession();
    RsbsSave_SetActiveSlot(kSlot);
    FreezeOoTBehindMM();
    AuthorLiveHalf();
    RunTitleSetup();
    NSG_ASSERT(Context_HasFrozenState(GAME_OOT) && RsbsSave_GetActiveSlot() == kSlot,
               "leg 5a: MM's title retired a vanilla file's session (no paired identity is resident)");
    NSG_ASSERT(LooksLikeNewSave(), "leg 5a: MM's title must author its bootstrap beside a vanilla session");
    // 5b. A staged generation with no file: identity stamped, no slot.
    ClearSession();
    gComboCtx.sourceIsRando = 1;
    gComboCtx.sharedRandoSeed = kSeed;
    gComboCtx.sharedRandoSettingsHash = kSettingsHash;
    AuthorLiveHalf();
    MM_Sram_InitNewSave();
    NSG_ASSERT(LooksLikeNewSave(), "leg 5b: a staged generation with no file open must not block MM's new save");
    NSG_ASSERT(Combo_ForeignPairingActive(), "leg 5b: MM's new save must not touch a staged identity");
    printf("[TEST]   leg 5 PASS: no paired file open, nothing refused or retired\n");

    // ---- 6. The paired creation event is exempt ---------------------------
    // The production step, not a hand-made bracket: a removed or unclosed
    // bracket in MM_Creation_AuthorBootstrap turns this leg red.
    ClearSession();
    OpenPairedFile();
    AuthorLiveHalf();
    MM_Creation_AuthorBootstrap();
    NSG_ASSERT(BootstrapAuthoredOverZero(),
               "leg 6: the creation event's bootstrap (MM_Creation_AuthorBootstrap) was refused with a paired file "
               "open (time %04X, healthCapacity %d); the creation authors this identity's MM half and must be "
               "exempt from the guard",
               (unsigned)gSaveContext.save.time, (int)gSaveContext.save.saveInfo.playerData.healthCapacity);
    AuthorLiveHalf();
    sBefore = gSaveContext;
    MM_Sram_InitNewSave();
    NSG_ASSERT(LiveHalfIntact(),
               "leg 6: the creation scope stayed open after MM_Creation_AuthorBootstrap returned; a later "
               "MM_Sram_InitNewSave authored over the open paired file");
    printf("[TEST]   leg 6 PASS: the creation event's bootstrap authors inside its scope, and the scope closes\n");

    ClearSession();
    printf("[TEST] mm-new-save-session-guard: PASS\n");
    fflush(stdout);
    return 0;
}
