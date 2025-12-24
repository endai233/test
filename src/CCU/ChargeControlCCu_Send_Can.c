
#include <linux/can.h>
#include <linux/can/raw.h>

#include "SECCProtocolData.h"
#include "ChargeControlCCu.h"
#include "TimeOut.h"
#include "crc.h"
#include "log.h"
#include "commonfunc.h"
#include "MsgQueue.h"
#include "mqttAnalysis.h"

#include "can_protocol.h"
#include "zcl_sdk_protocol.h"
#include "cp.h"

void Write_Can_BHM_Data(CAN_MSG_SEND *pCanFrame, struct CCu_Charge_Data* CCu_Data)
{
    uint32_t version = 0;

    pCanFrame->data[0] = 0x00;
    pCanFrame->data[1] = 0x00;

    if(CCu_Data->match == FLAG_SET)
    {// 匹配完成
        pCanFrame->data[2] = (uint8_t)CCu_Data->V2G_value.attenuation;
        pCanFrame->data[3] = can_pro_cp_bhm_status_get();           //
    }
    else
    {
        pCanFrame->data[2] = 0x00;
        pCanFrame->data[3] = can_pro_cp_bhm_status_get();         //没有握手完成
    }

    version = VERSION;
    pCanFrame->data[4] = (version >> 24) & 0xff;
    pCanFrame->data[5] = (version >> 16) & 0xff;
    pCanFrame->data[6] = (version >> 8) & 0xff;
    pCanFrame->data[7] = (version) & 0xff;

    pCanFrame->can_id = CAN_ID_BHM;
    pCanFrame->can_dlc = 0x08;

    can_pro_send_msg_input(pCanFrame);
}

void Write_Can_BAC_Data(CAN_MSG_SEND *pCanFrame, struct CCu_Charge_Data* CCu_Data)
{
    pCanFrame->data[0] = 0x01;
    pCanFrame->data[1] = 0x00;
    
    pCanFrame->data[2] = CCu_Data->V2G_value.evccid >> 40 & 0xff;
    pCanFrame->data[3] = CCu_Data->V2G_value.evccid >> 32 & 0xff;
    pCanFrame->data[4] = CCu_Data->V2G_value.evccid >> 24 & 0xff;
    pCanFrame->data[5] = CCu_Data->V2G_value.evccid >> 16 & 0xff;
    pCanFrame->data[6] = CCu_Data->V2G_value.evccid >> 8 & 0xff;
    pCanFrame->data[7] = CCu_Data->V2G_value.evccid & 0xff;

    pCanFrame->can_id = CAN_ID_BAC;
    pCanFrame->can_dlc = 0x08;

    can_pro_send_msg_input(pCanFrame);

    if (CCu_Data->V2G_value.evccid > 0
    && 2 == can_pro_cp_bhm_status_get())
    {
        cp_config_pwm(1000, 1000, true);
        printf("got evccid:%02x %02x %02x %02x %02x %02x",pCanFrame->data[2],pCanFrame->data[3],
            pCanFrame->data[4],pCanFrame->data[5],pCanFrame->data[6],pCanFrame->data[7]);
    }
}

void Write_Can_BRM_Data(CAN_MSG_SEND *pCanFrame, struct CCu_Charge_Data* CCu_Data)
{
    pCanFrame->data[0] = 0x01;
    pCanFrame->data[1] = 0x00;
    
    pCanFrame->data[2] = CCu_Data->V2G_value.evccid >> 40 & 0xff;
    pCanFrame->data[3] = CCu_Data->V2G_value.evccid >> 32 & 0xff;
    pCanFrame->data[4] = CCu_Data->V2G_value.evccid >> 24 & 0xff;
    pCanFrame->data[5] = CCu_Data->V2G_value.evccid >> 16 & 0xff;
    pCanFrame->data[6] = CCu_Data->V2G_value.evccid >> 8 & 0xff;
    pCanFrame->data[7] = CCu_Data->V2G_value.evccid & 0xff;

    pCanFrame->can_id = CAN_ID_BRM;
    pCanFrame->can_dlc = 0x08;

    can_pro_send_msg_input(pCanFrame);
}

