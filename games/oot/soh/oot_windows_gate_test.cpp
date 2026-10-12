/**
 * ROM-free lock for OoT's active-game window gate (#797). CTest label "redship", row OoTWindowsGate
 * (oot-windows-gate) in src/common/test_runner.cpp, which also runs the source scan over
 * SohGui::SetupGuiElements before calling this.
 *
 * gSaveContext is one shared buffer, so an OoT window that runs while Majora's Mask is the running game reads MM's
 * bytes through OoT's SaveContext layout and, for the Save Editor, writes them: DrawInfoTab clamps health and assigns
 * magicCapacity / magic on every drawn frame. Eight SoH windows touch OoT save or play state, and each is reached three
 * ways: Draw() from the Gui's per-frame window loop, Update() from its unconditional per-frame update, and
 * DrawElement() straight from the Port Menu when the window is hidden and its row embeds (SohGui/Menu.cpp), which
 * bypasses visibility entirely.
 *
 * The hard tripwire: this harness has NO ImGui context and NO OTRGlobals::Instance, and every window's visibility
 * CVar is forced on before construction (the ctor latches it; Show() would dereference the null Ship::Window). So any
 * of the three paths that is not gated reaches ImGui or dereferences a null pointer and kills the process. Surviving
 * the calls with Majora's Mask (and with no game) running is the assertion. The calls run DrawElement() first, so an
 * ungated build dies on the embed path, the one that writes MM's save with the window closed.
 *
 * That tripwire only locks paths whose body reaches ImGui, and it never runs a body under OoT. The real windows'
 * UpdateElement bodies are empty or wait for a click, so the Update leg is locked by a counting spy base window instead
 * (GateSpy below): through OoTActiveGated<GateSpy>, each of the three bodies must run exactly once under OoT and never
 * under Majora's Mask or with no game running. Removing any one of the wrapper's three overrides fails that check.
 *
 * The windows are constructed without Gui::AddGuiWindow on purpose: AddGuiWindow calls Init(), and the check
 * tracker's InitElement dereferences a null SaveManager::Instance here. The Message Viewer is leaked on purpose: its
 * InitElement allocates the buffers its destructor frees, and InitElement never runs.
 *
 * The window types and CVars below mirror SohGui::SetupGuiElements; the source scan in test_runner.cpp holds that
 * function to exactly these eight wrapped sites.
 *
 * #798 extends the row to OoT's non-window paths into the shared save. DebugConsole_Init registers every OoT console
 * command through one gating wrapper (debugconsole.cpp's CMD_REGISTER), so the row registers them here and drives them
 * through Ship::Console::Run, the path the Console window, Ctrl+R and Sail take. A memcmp canary over gSaveContext
 * (filled with a non-zero pattern standing in for Majora's Mask's save) must be unchanged after each of the seven
 * commands #798 names under Majora's Mask, then after every command DebugConsole_Init added, bare, under Majora's Mask
 * and with no game; under OoT, `item` and `map` must reach their handlers. The Giant's Knife toggle's callback
 * (SohGui::OnFixBrokenGiantsKnifeToggled) must not write the save or dereference the NULL play state with Majora's Mask
 * running, nor with OoT running outside Play (its file select).
 *
 * #826 adds to the pass-through leg: with OoT running outside Play the gate opens, and `give_item` and `entrance` must
 * refuse on the NULL OoT_gPlayState in their handlers (non-zero, the refusal in their output, the save unchanged)
 * instead of dereferencing it.
 *
 * #909 adds one more: with OoT running and a paired file loaded, `gen_rando` must refuse like the Generate Randomizer
 * button (file select only, no save loaded): non-zero with the refusal in its output. That refusal is the lock. The
 * identity, save and RandoGenerating checks beside it are smoke checks only: GenerateRandomizer runs the generation on
 * a worker thread the leg never awaits, so they could not catch the re-stamp by themselves.
 *
 * #913: on the file select with no file loaded, bare `gen_rando` while a generation is already running
 * (RandoGenerating set) must fail with "Rando generation already in progress" in its output instead of reading
 * args[1] past the end of its one-element argument vector.
 *
 * #930: a command whose numeric argument std::stoi cannot parse (`gen_rando 99999999999` overflows int, `item 0
 * notanumber` is no number in a handler with no try) must fail with the gating wrapper's error line in its output
 * instead of throwing out of Ship::Console::Run and terminating the process.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <libultraship/libultraship.h>
#include <ship/Context.h>
#include <ship/debug/Console.h>
#include <ship/window/gui/GuiWindow.h>

#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/OoTActiveGated.h"
#include "soh/Enhancements/debugconsole.h"
#include "soh/Enhancements/debugger/MessageViewer.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"
#include "soh/cvar_prefixes.h"
#include "context.h"

extern "C" {
#include <z64.h>
#include "variables.h"
#include "macros.h"
extern PlayState* OoT_gPlayState;
}

namespace SohGui {
// SohMenuEnhancements.cpp: the "Fix Broken Giant's Knife Bug" toggle's callback (#798).
void OnFixBrokenGiantsKnifeToggled();
} // namespace SohGui

namespace {

#define OWG_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            fflush(stdout);                                               \
            return 1;                                                     \
        }                                                                 \
    } while (0)

struct GatedWindow {
    const char* name;
    const char* visibilityCVar;
    std::shared_ptr<Ship::GuiWindow> window;
};

// The trackers' layout CVars: a floating tracker or a "only while paused" one takes a different early branch in
// DrawElement, so they are cleared to leave each window on its default path into ImGui.
const char* kTrackerLayoutCVars[] = {
    CVAR_TRACKER_ITEM("WindowType"),      CVAR_TRACKER_ITEM("ShowOnlyPaused"), CVAR_TRACKER_CHECK("WindowType"),
    CVAR_TRACKER_CHECK("ShowOnlyPaused"), CVAR_TRACKER_ENTRANCE("WindowType"), CVAR_TRACKER_ENTRANCE("ShowOnlyPaused"),
};

#define OWG_SPY_CVAR CVAR_WINDOW("OoTWindowsGateSpy")

// A base window that counts how often each of the three bodies runs and touches nothing else, so OoTActiveGated<> can
// be driven under every game, OoT included. The eight real windows cannot lock the Update leg: their UpdateElement
// bodies are empty or act only after a click, so an ungated Update() on them returns harmlessly. Nor can they show
// pass-through, because under OoT their bodies reach ImGui, which this harness does not have. The spy overrides Draw()
// itself (GuiWindow::Draw would reach ImGui), and that override is what OoTActiveGated<>::Draw forwards to.
class GateSpy : public Ship::GuiWindow {
  public:
    GateSpy(const std::string& consoleVariable, const std::string& name) : Ship::GuiWindow(consoleVariable, name) {
    }

    int draws = 0;
    int drawElements = 0;
    int updateElements = 0;

    void Draw() override {
        draws++;
    }
    void DrawElement() override {
        drawElements++;
    }

  protected:
    void InitElement() override {
    }
    void UpdateElement() override {
        updateElements++;
    }
};

// Drives the three ways into a window through the wrapper under one game, through the GuiWindow pointer the Gui and the
// Port Menu hold, and counts the failures: each body must run once when wantBodies, and never otherwise.
int RunSpy(GameId game, const char* gameName, bool wantBodies) {
    auto spy = std::make_shared<OoTActiveGated<GateSpy>>(OWG_SPY_CVAR, "OoTWindowsGate spy");
    std::shared_ptr<Ship::GuiWindow> asWindow = spy;
    Context_SetCurrentGame(game);
    asWindow->DrawElement();
    asWindow->Update();
    asWindow->Draw();
    printf("[TEST] oot-windows-gate: spy, %s running: DrawElement body ran %d, UpdateElement body ran %d, Draw body "
           "ran %d\n",
           gameName, spy->drawElements, spy->updateElements, spy->draws);
    const int want = wantBodies ? 1 : 0;
    int failures = 0;
    if (spy->drawElements != want) {
        printf("[TEST] FAIL: oot-windows-gate: %s running, the menu-embed DrawElement() body ran %d time(s), want %d\n",
               gameName, spy->drawElements, want);
        failures++;
    }
    if (spy->updateElements != want) {
        printf("[TEST] FAIL: oot-windows-gate: %s running, the per-frame Update() body ran %d time(s), want %d\n",
               gameName, spy->updateElements, want);
        failures++;
    }
    if (spy->draws != want) {
        printf("[TEST] FAIL: oot-windows-gate: %s running, the Draw() body ran %d time(s), want %d\n", gameName,
               spy->draws, want);
        failures++;
    }
    fflush(stdout);
    return failures;
}

// ---- #798: the OoT console commands and the Giant's Knife toggle -------------------------------------------------

// Fills the shared buffer with a non-zero pattern that stands in for Majora's Mask's live save, so any write through
// OoT's layout (a constant, a zero, an item id) shows up as a changed byte.
void FillSaveCanary(std::vector<uint8_t>& canary) {
    memset(&gSaveContext, 0x5A, sizeof(gSaveContext));
    canary.assign(reinterpret_cast<const uint8_t*>(&gSaveContext),
                  reinterpret_cast<const uint8_t*>(&gSaveContext) + sizeof(gSaveContext));
}

// Returns the first byte offset where gSaveContext differs from the canary, or -1.
long FirstSaveDiff(const std::vector<uint8_t>& canary) {
    const uint8_t* live = reinterpret_cast<const uint8_t*>(&gSaveContext);
    for (size_t i = 0; i < canary.size(); i++) {
        if (live[i] != canary[i]) {
            return (long)i;
        }
    }
    return -1;
}

// The seven commands #798 names: each writes gSaveContext or dereferences a NULL OoT_gPlayState when it runs. The
// last three reach the Console window (INFO_MESSAGE), the item table or OoT_gPlayState->..., all absent here, so an
// ungated build dies inside them; they run only once the four silent writers above them have passed.
struct ConsoleProbe {
    const char* line;
    bool diesUngated;
};
const ConsoleProbe kSaveWriters[] = {
    { "map", false },     { "bottle milk 1", false },      { "bItem 5", false },   { "item 0 3", false },
    { "rupee 50", true }, { "give_item vanilla 1", true }, { "entrance 0", true },
};

const char* GameName(GameId game) {
    return game == GAME_OOT ? "Ocarina of Time" : (game == GAME_MM ? "Majora's Mask" : "no game");
}

// Runs one console line through Ship::Console (the path the Console window, Ctrl+R and Sail use) with a non-OoT game
// running, and counts the failures: the command must be refused (non-zero, the refusal in its output) and leave
// every byte of the shared save unchanged.
int RunRefusedCommand(const std::shared_ptr<Ship::Console>& console, const char* line, GameId game,
                      const std::vector<uint8_t>& canary) {
    std::string output;
    const int32_t rc = console->Run(line, &output);
    int failures = 0;
    const long diff = FirstSaveDiff(canary);
    if (diff >= 0) {
        printf("[TEST] FAIL: oot-windows-gate: `%s` with %s running changed gSaveContext (first differing byte at "
               "0x%lx): an OoT console command wrote the other game's save (#798)\n",
               line, GameName(game), diff);
        failures++;
        memcpy(&gSaveContext, canary.data(), canary.size());
    }
    if (rc == 0 || output.find("only runs while Ocarina of Time") == std::string::npos) {
        printf("[TEST] FAIL: oot-windows-gate: `%s` with %s running returned %d with output \"%s\", want the "
               "OoT-only refusal\n",
               line, GameName(game), (int)rc, output.c_str());
        failures++;
    }
    fflush(stdout);
    return failures;
}

int RunConsoleAndKnifeGate() {
    auto console = Ship::Context::GetInstance()->GetConsole();
    OWG_ASSERT(console != nullptr, "Ship::Console missing: the console legs need it");

    // Register OoT's commands here and keep exactly the names DebugConsole_Init added.
    std::set<std::string> before;
    for (const auto& [name, entry] : console->GetCommands()) {
        before.insert(name);
    }
    OWG_ASSERT(before.count("gen_rando") == 0,
               "OoT's console commands were registered before this row: it cannot isolate DebugConsole_Init's set");
    DebugConsole_Init();
    std::vector<std::string> ootCommands;
    for (const auto& [name, entry] : console->GetCommands()) {
        if (before.count(name) == 0) {
            ootCommands.push_back(name);
        }
    }
    for (const ConsoleProbe& probe : kSaveWriters) {
        const std::string name = std::string(probe.line).substr(0, std::string(probe.line).find(' '));
        if (!console->HasCommand(name)) {
            printf("[TEST] FAIL: oot-windows-gate: DebugConsole_Init did not register `%s`\n", name.c_str());
            return 1;
        }
    }
    printf("[TEST] oot-windows-gate: DebugConsole_Init registered %zu OoT console commands\n", ootCommands.size());
    fflush(stdout);

    std::vector<uint8_t> saved(reinterpret_cast<const uint8_t*>(&gSaveContext),
                               reinterpret_cast<const uint8_t*>(&gSaveContext) + sizeof(gSaveContext));
    PlayState* const savedPlay = OoT_gPlayState;
    const GameId prevGame = Context_GetCurrentGame();
    // Majora's Mask running: OoT was suspended, and OoT_Graph_ResetRunFrameContext nulled its play state.
    OoT_gPlayState = nullptr;
    std::vector<uint8_t> canary;
    FillSaveCanary(canary);
    int failures = 0;

    // ---- 5. The seven named commands under Majora's Mask: the memcmp canary ----------------------------------------
    Context_SetCurrentGame(GAME_MM);
    for (const ConsoleProbe& probe : kSaveWriters) {
        if (probe.diesUngated && failures > 0) {
            printf("[TEST] oot-windows-gate: not running `%s`: the commands above were not refused, so this one "
                   "would kill the process\n",
                   probe.line);
            continue;
        }
        printf("[TEST] oot-windows-gate: Majora's Mask running, console `%s`\n", probe.line);
        fflush(stdout);
        failures += RunRefusedCommand(console, probe.line, GAME_MM, canary);
    }

    // ---- 6. Every command DebugConsole_Init registered, bare, under Majora's Mask and with no game ------------------
    // The gate is one wrapper at registration, so every OoT command must refuse, not only the seven above.
    if (failures == 0) {
        const GameId inactiveGames[] = { GAME_MM, GAME_NONE };
        int refused = 0;
        for (GameId game : inactiveGames) {
            Context_SetCurrentGame(game);
            for (const std::string& name : ootCommands) {
                const int f = RunRefusedCommand(console, name.c_str(), game, canary);
                failures += f;
                refused += f == 0 ? 1 : 0;
            }
        }
        printf("[TEST] oot-windows-gate: %d of %zu OoT console commands refused under Majora's Mask and with no game, "
               "gSaveContext unchanged\n",
               refused, ootCommands.size() * 2);
    }

    // ---- 7. Pass-through under OoT: the handlers are reachable through Ship::Console and the gate opens -------------
    if (failures == 0) {
        Context_SetCurrentGame(GAME_OOT);
        std::string output;
        const int32_t itemRc = console->Run("item 0 3", &output);
        const int32_t mapRc = console->Run("map", &output);
        printf("[TEST] oot-windows-gate: Ocarina of Time running: `item 0 3` returned %d (items[0] = %d), `map` "
               "returned %d (gameMode = %d, seqId = 0x%X)\n",
               (int)itemRc, (int)gSaveContext.inventory.items[0], (int)mapRc, (int)gSaveContext.gameMode,
               (unsigned)gSaveContext.seqId);
        if (itemRc != 0 || gSaveContext.inventory.items[0] != 3 || mapRc != 0 ||
            gSaveContext.gameMode != GAMEMODE_NORMAL || gSaveContext.seqId != 0xFF) {
            printf("[TEST] FAIL: oot-windows-gate: with Ocarina of Time running the console gate did not pass `item` "
                   "and `map` through to their handlers\n");
            failures++;
        }
        FillSaveCanary(canary);

        // #826: OoT running but not in Play (its title screen or file select), so the gate opens and OoT_gPlayState is
        // still NULL. entrance and give_item must refuse in their handlers instead of dereferencing it. This harness
        // does not run OTRGlobals, which is what creates ItemTableManager::Instance, so give_item's vanilla item-table
        // lookup would fault on that NULL instance before it reached the play state. A harness-local instance stands in
        // for it (an empty MOD_NONE table: the lookup returns GET_ITEM_NONE) and the lookup is exercised directly
        // first, so an unguarded give_item gets past it and faults in GiveItemEntryWithoutActor on the NULL play state
        // instead.
        ItemTableManager* const savedItemTables = ItemTableManager::Instance;
        std::unique_ptr<ItemTableManager> harnessItemTables;
        if (ItemTableManager::Instance == nullptr) {
            harnessItemTables = std::make_unique<ItemTableManager>();
            harnessItemTables->AddItemTable(MOD_NONE);
            ItemTableManager::Instance = harnessItemTables.get();
        }
        const GetItemEntry lookedUp = ItemTableManager::Instance->RetrieveItemEntry(MOD_NONE, 1);
        printf(
            "[TEST] oot-windows-gate: item-table lookup (MOD_NONE, 1) returned getItemId %d through the %s instance\n",
            (int)lookedUp.getItemId, harnessItemTables != nullptr ? "harness" : "real");
        fflush(stdout);
        const char* const kPlayStateCommands[] = { "entrance 0", "give_item vanilla 1" };
        for (const char* line : kPlayStateCommands) {
            printf("[TEST] oot-windows-gate: Ocarina of Time running, no play state, console `%s`\n", line);
            fflush(stdout);
            std::string playOutput;
            const int32_t rc = console->Run(line, &playOutput);
            const long diff = FirstSaveDiff(canary);
            printf("[TEST] oot-windows-gate: `%s` returned %d with output \"%s\"\n", line, (int)rc, playOutput.c_str());
            if (rc == 0 || playOutput.find("OoT_gPlayState == nullptr") == std::string::npos || diff >= 0) {
                printf("[TEST] FAIL: oot-windows-gate: `%s` with Ocarina of Time running and no play state did not "
                       "refuse with \"OoT_gPlayState == nullptr\" and an unchanged gSaveContext (first differing "
                       "byte %ld) (#826)\n",
                       line, diff);
                failures++;
                memcpy(&gSaveContext, canary.data(), canary.size());
            }
        }
        ItemTableManager::Instance = savedItemTables;
        fflush(stdout);
    }

    // ---- 7b. #909: gen_rando with a paired file loaded --------------------------------------------------------------
    // The Generate Randomizer button is disabled unless OoT is on its file select with no save loaded (its PreFunc in
    // SohMenuRandomizer.cpp), and that gate is what keeps the paired world's one identity stamp site (the creation
    // freeze in Playthrough_Init) unreachable while a file is loaded. gen_rando reaches the same generation from the
    // console, so it must refuse under the same predicate. The lock is the refusal: a non-zero return with the refusal
    // text in its output (Ship::Console::Run logs it). The gComboCtx identity, gSaveContext canary and RandoGenerating
    // comparisons below are smoke checks only, not locks: GenerateRandomizer starts the generation on a worker thread
    // (randoThread) and returns at once, and this leg never joins or waits for it, so even an unrefused generation
    // could not have reached Playthrough_Init's freeze when they are read, and RandoGenerating flips only if the worker
    // has already run its first line. An unrefused gen_rando starts that worker, which dereferences this harness's
    // absent window on its first lines and kills the process.
    if (failures == 0) {
        Context_SetCurrentGame(GAME_OOT);
        FillSaveCanary(canary);
        gSaveContext.gameMode = GAMEMODE_NORMAL;
        gSaveContext.fileNum = 0;
        canary.assign(reinterpret_cast<const uint8_t*>(&gSaveContext),
                      reinterpret_cast<const uint8_t*>(&gSaveContext) + sizeof(gSaveContext));
        const ComboContext savedCombo = gComboCtx;
        gComboCtx.sourceIsRando = true;
        gComboCtx.sharedRandoSeed = 0x0909C0DEu;
        gComboCtx.sharedRandoSettingsHash = 0x5E7790A9u;
        gComboCtx.mmProfileDigest = 0x3D16E909u;
        gComboCtx.comboSettingsHash = 0xC0B0909Au;
        std::vector<uint8_t> identity(reinterpret_cast<const uint8_t*>(&gComboCtx),
                                      reinterpret_cast<const uint8_t*>(&gComboCtx) + sizeof(gComboCtx));
        const int32_t generatingBefore = CVarGetInteger(CVAR_GENERAL("RandoGenerating"), 0);
        const char* const kGenRandoLines[] = { "gen_rando 909", "gen_rando" };
        for (const char* line : kGenRandoLines) {
            printf("[TEST] oot-windows-gate: Ocarina of Time running, paired file loaded (gameMode %d, fileNum %d, "
                   "seed %08X), console `%s`\n",
                   (int)gSaveContext.gameMode, (int)gSaveContext.fileNum, (unsigned)gComboCtx.sharedRandoSeed, line);
            fflush(stdout);
            std::string genOutput;
            const int32_t rc = console->Run(line, &genOutput);
            const long saveDiff = FirstSaveDiff(canary);
            const bool identityKept = memcmp(&gComboCtx, identity.data(), identity.size()) == 0;
            const int32_t generatingAfter = CVarGetInteger(CVAR_GENERAL("RandoGenerating"), 0);
            printf("[TEST] oot-windows-gate: `%s` returned %d with output \"%s\" (paired identity %s, seed %08X, "
                   "RandoGenerating %d)\n",
                   line, (int)rc, genOutput.c_str(), identityKept ? "unchanged" : "CHANGED",
                   (unsigned)gComboCtx.sharedRandoSeed, (int)generatingAfter);
            if (rc == 0 || genOutput.find("only runs on the file select") == std::string::npos || !identityKept ||
                saveDiff >= 0 || generatingAfter != generatingBefore) {
                printf("[TEST] FAIL: oot-windows-gate: `%s` with a paired file loaded did not refuse with \"only runs "
                       "on the file select\", leaving the paired identity, gSaveContext (first differing byte %ld) and "
                       "RandoGenerating unchanged (#909)\n",
                       line, saveDiff);
                failures++;
                memcpy(&gComboCtx, identity.data(), identity.size());
                memcpy(&gSaveContext, canary.data(), canary.size());
            }
        }
        gComboCtx = savedCombo;
        fflush(stdout);
    }

    // ---- 7c. #913: bare gen_rando while a generation is already running ---------------------------------------------
    // On OoT's file select with no file loaded the #909 refusal lets gen_rando through, and GenerateRandomizer()
    // returns false without starting a worker while RandoGenerating is set (a generation in progress, or one an
    // exception left stuck). The inherited handler then fell through to std::stoi(args[1]) on the one-element argument
    // vector, an out-of-range read. Bare gen_rando must instead fail with SoH's own "already in progress" error in its
    // output, start nothing and leave RandoGenerating set. Unfixed, the read kills this process: on the garbage
    // args[1], or, if that happens to parse, in ERROR_MESSAGE's dereference of this harness's absent Console window.
    if (failures == 0) {
        Context_SetCurrentGame(GAME_OOT);
        gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
        gSaveContext.fileNum = 0;
        OoT_gPlayState = nullptr;
        canary.assign(reinterpret_cast<const uint8_t*>(&gSaveContext),
                      reinterpret_cast<const uint8_t*>(&gSaveContext) + sizeof(gSaveContext));
        const int32_t generatingBefore = CVarGetInteger(CVAR_GENERAL("RandoGenerating"), 0);
        CVarSetInteger(CVAR_GENERAL("RandoGenerating"), 1);
        printf("[TEST] oot-windows-gate: Ocarina of Time on its file select, no file loaded, RandoGenerating 1 (a "
               "generation already running), console `gen_rando`\n");
        fflush(stdout);
        std::string genOutput;
        const int32_t rc = console->Run("gen_rando", &genOutput);
        const long saveDiff = FirstSaveDiff(canary);
        const int32_t generatingAfter = CVarGetInteger(CVAR_GENERAL("RandoGenerating"), 0);
        printf("[TEST] oot-windows-gate: bare `gen_rando` returned %d with output \"%s\" (RandoGenerating %d)\n",
               (int)rc, genOutput.c_str(), (int)generatingAfter);
        if (rc == 0 || genOutput.find("Rando generation already in progress") == std::string::npos ||
            generatingAfter != 1 || saveDiff >= 0) {
            printf("[TEST] FAIL: oot-windows-gate: bare `gen_rando` during a running generation did not fail with "
                   "\"Rando generation already in progress\", leaving RandoGenerating 1 and gSaveContext (first "
                   "differing byte %ld) unchanged (#913)\n",
                   saveDiff);
            failures++;
            memcpy(&gSaveContext, canary.data(), canary.size());
        }
        if (generatingBefore == 0) {
            CVarClear(CVAR_GENERAL("RandoGenerating"));
        } else {
            CVarSetInteger(CVAR_GENERAL("RandoGenerating"), generatingBefore);
        }
        fflush(stdout);
    }

    // ---- 7d. #930: a numeric argument the handler cannot parse fails the command instead of ending the process ------
    // The inherited handlers parse with std::stoi and catch only std::invalid_argument, some of them nothing at all, and
    // nothing between Ship::Console::Run and the handler catches either. `gen_rando 99999999999` throws
    // std::out_of_range inside its try (before any generation starts); `item 0 notanumber` throws
    // std::invalid_argument from a handler with no try (before its save write). Unfixed, either exception leaves
    // Ship::Console::Run uncaught and std::terminate ends this process. Each command must instead return non-zero with
    // the gating wrapper's error line in its output and leave gSaveContext and RandoGenerating unchanged.
    if (failures == 0) {
        Context_SetCurrentGame(GAME_OOT);
        gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
        gSaveContext.fileNum = 0;
        OoT_gPlayState = nullptr;
        canary.assign(reinterpret_cast<const uint8_t*>(&gSaveContext),
                      reinterpret_cast<const uint8_t*>(&gSaveContext) + sizeof(gSaveContext));
        const int32_t generatingBefore = CVarGetInteger(CVAR_GENERAL("RandoGenerating"), 0);
        struct ParseProbe {
            const char* line;
            const char* error;
        };
        const ParseProbe kParseProbes[] = {
            { "gen_rando 99999999999", "[SOH] 'gen_rando': a numeric argument is out of range." },
            { "item 0 notanumber", "[SOH] 'item': an argument must be a number." },
        };
        for (const ParseProbe& probe : kParseProbes) {
            printf("[TEST] oot-windows-gate: Ocarina of Time on its file select, no file loaded, console `%s`\n",
                   probe.line);
            fflush(stdout);
            std::string parseOutput;
            const int32_t rc = console->Run(probe.line, &parseOutput);
            const long saveDiff = FirstSaveDiff(canary);
            const int32_t generatingAfter = CVarGetInteger(CVAR_GENERAL("RandoGenerating"), 0);
            printf("[TEST] oot-windows-gate: `%s` returned %d with output \"%s\" (RandoGenerating %d)\n", probe.line,
                   (int)rc, parseOutput.c_str(), (int)generatingAfter);
            if (rc == 0 || parseOutput != probe.error || generatingAfter != generatingBefore || saveDiff >= 0) {
                printf("[TEST] FAIL: oot-windows-gate: `%s` did not fail with \"%s\", leaving RandoGenerating %d and "
                       "gSaveContext (first differing byte %ld) unchanged (#930)\n",
                       probe.line, probe.error, (int)generatingBefore, saveDiff);
                failures++;
                memcpy(&gSaveContext, canary.data(), canary.size());
            }
        }
        fflush(stdout);
    }

    // ---- 8. The Giant's Knife toggle: no write and no NULL-play dereference unless OoT is in play -------------------
    // Owns the Giant's Knife, broken flag clear, swordHealth 0: the mismatch that makes the callback call
    // func_800849EC(OoT_gPlayState), which writes equipment and the B button and then dereferences the play state.
    if (failures == 0) {
        const GameId knifeGames[] = { GAME_MM, GAME_OOT };
        for (GameId game : knifeGames) {
            Context_SetCurrentGame(game);
            gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
            gSaveContext.inventory.equipment &=
                ~OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);
            gSaveContext.swordHealth = 0.0f;
            canary.assign(reinterpret_cast<const uint8_t*>(&gSaveContext),
                          reinterpret_cast<const uint8_t*>(&gSaveContext) + sizeof(gSaveContext));
            printf("[TEST] oot-windows-gate: %s running, no OoT play state: Fix Broken Giant's Knife toggled\n",
                   GameName(game));
            fflush(stdout);
            SohGui::OnFixBrokenGiantsKnifeToggled();
            const long diff = FirstSaveDiff(canary);
            if (diff >= 0) {
                printf("[TEST] FAIL: oot-windows-gate: the Giant's Knife toggle with %s running and no OoT play state "
                       "changed gSaveContext (first differing byte at 0x%lx)\n",
                       GameName(game), diff);
                failures++;
            }
            FillSaveCanary(canary);
        }
    }

    memcpy(&gSaveContext, saved.data(), saved.size());
    OoT_gPlayState = savedPlay;
    Context_SetCurrentGame(prevGame);
    return failures;
}

} // namespace

extern "C" int OoT_WindowsGate_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    OWG_ASSERT(ctx != nullptr, "Ship::Context singleton missing: run the shared bring-up first");
    OWG_ASSERT(ctx->GetConsoleVariables() != nullptr, "ConsoleVariables missing: GuiWindow ctors read them");
    OWG_ASSERT(ctx->GetWindow() == nullptr, "harness unexpectedly has a real window: the tripwire below would be soft");

    for (const char* cvar : kTrackerLayoutCVars) {
        CVarClear(cvar);
    }

    GatedWindow windows[] = {
        { "Save Editor", CVAR_WINDOW("SaveEditor"), nullptr },
        { "Value Viewer", CVAR_WINDOW("ValueViewer"), nullptr },
        { "Message Viewer", CVAR_WINDOW("MessageViewer"), nullptr },
        { "Gameplay Stats", CVAR_WINDOW("GameplayStats"), nullptr },
        { "Check Tracker", CVAR_WINDOW("CheckTracker"), nullptr },
        { "Entrance Tracker", CVAR_WINDOW("EntranceTracker"), nullptr },
        { "Item Tracker", CVAR_WINDOW("ItemTracker"), nullptr },
        { "Time Splits", CVAR_WINDOW("TimeSplits"), nullptr },
    };

    // Visible BEFORE construction, so an ungated Draw() cannot leave on !IsVisible() before reaching ImGui.
    for (const GatedWindow& w : windows) {
        CVarSetInteger(w.visibilityCVar, 1);
    }

    windows[0].window = std::make_shared<OoTActiveGated<SaveEditorWindow>>(windows[0].visibilityCVar, windows[0].name,
                                                                           ImVec2(520, 600));
    windows[1].window = std::make_shared<OoTActiveGated<ValueViewerWindow>>(windows[1].visibilityCVar, windows[1].name,
                                                                            ImVec2(520, 600));
    {
        // Leaked on purpose (see the file comment): the holder is never deleted, so ~MessageViewer never frees the
        // buffers InitElement never allocated.
        auto* leaked = new std::shared_ptr<Ship::GuiWindow>(std::make_shared<OoTActiveGated<MessageViewer>>(
            windows[2].visibilityCVar, windows[2].name, ImVec2(520, 600)));
        windows[2].window = *leaked;
    }
    windows[3].window = std::make_shared<OoTActiveGated<GameplayStatsWindow>>(windows[3].visibilityCVar,
                                                                              windows[3].name, ImVec2(480, 550));
    windows[4].window = std::make_shared<OoTActiveGated<CheckTracker::CheckTrackerWindow>>(
        windows[4].visibilityCVar, windows[4].name, ImVec2(400, 540));
    windows[5].window = std::make_shared<OoTActiveGated<EntranceTracker::EntranceTrackerWindow>>(
        windows[5].visibilityCVar, windows[5].name, ImVec2(500, 750));
    windows[6].window = std::make_shared<OoTActiveGated<ItemTrackerWindow>>(windows[6].visibilityCVar, windows[6].name,
                                                                            ImVec2(350, 600));
    windows[7].window =
        std::make_shared<OoTActiveGated<TimeSplitWindow>>(windows[7].visibilityCVar, windows[7].name, ImVec2(450, 660));

    for (const GatedWindow& w : windows) {
        if (!w.window->IsVisible()) {
            printf("[TEST] FAIL: %s is not visible after its CVar was forced on: the tripwire would be vacuous\n",
                   w.name);
            return 1;
        }
    }

    const GameId prevGame = Context_GetCurrentGame();

    // ---- 1. The predicate ------------------------------------------------------------------------------------------
    Context_SetCurrentGame(GAME_OOT);
    const bool drawsUnderOoT = OoT_Gui_ShouldDraw();
    Context_SetCurrentGame(GAME_MM);
    const bool drawsUnderMM = OoT_Gui_ShouldDraw();
    Context_SetCurrentGame(GAME_NONE);
    const bool drawsUnderNone = OoT_Gui_ShouldDraw();
    Context_SetCurrentGame(prevGame);
    OWG_ASSERT(drawsUnderOoT, "OoT_Gui_ShouldDraw() false while OoT is the running game");
    OWG_ASSERT(!drawsUnderMM, "OoT_Gui_ShouldDraw() true while Majora's Mask is the running game");
    OWG_ASSERT(!drawsUnderNone, "OoT_Gui_ShouldDraw() true with no game running (GAME_NONE must be excluded)");

    // ---- 2. The wrapper, on a counting spy: each path blocked unless OoT runs, each path passed through when it does
    // -
    int spyFailures = 0;
    spyFailures += RunSpy(GAME_OOT, "Ocarina of Time", true);
    spyFailures += RunSpy(GAME_MM, "Majora's Mask", false);
    spyFailures += RunSpy(GAME_NONE, "no game", false);
    Context_SetCurrentGame(prevGame);
    CVarClear(OWG_SPY_CVAR);
    OWG_ASSERT(spyFailures == 0,
               "OoTActiveGated<> lets a body run under a non-OoT game or blocks one under OoT (above)");

    // ---- 3. The wired gate on the eight real windows: all three paths, every window ---------------------------------
    const GameId inactiveGames[] = { GAME_MM, GAME_NONE };
    for (GameId game : inactiveGames) {
        Context_SetCurrentGame(game);
        for (const GatedWindow& w : windows) {
            // One line per window before its calls, so a RED run names the window and path that faulted.
            printf("[TEST] oot-windows-gate: %s running, %s: DrawElement() (menu embed), Update(), Draw()\n",
                   game == GAME_MM ? "Majora's Mask" : "no game", w.name);
            fflush(stdout);
            w.window->DrawElement();
            w.window->Update();
            w.window->Draw();
        }
    }
    Context_SetCurrentGame(prevGame);

    // ---- 4. Dormant, not hidden ------------------------------------------------------------------------------------
    // A gated window must come back when OoT resumes, so the gate may not Hide() it or touch its CVar.
    for (const GatedWindow& w : windows) {
        if (!w.window->IsVisible() || CVarGetInteger(w.visibilityCVar, 0) != 1) {
            printf("[TEST] FAIL: %s lost its visibility while gated: it would not reappear when OoT resumes\n", w.name);
            return 1;
        }
    }

    for (const GatedWindow& w : windows) {
        CVarClear(w.visibilityCVar);
    }

    // ---- 5 to 8. The console commands and the Giant's Knife toggle (#798) ------------------------------------------
    OWG_ASSERT(RunConsoleAndKnifeGate() == 0,
               "an OoT console command or the Giant's Knife toggle ran against the shared save while OoT was not the "
               "running game (above)");

    printf("[TEST] PASS: oot-windows-gate: the eight OoT save/play-state windows run no Draw, Update or menu-embed "
           "DrawElement body unless OoT is the running game, the wrapper passes all three through under OoT, and the "
           "windows stay visible while gated; every OoT console command and the Giant's Knife toggle leave the shared "
           "save untouched unless OoT is the running game\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
