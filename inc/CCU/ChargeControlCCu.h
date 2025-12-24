/*
 * @Author: yang 1458900080@qq.com
 * @Date: 2025-12-19 16:37:10
 * @LastEditors: yang 1458900080@qq.com
 * @LastEditTime: 2025-12-24 14:46:17
 * @FilePath: \Can_CCu_To_V2g\inc\CCU\ChargeControlCCu.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef CHARGECONTROLCCU_H
#define CHARGECONTROLCCU_H

#ifdef __cplusplus
extern "C"{
#endif 

#include <stdint.h>
#include <unistd.h>
#include <mosquitto.h>
#include <sys/reboot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <mqueue.h>
#include <pthread.h>
#include "sync_vars.h"

#define TIME_TOLERANCE 5
#define HEX_STR_BYTE_SIZE 3

#define SECC_CCU_HEAD_LENGTH_CRC_SIZE 5
#define SECC_CCU_DATA_MAX_SIZE 20
#define SECC_CCU_CRC_SIZE 2
#define SECC_CCU_LENGTH_SIZE 1
#define SECC_CCU_HEAD_SIZE 2
#define SECC_CCU_DATA_HEAD_1 0x5A
#define SECC_CCU_DATA_HEAD_2 0xA5
#define DATA_SEND_TIME 230000
#define GROUP_DATA_SEND_TIME 15

#define SLAC_READY_IND_TIMEOUT 1
#define SLAC_MATCH_TIMEOUT 25
#define BHM_CRM_TIMEOUT 5
#define BRM_CRM_TIMEOUT 15
#define BCP_CML_TIMEOUT 15
#define BRO_CRO00_TIMEOUT 15
#define BRO_CROAA_TIMEOUT 60
#define BCS_CCS_TIMEOUT 5
#define BST_CST_TIMEOUT 3
#define BST_CSD00_TIMEOUT 5
#define BSD00_BSD01_TIMEOUT 5
#define BSD01_CSD01_TIMEOUT 1

// extern mqd_t log_msgid;
// extern mqd_t ccu_msgid;
// extern mqd_t ccu_msgid;
// extern atomic_uchar timer_expired; 
// extern atomic_uchar syn_charge_over;



//CP状态
typedef enum {
    UNKNWON,
    CPStatusA,
    CPStatusB1,
    CPStatusB2,
    CPStatusC1,
    CPStatusC2,
    CPStatusD1,
    CPStatusD2,
    CPStatusE,
    CPStatusF
} CPStatus;


//数据包结构体
struct DataPacket{
	uint8_t payload[SECC_CCU_DATA_MAX_SIZE];
	uint8_t head_1;
	uint8_t head_2;
	uint8_t length;
	uint32_t cmd;
	uint16_t crc;
};

//各个Can报文的数据结构体
struct CCu_CPM{
    CPStatus CPStatus;
    uint32_t UTCSeconds;
    uint16_t real_voltage;
};

struct CCu_CHM{
    uint8_t SECC_Ongoing;
    uint16_t chargerMaximumPower;
    uint8_t EVCCIDACK;
};

struct CCu_CRM{
    uint8_t identification;
    uint32_t EVSEID;
};

struct CCu_CML{
    uint16_t chargerMaximumCurrent;
    uint16_t chargerMinimumCurrent;
    uint16_t chargerMaximumVoltage;
    uint16_t chargerMinimumVoltage;
};

struct CCu_CTS{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t days;
    uint8_t months;
    uint8_t years;
    uint8_t PeakCurrentRipple;
};

struct CCu_CRO{
    uint8_t identification;
};

struct CCu_CCS{
    uint16_t chargerCurrent;
    uint16_t chargerVoltage;
    uint16_t chargerMaximumPower;
};

struct CCu_CST{
    uint8_t CCu_StopType;
    CCu_ChargeStopCause CCu_StopCause;
};

struct CCu_CSD{
    uint8_t chargeoverACK;
};

//v2g协议相关报文
struct SECC_MAC_ID{
    uint64_t evccid;
};

struct SECC_SLAC_RES{
    uint16_t attenuation;
    uint8_t match:1;
};

struct SECC_SLAC_READY{
    uint8_t readycnf:1;
};

struct SECC_STOP{
    uint16_t evchargeStopCause;
    uint8_t EV_Abnormal_Stop_flag:1;
    uint8_t EV_Normal_Stop_flag:1;    
};

struct SECC_MAX_PARM{
    uint16_t vehicleMaximumVoltage;  
    uint16_t vehicleMaximumCurrent;
    uint16_t vehicleMaximumPower;     
};

struct SECC_DEMAND_PARM{
    uint16_t vehicleDemandVoltage;                      
	uint16_t vehicleDemandCurrent; 
};

struct SECC_SOC_PARM{
    uint8_t fullsoc;
	uint8_t bulksoc; 
    uint8_t soc;
};

struct SECC_DEMAND_SOC{
    uint8_t soc;
};

struct SECC_TIME{
    uint16_t fullchargeRemainingTime;                   
	uint16_t bulkchargeRemainingTime;     
};

struct SECC_V2GMSG{
    uint8_t startChargeParamDiscovery:1;
    uint8_t startCableCheck:1;
    uint8_t startPreCharge:1;
    uint8_t vehicleReadyToCharge:1; 
    uint8_t sessionStop:1; 
};

struct SECC_V2G{
    uint64_t evccid;                                    
    uint16_t attenuation;                               

    /*Maximum charging voltage of the vehicle*/
	uint16_t vehicleMaximumVoltage;                    
    
    /*Maximum charging Current of the vehicle*/
	uint16_t vehicleMaximumCurrent;                     

	/*maximum Power*/
	uint16_t vehicleMaximumPower;                       

    /*Vehicle demand voltage*/
	uint16_t vehicleDemandVoltage;                      

	/*Vehicle demand current*/
	uint16_t vehicleDemandCurrent;                     

    uint16_t fullchargeRemainingTime;                   
	uint16_t bulkchargeRemainingTime;                   
    uint16_t evchargeStopCause;         

    uint8_t soc;
	uint8_t fullsoc;
	uint8_t bulksoc;                
};

