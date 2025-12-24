#include "ChargeControlCCu_Data.h"
#include "ChargeControlCCu_SM.h"
#include "ChargeControlCCu.h"
#include "commonfunc.h"
#include "SECCProtocolData.h"
#include "mqttAnalysis.h"
#include "log.h"
#include "secc_ccu_sync.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "zlog.h"
#include "ChargeControlCCu_Share.h"
#include "ChargeControlCCu_Task.h"
#include "V2G.h"
/**
 * @brief 分析来自CCu的SECC数据
 *
 * 该函数用于分析从CCu接收到的SECC数据，并根据数据内容执行相应的操作。
 *
 * @param payload 指向包含SECC数据的缓冲区指针
 */
void analyseCMD_CBM_Data(uint8_t *payload)
{

    if (payload == NULL)
    {
        ERROR_LOG(log_msgid, "Invalid payload pointer");
        return;
    }
    if (payload[0] == 0x01)
    {
        ERROR_LOG(log_msgid, "CCu SECC Communication error, SECC Reboot");
        if (geteuid() != 0)
        {
            ERROR_LOG(log_msgid, "This program must be run as root.");
        }
        else
        {
            zlog_reload("/root/config/zlog.conf");
            /*
            if (reboot(RB_AUTOBOOT) == -1) {
                ERROR_LOG(log_msgid, "reboot failed");
            }
            */
        }
    }
}



void analyseCMD_CPM_Data(uint8_t *payload)
{
    struct CCu_CPM cpm_data;
    memset(&cpm_data, 0, sizeof(cpm_data));
    uint8_t cp_value_data = 0;
    
    // 检查payload指针是否有效
    if (payload == NULL)
    {
        // 记录错误日志并返回
        ERROR_LOG(log_msgid, "Invalid payload or insufficient length");
        return;
    }
    // 从payload中提取CPM状态信息
    cpm_data.CPStatus = payload[0];
    cpm_data.UTCSeconds = (((uint32_t)payload[5] << 24) | ((uint32_t)payload[4] << 16) | ((uint32_t)payload[3] << 8) | ((uint32_t)payload[2]));
    set_system_time(cpm_data.UTCSeconds);
    cpm_data.real_voltage = (((uint16_t)payload[7] << 8) | payload[6]);
    mqtt_cp_stat_send(cpm_data); // 发送CPM数据到MQTT

    cp_value_data = (uint8_t)cpm_data.real_voltage;
    Set_CCU_Public_Data(Get_CCU_Public_Data(), CP_Value_TYPE, &cp_value_data);
    Set_CCu_Internal_Data(Get_CCu_Charge_Data(), CCu_CPM_TYPE, &cpm_data);

}


void analyseCMD_CHM_Data(uint8_t *payload)
{
    struct CCu_CHM chm_data;
    struct SECC_ChargeState charge_state_data;
    memset(&chm_data, 0, sizeof(chm_data));
    memset(&charge_state_data, 0, sizeof(charge_state_data));

    if (payload == NULL)
    {
        ERROR_LOG(log_msgid, "Invalid payload pointer");
        return;
    }

    chm_data.EVCCIDACK = payload[0];
    chm_data.SECC_Ongoing = payload[2];
    chm_data.chargerMaximumPower = (uint16_t)(payload[5] << 8) | (uint16_t)(payload[4]);

    charge_state_data.chargerMaximumPower = (uint16_t)(payload[5] << 8) | (uint16_t)(payload[4]);
    
    Set_CCU_Public_Data(Get_CCU_Public_Data(), ChargeState_TYPE, &charge_state_data);
    Set_CCu_Internal_Data(Get_CCu_Charge_Data(), CCu_CHM_TYPE, &chm_data);
}



