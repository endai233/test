/****************************************Copyright (c)**************************************************
;**                                珠海派诺科技股份有限公司
;**
;**
;**--------------File Info-------------------------------------------------------------------------------
;** 文件名:    can_protocol.c
;** 版本:     1.0
;** 概述:   can协议收发功能
;** 索引:
;**------------------------------------------------------------------------------------------------------
;** 作者: weiq
;** 日期: 2025.07.14
;**------------------------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "led.h"
#include "can_protocol.h"
#include "ChargeControlCCu.h"
#include "cp.h"
#include "log.h"
#include "../../CCU/inc/CCU_Can_Interface.h"

// J1939相关定义
#define J1939_PGN_TP_CM 0x1CEC56F4 // 传输协议连接管理
#define J1939_PGN_TP_DT 0x1CEB56F4 // 传输协议数据传输

// 连接管理命令
#define CM_RTS 0x10   // 请求发送
#define CM_CTS 0x11   // 清除发送
#define CM_ACK 0x13   // 确认
#define CM_ABORT 0x15 // 中止

// 超时设置（毫秒）
#define J1939_TIMEOUT_RTS (100)        // RTS响应超时
#define J1939_TIMEOUT_CTS 100          // CTS响应超时
#define J1939_TIMEOUT_CNT 5            // 等待CTS的次数， 5*100ms
#define J1939_DELAY_BETWEEN_PACKETS 20 // 两帧之间的间隔 20ms

#define CAN_RING_NUM 64 // 环形缓存数量64

typedef struct
{
    CAN_MSG_SEND queue[CAN_RING_NUM];
    uint32_t wptr;
    uint32_t rptr;
} CAN_SEND_RING;

typedef struct
{
    uint32_t order;       // 区域操作流水号
    pthread_mutex_t lock; // 互斥锁
    CAN_SEND_RING msg;    // 消息内容
} CAN_SEND_BUF;

// 定义接收和发送的全局缓存
static CAN_SEND_BUF gRecvBuf = {0};
static CAN_SEND_BUF gSendBuf = {0};

static void can_pro_recv_queue_input(uint32_t id, uint8_t dlc, uint8_t *data);
/**
 * @brief 检查是否可以发送外部CAN帧
 *
 * 该函数用于检查是否可以发送一个外部CAN帧。它首先检查传入的参数是否有效，然后配置并发送CAN帧。
 *
 * @param s socket文件描述符，用于发送CAN帧
 * @param id CAN帧的ID
 * @param data 指向要发送的数据的指针
 * @param dlc 数据长度码，表示要发送的数据长度
 *
 * @return 0 表示成功发送CAN帧，-1 表示发生错误
 */
static int can_send_extern_frame(int s, uint32_t id, const uint8_t *data, uint8_t dlc)
{
    struct can_frame frame = {0};
    uint32_t can_id;

    if (s <= 0 || NULL == data)
    {
        return -1;
    }

    if (0 == dlc || dlc > 8)
    {
        return 0;
    }

    can_id = id;

    // 配置CAN帧
    frame.can_id = can_id | CAN_EFF_FLAG; // 设置扩展帧标志
    frame.can_dlc = dlc;
    memcpy(frame.data, data, dlc);

    // 发送CAN帧
    if (write(s, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame))
    {
        perror("Write failed");
        return -1;
    }
    // 添加CAN发送时LED2闪烁
    //led_can_flash();
    led_simple_can_flash();
    
    return 0;
}

/**
 * 发送J1939长数据（使用TP协议）
 * @param s 套接字描述符
 * @param pgn 参数组号
 * @param src 源地址
 * @param dst 目标地址
 * @param data 数据缓冲区
 * @param length 数据长度(最大1785字节)
 * @return 成功返回0，失败返回-1
 */
