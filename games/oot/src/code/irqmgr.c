#include "global.h"
#include "vt.h"

vu32 OoT_gIrqMgrResetStatus = 0;
volatile OSTime OoT_sIrqMgrResetTime = 0;
volatile OSTime OoT_gIrqMgrRetraceTime = 0;
u32 OoT_sIrqMgrRetraceCount = 0;

#define RETRACE_MSG 666
#define PRE_NMI_MSG 669
#define PRENMI450_MSG 671
#define PRENMI480_MSG 672
#define PRENMI500_MSG 673

#define STATUS_IDLE 0
#define STATUS_PRENMI 1
#define STATUS_NMI 2

void OoT_IrqMgr_AddClient(IrqMgr* this, IrqMgrClient* c, OSMesgQueue* msgQ) {
    OSIntMask prevInt;

    LOG_CHECK_NULL_POINTER("this", this);
    LOG_CHECK_NULL_POINTER("c", c);
    LOG_CHECK_NULL_POINTER("msgQ", msgQ);

    prevInt = osSetIntMask(1);

    c->queue = msgQ;
    c->prev = this->clients;
    this->clients = c;

    osSetIntMask(prevInt);

    if (this->resetStatus > STATUS_IDLE) {
        osSendMesgPtr(c->queue, &this->prenmiMsg, OS_MESG_NOBLOCK);
    }

    if (this->resetStatus >= STATUS_NMI) {
        osSendMesgPtr(c->queue, &this->nmiMsg, OS_MESG_NOBLOCK);
    }
}

void OoT_IrqMgr_RemoveClient(IrqMgr* this, IrqMgrClient* c) {
    IrqMgrClient* iter = this->clients;
    IrqMgrClient* lastIter = NULL;
    OSIntMask prevInt;

    LOG_CHECK_NULL_POINTER("this", this);
    LOG_CHECK_NULL_POINTER("c", c);

    prevInt = osSetIntMask(1);

    while (iter != NULL) {
        if (iter == c) {
            if (lastIter) {
                lastIter->prev = c->prev;
            } else {
                this->clients = c->prev;
            }
            break;
        }
        lastIter = iter;
        iter = iter->prev;
    }

    osSetIntMask(prevInt);
}

void OoT_IrqMgr_SendMesgForClient(IrqMgr* this, OSMesg msg) {
    IrqMgrClient* iter = this->clients;

    while (iter != NULL) {
        if (iter->queue->validCount >= iter->queue->msgCount) {
            // "irqmgr_SendMesgForClient: Message queue is overflowing mq=%08x cnt=%d"
            osSyncPrintf(
                VT_COL(RED, WHITE) "irqmgr_SendMesgForClient:メッセージキューがあふれています mq=%08x cnt=%d\n" VT_RST,
                iter->queue, iter->queue->validCount);
        } else {
            OoT_osSendMesg(iter->queue, msg, OS_MESG_NOBLOCK);
        }

        iter = iter->prev;
    }
}

void OoT_IrqMgr_JamMesgForClient(IrqMgr* this, OSMesg msg) {
    IrqMgrClient* iter = this->clients;

    while (iter != NULL) {
        if (iter->queue->validCount >= iter->queue->msgCount) {
            // "irqmgr_JamMesgForClient: Message queue is overflowing mq=%08x cnt=%d"
            osSyncPrintf(
                VT_COL(RED, WHITE) "irqmgr_JamMesgForClient:メッセージキューがあふれています mq=%08x cnt=%d\n" VT_RST,
                iter->queue, iter->queue->validCount);
        } else {
            // mistake? the function's name suggests it would use OoT_osJamMesg
            OoT_osSendMesg(iter->queue, msg, OS_MESG_NOBLOCK);
        }
        iter = iter->prev;
    }
}

void OoT_IrqMgr_HandlePreNMI(IrqMgr* this) {
    u64 temp = STATUS_PRENMI; // required to match

    OoT_gIrqMgrResetStatus = temp;
    this->resetStatus = STATUS_PRENMI;
    OoT_sIrqMgrResetTime = this->resetTime = osGetTime();

    OoT_osSetTimer(&this->timer, OS_USEC_TO_CYCLES(450000), 0ull, &this->queue, OS_MESG_32(PRENMI450_MSG));
    OoT_IrqMgr_JamMesgForClient(this, OS_MESG_PTR(&this->prenmiMsg));
}

void OoT_IrqMgr_CheckStack() {
    osSyncPrintf("irqmgr.c: PRENMIから0.5秒経過\n"); // "0.5 seconds after PRENMI"
    if (OoT_StackCheck_Check(NULL) == 0) {
        osSyncPrintf("スタックは大丈夫みたいです\n"); // "The stack looks ok"
    } else {
        osSyncPrintf("%c", BEL);
        osSyncPrintf(VT_FGCOL(RED));
        // "Stack overflow or dangerous"
        osSyncPrintf("スタックがオーバーフローしたか危険な状態です\n");
        // "Increase stack size early or don't consume stack"
        osSyncPrintf("早々にスタックサイズを増やすか、スタックを消費しないようにしてください\n");
        osSyncPrintf(VT_RST);
    }
}

