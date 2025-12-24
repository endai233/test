/*
 * @Author: yang 1458900080@qq.com
 * @Date: 2025-12-24 10:57:50
 * @LastEditors: yang 1458900080@qq.com
 * @LastEditTime: 2025-12-24 10:58:13
 * @FilePath: \Can_CCu_To_V2g\src\commonfunc\sync_vars.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*******************************************************************************
 * 文件名称：sync_vars.c
 * 功能描述：线程同步变量访问实现，替代原子操作
 * ----------------------------------文件记录----------------------------------
 * 版本    日期       作者    描述
 * V1.00  2025-11-11          替代原子操作的线程安全访问实现
 *******************************************************************************/

#include "sync_vars.h"

// 内部变量（不直接暴露给外部）
static uint8_t timer_expired_internal = 0;
static uint8_t syn_charge_over_internal = 0;

// 保护变量的互斥锁
static pthread_mutex_t timer_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t charge_sync_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 获取timer_expired的值（线程安全）
 */
uint8_t get_timer_expired(void)
{
    uint8_t value;
    pthread_mutex_lock(&timer_lock);
    value = timer_expired_internal;
    pthread_mutex_unlock(&timer_lock);
    return value;
}

/**
 * @brief 设置timer_expired的值（线程安全）
 */
void set_timer_expired(uint8_t value)
{
    pthread_mutex_lock(&timer_lock);
    timer_expired_internal = value;
    pthread_mutex_unlock(&timer_lock);
}

/**
 * @brief 获取syn_charge_over的值（线程安全）
 */
uint8_t get_syn_charge_over(void)
{
    uint8_t value;
    pthread_mutex_lock(&charge_sync_lock);
    value = syn_charge_over_internal;
    pthread_mutex_unlock(&charge_sync_lock);
    return value;
}

/**
 * @brief 设置syn_charge_over的值（线程安全）
 */
void set_syn_charge_over(uint8_t value)
{
    pthread_mutex_lock(&charge_sync_lock);
    syn_charge_over_internal = value;
    pthread_mutex_unlock(&charge_sync_lock);
}

/**
 * @brief 重置所有同步变量到初始状态
 * @note 用于充电完成后的状态清理或程序启动时的初始化
 */
void reset_all_sync_vars(void)
{
    pthread_mutex_lock(&timer_lock);
    timer_expired_internal = FLAG_RESET;
    pthread_mutex_unlock(&timer_lock);
    
    pthread_mutex_lock(&charge_sync_lock);
    syn_charge_over_internal = FLAG_RESET;
    pthread_mutex_unlock(&charge_sync_lock);
}