static int can_send_long_message(int s, uint32_t pgn, uint8_t src, uint8_t dst, const uint8_t *data, uint32_t length)
{
    uint8_t buffer[8];
    uint8_t sequence_number = 1;
    uint32_t total_packets;
    int32_t remaining_bytes = length;
    uint32_t sent_bytes = 0;
    struct timeval timeout;
    fd_set readfds;
    int nfds = s + 1;
    struct can_frame frame;
    int ret;
    int recv_cnt = 0;

    // 检查数据长度限制（J1939最大支持1785字节）
    if (length > 1785)
    {
        fprintf(stderr, "Error: J1939 message too long (%u bytes, max 1785)\n", length);
        return -1;
    }

    // 计算需要的数据包总数
    if (length % 7 == 0)
    { // 每帧传输7字节数据，正好整除时不需要额外一包
        total_packets = length / 7;
    }
    else
    { // 如果有剩余数据，则需要额外一包
        total_packets = (length + 7) / 7;
    }

    // 1. 发送RTS (Request To Send)
    buffer[0] = CM_RTS;               // 控制字节
    buffer[1] = length & 0xFF;        // 数据长度低字节
    buffer[2] = (length >> 8) & 0xFF; // 数据长度高字节
    buffer[3] = total_packets;        // 数据包总数
    buffer[4] = pgn & 0xFF;           // PGN低字节
    buffer[5] = (pgn >> 8) & 0xFF;    // PGN中字节
    buffer[6] = (pgn >> 16) & 0xFF;   // PGN高字节
    buffer[7] = 0x00;                 // 保留

    printf("total pack is %d  len is %d\n", total_packets, length);

    if (can_send_extern_frame(s, J1939_PGN_TP_CM, buffer, 8) < 0)
    {
        fprintf(stderr, "Error sending RTS\n");
        return -1;
    }

    // 2. 等待CTS (Clear To Send)
    for (recv_cnt = 0; recv_cnt < J1939_TIMEOUT_CNT; ++recv_cnt)
    {
        FD_ZERO(&readfds);
        FD_SET(s, &readfds);
        timeout.tv_sec = J1939_TIMEOUT_RTS / 1000;
        timeout.tv_usec = (J1939_TIMEOUT_RTS % 1000) * 1000;

        ret = select(nfds, &readfds, NULL, NULL, &timeout);
        if (ret < 0)
        {
            perror("Select error while waiting for CTS");
            return -1;
        }
        else if (ret == 0)
        {
            fprintf(stderr, "Timeout waiting for CTS\n");
            continue; // 超时，继续等待CTS响应
        }

        // 读取CTS消息
        if (read(s, &frame, sizeof(struct can_frame)) < 0)
        {
            perror("Error reading CTS");
            return -1;
        }

        frame.can_id = frame.can_id & (~CAN_EFF_FLAG);
        printf("rec can id 0x%x\n", frame.can_id);
        // 验证是否为CTS消息
        if (frame.can_id == 0x1CECF456 && frame.data[0] == CM_CTS)
        { // 收到确认信息
            printf("CTS recved\n");
            break;
        }
        else
        { // 收到非CTS消息，放入到队列中等待下一次处理
            printf("not CTS recved,recv is 0x%x\n", frame.can_id);
            can_pro_recv_queue_input(frame.can_id, frame.can_dlc, (uint8_t *)frame.data);
        }
    }

    /*预期结果超限*/
    if (recv_cnt >= J1939_TIMEOUT_CNT)
    {
        printf("CTS recv timeout\n");
        return -1;
    }

    // 3. 发送数据帧
    while (remaining_bytes > 0)
    {
        uint8_t bytes_this_packet = (remaining_bytes > 7) ? 7 : remaining_bytes;

        // 构建数据帧
        buffer[0] = sequence_number; // 序列号
        memcpy(&buffer[1], &data[sent_bytes], bytes_this_packet);

        // 发送数据帧
        if (can_send_extern_frame(s, J1939_PGN_TP_DT, buffer, bytes_this_packet + 1) < 0)
        {
            fprintf(stderr, "Error sending data packet %u\n", sequence_number);
            return -1;
        }

        // 更新计数器
        remaining_bytes -= bytes_this_packet;
        sent_bytes += bytes_this_packet;
        sequence_number++;

        // 添加帧间延迟（防止总线过载）
        if (remaining_bytes > 0)
        {
            usleep(J1939_DELAY_BETWEEN_PACKETS * 1000);
        }
    }

    // 4. 等待ACK确认,取消等待确认，发送完成既结束

    return 0; // 发送成功
}

/*******************************接收消息处理******************************************/

/**
 * 将uint8类型的BCD码转换为十进制数
 * @param bcd 输入的BCD码（高4位为十位，低4位为个位）
 * @return 对应的十进制数（0-99）
 */
static uint8_t bcd_to_decimal(uint8_t bcd)
{
    // 提取高4位（十位），右移4位
    uint8_t tens = (bcd >> 4) & 0x0F;
    // 提取低4位（个位）
    uint8_t units = bcd & 0x0F;

    // 组合为十进制数（限制范围0-99）
    return (tens * 10 + units) % 100;
}

// 将CAN_PRO_CTS转换为time_t
/**
 * @brief 将CAN_PRO_CTS结构体转换为time_t类型的时间戳
 *
 * 将CAN_PRO_CTS结构体中的时间信息转换为time_t类型的时间戳。
 *
 * @param cts 指向CAN_PRO_CTS结构体的指针
 *
 * @return 返回time_t类型的时间戳
 */
