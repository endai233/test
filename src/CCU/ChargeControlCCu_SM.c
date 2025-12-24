#include "ChargeControlCCu_SM.h"
#include "ChargeControlCCu_Data.h"
#include "ChargeControlCCu_Task.h"
#include "ChargeControlCCu.h"
#include "commonfunc.h"
#include "Transport.h"
#include "TimeOut.h"
#include "log.h"
#include "cp.h"
#include "mqttAnalysis.h"
#include "secc_ccu_sync.h"
#include "zlog.h"
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "ChargeControlCCu_Send_Can.h"
#include "ChargeControlCCu_Share.h"
/**
 * @brief 调整充电步骤
 *
 * 该函数用于调整充电步骤，将充电步骤更新为传入的参数值。
 *
 * @param step 新的充电步骤值
 * @param CCu_Data 指向充电数据的指针
 */
void chargeStepJump(int step, struct CCu_Charge_Data *CCu_Data)
{
    CCu_Data->opt_old = CCu_Data->opt;
    CCu_Data->opt = step;
}

void secc_ccu_set_timer_stop(TimerIdType TimerID)
{
    struct CCu_Charge_Data *CCu_Data = Get_CCu_Charge_Data();
    if (CCu_Data == NULL)
    {
        return;
    }
    CCu_Data->TimerID = TimerID;
    chargeStepJump(CHARGE_FUN_T_STOP, CCu_Data);
}

/**
 * @brief 检查充电条件是否满足
 *
 * 根据输入的充电数据结构体 CCu_Data，检查充电条件是否满足，并根据条件执行相应的充电步骤跳转。
 *
 * @param CCu_Data 指向充电数据结构体 CCu_Charge_Data 的指针
 * @return 如果充电条件满足，则返回 FLAG_SET；否则返回 FLAG_RESET
 */

static int ChargeConditionCheck(struct CCu_Charge_Data *CCu_Data)
{
    // 检查CCu_Data指针是否为空，防止空指针解引用
    if (CCu_Data == NULL)
    {
        ERROR_LOG(log_msgid, "CCu_Data is NULL in ChargeConditionCheck");
        return FLAG_RESET;
    }

    if (CCu_Data->CPM_value.CPStatus == CPStatusA)
    {
        cp_config_pwm(1000, 1000, true);
        chargeStepJump(CHARGE_FUN_STANDBY, CCu_Data);
        return FLAG_SET;
    }

    if (CCu_Data->EV_Abnormal_Stop_flag == FLAG_SET)
    {
        chargeStepJump(CHARGE_FUN_A_EV_STOP, CCu_Data);
        return FLAG_SET;
    }

    if (CCu_Data->CST_recv_flag == FLAG_SET)
    {
        if (CCu_Data->CCu_Abnormal_Stop_flag == FLAG_SET)
        {
            chargeStepJump(CHARGE_FUN_A_CCu_STOP, CCu_Data);
            return FLAG_SET;
        }

        if (CCu_Data->CCu_Normal_Stop_flag == FLAG_SET)
        {
            chargeStepJump(CHARGE_FUN_N_CCu_STOP, CCu_Data);
            return FLAG_SET;
        }
    }

    if (CCu_Data->EV_Normal_Stop_flag == FLAG_SET)
    {
        if (CCu_Data->opt != CHARGE_FUN_N_EV_STOP && CCu_Data->opt_old != CHARGE_FUN_N_EV_STOP)
        {
            chargeStepJump(CHARGE_FUN_N_EV_STOP, CCu_Data);
            return FLAG_SET;
        }
    }

    return FLAG_RESET;
}

/**
 * @brief 进入充电待机状态
 *
 * 将充电状态设置为待机状态，并发送相应的数据包。
 *
 * @param fd 文件描述符，用于串口通信
 * @param CCu_Data 指向CCu_Charge_Data结构的指针，包含充电相关数据
 */
void Charge_StandBy(struct CCu_Charge_Data *CCu_Data)
{
    static StandByState state = SB_STATE_IDLE;
    struct DataPacket send_data;
    memset((void *)(&send_data), 0, sizeof(send_data));

    // 检查CCu_Data指针是否为空，防止空指针解引用
    if (CCu_Data == NULL)
    {
        ERROR_LOG(log_msgid, "CCu_Data is NULL in Charge_StandBy");
        return;
    }

    if (state == SB_STATE_IDLE)
    {
        CCu_Data->CHM_recv_flag = FLAG_RESET;
        INFO_LOG(log_msgid, "[GBT] Current stage:Charge_StandBy");
        state = SB_STATE_WAIT_PLUG;
    }

    writeCanData(&send_data, CCu_Data, SECC_CMD_BHM, 0x00, ChargeStopNoError);


    if (CCu_Data->CHM_value.SECC_Ongoing == 0x10)
    {
        cp_config_pwm(1000, 50, true);
        CCu_Data->CPM_value.CPStatus = CPStatusB2;
        chargeStepJump(CHARGE_FUN_INIT, CCu_Data);
        state = SB_STATE_IDLE;
    }

    else if (CCu_Data->CPM_value.CPStatus == CPStatusB2)
    {

        chargeStepJump(CHARGE_FUN_INIT, CCu_Data);
        CCu_Data->CHM_value.EVCCIDACK = 0;
        state = SB_STATE_IDLE;
    }

    CCu_Data->CST_recv_flag = FLAG_RESET;
    CCu_Data->CSD_recv_flag = FLAG_RESET;
    set_timer_expired(FLAG_RESET);
}



