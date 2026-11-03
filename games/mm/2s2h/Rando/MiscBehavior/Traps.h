#ifndef RANDO_TRAP_H
#define RANDO_TRAP_H

#include "Rando/Rando.h"
// RedShipBlueShip (#1424): TRAP_FIRE onward are OoTMM's trap types MM lacked
// (OoTMM packages/generator/src/common/traps.c). APPEND ONLY: a type's number is
// its bit in the frozen set every written save holds (randoSaveTrapTypes, then
// randoSaveTrapTypesHi) and its place in the roll's list. Fire = 7 and
// Knockback = 8 are upstream 2Ship's own numbers and keys (254cc60e1, #1452), so
// porting that commit adds no type and moves no frozen bit.
typedef enum {
    TRAP_FREEZE,
    TRAP_BLAST,
    TRAP_SHOCK,
    TRAP_JINX,
    TRAP_WALLET,
    TRAP_ENEMY,
    TRAP_TIME,
    TRAP_FIRE,
    TRAP_KNOCKBACK,
    TRAP_DRAIN,
    TRAP_ANTI_MAGIC,
    TRAP_RUPOOR,
    TRAP_MAX
} TrapTypes;

#ifdef RSBS_SINGLE_EXECUTABLE
// #1047: a trap's type is part of the world: the roll is seeded by the world's
// seed and the check, over the set frozen in the save at creation.
extern int RollTrapType(RandoCheckId randoCheckId);
extern std::string GetTrapMessage(RandoCheckId randoCheckId);
// The trap-type set the gRando.Traps.* CVars ask for: bit `type` per enabled
// type, as FrozenTrapTypesOf reads it back from a save (#1424: 16 bits wide).
extern uint16_t ResolveTrapTypesFromCVars();
// The frozen set a save's rando block holds, and the write that freezes one
// (randoSaveTrapTypes is bits 0-7, randoSaveTrapTypesHi bits 8-15).
extern uint16_t FrozenTrapTypesOf(const RandoSaveInfo& info);
extern void StoreFrozenTrapTypes(RandoSaveInfo& info, uint16_t mask);
// The CVar that enables trap type `type` (bit `type` of that set), and its name
// in the options; nullptr outside 0..TRAP_MAX-1.
extern const char* TrapTypeCVar(int type);
extern const char* TrapTypeLabel(int type);
#else
extern int RollTrapType();
extern std::string GetTrapMessage();
#endif

#endif
