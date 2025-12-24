#include "ChargeControlCCu_Task.h"
#include "ChargeControlCCu_SM.h"
#include "ChargeControlCCu_Data.h"
#include "ChargeControlCCu.h"
#include "commonfunc.h"
#include "log.h"
#include "secc_ccu_sync.h"
#include "TimeOut.h"
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include "ChargeControlCCu_Share.h"
#include "can_protocol.h"
#include "led.h"
#include "cp.h"

#if defined(__linux__)
#include <execinfo.h> // backtrace, backtrace_symbols_fd
#include <ucontext.h>
#endif



static pthread_t ccu_thread_id;
static volatile int ccu_running = 0;
static int g_can_sock = -1; // Global CAN socket


// CAN Initialization Function
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
    char command[64];
    snprintf(command, sizeof(command), "ip link set %s down", can_interface);
    system(command);
    snprintf(command, sizeof(command), "ip link set %s type can bitrate 250000", can_interface);
    system(command);
    snprintf(command, sizeof(command), "ip link set %s up", can_interface);
    system(command);

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

    // Set non-blocking mode
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        perror("F_GETFL failed");
        close(s);
        return -1;
    }
    if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("F_SETFL failed");
        close(s);
        return -1;
    }

    return s;
}

static long long get_current_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}


int ccu_init(void)
{
    ccu_running = 0;
    g_can_sock = -1;

    set_timer_expired(FLAG_RESET);
    memset((void *)(&CCu_Data), 0, sizeof(struct CCu_Charge_Data));

    // Init CAN
    if ((g_can_sock = can_init(CAN_PORT_NAME)) < 0)
    {
        ERROR_LOG(log_msgid, "[CCU] CAN initialization failed");

    }

    // Init CP and LED
    if (cp_init() < 0)
    {
        ERROR_LOG(log_msgid, "[CCU] CP init failed");
        return -1;
    }
    if (cp_config_pwm(1000, 1000, true) < 0)
    {
        ERROR_LOG(log_msgid, "[CCU] CP config failed");
        return -1;
    }
    if (cp_enable_pwm(true) < 0)
    {
        ERROR_LOG(log_msgid, "[CCU] CP enable failed");
        return -1;
    }

    led_init();
    can_pro_recv_msg_init();
    can_pro_send_msg_init();
    
    return 0;
}

int ccu_start(void)
{
    if (ccu_running)
    {
        printf("[CCU] Task already running\n");
        return -1;
    }

    ccu_running = 1;
    int ret = pthread_create(&ccu_thread_id, NULL, Charge_Ccu_task, NULL);
    if (ret != 0)
    {
        printf("[CCU] Failed to create thread: %s\n", strerror(ret));
        ccu_running = 0;
        return -1;
    }

    return 0;
}

void ccu_release(void)
{
    if (!ccu_running) return;

    printf("[CCU] Stopping task...\n");
    ccu_running = 0;
    pthread_join(ccu_thread_id, NULL);
    printf("[CCU] Task stopped\n");
}

/*与ccu侧的通讯充电控制任务*/
void *Charge_Ccu_task(void *arg)
{
    struct can_frame canFrame;
    long long last_ccu_run = 0;
    

    printf("[CCU] Merged Task Started. Waiting for messages...\n");

    while (ccu_running)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = 0;
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms timeout

        if (g_can_sock >= 0) {
            FD_SET(g_can_sock, &readfds);
            if (g_can_sock > max_fd) max_fd = g_can_sock;
        }

        int ret = select(max_fd + 1, &readfds, NULL, NULL, &tv);

        if (ret > 0)
        {
            // Handle CAN
            if (g_can_sock >= 0 && FD_ISSET(g_can_sock, &readfds))
            {
                int nbytes = read(g_can_sock, &canFrame, sizeof(struct can_frame));
                if (nbytes > 0)
                {
                    canFrame.can_id &= ~CAN_EFF_FLAG;
                    can_pro_recv_msg_proc(canFrame.can_id, canFrame.can_dlc, canFrame.data);
                    led_simple_can_flash();
                }
                else
                {
                    if (nbytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                    {
                        // Resource temporarily unavailable, retry later
                    }
                    else
                    {
                        ERROR_LOG(log_msgid, "[CCU] CAN Read Error/Re-init: %s", strerror(errno));
                        close(g_can_sock);
                        g_can_sock = can_init(CAN_PORT_NAME);
                    }
                }
            }

        }
        else if (ret < 0 && errno != EINTR)
        {
             ERROR_LOG(log_msgid, "[CCU] Select Error: %s", strerror(errno));
        }

        long long now = get_current_time_ms();
        if (now - last_ccu_run >= (DATA_SEND_TIME / 1000)) 
        {
            // Sync EVCC Share Data
            Sync_EVCC_Share_Data_To_CCu(CCu_Data);

            // Loop through state machine functions
            const CHARGE_TASK *task = Charge_Fun;
            while (task->fun != NULL)
            {
                if (task->opt == CCu_Data->opt)
                {
                    task->fun(CCu_Data);
                }
                task++;
            }
            last_ccu_run = now;
        }

        // Run CAN Send State Machine
        if (g_can_sock >= 0) {
            can_pro_send_msg_run(g_can_sock);
        }
    }

    if (g_can_sock >= 0) {
        close(g_can_sock);
        g_can_sock = -1;
    }
    pthread_exit(NULL);
}