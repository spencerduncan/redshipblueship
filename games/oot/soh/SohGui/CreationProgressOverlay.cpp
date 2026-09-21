/**
 * CreationProgressOverlay.cpp — paint the paired creation's progress from
 * inside the call that blocks the render thread (#582).
 *
 * ============================================================================
 * THE PROBLEM, STATED PRECISELY
 * ============================================================================
 * `OoT_RunPairedCreationEvent` runs at file select, inside
 * `OoT_Sram_InitSave` -> `Save_InitFile`, which runs inside
 * `OoT_GameState_Update` on the ONLY thread that renders: graph.c's
 * `OoT_RunFrame` is a stackless coroutine that returns once per frame, and
 * `Graph_ProcessGfxCommands` (the thing that draws and swaps) does not run
 * until the update returns. So for the whole of MM's fill -- 30 s of budget on
 * a reference host, 90 s on a slow one -- no frame is produced and no window
 * message is pumped. The player gets a frozen, "not responding" window and no
 * evidence that anything is happening.
 *
 * OoT's OWN seed generation does not have this problem: the menu hands it to a
 * worker thread (`GenerateRandomizer` -> `randoThread`) and the file-select
 * gamestate keeps rendering with its `generating` caption. That route is NOT
 * available here, and the reason is not squeamishness: MM's generation writes
 * `gSaveContext`, which is one buffer both games reinterpret
 * (src/common/unified_save.c), and file select READS `gSaveContext` every frame
 * to draw its file rows. Running the paired creation on a worker while the game
 * thread keeps drawing file select is a live data race on a 136 KB buffer whose
 * halves belong to two different games -- and the creation event exists exactly
 * to stop MM's world leaking into the OoT file being written. So the honest
 * option is the other one: keep the creation where it is and PRESENT FROM
 * INSIDE IT.
 *
 * ============================================================================
 * WHY PRESENTING FROM INSIDE THE BLOCKING CALL IS SAFE HERE
 * ============================================================================
 * This is not a new trick in this tree. `OTRGlobals::RunExtract` already drives
 * a live ImGui progress bar from inside a blocking loop, with this exact
 * sequence:
 *
 *     wnd->HandleEvents();  if (!wnd->IsFrameReady()) continue;
 *     gui->StartDraw(); fast->StartFrame(); fast->RunGuiOnly();
 *     ...ImGui...
 *     gui->EndDraw(); fast->EndFrame();
 *
 * Five properties make it safe to run that mid-update rather than only at boot:
 *
 * 1. `RunGuiOnly()` (libultraship/src/fast/interpreter.cpp) is `Run()` minus the
 *    display-list execution. It touches the interpreter's own RSP/RDP state and
 *    NOT OoT's `gfxCtx`, and `Run()` re-initialises exactly that state
 *    (`SpReset()`, viewport/scissor, framebuffer parameters) at its top. So the
 *    frame the outer update is in the middle of building is unaffected: it has
 *    not been submitted yet, and when it is, `Run()` starts from a clean slate.
 * 2. The ImGui frame is balanced inside one call. At the point the creation seam
 *    runs, the outer frame's `gui->StartDraw()` has NOT happened yet (it happens
 *    in `Graph_ProcessGfxCommands`, after the update returns), so there is no
 *    enclosing ImGui frame to nest inside. A re-entrancy latch below refuses
 *    anyway rather than trusting that analysis forever.
 * 3. Nothing here blocks. `IsFrameReady()` is polled, never waited on: SDL2's
 *    backend returns true unconditionally and DXGI's can decline, and a decline
 *    means "skip this paint", not "spin". A minimised window therefore costs
 *    nothing and cannot deadlock -- the creation keeps running and the stderr
 *    leg keeps its record.
 * 4. `HandleEvents()` is pumped, which is what keeps the OS from marking the
 *    window unresponsive, and its only destructive event (close) merely sets a
 *    flag (`GfxWindowBackendSDL2::Close`). We stop painting once that flag is
 *    down rather than drawing into a window on its way out.
 * 5. This only ever CONTINUES a live render loop; it never starts one. The
 *    `rando` test tier constructs a real Fast3dWindow and then drives generation
 *    directly, without ever running the game loop, so a frame pumped from inside
 *    a test's creation call would be this file's own invention. A counter
 *    incremented where a game frame demonstrably completed
 *    (`OoT_Graph_HasPresentedFrame`, OTRGlobals.cpp) is the precondition, so
 *    those rows keep exactly the behaviour they have today: the stderr leg.
 *
 * ============================================================================
 * THE ONE HAZARD THAT NEEDED A FIX, NOT A COMMENT
 * ============================================================================
 * `gui->StartDraw()`/`EndDraw()` draw the menu and every registered floating
 * window -- including SoH's item and check trackers, which read `gSaveContext`
 * on every draw. During the creation event `gSaveContext` holds MM's world
 * reinterpreted through OoT's layout, so a player who leaves the item tracker
 * open would have had it read MM bytes as OoT inventory: at best nonsense, at
 * worst an out-of-range index into a texture table, in the middle of creating
 * their file.
 *
 * So every painted frame runs under `OoT_Creation_PaintWithOoTSaveVisible`,
 * which swaps OoT's snapshot back in for the duration of the paint and MM's
 * in-flight bytes back afterwards. The swap is whole-buffer and therefore
 * invisible to the fill, and it makes the guarantee simple to state: a pumped
 * frame sees exactly the `gSaveContext` bytes the surrounding file-select frames
 * see.
 *
 * ============================================================================
 * WHAT THIS FILE DOES NOT DO
 * ============================================================================
 * It does not touch creation order, the RNG stream, or any number the fill
 * reads. It does not read a wall clock to decide anything (#581 section 2a): the
 * repaint interval is measured in milliseconds the FILL already computed for its
 * own timeout check, and the presentation time each paint costs is credited back
 * to the fill's budget at the call site, so a host with a window gets the same
 * generation headroom as a headless one.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "CreationProgressOverlay.h"

#include <imgui.h>
#include <fast/Fast3dWindow.h>
#include <libultraship/bridge.h>
#include <libultraship/bridge/windowbridge.h>
#include <libultraship/libultraship.h>
#include <ship/Context.h>
#include <ship/window/gui/Gui.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>

// src/common is on soh's include path (the same way ForeignItemsSingleExe.cpp
// reaches context.h); the header carries its own extern "C" guard.
#include "gen_progress_overlay.h"

// The creation seam's gSaveContext bracket (ForeignItemsSingleExe.cpp) and the
// render loop's own liveness counter (OTRGlobals.cpp). Declared here rather than
// included because OTRGlobals.h keeps most of its C entry points inside
// `#ifndef __cplusplus`, and this file needs exactly these two symbols.
extern "C" void OoT_Creation_PaintWithOoTSaveVisible(void (*paint)(void));
extern "C" int OoT_Graph_HasPresentedFrame(void);

namespace {

/**
 * The thread allowed to touch the renderer, latched at install time.
 *
 * NOT a nicety. The same phase channel is reported into by OoT's menu-side
 * generation, which runs on `randoThread` (randomizer.cpp). ImGui, the GL
 * context and the Fast3D interpreter are all single-threaded; a sink that
 * painted from there would corrupt the renderer while the main thread was
 * drawing the menu. The latch makes "only the creation seam paints" a checked
 * fact rather than a claim about who calls what.
 */