static time_t can_cts_to_time_t(const CAN_PRO_CTS *cts)
{
    struct tm t;
    int32_t year;

    memset(&t, 0, sizeof(t));

    t.tm_sec = bcd_to_decimal(cts->second);
    t.tm_min = bcd_to_decimal(cts->minute);
    t.tm_hour = bcd_to_decimal(cts->hour);
    t.tm_mday = bcd_to_decimal(cts->day);
    t.tm_mon = bcd_to_decimal(cts->month) - 1; // struct tm的月份是0-11
    year = bcd_to_decimal(cts->year_h) * 100 + bcd_to_decimal(cts->year_l);
    t.tm_year = year - 1900; // 假设year_h和year_l为年份低高字节

    printf("tm:%u %u %u %u %u %u\n", t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday, t.tm_mon, t.tm_year);

    return mktime(&t);
}

// 同步系统时间，如果差异大于5秒则设置
/**
 * @brief 处理CAN_PRO_CTS消息，并根据消息中的时间更新系统时间
 *
 * 如果cts为nullptr，则直接返回。
 * 首先将CAN_PRO_CTS消息中的时间转换为time_t类型，然后与当前系统时间进行比较。
 * 如果CAN_PRO_CTS消息中的时间与系统时间差大于5秒，则尝试将系统时间更新为CAN_PRO_CTS消息中的时间。
 *
 * @param cts CAN_PRO_CTS消息的指针
 */
static void can_cts_recv_do(const CAN_PRO_CTS *cts)
{
    if (!cts)
        return;
    time_t can_time = can_cts_to_time_t(cts);
    time_t sys_time = time(NULL);

    if (can_time == (time_t)-1)
        return;
    long diff = labs((long)can_time - (long)sys_time);
    if (diff > 5)
    { // 差距大于5s 就进行校时
        struct timeval tv;
        tv.tv_sec = can_time;
        tv.tv_usec = 0;
        if (settimeofday(&tv, NULL) == 0)
        {
            // printf("System time updated from CAN_PRO_CTS.\n");
        }
        else
        {
            perror("settimeofday failed");
        }
    }
}

static int can_recv_chm(uint8_t *buffer, CAN_PRO_CHM *pData, uint8_t len)
{
    uint8_t data[8] = {0};
    //printf("CAN recv proc chm\n");
    if (NULL == buffer || NULL == pData || len > 8)
    {
        return -1;
    }

    pData->res[0] = buffer[0];
    pData->res[1] = buffer[1];
    pData->res[2] = buffer[2];
    pData->lanch_secc = buffer[3];
    pData->max_power_l = buffer[4];
    pData->max_power_h = buffer[5];
    pData->res1[0] = buffer[6];
    pData->res1[1] = buffer[7];

    data[0] = 0x01; // 暂时设置默认是0x01，evcc确认信息 
    data[1] = 0x00;
    if (pData->lanch_secc < 0x10)
    {//区分启动标志位，0x10为启动标志位
        data[2] = 0x00;
    }
    else
    {
        data[2] = 0x10;
    }    
    data[3] = 0x00;

    data[4] = pData->max_power_l;
    data[5] = pData->max_power_h;
    data[6] = 0x00;
    data[7] = 0x00;

    analyseCMD_CHM_Data(data);
    return 0;
}

static int can_recv_crm(uint8_t *buffer, CAN_PRO_CRM *pData, uint8_t len)
{
    uint8_t data[8] = {0};
    printf("CAN recv proc crm flag 0X%X\n", pData->flag);
    if (NULL == buffer || NULL == pData || len > 8)
    {
        return -1;
    }

    pData->flag = buffer[0];

    // 固定写入一个值作为evseid
    pData->res[0] = 0x21;
    pData->res[1] = 0x05;
    pData->res[2] = 0x25;
    pData->res[3] = 0x20;
    pData->res[4] = buffer[5];
    pData->res[5] = buffer[6];
    pData->res[6] = buffer[7];

    if (0xAA == pData->flag)
    {
        data[0] = 0xAA;
    }
    else
    {
        data[0] = 0x00;
    }

    analyseCMD_CRM_Data(data);
    return 0;
}

