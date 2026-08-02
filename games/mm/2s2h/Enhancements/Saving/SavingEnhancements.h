#ifndef SAVING_ENHANCEMENTS_H
#define SAVING_ENHANCEMENTS_H

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

#ifdef __cplusplus
}
#endif

#endif // SAVING_ENHANCEMENTS_H
