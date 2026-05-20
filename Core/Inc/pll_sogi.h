/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : pll_sogi.h
  * @brief          : SOGI-PLL (Second Order Generalized Integrator PLL)
  *                   Software Phase-Locked Loop for grid synchronization
  *                   Input: TIM2 input capture (zero-crossing detection)
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

#ifndef __PLL_SOGI_H
#define __PLL_SOGI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* PLL参数 */
#define PLL_GRID_FREQ_NOMINAL       50.0f       /* 电网标称频率 50Hz */
#define PLL_GRID_FREQ_MIN           45.0f       /* 最低跟踪频率 45Hz */
#define PLL_GRID_FREQ_MAX           55.0f       /* 最高跟踪频率 55Hz */

/* SOGI参数 */
#define PLL_SOGI_K                  1.414f      /* SOGI阻尼系数 (sqrt(2)) */

/* PI控制器参数 */
#define PLL_KP                      100.0f      /* 比例增益 */
#define PLL_KI                      2000.0f     /* 积分增益 */

/* 更新周期 (TIM2捕获中断周期，约10ms) */
#define PLL_UPDATE_PERIOD_MS        10

/* 锁相状态 */
#define PLL_STATE_UNLOCKED          0           /* 未锁定 */
#define PLL_STATE_LOCKING           1           /* 锁定中 */
#define PLL_STATE_LOCKED            2           /* 已锁定 */

/* 锁定判断阈值 */
#define PLL_LOCK_THRESHOLD_HZ       0.5f        /* 频率误差阈值 */
#define PLL_LOCK_THRESHOLD_DEG      5.0f        /* 相位误差阈值 */
#define PLL_LOCK_COUNT              50          /* 连续满足条件次数 */

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/**
  * @brief  SOGI结构体 (二阶广义积分器)
  */
typedef struct
{
  float alpha;                 /* 正交输出 alpha (同相) */
  float beta;                  /* 正交输出 beta (90度滞后) */
  float alpha_prev;            /* 上一次的alpha */
  float beta_prev;             /* 上一次的beta */
  float k;                     /* 阻尼系数 */
} SOGI_Filter_t;

/**
  * @brief  PI控制器结构体
  */
typedef struct
{
  float kp;                    /* 比例增益 */
  float ki;                    /* 积分增益 */
  float integral;              /* 积分项 */
  float output;                /* 输出 */
  float outputMax;             /* 输出上限 */
  float outputMin;             /* 输出下限 */
} PI_Controller_t;

/**
  * @brief  PLL控制器结构体
  */
typedef struct
{
  /* SOGI滤波器 */
  SOGI_Filter_t sogi;

  /* PI控制器 */
  PI_Controller_t pi;

  /* 输入信号 */
  float input;                 /* 输入电压 (归一化) */
  float inputFreq;             /* 输入信号频率 (从TIM2测量) */

  /* PLL输出 */
  float theta;                 /* 输出相位角 (0-2π rad) */
  float omega;                 /* 输出角频率 (rad/s) */
  float freq;                  /* 输出频率 (Hz) */
  float sinTheta;              /* sin(theta) */
  float cosTheta;              /* cos(theta) */

  /* 误差信号 */
  float phaseError;            /* 相位误差 */
  float freqError;             /* 频率误差 */

  /* 状态 */
  uint8_t state;               /* 锁相状态 */
  uint32_t lockCount;          /* 锁定计数器 */

  /* TIM2捕获数据 */
  uint32_t capturePeriod;      /* 捕获周期 (TIM2计数) */
  uint32_t capturePrev;        /* 上一次捕获值 */
  uint8_t captureValid;        /* 捕获数据有效标志 */

} PLL_Controller_t;

/* USER CODE END ET */

/* Exported variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/**
  * @brief  PLL控制器实例
  */
extern PLL_Controller_t g_pll;

/* USER CODE END EV */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/**
  * @brief  初始化PLL控制器
  * @param  None
  * @retval None
  */
void PLL_Init(void);

/**
  * @brief  处理TIM2输入捕获中断
  * @param  captureValue: TIM2捕获寄存器值
  * @retval None
  * @note   在TIM2捕获中断中调用
  */
void PLL_TIM2_CaptureCallback(uint32_t captureValue);

/**
  * @brief  执行PLL更新 (SOGI-PLL算法)
  * @param  gridVoltage: 电网电压采样值 (V)
  * @retval None
  * @note   建议每10ms调用一次
  */
void PLL_Update(float gridVoltage);

/**
  * @brief  获取PLL输出相位
  * @param  None
  * @retval 相位角 (0-360度)
  */
float PLL_GetPhaseDegree(void);

/**
  * @brief  获取PLL输出频率
  * @param  None
  * @retval 频率 (Hz)
  */
float PLL_GetFrequency(void);

/**
  * @brief  获取PLL输出sin值
  * @param  None
  * @retval sin(theta) (-1.0 ~ 1.0)
  */
float PLL_GetSinTheta(void);

/**
  * @brief  获取PLL输出cos值
  * @param  None
  * @retval cos(theta) (-1.0 ~ 1.0)
  */
float PLL_GetCosTheta(void);

/**
  * @brief  获取PLL锁定状态
  * @param  None
  * @retval 状态值 (PLL_STATE_UNLOCKED/LOCKING/LOCKED)
  */
uint8_t PLL_GetState(void);

/**
  * @brief  检查PLL是否已锁定
  * @param  None
  * @retval true: 已锁定, false: 未锁定
  */
bool PLL_IsLocked(void);

/**
  * @brief  重置PLL控制器
  * @param  None
  * @retval None
  */
void PLL_Reset(void);

/**
  * @brief  从TIM2捕获计算电网频率
  * @param  None
  * @retval 频率 (Hz)
  * @note   用于初始频率估计
  */
float PLL_CalcFreqFromCapture(void);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __PLL_SOGI_H */
