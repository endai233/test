/****************************************Copyright (c)**************************************************
;**                                珠海兴诺能源技术有限公司
;**
;**
;**--------------File Info-------------------------------------------------------------------------------
;** 文件名:    main.c
;** 版本:     1.0
;** 概述:    主函数接口
;** 索引:
;**------------------------------------------------------------------------------------------------------
;** 作者: weiq
;** 日期: 2025.07.14
;**------------------------------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "can_protocol.h"
#include "zcl_sdk_protocol.h"
#include "led.h"
#include "cp.h"
#include "zlog.h"

/**
 * 初始化CAN接口
 * @param can_interface 接口名称，如"can0"
 * @return 成功返回套接字描述符，失败返回-1
 */
static int can_init(const char *can_interface)
{
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;

    if (NULL == can_interface)
    {
        perror("Invalid CAN interface");
        return -1;
    }

    // 配置波特率并启用CAN接口
    char command[48];
    snprintf(command, sizeof(command), "ip link set %s down", can_interface);
    if (system(command) != 0)
    {
        perror("Failed to set CAN interface down");
        return -1;
    }
    snprintf(command, sizeof(command), "ip link set %s type can bitrate 250000", can_interface);
    if (system(command) != 0)
    {
        perror("Failed to set CAN interface bitrate");
        return -1;
    }
    snprintf(command, sizeof(command), "ip link set %s up", can_interface);
    if (system(command) != 0)
    {
        perror("Failed to set CAN interface up");
        return -1;
    }

    // 创建CAN套接字
    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    // 指定CAN接口
    strcpy(ifr.ifr_name, can_interface);
    ioctl(s, SIOCGIFINDEX, &ifr);

    // 配置CAN地址
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    // 绑定套接字到CAN接口
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind failed");
        close(s);
        return -1;
    }

    return s;
}

/**
 * 使用select实现CAN接收管理
 * @param s CAN套接字描述符
 * @param timeout_ms 超时时间（毫秒），-1表示永久等待
 * @return 成功返回接收到的帧数，失败返回-1，超时返回0
 */
static int can_recv(int s, int timeout_ms, struct can_frame *pframeOut)
{
    fd_set readfds;
    struct timeval timeout;
    struct can_frame frame;
    int nfds = s + 1;
    int ret;

    if (NULL == pframeOut)
    {
        // 输入参数错误
        return 0;
    }

    // 初始化文件描述符集
    FD_ZERO(&readfds);
    FD_SET(s, &readfds);

    // 设置超时时间
    if (timeout_ms >= 0)
    {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
    }

    // 使用select等待数据
    ret = select(nfds, &readfds, NULL, NULL,
                 (timeout_ms >= 0) ? &timeout : NULL);

    if (ret < 0)
    {
        perror("Select failed");
        return -1;
    }
    else if (ret == 0)
    {
        // 超时
        return 0;
    }

    if (NULL == pframeOut)
    {
        // 输入参数错误
        return 0;
    }

    // 检查CAN套接字是否有数据可读
    if (FD_ISSET(s, &readfds))
    {
        // 读取CAN帧
        if (read(s, &frame, sizeof(struct can_frame)) < 0)
        {
            perror("Read failed");
            return -1;
        }

        pframeOut->can_id = frame.can_id & (~CAN_EFF_FLAG);
        pframeOut->can_dlc = frame.can_dlc;

        // 处理接收到的CAN帧
        //printf("Received CAN frame: ID=0x%03X, DLC=%d, Data=",
        //       pframeOut->can_id, pframeOut->can_dlc);
        for (int i = 0; i < frame.can_dlc; i++)
        {

            // printf("0x%02X ", frame.data[i]);
            pframeOut->data[i] = frame.data[i];
        }
        //printf("\n");

        return 1; // 成功接收1帧
    }

    return 0; // 未收到数据
}

/**
 * 关闭CAN接口
 * @param s 套接字描述符
 */
static void can_close(int s)
{
    close(s);
}

/**
 * @brief 接收并处理CAN消息的线程函数
 *
 * 该函数用于在单独的线程中接收并处理CAN消息。通过调用can_recv函数以非阻塞方式接收CAN消息，
 * 并在接收到消息后调用can_recv_proc函数进行处理。同时，该函数还负责在接收到消息后运行状态机。
 *
 * @param arg 指向传递给线程的参数的指针，本函数未使用
 *
 * @return void* 线程执行完毕后的返回值，本函数返回0
 */
void *can_task(void *arg)
{
    int s;
    int ret;
    struct can_frame canFrame;

    // 初始化CAN接口
    if ((s = can_init(CAN_PORT_NAME)) < 0)
    {
        fprintf(stderr, "CAN initialization failed\n");
        return NULL;
    }

    // 控制导引电路初始化
    ret = cp_init();
    if (ret < 0)
    {
        fprintf(stderr, "control pilot initialization failed\n");
        return NULL;
    }
    //设置输出模式是1KHz  100%duty 和高电平
    ret = cp_config_pwm(1000, 1000, true);
    if (ret < 0)
    {
        fprintf(stderr, "control pilot configure failed\n");
        return NULL;
    }
    //使能PWM 输出
    ret = cp_enable_pwm(true);
    if (ret < 0)
    {
        fprintf(stderr, "set pwm enable failed\n");
        return NULL;
    }

    // 初始化led
    led_init();

    // 初始化can的接收和发送缓存
    can_pro_recv_msg_init();
    can_pro_send_msg_init();

    printf("CAN receiver started. Waiting for messages...\n");

    // 主循环，使用select非阻塞接收CAN消息
    while (1)
    {
        int result = can_recv(s, 10, &canFrame); // 超时时间10ms

        if (result < 0)
        {
            fprintf(stderr, "Error in CAN reception\n");
            // 关闭CAN接口
            can_close(s);
            // 重新初始化
            s = can_init(CAN_PORT_NAME);
            continue;
        }
        else if (result > 0)
        {
            // 接收到数据，需要处理一次
            can_pro_recv_msg_proc(canFrame.can_id, canFrame.can_dlc, canFrame.data);

            // 闪烁灯
            led_simple_can_flash();
        }
        // 接受到数据处理后状态机的处理
        can_pro_send_msg_run(s);
    }

    // 关闭CAN接口
    can_close(s);

    return NULL;
}
