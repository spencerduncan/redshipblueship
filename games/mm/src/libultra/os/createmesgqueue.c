#include "ultra64.h"

void MM_osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msq, s32 count) {
    mq->mtQueue = (OSThread*)&__osThreadTail.next;
    mq->fullQueue = (OSThread*)&__osThreadTail.next;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msq;
}
