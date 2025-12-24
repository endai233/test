/*
 * @Author: yang 1458900080@qq.com
 * @Date: 2025-12-19 16:01:32
 * @LastEditors: yang 1458900080@qq.com
 * @LastEditTime: 2025-12-23 14:29:51
 * @FilePath: \Can_CCu_To_V2g\inc\CCU\ChargeControlCCu_Share.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef CHARGECONTROLCCU_SHARE_H
#define CHARGECONTROLCCU_SHARE_H

#include "V2G.h"
#include "ChargeControlCCu.h"
#include <stdbool.h>

struct EVCC_SHare_Data{
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
    

    uint8_t EV_Abnormal_Stop_flag:1;
    uint8_t EV_Normal_Stop_flag:1;    
    // Flags from SECC to control CCU state machine
    uint8_t startChargeParamDiscovery:1;
    uint8_t startCableCheck:1;
    uint8_t startPreCharge:1;
    uint8_t vehicleReadyToCharge:1; 
    uint8_t sessionStop:1; 
};

void Set_CCU_Public_Data(CCU_Public_Data_t *ccu_Public_data, SECC_Data_Type type, void *data);
void Set_CCu_Internal_Data(struct CCu_Charge_Data *ccu_data, SECC_Data_Type type, const void *data);
void Sync_EVCC_Share_Data_To_CCu(struct CCu_Charge_Data *ccu_data);

struct EVCC_SHare_Data *Get_EVCC_Share_Data(void);  //获取EVCC_Share_Data指针,读取EVCC侧的数据供CCU侧的SM使用
struct CCU_Public_Data_t *Get_CCU_Public_Data(void);  //获取CCU_Public_Data_t指针
struct CCu_Charge_Data *Get_CCu_Charge_Data(void);  //获取CCU_Charge_Data指针 供CCU侧的SM使用

#endif
