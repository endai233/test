#include "TimeOut.h"
#include "ChargeControlCCu.h"
#include "ChargeControlCCu_SM.h"
#include "SECCProtocolData.h"
#include "commonfunc.h"
#include "log.h"
#include "mqttdataProcess.h"
#include "secc_ccu_sync.h"
#include "sync_vars.h"
void signal_handler(int signum, siginfo_t *info, void *context){
    //atomic_store(&timer_expired, FLAG_SET);
    set_timer_expired(FLAG_SET);
    TimerIdType* TimerID = (TimerIdType*)info->si_value.sival_ptr;

    switch (*TimerID)
    {
        case SLAC_Match_Timer_id:{
            //atomic_store(&timer_expired, FLAG_SET);
            set_timer_expired(FLAG_SET);
            INFO_LOG(log_msgid, "[TIMER] SLAC_Match_Timer timeout fired, will send StopCause_SLAC_Match_Timeout_SetupTimer_Error");  // 新增
            secc_ccu_set_timer_stop(SLAC_Match_Timer_id);
            break;
        }
        case Sequence_Timer_id:{
            //atomic_store(&timer_expired, FLAG_SET);
            set_timer_expired(FLAG_SET);
            secc_ccu_set_timer_stop(Sequence_Timer_id);
            break;
        }
        case BHM_CRM_Timer_id:{
            secc_ccu_set_timer_stop(BHM_CRM_Timer_id);
            break;
        }
        case BRM_CRM_Timer_id:{
            secc_ccu_set_timer_stop(BRM_CRM_Timer_id);
            break;
        }
        case BCP_CML_Timer_id:{
            secc_ccu_set_timer_stop(BCP_CML_Timer_id);
            break;
        }
        case BRO_CRO_Timer_id:{
            secc_ccu_set_timer_stop(BRO_CRO_Timer_id);
            break;
        }
        case PreCharge_Timer_id:{
            //atomic_store(&timer_expired, FLAG_SET);
            set_timer_expired(FLAG_SET);
            secc_ccu_set_timer_stop(PreCharge_Timer_id);
            break;
        }
        case BCS_CCS_Timer_id:{
            secc_ccu_set_timer_stop(BCS_CCS_Timer_id);
            break;
        }
        case PowerDeliveryTimer_id:{
            //atomic_store(&timer_expired, FLAG_SET);
            set_timer_expired(FLAG_SET);
            secc_ccu_set_timer_stop(PowerDeliveryTimer_id);
            break;
        }
        case CPStateChangeTimer_id:{
            //atomic_store(&timer_expired, FLAG_SET);
            set_timer_expired(FLAG_SET);
            secc_ccu_set_timer_stop(CPStateChangeTimer_id);
            break;
        }
        default:
            break;
    }
    
    free(TimerID);
    TimerID = NULL;
}

void addsig(int sig, void(handler)(int, siginfo_t*, void*), bool restart){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    if (restart)
        sa.sa_flags |= SA_RESTART;

    sigfillset(&sa.sa_mask);

    if (sigaction(sig, &sa, NULL) == -1){
        ERROR_LOG(log_msgid, "[SYS] sigaction failed: %s", strerror(errno));
    }
}


void TimerCreate(TimerType TimerType, timer_t* timerid){
    struct sigevent sev;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM; 
    addsig(SIGALRM, signal_handler, 1);

    TimerIdType* TimerID = malloc(sizeof(TimerIdType));

    switch (TimerType)
    {
        case SLAC_Match_Timer:{
            *TimerID = SLAC_Match_Timer_id;
            sev.sigev_value.sival_ptr = (void*)TimerID;
            break;
        }
        case Sequence_Timer:{
            *TimerID = Sequence_Timer_id;
            sev.sigev_value.sival_ptr = (void*)TimerID;
            break;
        }
        case BHM_CRM_Timer:{
            *TimerID = BHM_CRM_Timer_id;
            sev.sigev_value.sival_ptr = (void*)TimerID;
            break;
        }
        case BRM_CRM_Timer:{
            *TimerID = BRM_CRM_Timer_id;
            sev.sigev_value.sival_ptr = (void*)TimerID;
            break;
        }
        case BCP_CML_Timer:{
            *TimerID = BCP_CML_Timer_id;
            sev.sigev_value.sival_ptr = (void*)TimerID;
            break;
        }
        case BRO_CRO_Timer:{
            *TimerID = BRO_CRO_Timer_id;
            sev.sigev_value.sival_ptr = (void*)TimerID;
            break;
        }
        case PreCharge_Timer:{
            *TimerID = PreCharge_Timer_id;
            sev.sigev_value.sival_ptr = (void*)TimerID;
            break;
        }
        case BCS_CCS_Timer:{
            *TimerID = BCS_CCS_Timer_id;
            sev.sigev_value.sival_ptr = (void*)TimerID;
            break;
        }
        case PowerDeliveryTimer:{
            *TimerID = PowerDeliveryTimer_id;
            sev.sigev_value.sival_ptr = (void*)TimerID;
            break;
        }
        case CPStateChangeTimer:{
            *TimerID = CPStateChangeTimer_id;
            sev.sigev_value.sival_ptr = (void*)TimerID;
            break;
        }
        default:
            break;
    }

    if (timer_create(CLOCK_REALTIME, &sev, timerid) == -1) {
        ERROR_LOG(log_msgid, "[SYS] timer_create failed:%s", strerror(errno));
        free(TimerID);
        TimerID = NULL;
    }
}

void TimerStart(timer_t* timerid, float sec, int intervalsec){
    struct itimerspec its;

    int int_part = (int)(sec);
    float flac_part = 0;
    if(int_part != sec){
        flac_part = sec - int_part;
    }
    its.it_value.tv_sec = int_part;
    its.it_value.tv_nsec = flac_part * 1000000000;
    its.it_interval.tv_sec = intervalsec; 
    its.it_interval.tv_nsec = 0;

    if (timer_settime(*timerid, 0, &its, NULL) == -1) {
        ERROR_LOG(log_msgid, "[SYS] TimerStart timer_settime failed:%s", strerror(errno));
    }
}

void TimerStop(timer_t* timerid) {
    struct itimerspec its = {0}; 

    if (timer_settime(*timerid, 0, &its, NULL) == -1) {
        ERROR_LOG(log_msgid, "[SYS] TimerStop timer_settime failed: %s", strerror(errno));
    }
}

void TimerDelete(timer_t* timerid){
    if (timer_delete(*timerid) == -1) {
        perror("timer_delete");
        ERROR_LOG(log_msgid, "[SYS] timer_delete failed: %s", strerror(errno));
    }
}