std::thread::id gRenderThread{};
bool gInstalled = false;
/** Refuses a nested paint. See property 2 in the file comment. */
bool gPainting = false;
bool gWarnedNoWindow = false;
bool gWarnedOffThread = false;
bool gWarnedNoFrames = false;
/**
 * Frames this file has ACTUALLY presented -- incremented where the whole
 * StartDraw / StartFrame / RunGuiOnly / EndDraw / EndFrame sequence completed,
 * not where the painter was merely invoked.
 *
 * The distinction is the whole value of the number. The state machine's own
 * ComboGenOverlay_PaintCount() counts painter CALLS, which a row could read
 * while every one of them bailed at a guard -- exactly the "green test asserting
 * nothing" shape this surface is supposed to end. This counter can only move
 * when a frame went out.
 */
uint32_t gPresentedFrames = 0;
/**
 * TEST SEAM. Stands in for "the game loop is running", and nothing in a shipping
 * path writes it -- the same shape and the same rule as
 * Combo_GenBudget_SetHostScalePercentOverride (gen_budget.h).
 *
 * It exists because the `rando` tier has a REAL Fast3dWindow and no game loop,
 * which is the only place the whole pump can be exercised at all: without it the
 * paint path would ship with no automated evidence that this renderer can
 * present a gui-only frame from inside a blocking call.
 */
