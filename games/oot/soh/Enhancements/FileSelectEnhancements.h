#ifndef FILE_SELECT_ENHANCEMENTS_H
#define FILE_SELECT_ENHANCEMENTS_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif
const char* SohFileSelect_GetSettingText(u8 optionIndex, u8 language);
void SohFileSelect_ShowPresetModal();
#ifdef RSBS_SINGLE_EXECUTABLE
// RSBS (#564 V2): the line the randomizer menu draws under "Start Randomizer"
// while no seed is loaded (RSM_NO_RANDOMIZER_GENERATED's place).
const char* OoT_FileSelect_NoSeedText(u8 language);
// RSBS (#952): the paired world's fingerprint and the build, beside the seed
// icons. `slot` is the file whose icons are drawn, or -1 for the generated world.
void OoT_FileSelect_DrawComboIdentity(GraphicsContext* gfx, s32 slot, u8 alpha);
#endif
#ifdef __cplusplus
};
#endif

typedef enum {
    RSM_START_RANDOMIZER,
    RSM_GENERATE_RANDOMIZER,
    RSM_OPEN_RANDOMIZER_SETTINGS,
    RSM_GENERATING,
    RSM_NO_RANDOMIZER_GENERATED,
    RSM_MAX,
} RandomizerSettingsMenuEnums;

#endif