void Charge_Initialize(struct CCu_Charge_Data *CCu_Data)
{
    static InitState state = INIT_STATE_IDLE;
    static ChargeType charge_type = CHARGE_TYPE_UNKNOWN;
    static timer_t slac_timer = 0;
    static timer_t crm_timer = 0;
    static time_t slac_ready_time; // SLAC就绪指示发送时间
    
    struct DataPacket send_bhm, send_bid;
    memset(&send_bhm, 0, sizeof(send_bhm));
    memset(&send_bid, 0, sizeof(send_bid));
    int is_fatal_error;

    // ========== 状态机初始化 ==========
    if (state == INIT_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT] Charge_Initialize: Enter IDLE state");

        // 先删除旧定时器
        if (slac_timer != 0)
        {
            TimerDelete(&slac_timer);
            slac_timer = 0;
        }
        if (crm_timer != 0)
        {
            TimerDelete(&crm_timer);
            crm_timer = 0;
        }
        // 创建定时器
        TimerCreate(SLAC_Match_Timer, &slac_timer);
        TimerCreate(BHM_CRM_Timer, &crm_timer);
        
        state = INIT_STATE_WAIT_CONDITION;
        charge_type = CHARGE_TYPE_UNKNOWN;
    }

    // ========== 异常退出与状态跳转检查 ==========
    if (CCu_Data->CPM_value.CPStatus == CPStatusA || 
        (ChargeConditionCheck(CCu_Data) && get_timer_expired()))
    {
        INFO_LOG(log_msgid, "[GBT] Abort: CPStatus=%d, expired=%d", 
                 CCu_Data->CPM_value.CPStatus, get_timer_expired());
        
        chargeStepJump(CHARGE_FUN_T_PLUGOUT, CCu_Data);
        cp_config_pwm(1000, 1000, true);
        // 清理资源
        state = INIT_STATE_IDLE;
        charge_type = CHARGE_TYPE_UNKNOWN;
        if (slac_timer != 0) { TimerDelete(&slac_timer); slac_timer = 0; }
        if (crm_timer != 0) { TimerDelete(&crm_timer); crm_timer = 0; }
        memset(CCu_Data, 0, sizeof(struct CCu_Charge_Data));
        return;
    }

    // ========== 始终发送BHM消息 ==========
    writeCanData(&send_bhm, CCu_Data, SECC_CMD_BHM, 0x00, ChargeStopNoError);

    // ========== 识别充电类型（仅识别一次）==========
    if (charge_type == CHARGE_TYPE_UNKNOWN && CCu_Data->CPM_value.CPStatus == CPStatusB2)
    {
        if (CCu_Data->CHM_value.EVCCIDACK == FLAG_RESET && CCu_Data->V2G_value.evccid > 0)
        {
            charge_type = CHARGE_TYPE_AUTO;
            INFO_LOG(log_msgid, "[GBT] Charge type: AUTO (evccid=%d)", CCu_Data->V2G_value.evccid);
        }
        else if (CCu_Data->CHM_value.EVCCIDACK == FLAG_SET && CCu_Data->V2G_value.evccid == 0)
        {
            charge_type = CHARGE_TYPE_CARD;
            INFO_LOG(log_msgid, "[GBT] Charge type: CARD (EVCCIDACK=1)");
        }
    }

    // ========== 状态机主逻辑 ==========
    switch (state)
    {
    case INIT_STATE_WAIT_CONDITION:
        // 等待充电条件满足：充电类型已识别 && CP状态为B2
        if (charge_type != CHARGE_TYPE_UNKNOWN && CCu_Data->CPM_value.CPStatus == CPStatusB2)
        {
            INFO_LOG(log_msgid, "[GBT] Condition met, sending BID (type=%d)", charge_type);
            writeCanData(&send_bid, CCu_Data, SECC_CMD_BID, 0x00, ChargeStopNoError);
            state = INIT_STATE_BID_SENT;
        }
        break;

    case INIT_STATE_BID_SENT:
        // BID已发送，等待CHM消息
        if (CCu_Data->CHM_value.SECC_Ongoing == 0x10)
        {
            INFO_LOG(log_msgid, "[GBT] CHM received, SECC ongoing");
            cp_config_pwm(1000, 50, true);
            CCu_Data->CPM_value.CPStatus = CPStatusB2;
            mqtt_slac_start_send();
            mqtt_cp_stat_send(CCu_Data->CPM_value);

            // 设置标志，防止在SLAC_MATCHING状态重复发送
            CCu_Data->slac_ready_send_flag = FLAG_SET;
            CCu_Data->slac_ready_retry_count = 1;
            slac_ready_time = time(NULL);
            TimerStart(&slac_timer, SLAC_MATCH_TIMEOUT, 0);

            state = INIT_STATE_SLAC_MATCHING;
        }
        break;

    case INIT_STATE_SLAC_MATCHING:
        // SLAC匹配过程：发送就绪指示并等待匹配完成
        if (CCu_Data->readycnf != FLAG_SET && 
            CCu_Data->slac_ready_retry_count < MAX_RETRIES &&
            CCu_Data->CPM_value.CPStatus == CPStatusB2 &&
            CCu_Data->CHM_recv_flag == FLAG_SET)
        {
            // 发送SLAC就绪指示
            if (CCu_Data->slac_ready_send_flag == FLAG_RESET)
            {
                slac_ready_time = time(NULL);
                mqtt_slac_start_send();
                
                // 首次发送时启动超时定时器
                if (CCu_Data->slac_ready_retry_count == 0)
                {
                    TimerStart(&slac_timer, SLAC_MATCH_TIMEOUT, 0);
                }
                
                CCu_Data->slac_ready_send_flag = FLAG_SET;
                CCu_Data->slac_ready_retry_count++;
                INFO_LOG(log_msgid, "[CCU] SLAC ready sent, retry=%d", CCu_Data->slac_ready_retry_count);
            }
            
            // 检查重发超时
            if (time(NULL) - slac_ready_time >= SLAC_READY_IND_TIMEOUT)
            {
                CCu_Data->slac_ready_send_flag = FLAG_RESET;
            }
        }

        // 检查SLAC匹配完成
        if (CCu_Data->match == FLAG_SET)
        {
            INFO_LOG(log_msgid, "[CCU] SLAC match done, waiting for CRM");
            TimerStop(&slac_timer);
            TimerStart(&crm_timer, BHM_CRM_TIMEOUT, 0);
            state = INIT_STATE_WAIT_CRM;
        }
        break;

    case INIT_STATE_WAIT_CRM:
        // 等待CRM消息
        if (CCu_Data->CRM_recv_flag == FLAG_SET && CCu_Data->CHM_value.SECC_Ongoing == 0x10)
        {
            INFO_LOG(log_msgid, "[GBT] CRM received (id=%02x, EVCCIDACK=%d), entering handshake", 
                     CCu_Data->CRM_value.identification, CCu_Data->CHM_value.EVCCIDACK);
            
            TimerStop(&crm_timer);
            chargeStepJump(CHARGE_FUN_HANDSHAKE, CCu_Data);
            
            // 清理资源
            state = INIT_STATE_IDLE;
            charge_type = CHARGE_TYPE_UNKNOWN;
            if (slac_timer != 0) { TimerDelete(&slac_timer); slac_timer = 0; }
            if (crm_timer != 0) { TimerDelete(&crm_timer); crm_timer = 0; }
            
            cp_config_pwm(1000, 50, true);
            return;
        }
        break;

    default:
        break;
    }
}