static int can_recv_cts(uint8_t *buffer, CAN_PRO_CTS *pData, uint8_t len)
{
    uint8_t data[8] = {0};
    printf("CAN recv proc cts\n");
    if (NULL == buffer || NULL == pData || len > 8)
    {
        return -1;
    }

    pData->second = buffer[0];
    pData->minute = buffer[1];
    pData->hour = buffer[2];
    pData->day = buffer[3];
    pData->month = buffer[4];
    pData->year_l = buffer[5];
    pData->year_h = buffer[6];
    pData->res = buffer[7];
    // 检查是否需要修改时间，并执行时间同步操作
    can_cts_recv_do(pData); // 处理时间同步操作

    data[0] = pData->second;
    data[1] = pData->minute;
    data[2] = pData->hour;
    data[3] = pData->day;
    data[4] = pData->month;
    data[5] = pData->year_l;
    data[6] = pData->year_h;
    analyseCMD_CTS_Data(data);

    return 0;
}

int can_recv_cml(uint8_t *buffer, CAN_PRO_CML *pData, uint8_t len)
{
    uint8_t data[8] = {0};
    printf("CAN recv proc cml\n");
    if (NULL == buffer || NULL == pData || len > 8)
    {
        return -1;
    }

    pData->max_volt_l = buffer[0];
    pData->max_volt_h = buffer[1];
    pData->min_volt_l = buffer[2];
    pData->min_volt_h = buffer[3];

    pData->max_cur_l = buffer[4];
    pData->max_cur_h = buffer[5];
    pData->min_cur_l = buffer[6];
    pData->min_cur_h = buffer[7];

    data[0] = pData->max_volt_l;
    data[1] = pData->max_volt_h;
    data[2] = pData->min_volt_l;
    data[3] = pData->min_volt_h;

    data[4] = pData->max_cur_l;
    data[5] = pData->max_cur_h;
    data[6] = pData->min_cur_l;
    data[7] = pData->min_cur_h;

    analyseCMD_CML_Data(data);
    return 0;
}

int can_recv_cro(uint8_t *buffer, CAN_PRO_CRO *pData, uint8_t len)
{
    uint8_t data[8] = {0};
    printf("CAN recv proc cro flag 0x%X\n", pData->flag);
    if (NULL == buffer || NULL == pData || len > 8)
    {
        return -1;
    }

    pData->flag = buffer[0];
    pData->res[0] = buffer[1];
    pData->res[1] = buffer[2];
    pData->res[2] = buffer[3];

    pData->res[3] = buffer[4];
    pData->res[4] = buffer[5];
    pData->res[5] = buffer[6];
    pData->res[6] = buffer[7];

    if (0xAA == pData->flag)
    {
        data[0] = 0xAA;
    }
    else
    {
        data[0] = 0x00;
    }
    analyseCMD_CRO_Data(data);
    return 0;
}

int can_recv_ccs(uint8_t *buffer, CAN_PRO_CCS *pData, uint8_t len)
{
    uint8_t data[8] = {0};
    printf("CAN recv proc ccs\n");
    if (NULL == buffer || NULL == pData || len > 8)
    {
        return -1;
    }

    pData->volt_l = buffer[0];
    pData->volt_h = buffer[1];
    pData->cur_l = buffer[2];
    pData->cur_h = buffer[3];

    pData->power_l = buffer[4];
    pData->power_h = buffer[5];
    pData->res[0] = buffer[6];
    pData->res[1] = buffer[7];

    data[0] = pData->volt_l;
    data[1] = pData->volt_h;
    data[2] = 0x00;

    data[3] = pData->cur_l;
    data[4] = pData->cur_h;
    data[5] = 0x00;

    data[6] = pData->power_l;
    data[7] = pData->power_h;

    analyseCMD_CCS_Data(data);
    return 0;
}

int can_recv_cst(uint8_t *buffer, CAN_PRO_CST *pData, uint8_t len)
{
    uint8_t data[8] = {0};
    printf("CAN recv proc cst\n");
    if (NULL == buffer || NULL == pData || len > 8)
    {
        return -1;
    }

    pData->stop_type = buffer[0];
    pData->fault_flag = buffer[1];
    pData->res[0] = buffer[2];
    pData->res[1] = buffer[3];

    pData->res[2] = buffer[4];
    pData->res[3] = buffer[5];
    pData->res[4] = buffer[6];
    pData->res[5] = buffer[7];

    data[0] = pData->stop_type;
    data[1] = 0x01; // 将fault flag转换成对应停止原因
    analyseCMD_CST_Data(data);
    return 0;
}

int can_recv_csd(uint8_t *buffer, CAN_PRO_CSD *pData, uint8_t len)
{
    uint8_t data[8] = {0};
    printf("CAN recv proc csd\n");
    if (NULL == buffer || NULL == pData || len > 8)
    {
        return -1;
    }

    pData->res[0] = buffer[0];
    pData->res[1] = buffer[1];
    pData->volt_l = buffer[2];
    pData->volt_h = buffer[3];

    pData->res1[0] = buffer[4];
    pData->res1[1] = buffer[5];
    pData->res1[2] = buffer[6];
    pData->res1[3] = buffer[7];

    data[0] = 0x01;
    analyseCMD_CSD_Data(data);
    return 0;
}

