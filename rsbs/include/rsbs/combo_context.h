#ifndef RSBS_COMBO_CONTEXT_H
#define RSBS_COMBO_CONTEXT_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    COMBO_GAME_NONE = 0,
    COMBO_GAME_OOT = 1,
    COMBO_GAME_MM = 2
} ComboGame;

typedef struct {
    char magic[8];        // "OoT+MM<3"
    uint32_t version;
    bool switchRequested;
    ComboGame targetGame;
    uint16_t targetEntrance;
    ComboGame sourceGame;
    uint16_t sourceEntrance;
    uint32_t sharedFlags[64];
    uint16_t sharedItems[32];
    int32_t saveSlot;

    // Cross-game rando state propagation
    bool sourceIsRando;        // Source game is in randomizer mode
    uint32_t sharedRandoSeed;  // Shared seed for synchronization
} ComboContext;

extern ComboContext gComboCtx;

void ComboContext_Init(void);
void ComboContext_RequestSwitch(ComboGame target, uint16_t entrance);
bool ComboContext_IsSwitchPending(void);
void ComboContext_ClearSwitch(void);

#endif
