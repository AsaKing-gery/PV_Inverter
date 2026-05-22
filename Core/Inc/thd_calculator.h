/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : thd_calculator.h
  * @brief          : THD (Total Harmonic Distortion) Calculator
  *                   基于FFT的谐波失真计算
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

#ifndef __THD_CALCULATOR_H
#define __THD_CALCULATOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* FFT参数 */
#define THD_FFT_POINTS              64          /* FFT点数 (必须是2的幂) */
#define THD_SAMPLE_FREQ             6400        /* 采样频率 (Hz), 64点 @ 100Hz基波 */
#define THD_SAMPLE_PERIOD_US        156         /* 采样周期 (us) = 1/6400 */

/* THD计算参数 */
#define THD_HARMONICS_MAX           10          /* 最大谐波次数 */
#define THD_FUNDAMENTAL_BIN         1           /* 基波在FFT结果中的位置 (100Hz在6400Hz采样率下) */

/* 滤波参数 */
#define THD_FILTER_ALPHA            0.1f        /* 低通滤波系数 (0-1, 越小越平滑) */

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/**
  * @brief  THD计算器结构体
  */
typedef struct
{
  /* 采样缓冲区 */
  float sampleBuffer[THD_FFT_POINTS];     /* 采样数据缓冲区 */
  uint16_t sampleIndex;                    /* 当前采样索引 */
  bool bufferFull;                         /* 缓冲区满标志 */
  
  /* FFT结果 */
  float fftMagnitude[THD_FFT_POINTS/2];    /* FFT幅度谱 (只保留正频率) */
  float fundamental;                       /* 基波幅度 */
  float harmonics[THD_HARMONICS_MAX];      /* 各次谐波幅度 */
  
  /* THD计算结果 */
  float thd;                               /* THD值 (%) */
  float thdFiltered;                       /* 滤波后的THD值 (%) */
  
  /* 状态 */
  bool ready;                              /* 计算完成标志 */
  uint32_t calculationCount;               /* 计算次数 */
  
} THD_Calculator_t;

/* USER CODE END ET */

/* Exported variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/**
  * @brief  THD计算器实例
  */
extern THD_Calculator_t g_thdCalc;

/* USER CODE END EV */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/**
  * @brief  初始化THD计算器
  * @param  None
  * @retval None
  */
void THD_Init(void);

/**
  * @brief  添加采样点
  * @param  sample: 采样值 (交流电压或电流)
  * @retval true: 缓冲区已满,可以计算THD
  *         false: 缓冲区未满
  * @note   应在ADC采样中断中调用
  */
bool THD_AddSample(float sample);

/**
  * @brief  计算THD
  * @param  None
  * @retval THD值 (%)
  * @note   应在主循环中调用,当THD_AddSample返回true时
  */
float THD_Calculate(void);

/**
  * @brief  获取当前THD值
  * @param  None
  * @retval THD值 (%)
  */
float THD_GetValue(void);

/**
  * @brief  获取滤波后的THD值
  * @param  None
  * @retval 滤波后的THD值 (%)
  */
float THD_GetFilteredValue(void);

/**
  * @brief  检查THD计算是否就绪
  * @param  None
  * @retval true: 就绪
  *         false: 未就绪
  */
bool THD_IsReady(void);

/**
  * @brief  重置THD计算器
  * @param  None
  * @retval None
  */
void THD_Reset(void);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __THD_CALCULATOR_H */
