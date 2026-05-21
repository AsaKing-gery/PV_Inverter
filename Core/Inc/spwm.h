/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : spwm.h
  * @brief          : SPWM (Sinusoidal PWM) generator for PV Micro-Inverter
  *                   Update rate: 50µs in TIM8 update interrupt
  *                   Output: TIM8 CCR1/CCR2 complementary PWM
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

#ifndef __SPWM_H
#define __SPWM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* SPWM参数 */
#define SPWM_CARRIER_FREQ           20000       /* 载波频率 20kHz (TIM8配置) */
#define SPWM_MODULATION_FREQ        50          /* 调制波频率 50Hz */
#define SPWM_UPDATE_PERIOD_US       50          /* 更新周期 50µs */

/* 正弦表大小 (360度/256点 = 1.4度/点) */
#define SPWM_SINE_TABLE_SIZE        256

/* 调制比范围 (0-100%) */
#define SPWM_MODULATION_MIN         0
#define SPWM_MODULATION_MAX         95          /* 最大95%，留死区余量 */

/* 相位偏移 (互补输出) */
#define SPWM_PHASE_SHIFT            180         /* 180度互补 */

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/**
  * @brief  SPWM控制器结构体
  */
typedef struct
{
  /* 调制参数 */
  uint8_t modulationIndex;         /* 调制比 (0-100) */
  uint16_t phaseAccumulator;       /* 相位累加器 (0-65535对应0-360度) */
  uint16_t phaseStep;              /* 相位步进 */

  /* 输出状态 */
  uint16_t ccr1;                   /* TIM8 CCR1值 */
  uint16_t ccr2;                   /* TIM8 CCR2值 */

  /* 运行状态 */
  uint8_t isRunning;               /* 运行标志 */
  uint32_t cycleCount;             /* 周期计数 */

} SPWM_Controller_t;

/* USER CODE END ET */

/* Exported variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/**
  * @brief  SPWM控制器实例
  */
extern SPWM_Controller_t g_spwm;

/**
  * @brief  正弦表 (256点)
  * @note   范围: 32768-72757 (16位精度，32位存储)
  */
extern const uint32_t spwm_sineTable[SPWM_SINE_TABLE_SIZE];

/* USER CODE END EV */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/**
  * @brief  初始化SPWM控制器
  * @param  None
  * @retval None
  * @note   初始化正弦表和相位参数
  */
void SPWM_Init(void);

/**
  * @brief  启动SPWM输出
  * @param  None
  * @retval None
  * @note   使能TIM8 PWM输出
  */
void SPWM_Start(void);

/**
  * @brief  停止SPWM输出
  * @param  None
  * @retval None
  * @note   关闭TIM8 PWM输出，输出低电平
  */
void SPWM_Stop(void);

/**
  * @brief  设置调制比
  * @param  modulation: 调制比值 (0-100)
  * @retval None
  * @note   自动限幅到 0-95%
  */
void SPWM_SetModulation(uint8_t modulation);

/**
  * @brief  获取当前调制比
  * @param  None
  * @retval 调制比值 (0-100)
  */
uint8_t SPWM_GetModulation(void);

/**
  * @brief  SPWM更新函数 (在TIM8中断中调用)
  * @param  None
  * @retval None
  * @note   每50µs执行一次
  * @note   查正弦表，更新TIM8 CCR1/CCR2
  */
void SPWM_Update(void);

/**
  * @brief  获取当前相位 (0-359度)
  * @param  None
  * @retval 相位值 (度)
  */
uint16_t SPWM_GetPhase(void);

/**
  * @brief  获取输出频率
  * @param  None
  * @retval 频率值 (Hz)
  */
float SPWM_GetFrequency(void);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __SPWM_H */
