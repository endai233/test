/*
 * @Author: Yangzhs 16159324+Yangzhs10@user.noreply.gitee.com
 * @Date: 2025-11-12 09:56:17
 * @LastEditors: yang 1458900080@qq.com
 * @LastEditTime: 2025-12-23 13:32:47
 * @FilePath: /v2gtp_bk1/inc/TimeOut/TimeOut.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef TIMEOUT_H
#define TIMEOUT_H

#ifdef __cplusplus
extern "C"{
#endif 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <mqueue.h>
#include "sync_vars.h"
//extern atomic_uchar timer_expired;
extern mqd_t log_msgid;

typedef enum {
    SLAC_Match_Timer = 0,
    Sequence_Timer = 1,
    BHM_CRM_Timer = 2,
    BRM_CRM_Timer = 3,
    BCP_CML_Timer = 4,
    BRO_CRO_Timer = 5,
    PreCharge_Timer = 6,
    BCS_CCS_Timer = 7,
    PowerDeliveryTimer = 8,
    CPStateChangeTimer = 9,
}  TimerType;

typedef enum{
    SLAC_Match_Timer_id = 8018,
    Sequence_Timer_id = 8019,
    BHM_CRM_Timer_id = 8020,
    BRM_CRM_Timer_id = 8021,
    BCP_CML_Timer_id = 8022,
    BRO_CRO_Timer_id = 8023,
    PreCharge_Timer_id = 8024,
    BCS_CCS_Timer_id = 8025,
    PowerDeliveryTimer_id = 8026,
    CPStateChangeTimer_id = 8027,

}  TimerIdType;


void signal_handler(int signum, siginfo_t *info, void *context);
void addsig(int sig, void(handler)(int, siginfo_t*, void*), bool restart);
int watchDog_init(timer_t* timer_id);
void TimerCreate(TimerType TimerType, timer_t* timerid);
void TimerStart(timer_t* timerid, float sec, int intervalsec);
void TimerStop(timer_t* timerid);
void TimerDelete(timer_t* timerid);

#ifdef __cplusplus
}
#endif
#endif //  TIMEOUT