void Write_Can_BCP_Data(CAN_MSG_SEND *pCanFrame, struct CCu_Charge_Data* CCu_Data)
{    
    CAN_PRO_BCP bcp = {0};
    uint16_t soc;

    bcp.res[0] = 0x00;
    bcp.res[1] = 0x00;

    if( CCu_Data->V2G_value.vehicleMaximumCurrent > 4000)
    {
        bcp.max_cur_h = (uint8_t)( ( CCu_Data->V2G_value.vehicleMaximumCurrent - 4000) >> 8) & 0xff;
        bcp.max_cur_l = (uint8_t)( CCu_Data->V2G_value.vehicleMaximumCurrent - 4000) & 0xff;
        bcp.max_cur_h |= 0x80;
    }
    else
    {
        bcp.max_cur_h = (uint8_t)( (4000 - CCu_Data->V2G_value.vehicleMaximumCurrent) >> 8) & 0xff;
        bcp.max_cur_l = (uint8_t)( (4000 - CCu_Data->V2G_value.vehicleMaximumCurrent) ) & 0xff;
        bcp.max_cur_h &= 0x7f;
    }

    bcp.res1[0] = 0x00;
    bcp.res1[1] = 0x00;
    
    
    bcp.max_volt_h = (uint8_t)( CCu_Data->V2G_value.vehicleMaximumVoltage >> 8) & 0xff;
    bcp.max_volt_l = (uint8_t)( CCu_Data->V2G_value.vehicleMaximumVoltage ) & 0xff;

    soc = CCu_Data->V2G_value.soc * 10;
    bcp.soc_h = (uint8_t)(soc >> 8) & 0xff;
    bcp.soc_l = (uint8_t)(soc) & 0xff;

    // can内容发送
    pCanFrame->can_id = CAN_ID_BCP;
    pCanFrame->can_dlc = sizeof(CAN_PRO_BCP);
    if (pCanFrame->can_dlc > CAN_SEND_LEN_MAX)
    {
        return;
    }
    memcpy(pCanFrame->data, (uint8_t *)&bcp, pCanFrame->can_dlc);
    can_pro_send_msg_input(pCanFrame);
}

void Write_Can_BRO_Data(CAN_MSG_SEND *pCanFrame, struct CCu_Charge_Data* CCu_Data, uint8_t val){
    
    if (0xAA == val)
    {
        pCanFrame->data[0] = val;
    }
    else
    {
        pCanFrame->data[0] = 0x00;
    }

    pCanFrame->can_id = CAN_ID_BRO;
    pCanFrame->can_dlc = 0x01;

    can_pro_send_msg_input(pCanFrame);
}

void Write_Can_BCL_Data(CAN_MSG_SEND *pCanFrame, struct CCu_Charge_Data* CCu_Data, uint8_t val)
{    
    pCanFrame->data[0] = (uint8_t)( CCu_Data->V2G_value.vehicleDemandVoltage ) & 0xff;
    pCanFrame->data[1] = (uint8_t)( CCu_Data->V2G_value.vehicleDemandVoltage >> 8) & 0xff;
    
    if(CCu_Data->V2G_value.vehicleDemandCurrent > 4000)
    {
        pCanFrame->data[3] = (uint8_t)( (CCu_Data->V2G_value.vehicleDemandCurrent - 4000) >> 8) & 0xff;
        pCanFrame->data[2] = (uint8_t)( CCu_Data->V2G_value.vehicleDemandCurrent - 4000) & 0xff;
        pCanFrame->data[3] |= 0x80;
    }
    else
    {
        pCanFrame->data[3] = (uint8_t)((4000 - CCu_Data->V2G_value.vehicleDemandCurrent) >> 8) & 0xff;
        pCanFrame->data[2] = (uint8_t)(4000 - CCu_Data->V2G_value.vehicleDemandCurrent) & 0xff;
        pCanFrame->data[3] &= 0x7f;
    }

    pCanFrame->data[4] = 0x00;
    
    pCanFrame->data[5] = (uint8_t)( CCu_Data->V2G_value.vehicleMaximumVoltage ) & 0xff;
    pCanFrame->data[6] = (uint8_t)( CCu_Data->V2G_value.vehicleMaximumVoltage >> 8) & 0xff;

    pCanFrame->data[7] = val;

    pCanFrame->can_id = CAN_ID_BCL;
    pCanFrame->can_dlc = 0x08;

    can_pro_send_msg_input(pCanFrame);
}