int can_recv_cdt(uint8_t *buffer, CAN_PRO_CDT *pData, uint8_t len)
{
    printf("CAN recv proc cdt\n");
    if (NULL == buffer || NULL == pData || len > 8)
    {
        return -1;
    }

    pData->fix_1 = buffer[0];
    pData->fix_5 = buffer[1];
    pData->fix_A = buffer[2];

    // res data
    pData->res[0] = buffer[3];
    pData->res[1] = buffer[4];
    pData->res[2] = buffer[5];
    pData->res[3] = buffer[6];
    pData->res[4] = buffer[7];

    // 调用接口产生pwm波形
    if (1 == can_pro_cp_bhm_status_get())
    {
        cp_config_pwm(1000, 50, true);
        printf("cdt recved and pwm start\n");
    }
    
    return 0;
}

#define CAN_CPM_CHANGE_CNT 6 //连续6此发生变化才算有效
#define CAN_CPM_CALC_CNT 50 //每隔50次强制发送一次cpm数据

typedef struct
{
    CPStatus status;
    uint32_t change_cnt;            //状态变化计数器    
    uint32_t calc_cnt;              //计算计数器，用于定时发送cpm数据
    uint32_t cp_value;              //cp值，扩大10倍后返回
} CAN_CPM_Struct;

static CAN_CPM_Struct gCpmData = {
    .status = UNKNWON,
    .change_cnt = 0,
    .calc_cnt = 0,
    .cp_value = 0
};

// 按照我司定义的cpm协议，计算cpm状态
static uint8_t can_pro_cpm_status_get(uint32_t cp_value)
{
    if (cp_value > 100)
    {
        return CPStatusA;
    }
    else if (cp_value > 75)
    {
        if (50 == cp_duty_get())
        {
            return CPStatusB2;
        }
        else
        {
            return CPStatusB1;
        }
    }
    else if (cp_value > 50)
    {
        if (50 == cp_duty_get())
        {
            return CPStatusC1;
        }
        else
        {
            return CPStatusC2;
        }
    }

    return CPStatusA;
}

static uint8_t can_pro_cpm_status_chk(void)
{ // 等待确认后编辑
    uint8_t ret = 0;
    uint32_t cp_value;
    double cp_double_value;
    CPStatus cur_status;

    gCpmData.calc_cnt++;

    //获取cp值，扩大10倍后返回
    cp_get_vol(&cp_double_value);
    cp_value = (cp_double_value * 10.0);
    cur_status = can_pro_cpm_status_get(cp_value);

    // 添加电压值监控
    //DEBUG_LOG(log_msgid, "[CP] Current voltage: %.2fV, status: %d", cp_double_value, cur_status);

    if (UNKNWON == gCpmData.status)
    {//特殊情况 unknown需要处理
        gCpmData.status = cur_status;
        gCpmData.cp_value =  cp_value;
        gCpmData.change_cnt = 0;
        gCpmData.calc_cnt = 0;
        INFO_LOG(log_msgid, "[CP] Initial status set to: %d, voltage: %.2fV", cur_status, cp_double_value);
        return 1;
    }   

    //有状态发生变化
    if (cur_status != gCpmData.status)
    {
        gCpmData.change_cnt++;
        DEBUG_LOG(log_msgid, "[CP] Status change detected: %d->%d, change_cnt:%d, voltage:%.2fV", 
                 gCpmData.status, cur_status, gCpmData.change_cnt, cp_double_value);
    }
    else
    {
        gCpmData.change_cnt = 0;
    }

    if (gCpmData.change_cnt >= CAN_CPM_CHANGE_CNT || gCpmData.calc_cnt >= CAN_CPM_CALC_CNT)
    {

         // 添加状态确认日志
        if (gCpmData.change_cnt >= CAN_CPM_CHANGE_CNT) {
            ERROR_LOG(log_msgid, "[CP] Frequent status changes! Final status: %d->%d, voltage: %.2fV", 
                      gCpmData.status, cur_status, cp_double_value);
        }
        gCpmData.status = cur_status;
        gCpmData.cp_value = cp_value;
        gCpmData.change_cnt = 0;
        gCpmData.calc_cnt = 0;
        ret = 1;
    }

    return ret;
}