void analyseCMD_CRM_Data(uint8_t *payload)
{
    struct CCu_CRM crm_data;
    struct SECC_SessionSetup session_setup_data;
    memset(&crm_data, 0, sizeof(crm_data));
    memset(&session_setup_data, 0, sizeof(session_setup_data));

    if (payload == NULL)
    {
        ERROR_LOG(log_msgid, "Invalid payload pointer");
        return;
    }

    crm_data.identification = payload[0];
    crm_data.EVSEID = ((uint32_t)(payload[4]) << 24) | ((uint32_t)(payload[3]) << 16) | ((uint32_t)(payload[2]) << 8) | payload[1];

    session_setup_data.identification = payload[0];
    session_setup_data.evseid = ((uint32_t)(payload[4]) << 24) | ((uint32_t)(payload[3]) << 16) | ((uint32_t)(payload[2]) << 8) | payload[1];

    Set_CCU_Public_Data(Get_CCU_Public_Data(), SessionSetup_TYPE, &session_setup_data);
    Set_CCu_Internal_Data(Get_CCu_Charge_Data(), CCu_CRM_TYPE, &crm_data);
}


void analyseCMD_CML_Data(uint8_t *payload)
{
    struct CCu_CML cml_data;
    struct SECC_ChargeParameterDiscovery charge_param_data;
    memset(&cml_data, 0, sizeof(cml_data));
    memset(&charge_param_data, 0, sizeof(charge_param_data));

    if (payload == NULL)
    {
        ERROR_LOG(log_msgid, "Invalid payload pointer");
        return;
    }

    cml_data.chargerMaximumVoltage = (uint16_t)(payload[1] << 8) | (uint16_t)(payload[0]);
    cml_data.chargerMinimumVoltage = (uint16_t)(payload[3] << 8) | (uint16_t)(payload[2]);

    charge_param_data.chargerMaximumVoltage = (uint16_t)(payload[1] << 8) | (uint16_t)(payload[0]);
    charge_param_data.chargerMinimumVoltage = (uint16_t)(payload[3] << 8) | (uint16_t)(payload[2]);

    if ((payload[5]) & 0x80)
    {
        cml_data.chargerMaximumCurrent = 4000 + ((uint16_t)((payload[5] & 0x7f) << 8) | (uint16_t)(payload[4]));
        charge_param_data.chargerMaximumCurrent = 4000 + ((uint16_t)((payload[5] & 0x7f) << 8) | (uint16_t)(payload[4]));
    }
    else
    {
        cml_data.chargerMaximumCurrent = 4000 - ((uint16_t)((payload[5]) << 8) | (uint16_t)(payload[4]));
        charge_param_data.chargerMaximumCurrent = 4000 - ((uint16_t)((payload[5]) << 8) | (uint16_t)(payload[4]));
    }
    if (payload[7] & 0x80)
    {
        cml_data.chargerMinimumCurrent = 4000 + ((uint16_t)((payload[7] & 0x7f) << 8) | (uint16_t)(payload[6]));
        charge_param_data.chargerMinimumCurrent = 4000 + ((uint16_t)((payload[7] & 0x7f) << 8) | (uint16_t)(payload[6]));
    }
    else
    {
        cml_data.chargerMinimumCurrent = 4000 - ((uint16_t)((payload[7]) << 8) | (uint16_t)(payload[6]));
        charge_param_data.chargerMinimumCurrent = 4000 - ((uint16_t)((payload[7]) << 8) | (uint16_t)(payload[6]));
    }

    Set_CCU_Public_Data(Get_CCU_Public_Data(), ChargeParameterDiscovery_TYPE, &charge_param_data);
    Set_CCu_Internal_Data(Get_CCu_Charge_Data(), CCu_CML_TYPE, &cml_data);
}


void analyseCMD_CTS_Data(uint8_t *payload)
{
    struct CCu_CTS cts_data;
    memset(&cts_data, 0, sizeof(cts_data));

    if (payload == NULL)
    {
        ERROR_LOG(log_msgid, "Invalid payload pointer");
        return;
    }

    cts_data.seconds = payload[0];
    cts_data.minutes = payload[1];
    cts_data.hours = payload[2];
    cts_data.days = payload[3];
    cts_data.months = payload[4];
    cts_data.years = (((uint16_t)payload[6] << 8) | (uint16_t)payload[5]);

    Set_CCu_Internal_Data(Get_CCu_Charge_Data(), CCu_CTS_TYPE, &cts_data);
}


