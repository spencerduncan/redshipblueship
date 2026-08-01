#ifndef SAVING_ENHANCEMENTS_H
#define SAVING_ENHANCEMENTS_H

void RegisterSavingEnhancements();
void RegisterAutosave();

#ifdef __cplusplus
extern "C" {
#endif

int SavingEnhancements_GetSaveEntrance();
bool SavingEnhancements_CanSave();
// #530: "is there anywhere to persist an owl save" — a real flash slot, or (in
// a single-exe cross-game session, where fileNum is pinned to the 0xFF
// sentinel) this session's unified .redsave slot. No PlayState dependency, so
// the headless lock can drive it directly.
bool SavingEnhancements_HasSaveTarget();
void SavingEnhancements_AdvancePlaytime();

#ifdef __cplusplus
}
#endif

#endif // SAVING_ENHANCEMENTS_H