// 根据实际情况检测cp状态，模拟发送cpm的数据
static void can_cpm_broad(void)
{
    uint32_t curSec;
    uint32_t value;
    uint8_t data[8];
    uint8_t flag = 0;

    flag = can_pro_cpm_status_chk();
    if (0 == flag)
    {
        return;
    }

    data[0] = (uint8_t)gCpmData.status;
    data[1] = 0x00;

    curSec = time(NULL);
    data[2] = curSec & 0xFF;
    data[3] = (curSec >> 8) & 0xFF;
    data[4] = (curSec >> 16) & 0xFF;
    data[5] = (curSec >> 24) & 0xFF;

    value = gCpmData.cp_value * 10;
    data[6] = value & 0xFF;
    data[7] = (value >> 8) & 0xFF;

    analyseCMD_CPM_Data(data);
}

/*定义一次充电的所有交互数据 evse和secc*/
typedef struct
{
    CAN_PRO_SECC_Data secc_data;
    CAN_PRO_EVSE_Data evse_data;
} CAN_PRO_Total_Data;

static CAN_PRO_Total_Data g_total_data = {0};

/*接收到的can数据进行处理*/
/**
 * @brief 处理接收到的CAN数据
 *
 * 该函数根据接收到的CAN消息ID调用相应的处理函数处理数据
 *
 * @param id CAN消息的ID
 * @param dlc CAN消息的数据长度
 * @param data CAN消息的数据指针
 */
void can_pro_recv_msg_proc(uint32_t id, uint8_t dlc, uint8_t *data)
{

    CAN_PRO_Total_Data *pTotData;

    pTotData = &g_total_data;

    if (dlc > 8 || NULL == data)
    {
        return;
    }

    switch (id)
    {
    case CAN_ID_CHM:
        can_recv_chm(data, &pTotData->evse_data.chm, dlc);
        break;

    case CAN_ID_CRM:
        can_recv_crm(data, &pTotData->evse_data.crm, dlc);
        break;

    case CAN_ID_CTS:
        can_recv_cts(data, &pTotData->evse_data.cts, dlc);
        break;

    case CAN_ID_CML:
        can_recv_cml(data, &pTotData->evse_data.cml, dlc);
        break;

    case CAN_ID_CRO:
        can_recv_cro(data, &pTotData->evse_data.cro, dlc);
        break;

    case CAN_ID_CCS:
        can_recv_ccs(data, &pTotData->evse_data.ccs, dlc);
        break;

    case CAN_ID_CST:
        can_recv_cst(data, &pTotData->evse_data.cst, dlc);
        break;

    case CAN_ID_CSD:
        can_recv_csd(data, &pTotData->evse_data.csd, dlc);
        break;

    case CAN_ID_CDT:
        can_recv_cdt(data, &pTotData->evse_data.cdt, dlc);
        break;
    default:
        break;
    }
}

/********************can 接收环形队列处理*****************************/

/**
 * @brief 尝试锁定状态互斥锁
 *
 * 该函数尝试锁定专业机器状态的互斥锁，若锁定失败，则按照指定次数重试，每次重试间隔10ms。
 *
 * @param cnt 最大重试次数
 *
 * @return 成功获取锁返回0，超过最大重试次数返回-1
 */
static int can_pro_try_lock(uint32_t cnt, pthread_mutex_t *pMutex)
{
    int retries = 0;
    int max_retries = cnt; // 将超时时间转换为重试次数上限

    while (pthread_mutex_trylock(pMutex) != 0)
    {
        if (++retries > max_retries)
        {
            return -1; // 超过最大重试次数，放弃获取锁，返回-1表示失败
        }
        usleep(10000); // 每次重试间隔10ms
    }
    return 0; // 成功获取锁，返回0表示成功
}

/**
 * @brief 向环形缓冲区输入CAN消息
 *
 * 将一个CAN消息输入到环形缓冲区中。
 *
 * @param id CAN消息的ID
 * @param dlc CAN消息的数据长度码（Data Length Code），范围在0到8之间
 * @param data 指向包含CAN消息数据的缓冲区
 */
static void can_pro_recv_queue_input(uint32_t id, uint8_t dlc, uint8_t *data)
{
    uint8_t next_wptr;
    CAN_SEND_RING *pr;

    pr = &gRecvBuf.msg;

    if (dlc > 8 || NULL == data)
    {
        return;
    }

    next_wptr = pr->wptr + 1;
    if (next_wptr == CAN_RING_NUM)
    {
        next_wptr = 0;
    }

    if (next_wptr != pr->rptr)
    {
        pr->queue[pr->wptr].can_id = id;
        pr->queue[pr->wptr].can_dlc = dlc;
        memcpy(pr->queue[pr->wptr].data, data, dlc);
        ++pr->wptr;
        if (pr->wptr >= CAN_RING_NUM)
        {
            pr->wptr = 0;
        }
    }
}

