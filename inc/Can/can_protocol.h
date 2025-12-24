#ifndef __CAN_PROTOCOL_H__
#define __CAN_PROTOCOL_H__

#define CAN_PORT_NAME "can0"         //定义can接口名称

#define CAN_ADDR_EVSE 0x56
#define CAN_ADDR_SECC 0xF4

//充电机侧常规报文ID
#define CAN_ID_CHM 0x1826F456
#define CAN_ID_CRM 0x1801F456
#define CAN_ID_CML 0x1808F456
#define CAN_ID_CTS 0x1807F456
#define CAN_ID_CRO 0x100AF456
#define CAN_ID_CCS 0x1812F456
#define CAN_ID_CST 0x101AF456
#define CAN_ID_CSD 0x181DF456
//autocharge报文ID
#define CAN_ID_CDT 0x18E2F456

//SECC侧常规报文ID
#define CAN_ID_BHM 0x182756F4
#define CAN_ID_BRM 0x1C0256F4
#define CAN_ID_BCP 0x000600FF       //PGN
#define CAN_ID_BRO 0x100956F4
#define CAN_ID_BCS 0x001100FF       //PGN
#define CAN_ID_BCL 0x181056F4
#define CAN_ID_BSM 0x181356F4
#define CAN_ID_BEM 0x081E56F4
#define CAN_ID_BST 0x101956F4
#define CAN_ID_BSD 0x181C56F4
//autocharge报文ID
#define CAN_ID_BAC 0x18F156F4

#if 1

//*******接收到EVSE发过来的数据*******
typedef struct 
{
	uint8_t res[3];             //预留
    uint8_t lanch_secc;         //启动secc
    uint8_t max_power_l;        //最大功率高位
    uint8_t max_power_h;        //最大功率低位
    uint8_t res1[2];            //预留
}CAN_PRO_CHM;

typedef struct 
{
	uint8_t flag;               //辨识标志
	uint8_t res[7];
}CAN_PRO_CRM;

typedef struct 
{
	uint8_t max_volt_l;
    uint8_t max_volt_h; //最大电压
    uint8_t min_volt_l;
    uint8_t min_volt_h; //最小电压
    uint8_t max_cur_l;
    uint8_t max_cur_h;  //最大电流
    uint8_t min_cur_l;
    uint8_t min_cur_h;  //最小电流
}CAN_PRO_CML;

typedef struct 
{
	uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year_l;
    uint8_t year_h;
    uint8_t res;
}CAN_PRO_CTS;

typedef struct 
{
	uint8_t flag;
    uint8_t res[7];
}CAN_PRO_CRO;

typedef struct 
{
	uint8_t volt_l;
    uint8_t volt_h;     //当前输出电压
    uint8_t cur_l;
    uint8_t cur_h;      //当前输出电流
    uint8_t power_l;
    uint8_t power_h;    //最大功率
    uint8_t res[2];
}CAN_PRO_CCS;

typedef struct 
{
	uint8_t stop_type;      //停止类型
    uint8_t fault_flag;     //异常标志
    uint8_t res[6];
}CAN_PRO_CST;

typedef struct 
{
	uint8_t res[2];      
    uint8_t volt_l;
    uint8_t volt_h;       //实际输出电压
    uint8_t res1[4];
}CAN_PRO_CSD;

typedef struct 
{
	uint8_t fix_1;      
    uint8_t fix_5;
    uint8_t fix_A;
    uint8_t res[5];
}CAN_PRO_CDT;

//桩端相关数据
typedef struct 
{
	CAN_PRO_CHM chm;
    CAN_PRO_CRM crm;
    CAN_PRO_CML cml;
    CAN_PRO_CTS cts;
    CAN_PRO_CRO cro;
    CAN_PRO_CCS ccs;
    CAN_PRO_CST cst;
    CAN_PRO_CSD csd;
    CAN_PRO_CDT cdt;
}CAN_PRO_EVSE_Data;