void Charge_HandShake(struct CCu_Charge_Data *CCu_Data)
{
    static HandShakeState state = HS_STATE_IDLE;
    static timer_t timerid = 0;
    struct DataPacket send_data;
    memset((void *)(&send_data), 0, sizeof(send_data));

    if (state == HS_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT] Charge_HandShake: Enter IDLE state");
        if (timerid != 0)
        {
            TimerDelete(&timerid);
            timerid = 0;
        }
        TimerCreate(BRM_CRM_Timer, &timerid);
        state = HS_STATE_BRM_SEND;
    }

    writeCanData(&send_data, CCu_Data, SECC_CMD_BRM, 0x00, ChargeStopNoError);

    // 检查是否满足充电条件或超时
    if (ChargeConditionCheck(CCu_Data) || get_timer_expired())
    {
        if (timerid != 0)
        {
            TimerDelete(&timerid);
            timerid = 0;
        }
        state = HS_STATE_IDLE;
        return;
    }

    switch (state)
    {
    case HS_STATE_BRM_SEND:
        TimerStart(&timerid, BRM_CRM_TIMEOUT, 0);
        INFO_LOG(log_msgid, "[GBT] BRM send");
        state = HS_STATE_WAIT_CRM;
        break;

    case HS_STATE_WAIT_CRM:
        if (CCu_Data->CRM_recv_flag == FLAG_SET && CCu_Data->CRM_value.identification == 0xAA)
        {
            TimerStop(&timerid);
            INFO_LOG(log_msgid, "[GBT] EVSEID:%02x", CCu_Data->CRM_value.EVSEID);
            CCu_Data->CRM_recv_flag = FLAG_RESET; 
            INFO_LOG(log_msgid, "[GBT] Entering Identification stage (merged with HandShake)");
            state = HS_STATE_IDENTIFICATION;
        }
        break;

    case HS_STATE_IDENTIFICATION:
        // 收到CRM后，直接跳转到参数交换阶段
        INFO_LOG(log_msgid, "[GBT] startChargeParamDiscovery received, moving to PARAMETEREXCHANGE");
        chargeStepJump(CHARGE_FUN_PARAMETEREXCHANGE, CCu_Data);
        
        if (timerid != 0) {
            TimerDelete(&timerid);
            timerid = 0;
        }
        state = HS_STATE_IDLE;
        break;

    default:
        break;
    }
}

void Charge_Param_Exchange(struct CCu_Charge_Data *CCu_Data)
{
    static ParamExchangeState state = PE_STATE_IDLE;
    static timer_t timerid = 0;
    struct DataPacket send_data;
    memset((void *)(&send_data), 0, sizeof(send_data));

    if (state == PE_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT] Charge_Param_Exchange: Enter IDLE state");
        if (timerid != 0) { TimerDelete(&timerid); timerid = 0; }
        TimerCreate(BCP_CML_Timer, &timerid);
        state = PE_STATE_BCP_SEND;
    }
    
    writeCanData(&send_data, CCu_Data, SECC_CMD_BCP, 0x00, ChargeStopNoError);

    if (ChargeConditionCheck(CCu_Data) || get_timer_expired())
    {
        if (timerid != 0) {
            TimerDelete(&timerid);
            timerid = 0;
        }
        state = PE_STATE_IDLE;
        return;
    }

    switch (state)
    {
    case PE_STATE_BCP_SEND:
        TimerStart(&timerid, BCP_CML_TIMEOUT, 0);
        INFO_LOG(log_msgid, "[GBT] BCP send");
        state = PE_STATE_WAIT_CML;
        break;

    case PE_STATE_WAIT_CML:
        if (CCu_Data->CML_recv_flag == FLAG_SET)
        {
            TimerStop(&timerid);
            INFO_LOG(log_msgid, "[GBT] CML receive");
            INFO_LOG(log_msgid, "[GBT] Date:%02x%02x%02x%02x%02x%02x", CCu_Data->CTS_value.years, CCu_Data->CTS_value.months, CCu_Data->CTS_value.days,
                     CCu_Data->CTS_value.hours, CCu_Data->CTS_value.minutes, CCu_Data->CTS_value.seconds);
            chargeStepJump(CHARGE_FUN_CABLECHECKREADY, CCu_Data);

            if (timerid != 0)
            {
                TimerDelete(&timerid);
                timerid = 0;
            }
            state = PE_STATE_IDLE;
        }
        else
        {
             // usleep(DATA_SEND_TIME);
        }
        break;

    default:
        break;
    }
}

void Charge_Isolation_Ready(struct CCu_Charge_Data *CCu_Data)
{
    static IsolationReadyState state = IR_STATE_IDLE;
    struct DataPacket send_data;
    memset((void *)(&send_data), 0, sizeof(send_data));

    if (state == IR_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT] Charge_Isolation_Ready: Enter IDLE state");
        state = IR_STATE_BRO00_SEND;
    }

    writeCanData(&send_data, CCu_Data, SECC_CMD_BRO, 0x00, ChargeStopNoError);

    if (ChargeConditionCheck(CCu_Data) || get_timer_expired())
    {
        state = IR_STATE_IDLE;
        return;
    }

    switch (state)
    {
    case IR_STATE_BRO00_SEND:
        INFO_LOG(log_msgid, "[GBT] BRO00 send");
        INFO_LOG(log_msgid, "[GBT] EV is not ready for Isolation check");
        state = IR_STATE_WAIT_CHECK;
        break;

    case IR_STATE_WAIT_CHECK:
        if (CCu_Data->startCableCheck == FLAG_SET)
        {
            chargeStepJump(CHARGE_FUN_CABLECHECK, CCu_Data);
            state = IR_STATE_IDLE;
        }
        break;

    default:
        break;
    }
}

