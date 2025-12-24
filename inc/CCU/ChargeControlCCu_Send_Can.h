/*
 * @Author: yang 1458900080@qq.com
 * @Date: 2025-12-23 13:23:33
 * @LastEditors: yang 1458900080@qq.com
 * @LastEditTime: 2025-12-23 13:24:02
 * @FilePath: \Can_CCu_To_V2g\inc\CCU\ChargeControlCCu_Send_Can.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef SECCPROTOCOLDATA_H
#define SECCPROTOCOLDATA_H

#ifdef __cplusplus
extern "C"{
#endif 

#include <stdint.h>
#include <stdatomic.h>
#include <mqueue.h>
#include <stdio.h>

#include "din_msgDefDatatypes.h"
#include "ChargeControlEVCC.h"

#define PORT 15118
#define MULTICAST_ADDR "ff02::1" 

#define SOCKET_BUFFER_SIZE 256
#define READ_BUFFER_SIZE 256

#define SECC_CMD_Code_SIZE 4
#define SECC_CMD_BHM_SIZE 12
#define SECC_CMD_BID_SIZE 10
#define SECC_CMD_BEM_SIZE 12
#define SECC_CMD_BRM_SIZE 12
#define SECC_CMD_BCP_SIZE 12
#define SECC_CMD_BRO_SIZE 5
#define SECC_CMD_BCL_SIZE 12
#define SECC_CMD_BCS_SIZE 12
#define SECC_CMD_BSM_SIZE 11
#define SECC_CMD_BST_SIZE 8
#define SECC_CMD_BSD_SIZE 12
#define CCu_CMD_MULTI_SEND_SIZE 12

#define CCu_CMD_CODE_SIZE 4
#define CCu_CMD_CPM_SIZE 12
#define CCu_CMD_CHM_SIZE 12
#define CCu_CMD_CRM_SIZE 12
#define CCu_CMD_CML_SIZE 12
#define CCu_CMD_CTS_SIZE 11
#define CCu_CMD_CRO_SIZE 5
#define CCu_CMD_CCS_SIZE 12
#define CCu_CMD_CST_SIZE 8
#define CCu_CMD_CSD_SIZE 12
#define CCu_CMD_MULTI_RECV_SIZE 12

#define BHM_CMD_Code 0x182756F4
#define BID_CMD_Code 0x183256F4
#define BRM_CMD_Code 0x1C0256F4
#define MULTI_CMD_SEND_Code 0x1CEC56F4
#define MULTI_CMD_SEND_DATA_CODE 0x1CEB56F4
#define BCP_CMD_Code 0x180656F4
#define BRO_CMD_Code 0x100956F4
#define BCS_CMD_Code 0x181156F4
#define BCL_CMD_Code 0x181056F4  
#define BSM_CMD_Code 0x181356F4
#define BEM_CMD_Code 0x081E56F4
#define BST_CMD_Code 0x101956F4
#define BSD_CMD_Code 0x181C56F4

#define CBM_CMD_Code 0x1800F456
#define CPM_CMD_Code 0x1825F456
#define CHM_CMD_Code 0x1826F456
#define CRM_CMD_Code 0x1801F456
#define CML_CMD_Code 0x1808F456
#define CTS_CMD_Code 0x1807F456
#define CRO_CMD_Code 0x100AF456
#define CCS_CMD_Code 0x1812F456
#define CST_CMD_Code 0x101AF456
#define CSD_CMD_Code 0x181DF456
#define MULTI_CMD_RECV_Code 0x1CECF456

void writeCanData(struct DataPacket* send_data, struct CCu_Charge_Data* CCu_Data, SECC_CMD_Code_type CodeType, uint8_t value, ChargeStopCause ChargeStopCause);

#ifdef __cplusplus
}
#endif
#endif