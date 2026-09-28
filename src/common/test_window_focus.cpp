/**
 * @file test_window_focus.cpp
 * @brief Test windows never take keyboard focus (see test_window_focus.h)
 */

#include "test_window_focus.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL2/SDL.h>

namespace {

bool sArmed = false;

bool SabotagedNoHint() {
    const char* s = std::getenv("RSBS_TEST_FOCUS_SABOTAGE");
    return s != nullptr && std::strstr(s, "no-hint") != nullptr;
}

const char* HintOrNull(const char* name) {
    const char* v = SDL_GetHint(name);
    return v != nullptr ? v : "(unset)";
}

int SDLCALL CountFocusEvents(void* userdata, SDL_Event* event) {
    if (event->type == SDL_WINDOWEVENT &&
        (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED || event->window.event == SDL_WINDOWEVENT_TAKE_FOCUS)) {
        (*static_cast<int*>(userdata))++;
    }
    return 1;
}

} // namespace

extern "C" {

void TestWindowFocus_ArmForTestMode(const char* mode) {
    if (SabotagedNoHint()) {
        fprintf(stderr, "[FOCUS] %s: RSBS_TEST_FOCUS_SABOTAGE=no-hint -- the no-activation hints are NOT armed\n",
                mode != nullptr ? mode : "?");
        return;
    }
    // OVERRIDE, not SDL_SetHint: a normal-priority set is refused outright while an
    // environment variable of the same name exists, and SDL_GetHint prefers that
    // variable, so only OVERRIDE is immune to what the environment says.
    SDL_SetHintWithPriority(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, "1", SDL_HINT_OVERRIDE);
#ifdef SDL_HINT_FORCE_RAISEWINDOW
    // Already SDL's default; pinned so no environment can turn SDL_RaiseWindow into
    // the AttachThreadInput foreground grab inside a test process.
    SDL_SetHintWithPriority(SDL_HINT_FORCE_RAISEWINDOW, "0", SDL_HINT_OVERRIDE);
#endif
    sArmed = true;
}

int TestWindowFocus_IsArmed(void) {
    return sArmed ? 1 : 0;
}

int TestWindowFocus_Probe(const char* where) {
    const bool video = SDL_WasInit(SDL_INIT_VIDEO) != 0;
    SDL_Window* focus = video ? SDL_GetKeyboardFocus() : nullptr;
    printf("[FOCUS] %s: video=%d keyboardFocus=%s noActivationHint=%s armed=%d\n", where != nullptr ? where : "?",
           video ? 1 : 0, focus != nullptr ? "TAKEN" : "none", HintOrNull(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN),
           sArmed ? 1 : 0);
    fflush(stdout);
    return focus != nullptr ? 1 : 0;
}

void TestWindowFocus_ShowProbeWindow(TestWindowFocusProbeResult* out) {
    std::memset(out, 0, sizeof(*out));

    const bool videoWasUp = SDL_WasInit(SDL_INIT_VIDEO) != 0;
    if (!videoWasUp && SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        std::snprintf(out->why, sizeof(out->why), "no video subsystem (%s)", SDL_GetError());
        return;
    }

    int focusEvents = 0;
    SDL_AddEventWatch(CountFocusEvents, &focusEvents);

    // A plain shown window, the way SDL_CreateWindow shows the game's GL window
    // (gfx_sdl2.cpp): no raise, no input-focus request -- if it takes focus, the
    // show itself took it.
    SDL_Window* w = SDL_CreateWindow("RedShipBlueShip focus probe", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                     96, 54, SDL_WINDOW_SHOWN);
    if (w == nullptr) {
        std::snprintf(out->why, sizeof(out->why), "SDL_CreateWindow failed (%s)", SDL_GetError());
        SDL_DelEventWatch(CountFocusEvents, &focusEvents);
        if (!videoWasUp) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
        return;
    }

    // "Right after the first frames": pump like a frame loop would.
    for (int i = 0; i < 10; i++) {
        SDL_PumpEvents();
        SDL_Delay(10);
    }

    out->ran = 1;
    out->keyboardFocus = (SDL_GetKeyboardFocus() == w) ? 1 : 0;
    out->inputFocusFlag = (SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS) != 0 ? 1 : 0;
    out->focusGainedEvents = focusEvents;

    SDL_DelEventWatch(CountFocusEvents, &focusEvents);
    SDL_DestroyWindow(w);
    if (!videoWasUp) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

} // extern "C"