void Charge_Isolation_Check(struct CCu_Charge_Data *CCu_Data)
{
    static IsolationCheckState state = IC_STATE_IDLE;
    static timer_t timerid = 0, timerid2 = 0;
    struct DataPacket send_data;
    memset((void *)(&send_data), 0, sizeof(send_data));

    if (state == IC_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT] Charge_Isolation_Check: Enter IDLE state");
        if (timerid != 0) { TimerDelete(&timerid); timerid = 0; }
        if (timerid2 != 0) { TimerDelete(&timerid2); timerid2 = 0; }
        TimerCreate(BRO_CRO_Timer, &timerid);
        TimerCreate(BRO_CRO_Timer, &timerid2);
        state = IC_STATE_BROAA_SEND;
    }

    writeCanData(&send_data, CCu_Data, SECC_CMD_BRO, 0xAA, ChargeStopNoError);

    if (ChargeConditionCheck(CCu_Data) || get_timer_expired())
    {
        if (timerid != 0) { TimerDelete(&timerid); timerid = 0; }
        if (timerid2 != 0) { TimerDelete(&timerid2); timerid2 = 0; }
        state = IC_STATE_IDLE;
        return;
    }

    switch (state)
    {
    case IC_STATE_BROAA_SEND:
        TimerStart(&timerid, BRO_CRO00_TIMEOUT, 0);
        TimerStart(&timerid2, BRO_CROAA_TIMEOUT, 0);
        INFO_LOG(log_msgid, "[GBT] BROAA send");
        INFO_LOG(log_msgid, "[GBT] EV is ready for Isolation check");
        state = IC_STATE_CRO00_GET;
        break;

    case IC_STATE_CRO00_GET:
        if (CCu_Data->CRO_recv_flag == FLAG_SET)
        {
            TimerStop(&timerid);
            state = IC_STATE_CROAA_GET;
        }
        break;

    case IC_STATE_CROAA_GET:
        if (CCu_Data->CRO_recv_flag == FLAG_SET && CCu_Data->CRO_value.identification == 0xAA)
        {
            TimerStop(&timerid2);
            INFO_LOG(log_msgid, "[GBT] CROAA receive");
            INFO_LOG(log_msgid, "[GBT] Isolation finish");

            if (CCu_Data->startPreCharge == FLAG_SET)
            {
                chargeStepJump(CHARGE_FUN_PRECHARGE, CCu_Data);
                
                if (timerid != 0) { TimerDelete(&timerid); timerid = 0; }
                if (timerid2 != 0) { TimerDelete(&timerid2); timerid2 = 0; }
                state = IC_STATE_IDLE;
            }
        }
        break;

    default:
        break;
    }
}

void Charge_PreCharge_Stage(struct CCu_Charge_Data *CCu_Data)
{
    INFO_LOG(log_msgid, "[CCU_TASK] Enter Charge_PreCharge_Stage");
    static PreChargeState state = PC_STATE_IDLE;
    static timer_t timerid = 0;
    struct DataPacket send_data1;
    struct DataPacket send_data2;
    memset((void *)(&send_data1), 0, sizeof(send_data1));
    memset((void *)(&send_data2), 0, sizeof(send_data2));

    if (state == PC_STATE_IDLE)
    {   
        INFO_LOG(log_msgid, "[GBT] Charge_PreCharge_Stage: Enter IDLE state");
        if (timerid != 0) { TimerDelete(&timerid); timerid = 0; }
        TimerCreate(BCS_CCS_Timer, &timerid);
        state = PC_STATE_BCL_SEND;
    }

    if (ChargeConditionCheck(CCu_Data) || get_timer_expired())
    {
        if (timerid != 0) { TimerDelete(&timerid); timerid = 0; }
        state = PC_STATE_IDLE;
        return;
    }

    // Always send BCL and BCS
    writeCanData(&send_data1, CCu_Data, SECC_CMD_BCL, 0x01, ChargeStopNoError);
    writeCanData(&send_data2, CCu_Data, SECC_CMD_BCS, 0x00, ChargeStopNoError);

    switch (state)
    {
    case PC_STATE_BCL_SEND:
        TimerStart(&timerid, BCS_CCS_TIMEOUT, 0);
        INFO_LOG(log_msgid, "[GBT] PreCharge BCS BCL send");
        state = PC_STATE_WAIT_CCS;
        break;

    case PC_STATE_WAIT_CCS:
        if (CCu_Data->CCS_recv_flag == FLAG_SET)
        {
            TimerStart(&timerid, BCS_CCS_TIMEOUT, 0); // Reset timer on receive
            CCu_Data->CCS_recv_flag = FLAG_RESET;
        }

        if (CCu_Data->vehicleReadyToCharge == FLAG_SET)
        {
            chargeStepJump(CHARGE_FUN_CHARGING, CCu_Data);
            if (timerid != 0) { TimerDelete(&timerid); timerid = 0; }
            state = PC_STATE_IDLE;
        }
        break;

    default:
        break;
    }
}

void Charge_Charging_Stage(struct CCu_Charge_Data *CCu_Data)
{
    INFO_LOG(log_msgid, "[CCU_TASK] Enter Charge_Charging_Stage");
    static ChargingState state = CH_STATE_IDLE;
    static timer_t timerid = 0;
    struct DataPacket send_data1;
    struct DataPacket send_data2;
    memset((void *)(&send_data1), 0, sizeof(send_data1));
    memset((void *)(&send_data2), 0, sizeof(send_data2));
    
    if (state == CH_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT] Charge_Charging_Stage: Enter IDLE state");
        if (timerid != 0) { TimerDelete(&timerid); timerid = 0; }
        TimerCreate(BCS_CCS_Timer, &timerid);
        state = CH_STATE_BCS_SEND;
    }
    
    if (ChargeConditionCheck(CCu_Data) || get_timer_expired())
    {
        if (timerid != 0) { TimerDelete(&timerid); timerid = 0; }
        state = CH_STATE_IDLE;
        return;
    }

    // Always send BCL and BCS
    writeCanData(&send_data1, CCu_Data, SECC_CMD_BCL, 0x00, ChargeStopNoError);
    writeCanData(&send_data2, CCu_Data, SECC_CMD_BCS, 0x00, ChargeStopNoError);
    

    switch (state)
    {
    case CH_STATE_BCS_SEND:
        TimerStart(&timerid, BCS_CCS_TIMEOUT, 0);
        state = CH_STATE_WAIT_CCS;
        break;

    case CH_STATE_WAIT_CCS:
        if (CCu_Data->CCS_recv_flag == FLAG_SET)
        {
            TimerStart(&timerid, BCS_CCS_TIMEOUT, 0); // Reset timer
            CCu_Data->CCS_recv_flag = FLAG_RESET;
        }
        break;

    default:
        break;
    }
}

