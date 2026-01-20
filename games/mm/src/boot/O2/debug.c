#include "global.h"
#include "fault.h"

void _dbg_hungup(const char* file, int lineNum) {
    // osGetThreadId(NULL);
    MM_Fault_AddHungupAndCrash(file, lineNum);
}

void Reset(void) {
    MM_Fault_AddHungupAndCrash("Reset", 0);
}
