/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : thd_calculator.c
  * @brief          : THD (Total Harmonic Distortion) Calculator Implementation
  *                   使用简化DFT算法计算谐波失真
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

/* Includes ------------------------------------------------------------------*/
#include "thd_calculator.h"
#include <math.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 2π */
#ifndef M_PI
  #define M_PI  3.14159265358979323846f
#endif

/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* THD计算器实例 */
THD_Calculator_t g_thdCalc = {0};

/* 正弦/余弦查找表 (用于DFT计算) */
static float sinTable[THD_FFT_POINTS];
static float cosTable[THD_FFT_POINTS];
static bool tableInitialized = false;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static void THD_InitLookupTable(void);
static void THD_ComputeDFT(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  初始化查找表
  * @param  None
  * @retval None
  */
static void THD_InitLookupTable(void)
{
  if (tableInitialized) return;
  
  for (int i = 0; i < THD_FFT_POINTS; i++)
  {
    float angle = 2.0f * M_PI * i / THD_FFT_POINTS;
    sinTable[i] = sinf(angle);
    cosTable[i] = cosf(angle);
  }
  
  tableInitialized = true;
}

/**
  * @brief  计算DFT (简化版,只计算需要的谐波)
  * @param  None
  * @retval None
  */
static void THD_ComputeDFT(void)
{
  float real[THD_HARMONICS_MAX + 1] = {0};
  float imag[THD_HARMONICS_MAX + 1] = {0};
  
  /* 计算基波和各次谐波的DFT */
  for (int h = 1; h <= THD_HARMONICS_MAX; h++)
  {
    real[h] = 0;
    imag[h] = 0;
    
    for (int n = 0; n < THD_FFT_POINTS; n++)
    {
      /* 计算当前采样点的角度 */
      int index = (h * n) % THD_FFT_POINTS;
      real[h] += g_thdCalc.sampleBuffer[n] * cosTable[index];
      imag[h] -= g_thdCalc.sampleBuffer[n] * sinTable[index];
    }
    
    /* 计算幅度 */
    g_thdCalc.fftMagnitude[h] = sqrtf(real[h] * real[h] + imag[h] * imag[h]) / THD_FFT_POINTS;
  }
  
  /* 基波幅度 */
  g_thdCalc.fundamental = g_thdCalc.fftMagnitude[1];
  
  /* 各次谐波幅度 */
  for (int h = 2; h <= THD_HARMONICS_MAX; h++)
  {
    g_thdCalc.harmonics[h - 2] = g_thdCalc.fftMagnitude[h];
  }
}

/* USER CODE END 0 */

/* Exported functions --------------------------------------------------------*/
/* USER CODE BEGIN EF */

/**
  * @brief  初始化THD计算器
  * @param  None
  * @retval None
  */
void THD_Init(void)
{
  /* 清零结构体 */
  memset(&g_thdCalc, 0, sizeof(THD_Calculator_t));
  
  /* 初始化查找表 */
  THD_InitLookupTable();
  
  /* 初始化滤波值 */
  g_thdCalc.thdFiltered = 2.0f;  /* 初始值2%,典型值 */
  
  g_thdCalc.ready = false;
}

/**
  * @brief  添加采样点
  * @param  sample: 采样值 (交流电压或电流)
  * @retval true: 缓冲区已满,可以计算THD
  *         false: 缓冲区未满
  */
bool THD_AddSample(float sample)
{
  /* 存储采样值 */
  g_thdCalc.sampleBuffer[g_thdCalc.sampleIndex++] = sample;
  
  /* 检查缓冲区是否已满 */
  if (g_thdCalc.sampleIndex >= THD_FFT_POINTS)
  {
    g_thdCalc.sampleIndex = 0;
    g_thdCalc.bufferFull = true;
    return true;
  }
  
  return false;
}

/**
  * @brief  计算THD
  * @param  None
  * @retval THD值 (%)
  * @note   THD = sqrt(sum(H2^2 + H3^2 + ...)) / H1 * 100%
  */
float THD_Calculate(void)
{
  if (!g_thdCalc.bufferFull)
  {
    return g_thdCalc.thd;  /* 返回上一次的值 */
  }
  
  /* 计算DFT */
  THD_ComputeDFT();
  
  /* 检查基波幅度 */
  if (g_thdCalc.fundamental < 0.001f)  /* 避免除零 */
  {
    g_thdCalc.thd = 0.0f;
    g_thdCalc.bufferFull = false;
    return 0.0f;
  }
  
  /* 计算谐波总功率 (2-10次谐波) */
  float harmonicPower = 0.0f;
  for (int h = 2; h <= THD_HARMONICS_MAX; h++)
  {
    harmonicPower += g_thdCalc.fftMagnitude[h] * g_thdCalc.fftMagnitude[h];
  }
  
  /* 计算THD */
  float thd = sqrtf(harmonicPower) / g_thdCalc.fundamental * 100.0f;
  
  /* 限制范围 (典型值2-10%) */
  if (thd < 0.0f) thd = 0.0f;
  if (thd > 50.0f) thd = 50.0f;  /* 异常保护 */
  
  g_thdCalc.thd = thd;
  
  /* 低通滤波 */
  g_thdCalc.thdFiltered = g_thdCalc.thdFiltered * (1.0f - THD_FILTER_ALPHA) 
                          + thd * THD_FILTER_ALPHA;
  
  /* 更新状态 */
  g_thdCalc.bufferFull = false;
  g_thdCalc.ready = true;
  g_thdCalc.calculationCount++;
  
  return g_thdCalc.thd;
}

/**
  * @brief  获取当前THD值
  * @param  None
  * @retval THD值 (%)
  */
float THD_GetValue(void)
{
  return g_thdCalc.thd;
}

/**
  * @brief  获取滤波后的THD值
  * @param  None
  * @retval 滤波后的THD值 (%)
  */
float THD_GetFilteredValue(void)
{
  return g_thdCalc.thdFiltered;
}

/**
  * @brief  检查THD计算是否就绪
  * @param  None
  * @retval true: 就绪
  *         false: 未就绪
  */
bool THD_IsReady(void)
{
  return g_thdCalc.ready;
}

/**
  * @brief  重置THD计算器
  * @param  None
  * @retval None
  */
void THD_Reset(void)
{
  g_thdCalc.sampleIndex = 0;
  g_thdCalc.bufferFull = false;
  g_thdCalc.ready = false;
  g_thdCalc.calculationCount = 0;
}

/* USER CODE END EF */