void Write_Can_BCS_Data(CAN_MSG_SEND *pCanFrame, struct CCu_Charge_Data* CCu_Data)
{
    CAN_PRO_BCS bcs = {0};

    bcs.res[0] = 0x00;
    bcs.res[1] = 0x00;
    bcs.res[2] = 0x00;
    bcs.res[3] = 0x00;
    bcs.res[4] = 0x00;
    bcs.res[5] = 0x00;

    bcs.soc = CCu_Data->V2G_value.soc;

    bcs.rest_time_h = (uint8_t)( CCu_Data->V2G_value.fullchargeRemainingTime >> 8) & 0xff;
    bcs.rest_time_l = (uint8_t)( CCu_Data->V2G_value.fullchargeRemainingTime) & 0xff; 

    pCanFrame->can_id = CAN_ID_BCS;
    pCanFrame->can_dlc = sizeof(CAN_PRO_BCS);
    
    if (pCanFrame->can_dlc > CAN_SEND_LEN_MAX)
    {
        return;
    }
    memcpy(pCanFrame->data, (uint8_t *)&bcs, pCanFrame->can_dlc);
    can_pro_send_msg_input(pCanFrame);
}

void Write_Can_BST_Data(CAN_MSG_SEND *pCanFrame, struct CCu_Charge_Data* CCu_Data, uint16_t ChargeStopCause)
{
    uint16_t value = 0;
    uint8_t  zcl = 0;
    uint8_t  zrn = 0;
    uint8_t  zrnCom = 0;
    uint8_t  zrnEV = 0;
    
    switch (ChargeStopCause)
    {//使用吉瓦特sdk中的SECC_SDK_ChargeStopCause
        case ChargeStopNoError:
            value = StopCause_CCUStop_Normal;
            break;
        
        case CM_SLAC_PARM_REQ_ERROR:
            value = StopCause_CCS2_SLAC_Wait_CM_SLAC_PARM_REQ_TimeOut;
            break;
        
        case CM_START_ATTEN_CHAR_IND_ERROR:
            value = StopCause_CCS2_SLAC_Wait_ATTEN_CHAR_IND_TimeOut;
            break;
        
        case CM_ATTEN_CHAR_RSP_ERROR:
            value = StopCause_CCS2_SLAC_Wait_ATTEN_CHAR_RSP_TimeOut;
            break;
        
        case CM_SLAC_MATCH_REQ_ERROR:
            value = StopCause_CCS2_SLAC_Wait_MATCH_OR_VALIDATE_TimeOut;
            break;
        
        case LINK_ERROR:
            value = StopCause_CCS2_SLAC_Wait_LinkDetected_TimeOut;
            break;
        
        case UNKNOWN_ERROR:
            value = UNKNOWN_ERROR;
            break;
        
        case StopCause_SLAC_Match_Timeout_SetupTimer_Error:
            value = StopCause_CCS2_SLAC_SlacTimeout;
            break;
        
        case StopCause_Sequence_Timeout_SetupTimer_Error:
            value = StopCause_ComTimeOut_startcharge;
            break;
        
        case StopCause_BHMCRM_Timeout_SetupTimer_Error:
            value = StopCause_ComTimeOut_startcharge;
            break;
        
        case StopCause_BRMCRM_Timeout_SetupTimer_Error:
            value = StopCause_ComTimeOut_startcharge;
            break;
        
        case StopCause_BCPCML_Timeout_SetupTimer_Error:
            value = StopCause_ComTimeOut_parametersync;
            break;
        
        case StopCause_BROCRO_Timeout_SetupTimer_Error:
            value = StopCause_ComTimeOut_parametersync;
            break;
        
        case StopCause_PreCharge_Timeout_SetupTimer_Error:
            value = StopCause_ComTimeOut_selftestcomplete;
            break;
        
        case StopCause_BCSCCS_Timeout_SetupTimer_Error:
            value = StopCause_ComTimeOut_chargingparameter;
            break;
        
        case StopCause_Wait_PowerDeliveryReq_Timeout_SetupTimer_Error:
            value = StopCause_Wait_PowerDeliveryReq_Timeout_SetupTimer_Error;
            break;
        
        case StopCause_Wait_CPstate_Timeout_SetupTimer_Error:
            value = StopCause_Wait_CPstate_Timeout_SetupTimer_Error;
            ERROR_LOG(log_msgid, "[CHARGE] Charging terminated due to CP state timeout");
            break;
        
        case RESSTemperatureInhibit:
            zrnEV = RESSTemperatureInhibit;
            break;
        
        case EVShiftPosition:
            zrnEV = EVShiftPosition;
            break;
        
        case ChargerConnectorLockFault:
            zrnEV = ChargerConnectorLockFault;
            break;
        
        case EVRESSMalfunction:
            zrnEV = EVRESSMalfunction;
            break;
        
        case ChargingCurrentdifferential:
            zrnEV = ChargingCurrentdifferential;
            break;
        
        case ChargingVoltageOutOfRange:
            zrnEV = ChargingVoltageOutOfRange;
            break;
        
        case ChargingSystemIncompatibility:
            zrnEV = ChargingSystemIncompatibility;
            break;
        
        case WrongChargeParameter:
            zrnEV = WrongChargeParameter;
            break;
        
        case BulkSOCComplete:
            value = BulkSOCComplete;
            break;
        
        case FullSOCComplete:
            value = FullSOCComplete;
            break;
        
        case SequenceError:
            zrnEV = SequenceError;
            ERROR_LOG(log_msgid, "[CHARGER] Sequence Error");
            break;
        
        case EVotherError:
            zrnEV = EVotherError;
            break;
        default:
            ERROR_LOG(log_msgid, "[MSGQ] Unknown error");
            break;
    }    

    pCanFrame->data[0] = zcl;
    pCanFrame->data[1] = 0x00;
    pCanFrame->data[2] = 0x00;
    pCanFrame->data[3] = zrn;

    pCanFrame->data[4] = (value >> 8) & 0xff;;
    pCanFrame->data[5] = value & 0xff;
    pCanFrame->data[6] = zrnCom;
    pCanFrame->data[7] = zrnEV;
    
    pCanFrame->can_id = CAN_ID_BST;
    pCanFrame->can_dlc = 0x08;

    can_pro_send_msg_input(pCanFrame);
}

