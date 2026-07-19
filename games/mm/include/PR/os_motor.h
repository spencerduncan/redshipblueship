#ifndef PR_OS_MOTOR_H
#define PR_OS_MOTOR_H

#include <libultraship/libultra/motor.h>

// RSBS: revived from the `#if 0` block below (the gu.h C4013 class).
// Definition: games/mm/src/libultra/io/motor.c. OSMesgQueue/OSPfs come from
// the LUS motor.h above.
s32 MM_osMotorInit(OSMesgQueue* mq, OSPfs* pfs, s32 channel);

#if 0

#include "ultratypes.h"
#include "os_pfs.h"
#include "os_message.h"

#define MOTOR_START 1
#define MOTOR_STOP  0

#define osMotorStart(x) __osMotorAccess((x), MOTOR_START)
#define osMotorStop(x)  __osMotorAccess((x), MOTOR_STOP)

s32 __osMotorAccess(OSPfs* pfs, s32 flag);

s32 MM_osMotorInit(OSMesgQueue* mq, OSPfs* pfs, s32 channel);

#endif // 0

#endif
