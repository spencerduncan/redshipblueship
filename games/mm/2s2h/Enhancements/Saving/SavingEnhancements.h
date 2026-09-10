#ifndef SAVING_ENHANCEMENTS_H
#define SAVING_ENHANCEMENTS_H

#include <stdint.h> // uint64_t for SavingEnhancements_GetLastAutosaveTimestamp, in C and C++ includers alike

void RegisterSavingEnhancements();
void RegisterAutosave();

#ifdef __cplusplus
extern "C" {
#endif

int SavingEnhancements_GetSaveEntrance();
bool SavingEnhancements_CanSave();
// RSBS (#530): "is there any durable destination for a save right now" — a real
// flash slot, or (cross-game) this session's active unified `.redsave` slot.
bool SavingEnhancements_HasDurableDestination();
void SavingEnhancements_AdvancePlaytime();

// ADR 0009 decision 4b: the Autosave enhancement's ARMED state and its interval
// clock, exposed so an out-of-band autosave point (MM's cross-game death-decline
// exit, MM_Combo_GameOverExitToOoT) shares the enhancement's own timer instead
// of inventing a second one. Contracts are on the definitions.
bool SavingEnhancements_AutosaveArmed();
void SavingEnhancements_ResetAutosaveInterval();
uint64_t SavingEnhancements_GetLastAutosaveTimestamp();

#ifdef __cplusplus
}
#endif

#endif // SAVING_ENHANCEMENTS_H
