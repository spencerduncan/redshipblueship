/**
 * @file test_window_focus.cpp
 * @brief Test windows never take keyboard focus (see test_window_focus.h)
 */

#include "test_window_focus.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL2/SDL.h>
#include <ship/Context.h>
#include <ship/audio/Audio.h>
#include <ship/config/Config.h>
#include <ship/controller/controldeck/ControlDeck.h>
#include <ship/controller/physicaldevice/ConnectedPhysicalDeviceManager.h>

namespace {

bool sArmed = false;
bool sAudioArmed = false;
bool sAudioOptedOut = false;
bool sSabotageConfig = false;
bool sSabotageRestore = false;
bool sAudioVerdictFailed = false;
// What TestWindowFocus_BeforeInitAudio changed on the live Ship::Context's config,
// for TestWindowFocus_AfterInitAudio to put back.
TestSilentAudioConfigState sLiveConfigState;

const char* kAudioBackendKey = "Window.AudioBackend";

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

TestSilentAudioConfigState TestWindowFocus_ForceNullAudioBackend(Ship::Config* config) {
    TestSilentAudioConfigState state;
    // Only an armed process: a normal-play run, and RSBS_TEST_AUDIO=1, keep their config.
    if (config == nullptr || !sAudioArmed || sSabotageConfig) {
        return state;
    }
    state.previous = config->GetString(kAudioBackendKey);
    state.hadKey = !state.previous.empty();
    // Forced in memory, then put back by TestWindowFocus_RestoreAudioBackend once the
    // audio player is built, so a test run never leaves "null" behind in a config a
    // real run reads. Audio::Init saves the config itself while the key is forced,
    // hence the restore saves it again.
    config->SetString(kAudioBackendKey, "null");
    state.forced = true;
    return state;
}

void TestWindowFocus_RestoreAudioBackend(Ship::Config* config, const TestSilentAudioConfigState& state) {
    if (config == nullptr || !state.forced || sSabotageRestore) {
        return;
    }
    if (state.hadKey) {
        config->SetString(kAudioBackendKey, state.previous);
    } else {
        config->Erase(kAudioBackendKey);
    }
    config->Save();
}

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

void TestWindowFocus_ArmSilentAudio(const char* mode) {
    const char* who = mode != nullptr ? mode : "?";
    const char* sabotage = std::getenv("RSBS_TEST_AUDIO_SABOTAGE");
    if (sabotage != nullptr && std::strstr(sabotage, "no-default") != nullptr) {
        fprintf(stderr, "[AUDIO] %s: RSBS_TEST_AUDIO_SABOTAGE=no-default -- the silent audio default is NOT armed\n",
                who);
        return;
    }
    const char* optOut = std::getenv("RSBS_TEST_AUDIO");
    if (optOut != nullptr && std::strcmp(optOut, "1") == 0) {
        sAudioOptedOut = true;
        fprintf(stderr, "[AUDIO] %s: RSBS_TEST_AUDIO=1 -- audio left at the platform default on purpose\n", who);
        return;
    }
    // Red-half switches for the config layer (see the header). Each stands in for
    // the call it names being removed, and is only honoured once the process is armed.
    sSabotageConfig = sabotage != nullptr && std::strstr(sabotage, "no-config") != nullptr;
    sSabotageRestore = sabotage != nullptr && std::strstr(sabotage, "no-restore") != nullptr;
    // An explicit SDL_AUDIODRIVER wins; otherwise "dummy". Pinned at OVERRIDE so a
    // later change to the environment cannot un-pin it (see the header).
    const char* explicitDriver = SDL_getenv("SDL_AUDIODRIVER");
    const bool fromEnv = explicitDriver != nullptr && explicitDriver[0] != '\0';
    const char* driver = fromEnv ? explicitDriver : "dummy";
    SDL_SetHintWithPriority(SDL_HINT_AUDIODRIVER, driver, SDL_HINT_OVERRIDE);
    SDL_setenv("SDL_AUDIODRIVER", driver, 0);
    sAudioArmed = true;
    fprintf(stderr, "[AUDIO] %s: SDL audio driver pinned to '%s'%s\n", who, driver,
            fromEnv ? " (from the environment)" : " (default)");
}

int TestWindowFocus_IsAudioArmed(void) {
    return sAudioArmed ? 1 : 0;
}

void TestWindowFocus_BeforeInitAudio(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConfig() == nullptr) {
        return;
    }
    sLiveConfigState = TestWindowFocus_ForceNullAudioBackend(ctx->GetConfig().get());
    if (sLiveConfigState.forced) {
        fprintf(stderr, "[AUDIO] Ship audio backend forced to 'null' for this test process (config had '%s')\n",
                sLiveConfigState.hadKey ? sLiveConfigState.previous.c_str() : "(no key)");
    }
}