void Write_Can_BSD_Data(CAN_MSG_SEND *pCanFrame, struct CCu_Charge_Data* CCu_Data, uint8_t val)
{    
    pCanFrame->data[0] = CCu_Data->V2G_value.soc;    
    pCanFrame->data[1] = 0x00;
    pCanFrame->data[2] = 0x00;
    pCanFrame->data[3] = 0x00;
    pCanFrame->data[4] = 0x00;
    pCanFrame->data[5] = 0x00;
    pCanFrame->data[6] = 0x00;
    
    pCanFrame->data[7] = 0x01;

    pCanFrame->can_id = CAN_ID_BSD;
    pCanFrame->can_dlc = 0x08;

    can_pro_send_msg_input(pCanFrame);
}

void writeCanData(struct DataPacket* send_data, struct CCu_Charge_Data* CCu_Data, SECC_CMD_Code_type CodeType, uint8_t value, ChargeStopCause ChargeStopCause){
    
    CAN_MSG_SEND can_data = {0};

    switch (CodeType){
        case SECC_CMD_BHM:
            Write_Can_BHM_Data(&can_data, CCu_Data);
            break;

        case SECC_CMD_BID:
            Write_Can_BAC_Data(&can_data, CCu_Data);
            break;
        
        case SECC_CMD_BRM:
            Write_Can_BRM_Data(&can_data, CCu_Data);
            break;

        case SECC_CMD_BCP:
            Write_Can_BCP_Data(&can_data, CCu_Data);
            break;

        case SECC_CMD_BRO:
            Write_Can_BRO_Data(&can_data, CCu_Data, value);
            break;

        case SECC_CMD_BCS:
            Write_Can_BCS_Data(&can_data, CCu_Data);
            break;

        case SECC_CMD_BCL:
            Write_Can_BCL_Data(&can_data, CCu_Data, value);
            break;

        case SECC_CMD_BST:
            Write_Can_BST_Data(&can_data, CCu_Data, ChargeStopCause);
            break;

        case SECC_CMD_BSD:
            Write_Can_BSD_Data(&can_data, CCu_Data, value);
            break;

        default:
            ERROR_LOG(log_msgid, "Error CMD CodeType");
            break;
    }
}


