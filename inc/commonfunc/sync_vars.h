/*
 * @Author: yang 1458900080@qq.com
 * @Date: 2025-12-24 10:58:20
 * @LastEditors: yang 1458900080@qq.com
 * @LastEditTime: 2025-12-24 10:58:44
 * @FilePath: \Can_CCu_To_V2g\inc\commonfunc\sync_vars.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*******************************************************************************
 * 文件名称：sync_vars.h
 * 功能描述：线程同步变量访问接口，替代原子操作
 * ----------------------------------文件记录----------------------------------
 * 版本    日期       作者    描述
 * V1.00  2025-11-11          替代原子操作的线程安全访问接口
 *******************************************************************************/

#ifndef SYNC_VARS_H
#define SYNC_VARS_H

#include <stdint.h>
#include <pthread.h>

// 定义同步变量的值
#define FLAG_RESET 0
#define FLAG_SET 1

// CCU停止完成标志
#define CCU_STOP_FIN 1
// EVCC停止完成标志
#define EVCC_STOP_FIN 2
// 停止完成标志
#define STOP_DONE 3

/**
 * @brief 获取timer_expired的值（线程安全）
 * @return timer_expired的当前值
 */
uint8_t get_timer_expired(void);

/**
 * @brief 设置timer_expired的值（线程安全）
 * @param value 要设置的值
 */
void set_timer_expired(uint8_t value);

/**
 * @brief 获取syn_charge_over的值（线程安全）
 * @return syn_charge_over的当前值
 */
uint8_t get_syn_charge_over(void);

/**
 * @brief 设置syn_charge_over的值（线程安全）
 * @param value 要设置的值
 */
void set_syn_charge_over(uint8_t value);

/**
 * @brief 重置所有同步变量到初始状态
 * @note 用于充电完成后的状态清理或程序启动时的初始化
 */
void reset_all_sync_vars(void);

#endif // SYNC_VARS_H
