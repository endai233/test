
#ifndef CHARGECONTROLCCU_DATA_H
#define CHARGECONTROLCCU_DATA_H

#ifdef __cplusplus
extern "C"{
#endif 

#include "ChargeControlCCu.h"


void analyseCMD_CBM_Data(uint8_t *payload);
void analyseCMD_CPM_Data(uint8_t *payload);
void analyseCMD_CHM_Data(uint8_t *payload);
void analyseCMD_CRM_Data(uint8_t *payload);
void analyseCMD_CML_Data(uint8_t *payload);
void analyseCMD_CTS_Data(uint8_t *payload);
void analyseCMD_CRO_Data(uint8_t *payload);
void analyseCMD_CCS_Data(uint8_t *payload);
void analyseCMD_CST_Data(uint8_t *payload);
void analyseCMD_CSD_Data(uint8_t *payload);

//void ccu_sync_decode(struct CCu_Charge_Data *CCu_Data, SeccEventId event_id);

#ifdef __cplusplus
}
#endif

#endif
