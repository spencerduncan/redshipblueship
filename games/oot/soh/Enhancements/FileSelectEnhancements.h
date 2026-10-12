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