/**
 * @brief 从环形缓冲区中读取一条CAN消息
 *
 * 从环形缓冲区中读取一条CAN消息，并将其复制到指定的输出消息结构体中。
 *
 * @param pOutMsg 指向用于存储读取到的CAN消息的结构体的指针
 *
 * @return 读取到的CAN消息的长度，如果缓冲区为空，则返回-1
 */
static int can_pro_recv_queue_read(struct can_frame *pOutMsg)
{
    CAN_MSG_SEND *rx_msg;
    int i, len;
    CAN_SEND_RING *pr;

    pr = &gRecvBuf.msg;

    if (pr->rptr == pr->wptr)
    {
        return -1;
    }

    rx_msg = &(pr->queue[pr->rptr]);

    pOutMsg->can_id = rx_msg->can_id;
    pOutMsg->can_dlc = rx_msg->can_dlc;
    len = rx_msg->can_dlc;

    if (len > 8)
    {
        len = 8; // 限制最大长度为8字节
    }

    for (i = 0; i < len; ++i)
    {
        pOutMsg->data[i] = rx_msg->data[i];
    }

    ++pr->rptr;
    if (pr->rptr == CAN_RING_NUM)
    {
        pr->rptr = 0;
    }

    return rx_msg->can_dlc; // 返回读取的消息长度
}

/*接收队列的初始化*/
void can_pro_recv_msg_init(void)
{
    gRecvBuf.msg.wptr = 0;
    gRecvBuf.msg.rptr = 0;
    pthread_mutex_init(&gRecvBuf.lock, NULL);
    gRecvBuf.order = 0;
}

/*将接收到的can消息插入到环形队列里*/
void can_pro_recv_msg_input(uint32_t id, uint8_t dlc, uint8_t *data)
{
    //pthread_mutex_t mutex = gRecvBuf.lock;

    pthread_mutex_lock(&gRecvBuf.lock); // 插入的时候必须获取到信号量
    can_pro_recv_queue_input(id, dlc, data);
    gRecvBuf.order++;
    pthread_mutex_unlock(&gRecvBuf.lock);
}

/*从队列里获取数据*/
int can_pro_recv_msg_get(CAN_MSG_SEND *pOut)
{
    struct can_frame CanFram = {0};
    int ret;
    static uint32_t g_ord = 0;

    if (NULL == pOut)
    {
        return -1;
    }

    if (g_ord == gRecvBuf.order)
    { // 相同序号
        return -1;
    }

    ret = can_pro_try_lock(5, &gRecvBuf.lock);

    if (ret < 0)
    { // 无法获取锁
        return -1;
    }

    ret = can_pro_recv_queue_read(&CanFram);

    if (ret < 0)
    { // 缓存无数据，刷新外部序号指针，解锁信号量
        g_ord = gRecvBuf.order;
        pthread_mutex_unlock(&gRecvBuf.lock);
        return -1;
    }

    pOut->can_id = CanFram.can_id;
    pOut->can_dlc = CanFram.can_dlc;

    for (uint8_t i = 0; i < CanFram.can_dlc; i++)
    {
        pOut->data[i] = CanFram.data[i];
    }

    pthread_mutex_unlock(&gRecvBuf.lock);
    return CanFram.can_dlc;
}

/*插入到待发送的数据区中*/
static void can_pro_send_queue_input(uint32_t id, uint8_t dlc, uint8_t *data)
{
    uint8_t next_wptr;
    CAN_SEND_RING *pr;

    pr = &gSendBuf.msg;

    if (dlc > CAN_SEND_LEN_MAX || NULL == data)
    {
        return;
    }

    next_wptr = pr->wptr + 1;
    if (next_wptr == CAN_RING_NUM)
    {
        next_wptr = 0;
    }

    if (next_wptr != pr->rptr)
    {
        pr->queue[pr->wptr].can_id = id;
        pr->queue[pr->wptr].can_dlc = dlc;
        memcpy(pr->queue[pr->wptr].data, data, dlc);
        ++pr->wptr;
        if (pr->wptr >= CAN_RING_NUM)
        {
            pr->wptr = 0;
        }
    }
}

/**
 * @brief 从环形缓冲区中读取一条CAN消息
 *
 * 从环形缓冲区中读取一条CAN消息，并将其复制到指定的输出消息结构体中。
 *
 * @param pOutMsg 指向用于存储读取到的CAN消息的结构体的指针
 *
 * @return 读取到的CAN消息的长度，如果缓冲区为空，则返回-1
 */
