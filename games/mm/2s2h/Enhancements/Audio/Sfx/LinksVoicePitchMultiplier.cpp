#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
}

// For Link's voice pitch SFX modifier
static f32 freqMultiplier = 1;

// #1239: SoH's Audio Editor writes ONE float leaf, "Link's Voice Pitch Multiplier" (0.4-2.5, default 1.0), and that
// row drives MM too. 2Ship split it into a bool .Enable beside a float .Scale; LUS stores CVars as dot-nested JSON,
// so SoH's leaf cannot also be their parent. This TU is the one translator (RSBS::kTranslatedKeys): SoH's value is
// MM's scale, and any value but 1.0 arms the hook. MM's two keys are left to the elided widgets.
#define CVAR_NAME "gAudioEditor.LinkVoiceFreqMultiplier"

namespace {
float VoicePitchScaleFromSoh() {
    const float scale = CVarGetFloat(CVAR_NAME, 1.0f);
    return scale <= 0.0f ? 1.0f : scale;
}

bool VoicePitchEnabledFromSoh() {
    return VoicePitchScaleFromSoh() != 1.0f;
}
} // namespace

#define CVAR VoicePitchEnabledFromSoh()

void RegisterLinksVoicePitchMultiplier() {
    COND_VB_SHOULD(VB_LINK_VOICE_PITCH_MULTIPLIER, CVAR, {
        // #1239: linked into the single exe; Player_PlaySfx can run without a play state.
        if (MM_gPlayState == nullptr) {
            return;
        }
        Player* player = GET_PLAYER(MM_gPlayState);
        u16 sfxId = *va_arg(args, u16*);

        if (sfxId >= NA_SE_VO_LI_SWORD_N && sfxId <= NA_SE_VO_DEMO_394 || sfxId == NA_SE_PL_TRANSFORM_VOICE) {

            *should = false;
            freqMultiplier = VoicePitchScaleFromSoh();

            AudioSfx_PlaySfx(sfxId, &player->actor.projectedPos, 4, &freqMultiplier, &MM_gSfxDefaultFreqAndVolScale,
                             &MM_gSfxDefaultReverb);
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterLinksVoicePitchMultiplier, { CVAR_NAME });