void Charge_Normal_EV_Stop(struct CCu_Charge_Data *CCu_Data)
{
    static NormalEVStopState state = NES_STATE_IDLE;
    static time_t start_time = 0;
    time_t current_time;
    struct DataPacket send_data;
    memset((void *)(&send_data), 0, sizeof(send_data));

    if (state == NES_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT] Current stage: Charge_Normal_EV_Stop");
        state = NES_STATE_BST_SEND;
    }

    writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, ChargeStopNoError);

    // Only process if EV_Normal_Stop_flag is set (which is checked in ConditionCheck before, but here explicit)
    if (CCu_Data->EV_Normal_Stop_flag == FLAG_SET)
    {
        switch (state)
        {
        case NES_STATE_BST_SEND:
            start_time = time(NULL);
            INFO_LOG(log_msgid, "[GBT] BST send");
            state = NES_STATE_WAIT_CST;
            break;

        case NES_STATE_WAIT_CST:
            current_time = time(NULL);
            if (current_time - start_time > BST_CST_TIMEOUT)
            {
                ERROR_LOG(log_msgid, "[GBT] BSTCST_Timeout_SetupTimer_Error");
                chargeStepJump(CHARGE_FUN_WELDINGDECTION, CCu_Data);
                state = NES_STATE_IDLE;
                return;
            }

            if (CCu_Data->CST_recv_flag == FLAG_SET)
            {
                INFO_LOG(log_msgid, "[GBT] CST receive");
                chargeStepJump(CHARGE_FUN_WELDINGDECTION, CCu_Data);
                state = NES_STATE_IDLE;
                return;
            }
            break;

        default:
            break;
        }
    }
}

void Charge_Normal_CCu_Stop(struct CCu_Charge_Data *CCu_Data)
{
    static NormalCCuStopState state = NCS_STATE_IDLE;
    static time_t start_time = 0;
    time_t current_time;
    struct DataPacket send_data;
    memset((void *)(&send_data), 0, sizeof(send_data));

    if (state == NCS_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT] Current stage: Charge_Normal_CCu_Stop");
        state = NCS_STATE_WAIT_CST;
    }

    // 写BST统计报文
    writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, ChargeStopNoError);

    if (CCu_Data->CCu_Normal_Stop_flag == FLAG_SET && CCu_Data->CST_recv_flag == FLAG_SET)
    {
        switch (state)
        {
        case NCS_STATE_WAIT_CST:
            INFO_LOG(log_msgid, "[GBT] CST receive");
            state = NCS_STATE_BST_SEND;
            break;

        case NCS_STATE_BST_SEND:
            start_time = time(NULL);
            INFO_LOG(log_msgid, "[GBT] BST send");
            state = NCS_STATE_WAIT_CSD;
            break;

        case NCS_STATE_WAIT_CSD:
            current_time = time(NULL);
            if (current_time - start_time > BST_CSD00_TIMEOUT)
            {
                if (CCu_Data->sessionStop == FLAG_SET)
                {
                    INFO_LOG(log_msgid, "[GBT] BST->CSD00 timeout but sessionStop is set, continue to WeldingDetection");
                }
                else
                {
                    ERROR_LOG(log_msgid, "[GBT] BSTCSD00_Timeout_SetupTimer_Error");
                }
                chargeStepJump(CHARGE_FUN_WELDINGDECTION, CCu_Data);
                state = NCS_STATE_IDLE;
                return;
            }
            if (CCu_Data->CSD_recv_flag == FLAG_SET)
            {
                INFO_LOG(log_msgid, "[GBT] CSD00 receive");
                chargeStepJump(CHARGE_FUN_WELDINGDECTION, CCu_Data);
                state = NCS_STATE_IDLE;
                return;
            }
            break;

        default:
            break;
        }
    }
}

void Charge_WeldingDetection(struct CCu_Charge_Data *CCu_Data)
{
    static WeldingDetectionState state = WD_STATE_IDLE;
    static time_t start_time;
    time_t current_time;
    struct DataPacket send_data;
    memset((void *)(&send_data), 0, sizeof(send_data));

    if (state == WD_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT] Current stage:Charge_WeldingDetection");
        state = WD_STATE_BSD00_SEND;
    }

    if (state == WD_STATE_BSD00_SEND || state == WD_STATE_BSD01_SEND)
    {
        writeCanData(&send_data, CCu_Data, SECC_CMD_BSD, 0x00, ChargeStopNoError);
        if (state == WD_STATE_BSD00_SEND)
        {
            start_time = time(NULL);
            INFO_LOG(log_msgid, "[GBT] BSD00 send");
            state = WD_STATE_BSD01_SEND;
        }
        if (CCu_Data->sessionStop == FLAG_SET)
        {
            state = WD_STATE_WAIT_CSD01;
            start_time = time(NULL);
            INFO_LOG(log_msgid, "[GBT] BSD01 send");
        }
        current_time = time(NULL);
        if (current_time - start_time > BSD00_BSD01_TIMEOUT)
        {
            if (state == WD_STATE_BSD01_SEND)
                ERROR_LOG(log_msgid, "[GBT] BSD00_Timeout_SetupTimer_Error");
            state = WD_STATE_WAIT_CSD01;
            start_time = time(NULL);
            INFO_LOG(log_msgid, "[GBT] BSD01 send");
            set_timer_expired(FLAG_SET);
        }
    }
    else if (state == WD_STATE_WAIT_CSD01)
    {

        writeCanData(&send_data, CCu_Data, SECC_CMD_BSD, 0x01, ChargeStopNoError);

        current_time = time(NULL);
        if (current_time - start_time > BSD01_CSD01_TIMEOUT)
        {
            ERROR_LOG(log_msgid, "[GBT] BSD01_CSD01_Timeout_SetupTimer_Error");
            INFO_LOG(log_msgid, "[GBT] BSD01_CSD01_Timeout sessionStop=%d, CSD_recv_flag=%d, chargeoverACK=%d",
                     CCu_Data->sessionStop, CCu_Data->CSD_recv_flag,
                     (CCu_Data->CSD_recv_flag == FLAG_SET) ? CCu_Data->CSD_value.chargeoverACK : -1);
            chargeStepJump(CHARGE_FUN_T_PLUGOUT, CCu_Data);
            state = WD_STATE_IDLE;
            // 关闭pwm
            cp_config_pwm(1000, 1000, true);
            return;
        }
        if (CCu_Data->CSD_recv_flag == FLAG_SET && CCu_Data->CSD_value.chargeoverACK == FLAG_SET && CCu_Data->sessionStop)
        {
            INFO_LOG(log_msgid, "[GBT] CSD01 receive");
            chargeStepJump(CHARGE_FUN_T_PLUGOUT, CCu_Data);
            state = WD_STATE_IDLE;
            // 关闭pwm
            cp_config_pwm(1000, 1000, true);
            return;
        }
    }
}

