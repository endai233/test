#ifndef CHARGECONTROLCCU_SM_H
#define CHARGECONTROLCCU_SM_H

#ifdef __cplusplus
extern "C"{
#endif 

#include "ChargeControlCCu.h"
#include "TimeOut.h"

typedef struct
{
    optType opt;
    void (*fun)(struct CCu_Charge_Data *);
} CHARGE_TASK;


// 充电类型枚举：区分自动充电和刷卡充电
typedef enum {
    CHARGE_TYPE_UNKNOWN = 0,  // 未知类型
    CHARGE_TYPE_AUTO = 1,     // 自动充电（EVCCIDACK=0, evccid>0）
    CHARGE_TYPE_CARD = 2      // 刷卡充电（EVCCIDACK=1, evccid=0）
} ChargeType;

// 初始化阶段状态机
typedef enum {
    INIT_STATE_IDLE = 0,          // 空闲状态
    INIT_STATE_WAIT_CONDITION,    // 等待充电条件满足
    INIT_STATE_BID_SENT,          // BID已发送
    INIT_STATE_WAIT_CHM,          // 等待CHM消息
    INIT_STATE_SLAC_MATCHING,     // SLAC匹配中
    INIT_STATE_WAIT_CRM           // 等待CRM消息
} InitState;


// // 待机状态枚举
typedef enum {
    SB_STATE_IDLE = 0,
    SB_STATE_WAIT_PLUG
} StandByState;

// // 握手阶段状态枚举
typedef enum {
    HS_STATE_IDLE = 0,
    HS_STATE_BRM_SEND,
    HS_STATE_WAIT_CRM,
    HS_STATE_IDENTIFICATION
} HandShakeState;

// // 参数交换状态枚举
typedef enum {
    PE_STATE_IDLE = 0,
    PE_STATE_BCP_SEND,
    PE_STATE_WAIT_CML
} ParamExchangeState;

// // 绝缘检测准备状态枚举
typedef enum {
    IR_STATE_IDLE = 0,
    IR_STATE_BRO00_SEND,
    IR_STATE_WAIT_CHECK
} IsolationReadyState;

// // 绝缘检测执行状态枚举
typedef enum {
    IC_STATE_IDLE = 0,
    IC_STATE_BROAA_SEND,
    IC_STATE_CRO00_GET,
    IC_STATE_CROAA_GET
} IsolationCheckState;

// // 预充电状态枚举
typedef enum {
    PC_STATE_IDLE = 0,
    PC_STATE_BCL_SEND,
    PC_STATE_WAIT_CCS
} PreChargeState;

// // 充电中状态枚举
typedef enum {
    CH_STATE_IDLE = 0,
    CH_STATE_BCS_SEND,
    CH_STATE_WAIT_CCS
} ChargingState;

// // 正常EV停止状态枚举
typedef enum {
    NES_STATE_IDLE = 0,
    NES_STATE_BST_SEND,
    NES_STATE_WAIT_CST
} NormalEVStopState;

// // 正常CCu停止状态枚举
typedef enum {
    NCS_STATE_IDLE = 0,
    NCS_STATE_WAIT_CST,
    NCS_STATE_BST_SEND,
    NCS_STATE_WAIT_CSD
} NormalCCuStopState;

// // 粘连检测状态枚举
typedef enum {
    WD_STATE_IDLE = 0,
    WD_STATE_BSD00_SEND,
    WD_STATE_BSD01_SEND,
    WD_STATE_WAIT_CSD01
} WeldingDetectionState;

// // 超时停止状态枚举
typedef enum {
    TS_STATE_IDLE = 0,
    TS_STATE_BST_SEND,
    TS_STATE_WAIT_CST
} TimeoutStopState;

// // 异常CCu停止状态枚举
typedef enum {
    ACS_STATE_IDLE = 0,
    ACS_STATE_WAIT_CST,
    ACS_STATE_BST_SEND,
    ACS_STATE_WAIT_CSD
} AbnormalCCuStopState;

// // 异常EV停止状态枚举
typedef enum {
    AES_STATE_IDLE = 0,
    AES_STATE_BST_SEND,
    AES_STATE_WAIT_CST
} AbnormalEVStopState;

// // 充电完成状态枚举
typedef enum {
    DONE_STATE_IDLE = 0
} DoneState;

//CCu停止原因枚举
typedef enum {
    NoError = 0,
    OtherError = 10,
    ChargeParameterNotMatch,
	IsolationWarning,
    IsolationFault,
    TimeOutError,
    PresentVoltageTooLow,
    CPStatusErrorA,
    CPStatusErrorB1,
    CPStatusErrorB2,
    CPStatusErrorC1,
    CPStatusErrorC2,
    CPStatusErrorD1,
    CPStatusErrorD2,
    CPStatusErrorE,
    CPStatusErrorF

}CCu_ChargeStopCause;

typedef enum{
    CHARGE_FUN_STANDBY,
    CHARGE_FUN_INIT,
    CHARGE_FUN_HANDSHAKE,
    CHARGE_FUN_IDENTIFICATION,
    CHARGE_FUN_PARAMETEREXCHANGE,
    CHARGE_FUN_CABLECHECKREADY,
    CHARGE_FUN_CABLECHECK,
    CHARGE_FUN_PRECHARGE,
    CHARGE_FUN_CHARGING,
    CHARGE_FUN_N_EV_STOP,
    CHARGE_FUN_N_CCu_STOP,
    CHARGE_FUN_WELDINGDECTION,
    CHARGE_FUN_A_CCu_STOP,
    CHARGE_FUN_A_EV_STOP,
    CHARGE_FUN_T_STOP,
    CHARGE_FUN_S_STOP,
    CHARGE_FUN_T_PLUGOUT
}optType;


extern const CHARGE_TASK Charge_Fun[];

void chargeStepJump(int step, struct CCu_Charge_Data *CCu_Data);

void Charge_StandBy(struct CCu_Charge_Data *CCu_Data);
void Charge_Initialize(struct CCu_Charge_Data *CCu_Data);
void Charge_HandShake(struct CCu_Charge_Data *CCu_Data);
void Charge_Param_Exchange(struct CCu_Charge_Data *CCu_Data);
void Charge_Isolation_Ready(struct CCu_Charge_Data *CCu_Data);
void Charge_Isolation_Check(struct CCu_Charge_Data *CCu_Data);
void Charge_PreCharge_Stage(struct CCu_Charge_Data *CCu_Data);
void Charge_Charging_Stage(struct CCu_Charge_Data *CCu_Data);
void Charge_Normal_EV_Stop(struct CCu_Charge_Data *CCu_Data);
void Charge_Normal_CCu_Stop(struct CCu_Charge_Data *CCu_Data);
void Charge_WeldingDetection(struct CCu_Charge_Data *CCu_Data);
void Charge_Abnormal_CCu_Stop(struct CCu_Charge_Data *CCu_Data);
void Charge_Abnormal_EV_Stop(struct CCu_Charge_Data *CCu_Data);
void Charge_Timeout_Stop(struct CCu_Charge_Data *CCu_Data);
void Charge_Done(struct CCu_Charge_Data *CCu_Data);
void secc_ccu_set_timer_stop(TimerIdType TimerID);

#ifdef __cplusplus
}
#endif

#endif