struct CCu_Charge_Data{
    optType opt;
    optType opt_old;

    struct CCu_CPM CPM_value;
    struct CCu_CHM CHM_value;
    struct CCu_CRM CRM_value;
    struct CCu_CML CML_value;
    struct CCu_CTS CTS_value;
    struct CCu_CRO CRO_value;
    struct CCu_CCS CCS_value;
    struct CCu_CST CST_value;
    struct CCu_CSD CSD_value;
    struct SECC_V2G V2G_value;

    uint16_t  TimerID;
    uint8_t   slac_ready_retry_count;
    uint8_t charge_completed_flag;     // 充电完成标志位



    uint16_t  CHM_recv_flag:1;
    uint16_t  CRM_recv_flag:1;      
    uint16_t  CML_recv_flag:1;
    uint16_t  CTS_recv_flag:1;
    uint16_t  CRO_recv_flag:1;
    uint16_t  CST_recv_flag:1;
    uint16_t  CCS_recv_flag:1;
    uint16_t  CSD_recv_flag:1;
    uint16_t  CCu_Normal_Stop_flag:1;
    uint16_t  CCu_Abnormal_Stop_flag:1;

    uint16_t  EV_Abnormal_Stop_flag:1; 
    uint16_t  EV_Normal_Stop_flag:1;

    uint8_t   slac_ready_send_flag:1;
    uint8_t   match:1;
    uint8_t   readycnf:1;
    uint8_t   startChargeParamDiscovery:1;
    uint8_t   startCableCheck:1;
    uint8_t   startPreCharge:1;
    uint8_t   vehicleReadyToCharge:1; //PowerDelivery1_Req_recv
    uint8_t   sessionStop:1; 
};


#ifdef __cplusplus
}
#endif
#endif