static int can_pro_send_queue_read(CAN_MSG_SEND *pOutMsg)
{
    CAN_MSG_SEND *pMsg;
    int i, len;
    CAN_SEND_RING *pr;

    pr = &gSendBuf.msg;

    if (pr->rptr == pr->wptr)
    {
        return -1;
    }

    pMsg = &(pr->queue[pr->rptr]);

    pOutMsg->can_id = pMsg->can_id;
    pOutMsg->can_dlc = pMsg->can_dlc;
    len = pMsg->can_dlc;

    if (len > CAN_SEND_LEN_MAX)
    {
        len = CAN_SEND_LEN_MAX; // 限制最大长度为8字节
    }

    for (i = 0; i < len; ++i)
    {
        pOutMsg->data[i] = pMsg->data[i];
    }

    ++pr->rptr;
    if (pr->rptr == CAN_RING_NUM)
    {
        pr->rptr = 0;
    }

    return pMsg->can_dlc; // 返回读取的消息长度
}

/*发送队列的初始化*/
void can_pro_send_msg_init(void)
{
    gSendBuf.msg.wptr = 0;
    gSendBuf.msg.rptr = 0;
    pthread_mutex_init(&gSendBuf.lock, NULL);
    gSendBuf.order = 0;
}

void can_pro_send_msg_input(CAN_MSG_SEND *pIn)
{
    //pthread_mutex_t mutex = gSendBuf.lock;

    if (NULL == pIn)
    {
        return;
    }

    pthread_mutex_lock(&gSendBuf.lock); // 插入的时候必须获取到信号量
    can_pro_send_queue_input(pIn->can_id, pIn->can_dlc, pIn->data);
    gSendBuf.order++;
    pthread_mutex_unlock(&gSendBuf.lock);

    // 使用trylock避免长时间阻塞
    // int ret = pthread_mutex_trylock(&gSendBuf.lock);
    // if (ret != 0)
    // {
    //     // 锁被占用，记录警告但不阻塞
    //     static int warn_count = 0;
    //     if (warn_count++ % 100 == 0)
    //     {
    //         DEBUG_LOG(log_msgid, "[CAN] Send queue lock busy");
    //     }
    //     return;
    // }

    // can_pro_send_queue_input(pIn->can_id, pIn->can_dlc, pIn->data);
    // gSendBuf.order++;
    // pthread_mutex_unlock(&gSendBuf.lock);
}

void can_pro_send_msg_run(int s)
{
    int ret;
    static uint32_t g_ord = 0;
    CAN_MSG_SEND Can;
    struct can_frame CanFram;

    // 检查是否有cp状态变化，内部出发变化
    can_cpm_broad();

    // 先检查是否存在发送多帧报文时接收到的数据,并进行处理
    ret = can_pro_recv_queue_read(&CanFram);
    if (ret > 0)
    {
        can_pro_recv_msg_proc(CanFram.can_id, CanFram.can_dlc, (uint8_t *)CanFram.data);
    }

    if (g_ord == gSendBuf.order)
    {
        return;
    }

    ret = can_pro_try_lock(5, &gSendBuf.lock);
    if (ret < 0)
    {
        return;
    }

    ret = can_pro_send_queue_read(&Can);
    uint32_t current_order = gSendBuf.order; // 保存当前序号
    pthread_mutex_unlock(&gSendBuf.lock);    //  立即释放锁！
    if (ret < 0)
    { // 缓存无数据，刷新变量计数值，解锁信号量
        g_ord = gSendBuf.order;
        pthread_mutex_unlock(&gSendBuf.lock);
        return;
    }

    if (Can.can_dlc > 8)
    { // 多帧发送
        if (CAN_ID_BCP == Can.can_id || CAN_ID_BCS == Can.can_id)
        { // 只有bcp和bcs会多帧发送
            can_send_long_message(s, Can.can_id, CAN_ADDR_SECC, CAN_ADDR_EVSE, Can.data, Can.can_dlc);
        }
    }
    else
    {
        can_send_extern_frame(s, Can.can_id, Can.data, Can.can_dlc);
    }

    //pthread_mutex_unlock(&gSendBuf.lock);
}

// 按照吉瓦特的协议里BHM部分生成CP的工作状态，0-12V 1-9V 2-9V/5%  3-6V/5%
uint8_t can_pro_cp_bhm_status_get(void)
{
    if (gCpmData.cp_value > 100)
    {
        return 0;
    }
    else if (gCpmData.cp_value > 75)
    {
        if (50 == cp_duty_get())
        {
            return 2;
        }
        else
        {
            return 1;
        }
    }
    else if (gCpmData.cp_value > 50)
    {
        if (50 == cp_duty_get())
        {
            return 3;
        }
        else
        {
            return 0;
        }
    }

    return 0;
}