void OoT_IrqMgr_HandlePRENMI450(IrqMgr* this) {
    u64 temp = STATUS_NMI; // required to match
    OoT_gIrqMgrResetStatus = temp;
    this->resetStatus = STATUS_NMI;

    OoT_osSetTimer(&this->timer, OS_USEC_TO_CYCLES(30000), 0ull, &this->queue, OS_MESG_32(PRENMI480_MSG));
    OoT_IrqMgr_SendMesgForClient(this, OS_MESG_PTR(&this->nmiMsg));
}

void OoT_IrqMgr_HandlePRENMI480(IrqMgr* this) {
    u32 ret;

    OoT_osSetTimer(&this->timer, OS_USEC_TO_CYCLES(20000), 0ull, &this->queue, OS_MESG_32(PRENMI500_MSG));
    ret = OoT_osAfterPreNMI();
    if (ret) {
        // "OoT_osAfterPreNMI returned %d !?"
        osSyncPrintf("osAfterPreNMIが %d を返しました！？\n", ret);
        OoT_osSetTimer(&this->timer, OS_USEC_TO_CYCLES(1000), 0ull, &this->queue, OS_MESG_32(PRENMI480_MSG));
    }
}

void OoT_IrqMgr_HandlePRENMI500(IrqMgr* this) {
    OoT_IrqMgr_CheckStack();
}

void OoT_IrqMgr_HandleRetrace(IrqMgr* this) {
    if (OoT_gIrqMgrRetraceTime == 0ull) {
        if (this->retraceTime == 0) {
            this->retraceTime = osGetTime();
        } else {
            OoT_gIrqMgrRetraceTime = osGetTime() - this->retraceTime;
        }
    }
    OoT_sIrqMgrRetraceCount++;
    OoT_IrqMgr_SendMesgForClient(this, OS_MESG_PTR(&this->retraceMsg));
}

void OoT_IrqMgr_ThreadEntry(void* arg0) {
    OSMesg msg;
    IrqMgr* this = (IrqMgr*)arg0;
    u8 exit;

    msg.data32 = 0;
    osSyncPrintf("ＩＲＱマネージャスレッド実行開始\n"); // "Start IRQ manager thread execution"
    exit = false;

    while (!exit) {
        OoT_osRecvMesg(&this->queue, &msg, OS_MESG_BLOCK);
        switch (msg.data32) {
            case RETRACE_MSG:
                OoT_IrqMgr_HandleRetrace(this);
                break;
            case PRE_NMI_MSG:
                osSyncPrintf("PRE_NMI_MSG\n");
                // "Scheduler: Receives PRE_NMI message"
                osSyncPrintf("スケジューラ：PRE_NMIメッセージを受信\n");
                OoT_IrqMgr_HandlePreNMI(this);
                break;
            case PRENMI450_MSG:
                osSyncPrintf("PRENMI450_MSG\n");
                // "Scheduler: Receives PRENMI450 message"
                osSyncPrintf("スケジューラ：PRENMI450メッセージを受信\n");
                OoT_IrqMgr_HandlePRENMI450(this);
                break;
            case PRENMI480_MSG:
                osSyncPrintf("PRENMI480_MSG\n");
                // "Scheduler: Receives PRENMI480 message"
                osSyncPrintf("スケジューラ：PRENMI480メッセージを受信\n");
                OoT_IrqMgr_HandlePRENMI480(this);
                break;
            case PRENMI500_MSG:
                osSyncPrintf("PRENMI500_MSG\n");
                // "Scheduler: Receives PRENMI500 message"
                osSyncPrintf("スケジューラ：PRENMI500メッセージを受信\n");
                exit = true;
                OoT_IrqMgr_HandlePRENMI500(this);
                break;
            default:
                // "Unexpected message received"
                osSyncPrintf("irqmgr.c:予期しないメッセージを受け取りました(%08x)\n", msg);
                break;
        }
    }

    osSyncPrintf("ＩＲＱマネージャスレッド実行終了\n"); // "End of IRQ manager thread execution"
}

void OoT_IrqMgr_Init(IrqMgr* this, void* stack, OSPri pri, u8 retraceCount) {
    LOG_CHECK_NULL_POINTER("this", this);
    LOG_CHECK_NULL_POINTER("stack", stack);

    this->clients = NULL;
    this->retraceMsg.type = OS_SC_RETRACE_MSG;
    this->prenmiMsg.type = OS_SC_PRE_NMI_MSG;
    this->nmiMsg.type = OS_SC_NMI_MSG;
    this->resetStatus = STATUS_IDLE;
    this->resetTime = 0;

    OoT_osCreateMesgQueue(&this->queue, this->msgBuf, ARRAY_COUNT(this->msgBuf));
    OoT_osSetEventMesg(OS_EVENT_PRENMI, &this->queue, OS_MESG_32(PRE_NMI_MSG));
    OoT_osViSetEvent(&this->queue, OS_MESG_32(RETRACE_MSG), retraceCount);
    osCreateThread(&this->thread, 0x13, OoT_IrqMgr_ThreadEntry, this, stack, pri);
    OoT_osStartThread(&this->thread);
}