void TestWindowFocus_AfterInitAudio(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConfig() == nullptr) {
        return;
    }
    TestWindowFocus_RestoreAudioBackend(ctx->GetConfig().get(), sLiveConfigState);
    sLiveConfigState = TestSilentAudioConfigState{};
}

int TestWindowFocus_AudioProbe(const char* where) {
    const char* current = SDL_WasInit(SDL_INIT_AUDIO) != 0 ? SDL_GetCurrentAudioDriver() : nullptr;
    const char* hint = SDL_GetHint(SDL_HINT_AUDIODRIVER);
    // The Ship audio player the game actually built: "null" is the only silent one.
    const char* backend = "(audio not up)";
    bool backendSilent = true;
    auto ctx = Ship::Context::GetInstance();
    if (ctx != nullptr && ctx->GetAudio() != nullptr) {
        switch (ctx->GetAudio()->GetCurrentAudioBackend()) {
            case Ship::AudioBackend::NUL:
                backend = "null";
                break;
            case Ship::AudioBackend::SDL:
                backend = "sdl";
                backendSilent = false;
                break;
            case Ship::AudioBackend::WASAPI:
                backend = "wasapi";
                backendSilent = false;
                break;
        }
    }
    const bool silent = sAudioOptedOut || (sAudioArmed && backendSilent);
    printf("[AUDIO] %s: currentDriver=%s hint=%s shipBackend=%s armed=%d optOut=%d verdict=%s\n",
           where != nullptr ? where : "?", current != nullptr ? current : "(audio not up)",
           hint != nullptr ? hint : "(unset)", backend, sAudioArmed ? 1 : 0, sAudioOptedOut ? 1 : 0,
           silent ? "silent" : "NOT-SILENT");
    fflush(stdout);
    if (!silent) {
        sAudioVerdictFailed = true;
    }
    return silent ? 0 : 1;
}

int TestWindowFocus_AudioVerdictFailed(void) {
    return sAudioVerdictFailed ? 1 : 0;
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

void TestWindowFocus_ReadControllers(TestWindowControllerState* out) {
    std::memset(out, 0, sizeof(*out));
    std::snprintf(out->background, sizeof(out->background), "%s",
                  HintOrNull(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS));
    out->controlDeckPads = -1;
    out->joystickUp = SDL_WasInit(SDL_INIT_JOYSTICK) != 0 ? 1 : 0;
    if (out->joystickUp) {
        out->joysticks = SDL_NumJoysticks();
        for (int i = 0; i < out->joysticks; i++) {
            if (SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(i)) != nullptr) {
                out->openJoysticks++;
            }
        }
    }
    auto ctx = Ship::Context::GetInstance();
    if (ctx != nullptr && ctx->GetControlDeck() != nullptr &&
        ctx->GetControlDeck()->GetConnectedPhysicalDeviceManager() != nullptr) {
        out->controlDeckPads =
            (int)ctx->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadNames().size();
    }
}

int TestWindowFocus_ScanControllersLikeControlDeck(int* joysticksSeen, char* why, int whyLen) {
    if (joysticksSeen != nullptr) {
        *joysticksSeen = 0;
    }
    const bool wasUp = SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0;
    if (!wasUp && SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        if (why != nullptr && whyLen > 0) {
            std::snprintf(why, (size_t)whyLen, "no game controller subsystem (%s)", SDL_GetError());
        }
        return -1;
    }
    if (joysticksSeen != nullptr) {
        *joysticksSeen = SDL_NumJoysticks();
    }
    // The ControlDeck's own class, so the count is what a booted game would hold.
    Ship::ConnectedPhysicalDeviceManager scan;
    scan.RefreshConnectedSDLGamepads();
    const int opened = (int)scan.GetConnectedSDLGamepadNames().size();
    // Port 0 ignores nothing (the scan only ignores new pads on ports 1-3), so it
    // hands back every pad the scan opened; close each once, as it opened each once.
    for (const auto& [instanceId, pad] : scan.GetConnectedSDLGamepadsForPort(0)) {
        SDL_GameControllerClose(pad);
    }
    if (!wasUp) {
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
    return opened;
}

} // extern "C"
