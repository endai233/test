/*
 * @Author: yangzh 16159324+Yangzhs10@user.noreply.gitee.com
 * @Date: 2025-12-08 09:57:39
 * @LastEditors: yang 1458900080@qq.com
 * @LastEditTime: 2025-12-24 14:38:06
 * @FilePath: /v2gtp_bk1_Reconstruct_2/EVCC/Din70121/inc/ChargeControlEVCC.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef CHARGECONTROLEVCC_H
#define CHARGECONTROLEVCC_H

#ifdef __cplusplus
extern "C"{
#endif 

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <mqueue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>     
#include <netinet/in.h>
#include "sync_vars.h"
#include "exi_bitstream.h"
#include "din_msgDefDatatypes.h"
#include "SDP_Datatypes.h"
#include "appHand_Datatypes.h"
#include "ChargeControlCCu.h"

#define INTERFACENUM 16
#define AppHandProtocol_Provided_ARRAY_SIZE 5
#define EVENT_NUM 6
#define PORT 15118
#define MULTICAST_ADDR "ff02::1" 
#define V2G_BUFFER_SIZE 256
#define V2GSESSIONID  0x6061696666696160
#define SEQUENCE_TIMEOUT 60
#define POWERDELIVER_TIMEOUT 10
#define PRECHARGE_TIMEOUT 20
#define POWERDELIVERY_NO1 1
#define POWERDELIVERY_NO2 2



extern atomic_uchar timer_expired; 
extern atomic_uchar syn_charge_over;
 
extern mqd_t log_msgid;




typedef enum {
    SECCDiscoveryProtocol,
    APPHandShake,
    SessionSetup,
    ServiceDiscovery,
    ServiceDetail,
    ServicePaymentSelection,
    PaymentDetails,
    ContractAuthentication,
    CertificateUpdate,
    CertificateInstallation,
    ChargeParameterDiscovery,
    CableCheck,
    PreCharge,
    PowerDelivery1,
    CurrentDemand,
    PowerDelivery2,
    WeldingDetection,
    SessionStop,
}SessionSequence;

typedef enum{
    SECC_STATE_STANDBY,
    SECC_STATE_INIT_1,
    SECC_STATE_INIT_2,
    SECC_STATE_END,
    SECC_STATE_END_SYNC
}SECC_STATE;

typedef enum {
    SET_START_CHARGE_PARAM_DISCOVERY,
    SET_START_CABLE_CHECK,
    SET_START_PRE_CHARGE,
    SET_VEHICLE_READY_TO_CHARGE,
    SET_SESSION_STOP
}V2G_MSG_TYPE;

typedef enum{
	ChargeStopNoError = 0,

	StopCause_SLAC_Match_Timeout_SetupTimer_Error = 8018,
	StopCause_Sequence_Timeout_SetupTimer_Error = 8019,
	StopCause_BHMCRM_Timeout_SetupTimer_Error = 8020,
	StopCause_BRMCRM_Timeout_SetupTimer_Error = 8021,
	StopCause_BCPCML_Timeout_SetupTimer_Error = 8022,
    StopCause_BROCRO_Timeout_SetupTimer_Error = 8023,
	StopCause_PreCharge_Timeout_SetupTimer_Error = 8024,
	StopCause_BCSCCS_Timeout_SetupTimer_Error = 8025,
    StopCause_Wait_PowerDeliveryReq_Timeout_SetupTimer_Error = 8026,
	StopCause_Wait_CPstate_Timeout_SetupTimer_Error = 8027,

    //EV Error Code
	RESSTemperatureInhibit = 8100,
    EVShiftPosition,
    ChargerConnectorLockFault,
    EVRESSMalfunction,
    ChargingCurrentdifferential,
    ChargingVoltageOutOfRange,
    ChargingSystemIncompatibility,   //e.g.WrongEnergyTransferType
    NoData,
	WrongChargeParameter,  			
	BulkSOCComplete,
	FullSOCComplete,
	SequenceError,
	EVotherError

}ChargeStopCause;



typedef enum {
    SECC_CMD_BHM,
	SECC_CMD_BID,
    SECC_CMD_BRM,
    SECC_CMD_BCP,
    SECC_CMD_BRO,
    SECC_CMD_BCS,
    SECC_CMD_BCL,
    SECC_CMD_BSM,
    SECC_CMD_BEM,
    SECC_CMD_BST,
    SECC_CMD_BSD
}SECC_CMD_Code_type;

typedef enum{
    SessionSetup_TYPE,
    ChargeParameterDiscovery_TYPE,
    CableCheck_TYPE,
    ChargeState_TYPE,
    Stop_TYPE ,
    CP_Value_TYPE ,
    TIMERSTOP_TYPE ,
    SLAC_RESULT_TYPE,
    CTS_TYPE,
    CSD_TYPE,
    CCu_CPM_TYPE,
    CCu_CHM_TYPE,
    CCu_CRM_TYPE,
    CCu_CML_TYPE,
    CCu_CRO_TYPE,
    CCu_CCS_TYPE,
    CCu_CST_TYPE,
    CCu_CTS_TYPE,
    CCu_CSD_TYPE,
    V2G_Msg_TYPE
}SECC_Data_Type;

struct SECC_SDP{
    uint16_t tcp_port;
};

struct providedApphandProtocolNamespaceType{
    struct {
        char characters[appHand_ProtocolNamespace_CHARACTER_SIZE];
        uint16_t charactersLen;
    } ProvidedProtocolNamespace;
    uint32_t VersionNumberMajor;
    uint32_t VersionNumberMinor;
};

struct providedApphandProtocolType{
    struct providedApphandProtocolNamespaceType array[AppHandProtocol_Provided_ARRAY_SIZE];
    uint16_t arrayLen;
};

struct SECC_ApphandProtocol{
    struct providedApphandProtocolType providedApphandProtocol;
};

struct SECC_SessionSetup{
    uint32_t evseid;
    uint8_t identification;
};

struct SECC_ChargeParameterDiscovery{
    uint16_t chargerMaximumCurrent;
    uint16_t chargerMaximumVoltage;
    uint16_t chargerMinimumCurrent;
    uint16_t chargerMinimumVoltage;
};

struct SECC_CableCheck{
    uint8_t identification;
};

struct SECC_ChargeState{
    uint16_t chargerMaximumPower;
    uint16_t chargerVoltage;
    uint16_t chargerCurrent;
};

struct CCu_Stop{
    CCu_ChargeStopCause CCu_StopCause;
    uint8_t CCu_StopType;
};

struct SECC_CONFIG{
    struct ipv6_mreq mreq;
    struct sockaddr_in6 client_addr;
    int epoll_fd;
    int udp_socket;
    int tcp_listen_socket;
    int tcp_connect_socket;
    char ifn[INTERFACENUM];
};

struct SECC_Charge_Data{
	SessionSequence opt;
    SessionSequence opt_old;

    struct SECC_CONFIG Config_value;

    struct SECC_SDP SDP_value;
    struct SECC_ApphandProtocol ApphandProtocol_value;
    struct SECC_SessionSetup SessionSetup_value;
    struct SECC_ChargeParameterDiscovery ChargeParameterDiscovery_value;
    struct SECC_CableCheck CableCheck_value;
    struct SECC_ChargeState Charge_value;
    struct CCu_UART CCu_value;

    uint8_t CP_Value;

    uint16_t vehicleDemandCurrent;
    uint16_t vehicleDemandVoltage;


    
    uint8_t CCu_Abnormal_Stop_flag:1;
    uint8_t CCu_Normal_Stop_flag:1;
    uint8_t CML_recv_flag:1;
    uint8_t CRM_recv_flag:1;
    uint8_t charge_start:1;
    uint8_t Charge_stop_flag:1;
    uint8_t cableCheckComplete:1;
	uint8_t currenDemandFinished:1;
    uint8_t startWeldingDetection:1;
    uint8_t vehicleReadyToShutDown:1;  //PowerDelivery2_Req_recv
	uint8_t emergencyStop:1;
};

struct CCU_Public_Data_t {
   
    struct SECC_SessionSetup evcc_session_setup;
    struct SECC_ChargeParameterDiscovery evcc_charge_param;
    struct SECC_CableCheck evcc_cable_check;
    struct SECC_ChargeState evcc_charge_state;
    struct CCu_Stop CCu_stop;
    uint8_t evcc_cp_value;
    uint8_t evcc_slac_result;
};


void* Charge_Evcc_task(void* arg);


static int EVCC_Parse_Protocol_Message(
    struct SECC_Charge_Data *SECC_Data,
    struct SDP_Document *SDPexiOut,
    struct appHand_exiDocument *handshakeOut,
    struct din_exiDocument *exiOut,
    exi_bitstream_t *rcv_stream,
    uint32_t *payloadlength);





#ifdef __cplusplus
}
#endif
#endif
