#include "global.h"

void OoT_Sleep_Cycles(OSTime cycles) {
    OSMesgQueue mq;
    OSMesg msg;
    OSTimer timer;

    osCreateMesgQueue(&mq, &msg, OS_MESG_BLOCK);
    osSetTimer(&timer, cycles, 0, &mq, OS_MESG_PTR(NULL));
    osRecvMesg(&mq, NULL, OS_MESG_BLOCK);
}

void OoT_Sleep_Nsec(u32 nsec) {
    OoT_Sleep_Cycles(OS_NSEC_TO_CYCLES(nsec));
}

void OoT_Sleep_Usec(u32 usec) {
    OoT_Sleep_Cycles(OS_USEC_TO_CYCLES(usec));
}

// originally "msleep"
void OoT_Sleep_Msec(u32 ms) {
    OoT_Sleep_Cycles((ms * OS_CPU_COUNTER) / 1000ull);
}

void OoT_Sleep_Sec(u32 sec) {
    OoT_Sleep_Cycles(sec * OS_CPU_COUNTER);
}