void Charge_Abnormal_CCu_Stop(struct CCu_Charge_Data *CCu_Data)
{
    static AbnormalCCuStopState state = ACS_STATE_IDLE;
    static time_t start_time;
    time_t current_time;
    struct DataPacket send_data;
    memset((void *)(&send_data), 0, sizeof(send_data));

    if (state == ACS_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT]  Current stage: Charge_Abnormal_CCu_Stop");
        state = ACS_STATE_WAIT_CST;
    }

    if (CCu_Data->CCu_Abnormal_Stop_flag == FLAG_SET && CCu_Data->CST_recv_flag == FLAG_SET)
    {
        writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, ChargeStopNoError);

        if (state == ACS_STATE_WAIT_CST)
        {
            switch (CCu_Data->CST_value.CCu_StopCause)
            {
                case OtherError:
                    ERROR_LOG(log_msgid, "[GBT] CCu OtherError error");
                    break;
                case ChargeParameterNotMatch:
                    ERROR_LOG(log_msgid, "[GBT] ChargeParameterNotMatch error");
                    break;
                case IsolationWarning:
                    ERROR_LOG(log_msgid, "[GBT] IsolationWarning error");
                    break;
                case IsolationFault:
                    ERROR_LOG(log_msgid, "[GBT] IsolationFault error");
                    break;
                case TimeOutError:
                    ERROR_LOG(log_msgid, "[GBT] CCu TimeOutError error");
                    break;
                case PresentVoltageTooLow:
                    ERROR_LOG(log_msgid, "[GBT] PresentVoltageTooLow error");
                    break;
                case CPStatusErrorA:
                    ERROR_LOG(log_msgid, "[GBT] CP status A error");
                    break;
                case CPStatusErrorB1:
                    ERROR_LOG(log_msgid, "[GBT] CP status B1 error");
                    break;
                case CPStatusErrorB2:
                    ERROR_LOG(log_msgid, "[GBT] CP status B2 error");
                    break;
                case CPStatusErrorC1:
                    ERROR_LOG(log_msgid, "[GBT] CP status C1 error");
                    break;
                case CPStatusErrorC2:
                    ERROR_LOG(log_msgid, "[GBT] CP status C2 error");
                    break;
                case CPStatusErrorD1:
                    ERROR_LOG(log_msgid, "[GBT] CP status D1 error");
                    break;
                case CPStatusErrorD2:
                    ERROR_LOG(log_msgid, "[GBT] CP status D2 error");
                    break;
                case CPStatusErrorE:
                    ERROR_LOG(log_msgid, "[GBT] CP status E error");
                    break;
                case CPStatusErrorF:
                    ERROR_LOG(log_msgid, "[GBT] CP status F error");
                    break;
                default:
                    break;
            }

            INFO_LOG(log_msgid, "[GBT] CST receive");
            state = ACS_STATE_BST_SEND;
        }

        if (state == ACS_STATE_BST_SEND)
        {
            start_time = time(NULL);
            INFO_LOG(log_msgid, "[GBT] BST send");
            state = ACS_STATE_WAIT_CSD;
        }

        current_time = time(NULL);
        if (current_time - start_time > BST_CSD00_TIMEOUT)
        {
            // 与正常停相同：如果 SessionStop 已经建立，则不视为错误，直接进入焊检
            if (CCu_Data->sessionStop == FLAG_SET)
            {
                INFO_LOG(log_msgid, "[GBT] BST->CSD00 timeout but sessionStop is set, continue to WeldingDetection");
            }
            else
            {
                ERROR_LOG(log_msgid, "[GBT] BSTCSD00_Timeout_SetupTimer_Error");
            }
            chargeStepJump(CHARGE_FUN_WELDINGDECTION, CCu_Data);
            state = ACS_STATE_IDLE;
            return;
        }
        if (CCu_Data->CSD_recv_flag == FLAG_SET)
        {
            INFO_LOG(log_msgid, "[GBT] CSD00 receive");
            chargeStepJump(CHARGE_FUN_WELDINGDECTION, CCu_Data);
            state = ACS_STATE_IDLE;
            return;
        }
    }
}