void analyseCMD_CRO_Data(uint8_t *payload)
{
    struct CCu_CRO cro_data;
    struct SECC_CableCheck cable_check_data;
    memset(&cro_data, 0, sizeof(cro_data));
    memset(&cable_check_data, 0, sizeof(cable_check_data));

    if (payload == NULL)
    {
        ERROR_LOG(log_msgid, "Invalid payload pointer");
        return;
    }

    cro_data.identification = payload[0];
    cable_check_data.identification = payload[0];

    Set_CCU_Public_Data(Get_CCU_Public_Data(), CableCheck_TYPE, &cable_check_data);
    Set_CCu_Internal_Data(Get_CCu_Charge_Data(), CCu_CRO_TYPE, &cro_data);
    
}


void analyseCMD_CCS_Data(uint8_t *payload)
{
    struct CCu_CCS ccs_data;
    struct SECC_ChargeState charge_state_data;
    memset(&ccs_data, 0, sizeof(ccs_data));
    memset(&charge_state_data, 0, sizeof(charge_state_data));

    if (payload == NULL)
    {
        ERROR_LOG(log_msgid, "Invalid payload pointer");
        return;
    }

    ccs_data.chargerVoltage = ((uint16_t)(payload[1] << 8) | (uint16_t)(payload[0]));
    charge_state_data.chargerVoltage = ((uint16_t)(payload[1] << 8) | (uint16_t)(payload[0]));

    if ((payload[4]) & 0x80)
    {
        ccs_data.chargerCurrent = 4000 + ((uint16_t)((payload[4] & 0x7f) << 8) | (uint16_t)(payload[3]));
        charge_state_data.chargerCurrent = 4000 + ((uint16_t)((payload[4] & 0x7f) << 8) | (uint16_t)(payload[3]));
    }
    else
    {
        ccs_data.chargerCurrent = 4000 - ((uint16_t)((payload[4]) << 8) | (uint16_t)(payload[3]));
        charge_state_data.chargerCurrent = 4000 - ((uint16_t)((payload[4]) << 8) | (uint16_t)(payload[3]));
    }

    ccs_data.chargerMaximumPower = ((uint16_t)(payload[7] << 8) | (uint16_t)(payload[6]));
    charge_state_data.chargerMaximumPower = ((uint16_t)(payload[7] << 8) | (uint16_t)(payload[6]));

    Set_CCU_Public_Data(Get_CCU_Public_Data(), ChargeState_TYPE, &charge_state_data);
    Set_CCu_Internal_Data(Get_CCu_Charge_Data(), CCu_CCS_TYPE, &ccs_data);
  
}


void analyseCMD_CST_Data(uint8_t *payload)
{
    struct CCu_CST cst_data;
    struct CCu_Stop ccu_stop_data;
    memset(&cst_data, 0, sizeof(cst_data));
    memset(&ccu_stop_data, 0, sizeof(ccu_stop_data));

    if (payload == NULL)
    {
        ERROR_LOG(log_msgid, "Invalid payload pointer");
        return;
    }

    cst_data.CCu_StopType = payload[0];
    cst_data.CCu_StopCause = payload[1];

    ccu_stop_data.CCu_StopType = payload[0];
    ccu_stop_data.CCu_StopCause = payload[1];
    
    Set_CCU_Public_Data(Get_CCU_Public_Data(), Stop_TYPE, &ccu_stop_data);
    Set_CCu_Internal_Data(Get_CCu_Charge_Data(), CCu_CST_TYPE, &cst_data);
    
}


void analyseCMD_CSD_Data(uint8_t *payload)
{
    struct CCu_CSD csd_data;

    memset(&csd_data, 0, sizeof(csd_data));

    if (payload == NULL)
    {
        ERROR_LOG(log_msgid, "Invalid payload pointer");
        return;
    }

    csd_data.chargeoverACK = (uint8_t)(payload[0]);

    Set_CCu_Internal_Data(Get_CCu_Charge_Data(), CCu_CSD_TYPE, &csd_data);
}