bool gForceRenderLoopForTest = false;

/** The view being painted. A file static because the gSaveContext bracket takes
 *  a plain `void(*)()` -- it is a C seam and giving it a capture would mean
 *  giving it a std::function. Only ever read on the render thread, under
 *  gPainting. */
const ComboGenOverlayView* gPaintingView = nullptr;

const char* kWindowName = "Creating your paired world";

/**
 * The ImGui half. Runs with OoT's gSaveContext visible and an ImGui frame
 * already open.
 *
 * A PLAIN WINDOW PLUS A DIM RECT, NOT A BeginPopupModal, and the reason is the
 * teardown. A popup's open/closed flag lives in the ImGui context BETWEEN our
 * pumped frames and the real game frames, so a creation that ended without our
 * terminal paint reaching `CloseCurrentPopup` would leave a dead modal sitting
 * over file select, and the public API has no "close that popup by name". A
 * window is submitted or it is not: when the state machine stops being SHOWN we
 * simply stop drawing, and there is no residue to clean up. The dimming rect
 * carries the modal MEANING (nothing else on screen is actionable) without the
 * modal state.
 */
void DrawOverlayContents() {
    const ComboGenOverlayView* view = gPaintingView;
    if (view == nullptr || view->state != (uint8_t)RSBS_GENOVERLAY_SHOWN) {
        // Not SHOWN covers both terminal paints. On success file select must be
        // clean the instant the file exists; on failure the creation seam's own
        // toast (OoT_Creation_ReportFailureAtFileSelect) owns the screen and
        // must not be covered by a dead progress bar. Drawing nothing is the
        // whole teardown.
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
        IM_COL32(0, 0, 0, 170));

    ImGui::SetNextWindowPos(
        ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f), ImGuiCond_Always,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
    if (ImGui::Begin(kWindowName, nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoDocking)) {
        ImGui::Text("%s", view->caption);

        // The elapsed counter is the honest part of the surface: the fraction is
        // a phase position (gen_progress_overlay.h says so), the seconds are
        // measured. The budget is shown only while there is one, because "0 s
        // budget" would read as a broken bar rather than as "this phase is not
        // the timed one".
        if (view->budgetMs > 0) {
            ImGui::Text("%.1f s elapsed  -  this attempt may take up to %.0f s", (double)view->elapsedMs / 1000.0,
                        (double)view->budgetMs / 1000.0);
        } else {
            ImGui::Text("%.1f s elapsed", (double)view->elapsedMs / 1000.0);
        }

        ImGui::ProgressBar(view->fraction, ImVec2(520.0f, 32.0f), "");
        ImGui::TextDisabled("Both halves of this world are being generated now, under one frozen identity.");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

/** Step 3 of the paint: everything that needs an ImGui frame, with OoT's save
 *  bytes visible. Plain function so the C bracket can take its address. */
void PaintUnderOoTSave() {
    DrawOverlayContents();
}

/**
 * Present ONE gui-only frame. The sequence is RunExtract's, and every guard is
 * justified in the file comment.
 */
bool PresentOneGuiFrame(const ComboGenOverlayView* view) {
    if (std::this_thread::get_id() != gRenderThread) {
        if (!gWarnedOffThread) {
            gWarnedOffThread = true;
            fprintf(stderr, "[OoT] creation overlay: a progress report arrived off the render thread — stderr only "
                            "(this is the menu-side worker; the menu draws its own spinner)\n");
        }
        return false;
    }
    if (gPainting) {
        return false;
    }
    if (!WindowIsRunning()) {
        // The player closed the window mid-creation. The backend's Close() only
        // sets a flag (GfxWindowBackendSDL2::Close), so this is safe to observe
        // and the right thing to do with it is stop drawing into a window on its
        // way out; the creation finishes and the game loop exits after it.
        return false;
    }
    if (!gForceRenderLoopForTest && !OoT_Graph_HasPresentedFrame()) {
        // NOT A LIVE RENDER LOOP. A unit-test harness constructs a real
        // Fast3dWindow (the `rando` tier needs one) and never runs a frame
        // through it, so pumping one from inside a test's creation call would be
        // this file inventing a render loop rather than continuing one. The
        // stderr leg is the whole surface there, which is what those rows read.
        if (!gWarnedNoFrames) {
            gWarnedNoFrames = true;
            fprintf(stderr, "[OoT] creation overlay: no frame has been presented in this process — creation "
                            "progress stays on stderr\n");
        }
        return false;
    }

    std::shared_ptr<Ship::Context> ctx = Ship::Context::GetInstance();
    if (ctx == nullptr) {
        return false;
    }
    std::shared_ptr<Ship::Window> window = ctx->GetWindow();
    std::shared_ptr<Fast::Fast3dWindow> fast = std::dynamic_pointer_cast<Fast::Fast3dWindow>(window);
    if (fast == nullptr) {
        // A headless row, or a backend that is not Fast3D. The channel's stderr
        // leg is the whole surface there, which is what every existing lock
        // reads anyway.
        if (!gWarnedNoWindow) {
            gWarnedNoWindow = true;
            fprintf(stderr, "[OoT] creation overlay: no Fast3D window — creation progress stays on stderr\n");
        }
        return false;
    }
    std::shared_ptr<Ship::Gui> gui = fast->GetGui();
    if (gui == nullptr) {
        return false;
    }

    gPainting = true;
    gPaintingView = view;

    // Pump OS messages first: this is what stops Windows painting the window
    // white and calling it unresponsive, and it is why a bar drawn from here is
    // actually visible rather than queued behind a stalled message loop.
    fast->HandleEvents();

    bool presented = false;
    if (fast->IsFrameReady()) {
        gui->StartDraw();
        fast->StartFrame();
        fast->RunGuiOnly();
        OoT_Creation_PaintWithOoTSaveVisible(&PaintUnderOoTSave);
        gui->EndDraw();
        fast->EndFrame();
        presented = true;
        gPresentedFrames++;
    }

    gPaintingView = nullptr;
    gPainting = false;
    return presented;
}

void OverlayPainter(const ComboGenOverlayView* view) {
    PresentOneGuiFrame(view);
}

void CreationProgressSink(const ComboGenProgress* progress) {
    // The state machine paints through the painter above; feeding it is all this
    // sink does. It runs on whatever thread reported, and the painter's latch is
    // what keeps a worker thread's report off the renderer.
    ComboGenOverlay_OnProgress(progress);
}

} // namespace

extern "C" void OoT_CreationProgressOverlay_Install(void) {
    gRenderThread = std::this_thread::get_id();
    if (gInstalled) {
        return;
    }
    gInstalled = true;
    ComboGenOverlay_SetPainter(&OverlayPainter);
    Combo_GenProgress_SetDisplaySink(&CreationProgressSink);
}

extern "C" uint32_t OoT_CreationProgressOverlay_TestPresentedFrames(void) {
    return gPresentedFrames;
}

extern "C" int OoT_CreationProgressOverlay_TestPresentOnce(void) {
    // Install first, so the render-thread latch is this thread, then stand in for
    // the game loop and try to present exactly one frame.
    //
    // WHAT THE RETURN VALUE IS FOR. The `rando` tier's renderer may or may not be
    // able to present at all (software GL under Xvfb, a headless CI box, a
    // backend that declines frames). A row that demanded a paint would then go
    // red for a reason that has nothing to do with this code. So the row asks
    // FIRST: if this returns 0, presenting is simply not available here and the
    // overlay leg skips; if it returns 1, the renderer demonstrably works and a
    // creation that then paints NOTHING is a real wiring defect.
    //
    // The force flag stays set on purpose: the creation the caller is about to
    // run is the thing under test, and it has to reach the same pump.
    OoT_CreationProgressOverlay_Install();
    gForceRenderLoopForTest = true;

    ComboGenOverlayView probe;
    memset(&probe, 0, sizeof(probe));
    probe.state = (uint8_t)RSBS_GENOVERLAY_SHOWN;
    probe.phase = (uint8_t)RSBS_GENPHASE_MM_FILL;
    probe.attempt = 1;
    probe.maxAttempts = 1;
    probe.fraction = 0.5f;
    snprintf(probe.caption, sizeof(probe.caption), "Creation overlay self-probe");
    return PresentOneGuiFrame(&probe) ? 1 : 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