void Charge_Abnormal_EV_Stop(struct CCu_Charge_Data *CCu_Data)
{
    static AbnormalEVStopState state = AES_STATE_IDLE;
    static time_t start_time;
    time_t current_time;
    struct DataPacket send_data;
    memset((void *)(&send_data), 0, sizeof(send_data));

    if (state == AES_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT]  Current stage:Charge_Abnormal_EV_Stop");
        state = AES_STATE_BST_SEND;
    }

    if (CCu_Data->EV_Abnormal_Stop_flag == FLAG_SET)
    {
        switch (CCu_Data->V2G_value.evchargeStopCause)
        {
            case CM_SLAC_PARM_REQ_ERROR:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, CM_SLAC_PARM_REQ_ERROR);
                break;
            case CM_START_ATTEN_CHAR_IND_ERROR:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, CM_START_ATTEN_CHAR_IND_ERROR);
                break;
            case CM_ATTEN_CHAR_RSP_ERROR:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, CM_ATTEN_CHAR_RSP_ERROR);
                break;
            case CM_SLAC_MATCH_REQ_ERROR:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, CM_SLAC_MATCH_REQ_ERROR);
                break;
            case LINK_ERROR:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, LINK_ERROR);
                break;
            case RESSTemperatureInhibit:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, RESSTemperatureInhibit);
                break;
            case EVShiftPosition:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, EVShiftPosition);
                break;
            case ChargerConnectorLockFault:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, ChargerConnectorLockFault);
                break;
            case EVRESSMalfunction:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, EVRESSMalfunction);
                break;
            case ChargingCurrentdifferential:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, ChargingCurrentdifferential);
                break;
            case ChargingVoltageOutOfRange:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, ChargingVoltageOutOfRange);
                break;
            case ChargingSystemIncompatibility:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, ChargingSystemIncompatibility);
                break;
            case NoData:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, NoData);
                break;
            case WrongChargeParameter:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, WrongChargeParameter);
                break;
            case BulkSOCComplete:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, BulkSOCComplete);
                break;
            case FullSOCComplete:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, FullSOCComplete);
                break;
            case SequenceError:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, SequenceError);
                break;
            case EVotherError:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, EVotherError);
                break;
            default:
                writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, UNKNOWN_ERROR);
                break;
        }

        if (state == AES_STATE_BST_SEND)
        {
            switch (CCu_Data->V2G_value.evchargeStopCause)
            {
                case CM_SLAC_PARM_REQ_ERROR:
                    ERROR_LOG(log_msgid, "[GBT] CM_SLAC_PARM_REQ_ERROR");
                    break;
                case CM_START_ATTEN_CHAR_IND_ERROR:
                    ERROR_LOG(log_msgid, "[GBT] CM_START_ATTEN_CHAR_IND_ERROR");
                    break;
                case CM_ATTEN_CHAR_RSP_ERROR:
                    ERROR_LOG(log_msgid, "[GBT] CM_ATTEN_CHAR_RSP_ERROR");
                    break;
                case CM_SLAC_MATCH_REQ_ERROR:
                    ERROR_LOG(log_msgid, "[GBT] CM_SLAC_MATCH_REQ_ERROR");
                    break;
                case LINK_ERROR:
                    ERROR_LOG(log_msgid, "[GBT] LINK_ERROR");
                    break;
                case RESSTemperatureInhibit:
                    ERROR_LOG(log_msgid, "[GBT] RESSTemperatureInhibit");
                    break;
                case EVShiftPosition:
                    ERROR_LOG(log_msgid, "[GBT] EVShiftPosition");
                    break;
                case ChargerConnectorLockFault:
                    ERROR_LOG(log_msgid, "[GBT] ChargerConnectorLockFault");
                    break;
                case EVRESSMalfunction:
                    ERROR_LOG(log_msgid, "[GBT] EVRESSMalfunction");
                    break;
                case ChargingCurrentdifferential:
                    ERROR_LOG(log_msgid, "[GBT] ChargingCurrentdifferential");
                    break;
                case ChargingVoltageOutOfRange:
                    ERROR_LOG(log_msgid, "[GBT] ChargingVoltageOutOfRange");
                    break;
                case ChargingSystemIncompatibility:
                    ERROR_LOG(log_msgid, "[GBT] ChargingSystemIncompatibility");
                    break;
                case NoData:
                    ERROR_LOG(log_msgid, "[GBT] NoData");
                    break;
                case WrongChargeParameter:
                    ERROR_LOG(log_msgid, "[GBT] WrongChargeParameter");
                    break;
                case BulkSOCComplete:
                    ERROR_LOG(log_msgid, "[GBT] BulkSOCComplete");
                    break;
                case FullSOCComplete:
                    ERROR_LOG(log_msgid, "[GBT] FullSOCComplete");
                    break;
                case SequenceError:
                    ERROR_LOG(log_msgid, "[GBT] SequenceError");
                    break;
                case EVotherError:
                    ERROR_LOG(log_msgid, "[GBT] EVotherError");
                    break;
                default:
                    ERROR_LOG(log_msgid, "[GBT] UNKNOWN_ERROR");
                    break;
            }

            start_time = time(NULL);
            INFO_LOG(log_msgid, "[GBT] BST send");
            state = AES_STATE_WAIT_CST;
        }

        current_time = time(NULL);
        if (current_time - start_time > BST_CST_TIMEOUT)
        {
            ERROR_LOG(log_msgid, "[GBT] BSTCST_Timeout_SetupTimer_Error");
            chargeStepJump(CHARGE_FUN_WELDINGDECTION, CCu_Data);
            state = AES_STATE_IDLE;
            return;
        }

        if (CCu_Data->CST_recv_flag == FLAG_SET)
        {
            INFO_LOG(log_msgid, "[GBT] CST receive");
            chargeStepJump(CHARGE_FUN_WELDINGDECTION, CCu_Data);
            state = AES_STATE_IDLE;
            return;
        }
    }
}

