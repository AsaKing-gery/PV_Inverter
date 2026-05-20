/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : mppt.h
  * @brief          : MPPT (Maximum Power Point Tracking) algorithm for PV Micro-Inverter
  *                   Algorithm: Perturb and Observe (P&O)
  *                   Execution: 100ms interval
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MPPT_H
#define __MPPT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* MPPT执行周期 (ms) */
#define MPPT_PERIOD_MS              100

/* PWM占空比限幅 */
#define MPPT_DUTY_MIN               5       /* 最小占空比 5% */
#define MPPT_DUTY_MAX               90      /* 最大占空比 90%，防止自举电容充电不足 */
#define MPPT_DUTY_STEP              1       /* 扰动步进 1% */

/* MPPT状态 */
#define MPPT_STATE_SCANNING         0       /* 扫描状态：寻找最大功率点 */
#define MPPT_STATE_TRACKING         1       /* 跟踪状态：扰动观察 */
#define MPPT_STATE_LOCKED           2       /* 锁定状态：功率稳定 */

/* 功率变化阈值 (用于判断是否锁定) */
#define MPPT_POWER_LOCK_THRESHOLD   0.5f    /* 功率变化小于0.5W视为稳定 */
#define MPPT_LOCK_COUNT_THRESHOLD   10      /* 连续10次稳定进入锁定状态 */

/* 扫描参数 */
#define MPPT_SCAN_STEP              5       /* 扫描步进 5% */
#define MPPT_SCAN_START_DUTY        10      /* 扫描起始占空比 10% */

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/**
  * @brief  MPPT算法结构体
  */
typedef struct
{
  /* 当前值 */
  float voltage;               /* 当前光伏电压 (V) */
  float current;               /* 当前光伏电流 (A) */
  float power;                 /* 当前光伏功率 (W) */
  uint8_t duty;                /* 当前PWM占空比 (%) */

  /* 历史值 */
  float lastVoltage;           /* 上一次电压 (V) */
  float lastCurrent;           /* 上一次电流 (A) */
  float lastPower;             /* 上一次功率 (W) */
  uint8_t lastDuty;            /* 上一次占空比 (%) */

  /* 扰动方向 */
  int8_t perturbDirection;     /* +1: 增加占空比, -1: 减小占空比 */

  /* 状态 */
  uint8_t state;               /* MPPT状态 */
  uint8_t lockCount;           /* 锁定计数器 */

  /* 扫描参数 */
  uint8_t scanDuty;            /* 扫描当前占空比 */
  float maxPower;              /* 扫描到的最大功率 */
  uint8_t maxPowerDuty;        /* 最大功率对应的占空比 */

} MPPT_Controller_t;

/* USER CODE END ET */

/* Exported variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/**
  * @brief  MPPT控制器实例
  */
extern MPPT_Controller_t g_mppt;

/* USER CODE END EV */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/**
  * @brief  初始化MPPT控制器
  * @param  None
  * @retval None
  * @note   设置初始状态为扫描模式
  */
void MPPT_Init(void);

/**
  * @brief  执行MPPT算法 (P&O扰动观察法)
  * @param  voltage: 当前光伏电压 (V)
  * @param  current: 当前光伏电流 (A)
  * @retval None
  * @note   每100ms调用一次
  * @note   自动更新PWM占空比
  */
void MPPT_Execute(float voltage, float current);

/**
  * @brief  设置PWM占空比
  * @param  duty: 占空比值 (0-100)
  * @retval None
  * @note   自动限幅到 5%-90%
  * @note   更新TIM1 CCR1寄存器
  */
void MPPT_SetDuty(uint8_t duty);

/**
  * @brief  获取当前PWM占空比
  * @param  None
  * @retval 占空比值 (0-100)
  */
uint8_t MPPT_GetDuty(void);

/**
  * @brief  获取当前MPPT状态
  * @param  None
  * @retval 状态值 (MPPT_STATE_SCANNING/TRACKING/LOCKED)
  */
uint8_t MPPT_GetState(void);

/**
  * @brief  获取当前功率
  * @param  None
  * @retval 功率值 (W)
  */
float MPPT_GetPower(void);

/**
  * @brief  获取MPPT效率
  * @param  None
  * @retval 效率值 (0-100)
  * @note   基于当前功率与理论最大功率的比值
  */
float MPPT_GetEfficiency(void);

/**
  * @brief  强制进入扫描模式
  * @param  None
  * @retval None
  * @note   用于启动或环境突变时重新寻找MPP
  */
void MPPT_ForceScan(void);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __MPPT_H */
