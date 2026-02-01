#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "PR/os_cont.h"

// Guard BTN_* macros to prevent redefinition warnings (C4005)
// These are also defined in libultraship/include/libultraship/libultra/controller.h
#ifndef BTN_A
#define BTN_A           A_BUTTON
#endif
#ifndef BTN_B
#define BTN_B           B_BUTTON
#endif
#ifndef BTN_Z
#define BTN_Z           Z_TRIG
#endif
#ifndef BTN_START
#define BTN_START       START_BUTTON
#endif
#ifndef BTN_DUP
#define BTN_DUP         U_JPAD
#endif
#ifndef BTN_DDOWN
#define BTN_DDOWN       D_JPAD
#endif
#ifndef BTN_DLEFT
#define BTN_DLEFT       L_JPAD
#endif
#ifndef BTN_DRIGHT
#define BTN_DRIGHT      R_JPAD
#endif
#ifndef BTN_L
#define BTN_L           L_TRIG
#endif
#ifndef BTN_R
#define BTN_R           R_TRIG
#endif
#ifndef BTN_CUP
#define BTN_CUP         U_CBUTTONS
#endif
#ifndef BTN_CDOWN
#define BTN_CDOWN       D_CBUTTONS
#endif
#ifndef BTN_CLEFT
#define BTN_CLEFT       L_CBUTTONS
#endif
#ifndef BTN_CRIGHT
#define BTN_CRIGHT      R_CBUTTONS
#endif

// BTN_RESET is MM-specific, not in libultraship
#ifndef BTN_RESET
#define BTN_RESET       0x0080 /* "neutral reset": Corresponds to holding L+R and pressing S */
#endif

#define CHECK_BTN_ALL(state, combo) (~((state) | ~(combo)) == 0)
#define CHECK_BTN_ANY(state, combo) (((state) & (combo)) != 0)

#endif