//*******SECC收到的车端数据*******
typedef struct 
{
    uint8_t res[2];
    uint8_t ccsAvgAttn;
    uint8_t status;         //枪状态+slac状态
    uint8_t version1;
    uint8_t version2;
    uint8_t version3;
    uint8_t version4;
}CAN_PRO_BHM;

typedef struct 
{
    uint8_t fix_1;      //固定是0x01
    uint8_t fix_0;      //固定是0x00
    uint8_t mac[6];
}CAN_PRO_BRM;

typedef struct 
{
    uint8_t res[2];
    uint8_t max_cur_l;
    uint8_t max_cur_h;
    uint8_t res1[2];
    uint8_t max_volt_l;
    uint8_t max_volt_h;
    uint8_t res2;
    uint8_t soc_l;
    uint8_t soc_h;
    uint8_t res3[3];    
}CAN_PRO_BCP;

typedef struct 
{
    uint8_t flag;
    uint8_t res[7];
}CAN_PRO_BRO;

typedef struct 
{
    uint8_t res[6];
    uint8_t soc;
    uint8_t rest_time_l;
    uint8_t rest_time_h;
}CAN_PRO_BCS;

typedef struct 
{
    uint8_t volt_req_l;
    uint8_t volt_req_h;
    uint8_t cur_req_l;
    uint8_t cur_req_h;
    uint8_t res;
    uint8_t max_volt_l;
    uint8_t max_volt_h;
    uint8_t pre_done_flag;          //预充完成标志
}CAN_PRO_BCL;

typedef struct 
{
    uint8_t res[6];
    uint8_t fixed;              //固定为0x10
    uint8_t res1;
}CAN_PRO_BSM;

typedef struct 
{
    uint8_t res[3];
    uint8_t zrn_pro_value;
    uint8_t zcl_code_l;
    uint8_t zcl_code_h;
    uint8_t zrn_code_l;
    uint8_t zrn_code_h;
}CAN_PRO_BEM;

typedef struct 
{
    uint8_t res[3];
    uint8_t zrn_pro_value;
    uint8_t zcl_code_l;
    uint8_t zcl_code_h;
    uint8_t zrn_code_l;
    uint8_t zrn_code_h;
}CAN_PRO_BST;

typedef struct 
{
    uint8_t soc;
    uint8_t res[6];
    uint8_t welding_check;
}CAN_PRO_BSD;

typedef struct 
{
    uint8_t result;
    uint8_t res;
    uint8_t mac[6];
}CAN_PRO_BAC;

typedef struct 
{
	CAN_PRO_BHM bhm;
    CAN_PRO_BRM brm;
    CAN_PRO_BCP bcp;
    CAN_PRO_BRO bro;
    CAN_PRO_BCS bcs;
    CAN_PRO_BCL bcl;
    CAN_PRO_BSM bsm;
    CAN_PRO_BEM bem;
    CAN_PRO_BST bst;
    CAN_PRO_BSD bsd;
    CAN_PRO_BAC bac;
}CAN_PRO_SECC_Data;


/***********************函数区处理***************************/
#define CAN_SEND_LEN_MAX    64      //多帧报文最大发送数据长度64

typedef struct
{
    uint32_t can_id;
    uint32_t can_dlc;
    uint8_t  data[CAN_SEND_LEN_MAX];
} CAN_MSG_SEND;

//接收队列初始化
void can_pro_recv_msg_init(void);    
//接收消息处理
void can_pro_recv_msg_proc(uint32_t id, uint8_t dlc, uint8_t *data);    

void can_pro_send_msg_init(void);         //发送队列初始化
void can_pro_send_msg_input(CAN_MSG_SEND *pIn);

void can_pro_send_msg_run(int s);

void* can_task(void* arg);          // can的主任务

// 按照吉瓦特的协议里BHM部分生成CP的工作状态，0-12V 1-9V 2-9V/5%  3-6V/5%
uint8_t can_pro_cp_bhm_status_get(void);
#endif
#endif
