#include <stdint.h>

#ifndef MODS_H
#define MODS_H

#ifdef __cplusplus
extern "C" {
#endif

void UpdateHyperBossesState();
void InitMods();
// #577 spike surface: draws one MM-exclusive model from OoT when
// RSBS_CROSSGAME_MODEL_SPIKE is set. Defined in crossgame_model_spike.cpp.
void RegisterCrossGameModelSpike();
void SwitchAge();

#ifdef __cplusplus
}
#endif

#endif
