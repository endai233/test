#include "V2G.h"
#include "ChargeControlCCu_Share.h"
#include <string.h>
#include <pthread.h>
#include "ChargeControlCCu.h"

static struct CCU_Public_Data_t g_ccu_public_data;
static struct EVCC_SHare_Data g_evcc_share_data;
static pthread_mutex_t g_share_data_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct CCu_Charge_Data CCu_Data;

struct CCu_Charge_Data *Get_CCu_Charge_Data(void)
{
    return &CCu_Data;
}

struct CCU_Public_Data_t *Get_CCU_Public_Data(void)
{
    return &g_ccu_public_data;
}


struct EVCC_SHare_Data *Get_EVCC_Share_Data(void)
{
    return &g_evcc_share_data;
}


/**
 * @brief Set data to EVCC_Public_Data_t based on the data type
 * 
 * @param evcc_data Pointer to the EVCC_Public_Data_t structure
 * @param type The type of data being set (SECC_Data_Type)
 * @param data Pointer to the source data
 */
void Set_CCU_Public_Data(struct CCU_Public_Data_t *ccu_Public_data, SECC_Data_Type type, const void *data)
{
    if (ccu_Public_data == NULL || data == NULL)
    {
        return;
    }

    pthread_mutex_lock(&g_share_data_mutex);
    switch (type)
    {
    case SessionSetup_TYPE:
        ccu_Public_data->evcc_session_setup = *(const struct SECC_SessionSetup *)data;
        break;
    case ChargeParameterDiscovery_TYPE:
        ccu_Public_data->evcc_charge_param = *(const struct SECC_ChargeParameterDiscovery *)data;
        break;
    case CableCheck_TYPE:
        ccu_Public_data->evcc_cable_check = *(const struct SECC_CableCheck *)data;
        break;
    case ChargeState_TYPE:
        ccu_Public_data->evcc_charge_state = *(const struct SECC_ChargeState *)data;
        break;
    case Stop_TYPE:
        ccu_Public_data->CCu_stop = *(const struct CCu_Stop *)data;
        break;
    case CP_Value_TYPE:
        ccu_Public_data->evcc_cp_value = *(const uint8_t *)data;
        break;
    case SLAC_RESULT_TYPE:
        ccu_Public_data->evcc_slac_result = *(const uint8_t *)data;
        break;
    default:
        // Handle other types or ignore
        break;
    }
    pthread_mutex_unlock(&g_share_data_mutex);
}

/**
 * @brief Sync EVCC_Share_Data to CCu_Charge_Data
 * 
 * @param ccu_data Pointer to the CCu_Charge_Data structure
 */
void Sync_EVCC_Share_Data_To_CCu(struct CCu_Charge_Data *ccu_data)
{
    if (ccu_data == NULL)
    {
        return;
    }
    
    struct EVCC_SHare_Data *share_data = Get_EVCC_Share_Data();
    
    pthread_mutex_lock(&g_share_data_mutex);
    ccu_data->V2G_value.evccid = share_data->evccid;
    ccu_data->V2G_value.attenuation = share_data->attenuation;
    ccu_data->V2G_value.vehicleMaximumVoltage = share_data->vehicleMaximumVoltage;
    ccu_data->V2G_value.vehicleMaximumCurrent = share_data->vehicleMaximumCurrent;
    ccu_data->V2G_value.vehicleMaximumPower = share_data->vehicleMaximumPower;
    ccu_data->V2G_value.vehicleDemandVoltage = share_data->vehicleDemandVoltage;
    ccu_data->V2G_value.vehicleDemandCurrent = share_data->vehicleDemandCurrent;
    ccu_data->V2G_value.fullchargeRemainingTime = share_data->fullchargeRemainingTime;
    ccu_data->V2G_value.bulkchargeRemainingTime = share_data->bulkchargeRemainingTime;
    ccu_data->V2G_value.evchargeStopCause = share_data->evchargeStopCause;
    ccu_data->V2G_value.soc = share_data->soc;
    ccu_data->V2G_value.fullsoc = share_data->fullsoc;
    ccu_data->V2G_value.bulksoc = share_data->bulksoc;

    // Update stop flags based on is_ev_normal_stop
    ccu_data->EV_Abnormal_Stop_flag = share_data->EV_Abnormal_Stop_flag;
    ccu_data->EV_Normal_Stop_flag = share_data->EV_Normal_Stop_flag;
    
    // Sync control flags from EVCC to CCU
    ccu_data->startChargeParamDiscovery = share_data->startChargeParamDiscovery;
    ccu_data->startCableCheck = share_data->startCableCheck;
    ccu_data->startPreCharge = share_data->startPreCharge;
    ccu_data->vehicleReadyToCharge = share_data->vehicleReadyToCharge; 
    ccu_data->sessionStop = share_data->sessionStop; 

    pthread_mutex_unlock(&g_share_data_mutex);
}

/**
 * @brief Set data to CCu_Charge_Data based on the data type
 * 
 * @param ccu_data Pointer to the CCu_Charge_Data structure
 * @param type The type of data being set (SECC_Data_Type)
 * @param data Pointer to the source data
 */
void Set_CCu_Internal_Data(struct CCu_Charge_Data *ccu_data, SECC_Data_Type type, const void *data)
{
    if (ccu_data == NULL || data == NULL)
    {
        return;
    }

    switch (type)
    {
    case CCu_CPM_TYPE:
        ccu_data->CPM_value = *(const struct CCu_CPM *)data;
        break;
    case CCu_CHM_TYPE:
        ccu_data->CHM_value = *(const struct CCu_CHM *)data;
        ccu_data->CHM_recv_flag = FLAG_SET;
        break;
    case CCu_CRM_TYPE:
        ccu_data->CRM_value = *(const struct CCu_CRM *)data;
        ccu_data->CRM_recv_flag = FLAG_SET;
        break;
    case CCu_CML_TYPE:
        ccu_data->CML_value = *(const struct CCu_CML *)data;
        ccu_data->CML_recv_flag = FLAG_SET;
        break;
    case CCu_CTS_TYPE: 
        ccu_data->CTS_value = *(const struct CCu_CTS *)data;
        ccu_data->CTS_recv_flag = FLAG_SET;
        break;
    case CCu_CRO_TYPE:
        ccu_data->CRO_value = *(const struct CCu_CRO *)data;
        ccu_data->CRO_recv_flag = FLAG_SET;
        break;
    case CCu_CCS_TYPE:
        ccu_data->CCS_value = *(const struct CCu_CCS *)data;
        ccu_data->CCS_recv_flag = FLAG_SET;
        break;
    case CCu_CST_TYPE:
        ccu_data->CST_value = *(const struct CCu_CST *)data;
        ccu_data->CST_recv_flag = FLAG_SET;
        break;
    case SECC_CSD:
    case CCu_CSD_TYPE:
        ccu_data->CSD_value = *(const struct CCu_CSD *)data;
        ccu_data->CSD_recv_flag = FLAG_SET;
        break;
    default:
        // Handle other types or ignore
        break;
    }
}


