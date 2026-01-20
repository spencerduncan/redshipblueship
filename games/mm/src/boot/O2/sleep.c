#include "global.h"

void MM_Sleep_Cycles(u64 time) {
    // OSMesgQueue mq;
    // OSMesg msg[1];
    // OSTimer timer;

    // osCreateMesgQueue(&mq, msg, ARRAY_COUNT(msg));
    // osSetTimer(&timer, time, 0, &mq, NULL);
    // osRecvMesg(&mq, NULL, OS_MESG_BLOCK);
}

void MM_Sleep_Nsec(u32 nsec) {
    // MM_Sleep_Cycles(OS_NSEC_TO_CYCLES(nsec));
}

void MM_Sleep_Usec(u32 usec) {
    // MM_Sleep_Cycles(OS_USEC_TO_CYCLES(usec));
}

void MM_Sleep_Msec(u32 ms) {
    // MM_Sleep_Cycles((ms * OS_CPU_COUNTER) / 1000ULL);
}

void MM_Sleep_Sec(u32 sec) {
    // MM_Sleep_Cycles(sec * OS_CPU_COUNTER);
}
