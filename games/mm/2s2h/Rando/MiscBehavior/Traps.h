#ifndef RANDO_TRAP_H
#define RANDO_TRAP_H

#include "Rando/Rando.h"
typedef enum { TRAP_FREEZE, TRAP_BLAST, TRAP_SHOCK, TRAP_JINX, TRAP_WALLET, TRAP_ENEMY, TRAP_TIME, TRAP_MAX } TrapTypes;

#ifdef RSBS_SINGLE_EXECUTABLE
// #1047: a trap's type is part of the world: the roll is seeded by the world's
// seed and the check, over the set frozen in the save at creation.
extern int RollTrapType(RandoCheckId randoCheckId);
extern std::string GetTrapMessage(RandoCheckId randoCheckId);
// The trap-type set the gRando.Traps.* CVars ask for, as randoSaveTrapTypes holds it.
extern uint8_t ResolveTrapTypesFromCVars();
// The CVar that enables trap type `type` (bit `type` of that set), and its name
// in the options; nullptr outside 0..TRAP_MAX-1.
extern const char* TrapTypeCVar(int type);
extern const char* TrapTypeLabel(int type);
#else
extern int RollTrapType();
extern std::string GetTrapMessage();
#endif

#endif