void Charge_Timeout_Stop(struct CCu_Charge_Data *CCu_Data)
{
    static TimeoutStopState state = TS_STATE_IDLE;
    static time_t start_time;
    time_t current_time;
    struct DataPacket send_data;
    memset((void *)(&send_data), 0, sizeof(send_data));

    if (state == TS_STATE_IDLE)
    {
        INFO_LOG(log_msgid, "[GBT] Current stage: Charge_Timeout_Stop");
        state = TS_STATE_BST_SEND;
    }

    switch (CCu_Data->TimerID)
    {
        case SLAC_Match_Timer_id:
        {
            writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, StopCause_SLAC_Match_Timeout_SetupTimer_Error);
            break;
        }
        case Sequence_Timer_id:
        {
            writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, StopCause_Sequence_Timeout_SetupTimer_Error);
            break;
        }
        case BHM_CRM_Timer_id:
        {
            writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, StopCause_BHMCRM_Timeout_SetupTimer_Error);
            break;
        }
        case BRM_CRM_Timer_id:
        {
            writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, StopCause_BRMCRM_Timeout_SetupTimer_Error);
            break;
        }
        case BCP_CML_Timer_id:
        {
            writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, StopCause_BCPCML_Timeout_SetupTimer_Error);
            break;
        }
        case BRO_CRO_Timer_id:
        {
            writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, StopCause_BROCRO_Timeout_SetupTimer_Error);
            break;
        }
        case PreCharge_Timer_id:
        {
            writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, StopCause_PreCharge_Timeout_SetupTimer_Error);
            break;
        }
        case BCS_CCS_Timer_id:
        {
            writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, StopCause_BCSCCS_Timeout_SetupTimer_Error);
            break;
        }
        case PowerDeliveryTimer_id:
        {
            writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, StopCause_Wait_PowerDeliveryReq_Timeout_SetupTimer_Error);
            break;
        }
        case CPStateChangeTimer_id:
        {
            writeCanData(&send_data, CCu_Data, SECC_CMD_BST, 0x00, StopCause_Wait_CPstate_Timeout_SetupTimer_Error);
            break;
        }
        default:
            break;
    }

    if (state == TS_STATE_BST_SEND)
    {
        switch (CCu_Data->TimerID)
        {
            case SLAC_Match_Timer_id:
            {
                ERROR_LOG(log_msgid, "[GBT] StopCause_SLAC_Match_Timeout_SetupTimer_Error");
                break;
            }
            case Sequence_Timer_id:
            {
                ERROR_LOG(log_msgid, "[GBT] StopCause_Sequence_Timeout_SetupTimer_Error");
                break;
            }
            case BHM_CRM_Timer_id:
            {
                ERROR_LOG(log_msgid, "[GBT] StopCause_BHMCRM00_Timeout_SetupTimer_Error");
                break;
            }
            case BRM_CRM_Timer_id:
            {
                ERROR_LOG(log_msgid, "[GBT] StopCause_BRMCRMAA_Timeout_SetupTimer_Error");
                break;
            }
            case BCP_CML_Timer_id:
            {
                ERROR_LOG(log_msgid, "[GBT] StopCause_BCPCML_Timeout_SetupTimer_Error");
                break;
            }
            case BRO_CRO_Timer_id:
            {
                ERROR_LOG(log_msgid, "[GBT] StopCause_BROCRO_Timeout_SetupTimer_Error");
                break;
            }
            case PreCharge_Timer_id:
            {
                ERROR_LOG(log_msgid, "[GBT] StopCause_PreCharge_Timeout_SetupTimer_Error");
                break;
            }
            case BCS_CCS_Timer_id:
            {
                ERROR_LOG(log_msgid, "[GBT] StopCause_BCSCCS_Timeout_SetupTimer_Error");
                break;
            }
            case PowerDeliveryTimer_id:
            {
                ERROR_LOG(log_msgid, "[GBT] StopCause_Wait_PowerDeliveryReq_Timeout_SetupTimer_Error");
                break;
            }
            case CPStateChangeTimer_id:
            {
                ERROR_LOG(log_msgid, "[GBT] StopCause_Wait_CPstate_Timeout_SetupTimer_Error");
                break;
            }
            default:
                break;
        }
        start_time = time(NULL);
        INFO_LOG(log_msgid, "[GBT] BST send");
        state = TS_STATE_WAIT_CST;
    }

    current_time = time(NULL);
    if (current_time - start_time > BST_CST_TIMEOUT)
    {
        ERROR_LOG(log_msgid, "[GBT] BSTCST_Timeout_SetupTimer_Error");
        chargeStepJump(CHARGE_FUN_WELDINGDECTION, CCu_Data);
        state = TS_STATE_IDLE;
    }

    if (CCu_Data->CST_recv_flag == FLAG_SET)
    {
        INFO_LOG(log_msgid, "[GBT] CST receive");
        chargeStepJump(CHARGE_FUN_WELDINGDECTION, CCu_Data);
        state = TS_STATE_IDLE;
    }
}

void Charge_Done(struct CCu_Charge_Data *CCu_Data)
{
    static DoneState state = DONE_STATE_IDLE;
    struct DataPacket send_data;

    memset((void *)(&send_data), 0, sizeof(send_data));

    writeCanData(&send_data, CCu_Data, SECC_CMD_BHM, 0x00, ChargeStopNoError);

    if (state == DONE_STATE_IDLE)
    {
        // 检查syn_charge_over变量的当前状态，
        if (get_syn_charge_over() == FLAG_RESET)
        {
            set_syn_charge_over(CCU_STOP_FIN);
        }

        // 处理EV停止的情况
        if (get_syn_charge_over() == EVCC_STOP_FIN)
        {
            INFO_LOG(log_msgid, "[GBT] Charging is completed, and the interaction between SECC and CCu is completed");

            INFO_LOG(log_msgid, "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n\n");

            memset((void *)(CCu_Data), 0, sizeof(struct CCu_Charge_Data));
            state = DONE_STATE_IDLE;

            set_syn_charge_over(STOP_DONE);
            chargeStepJump(CHARGE_FUN_STANDBY, CCu_Data);
            return;
        }
    }
}

const CHARGE_TASK Charge_Fun[] =
    {
        {CHARGE_FUN_STANDBY, Charge_StandBy},                  // 待机阶段
        {CHARGE_FUN_INIT, Charge_Initialize},                  // 初始化阶段
        {CHARGE_FUN_HANDSHAKE, Charge_HandShake},              // 报文握手并确认阶段
        {CHARGE_FUN_IDENTIFICATION, Charge_HandShake},    // 充电辨识报文交互阶段(已经合并到握手阶段)
        {CHARGE_FUN_PARAMETEREXCHANGE, Charge_Param_Exchange}, // 参数交换阶段
        {CHARGE_FUN_CABLECHECKREADY, Charge_Isolation_Ready},  // 绝缘检测准备阶段
        {CHARGE_FUN_CABLECHECK, Charge_Isolation_Check},       // 绝缘检测开始阶段
        {CHARGE_FUN_PRECHARGE, Charge_PreCharge_Stage},        // 预充电阶段
        {CHARGE_FUN_CHARGING, Charge_Charging_Stage},          // 充电阶段
        {CHARGE_FUN_N_EV_STOP, Charge_Normal_EV_Stop},         // 正常停止(车辆停止)
        {CHARGE_FUN_N_CCu_STOP, Charge_Normal_CCu_Stop},       // 正常停止(充电机停止)
        {CHARGE_FUN_WELDINGDECTION, Charge_WeldingDetection},  // 粘连检测阶段
        {CHARGE_FUN_A_CCu_STOP, Charge_Abnormal_CCu_Stop},     // 在非正常状态下停止(充电机方面的原因)
        {CHARGE_FUN_A_EV_STOP, Charge_Abnormal_EV_Stop},       // 在非正常状态下停止(车辆方面的原因)
        {CHARGE_FUN_T_STOP, Charge_Timeout_Stop},              // 通讯超时
        {CHARGE_FUN_T_PLUGOUT, Charge_Done},
        {0, NULL}};
