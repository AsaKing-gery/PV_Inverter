/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : pll_sogi.c
  * @brief          : SOGI-PLL (Second Order Generalized Integrator PLL) implementation
  *                   Grid synchronization using software PLL
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
#include "pll_sogi.h"
#include "tim.h"
#include "data.h"
#include <math.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 数学常量 */
#define PI                          3.14159265359f
#define TWO_PI                      (2.0f * PI)

/* TIM2时钟频率 (168MHz / (167+1) = 1MHz) */
#define TIM2_CLK_FREQ               1000000.0f

/* 角度转弧度 */
#define DEG_TO_RAD(deg)             ((deg) * PI / 180.0f)
#define RAD_TO_DEG(rad)             ((rad) * 180.0f / PI)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* TIM2句柄 - 由CubeMX生成 */
extern TIM_HandleTypeDef htim2;

/* PLL控制器实例 */
PLL_Controller_t g_pll;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static void SOGI_Update(SOGI_Filter_t *sogi, float input, float omega, float dt);
static float PI_Update(PI_Controller_t *pi, float error);
static void PLL_UpdateState(void);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  初始化PLL控制器
  * @param  None
  * @retval None
  */
void PLL_Init(void)
{
  /* 清零结构体 */
  memset(&g_pll, 0, sizeof(PLL_Controller_t));

  /* 初始化SOGI滤波器 */
  g_pll.sogi.k = PLL_SOGI_K;

  /* 初始化PI控制器 */
  g_pll.pi.kp = PLL_KP;
  g_pll.pi.ki = PLL_KI;
  g_pll.pi.integral = 0.0f;
  g_pll.pi.outputMax = 2.0f * PI * PLL_GRID_FREQ_MAX;  /* 最大角频率 */
  g_pll.pi.outputMin = 2.0f * PI * PLL_GRID_FREQ_MIN;  /* 最小角频率 */

  /* 初始化PLL输出 */
  g_pll.omega = 2.0f * PI * PLL_GRID_FREQ_NOMINAL;  /* 初始角频率 */
  g_pll.freq = PLL_GRID_FREQ_NOMINAL;
  g_pll.theta = 0.0f;

  /* 初始状态 */
  g_pll.state = PLL_STATE_UNLOCKED;

  /* 启动TIM2输入捕获 */
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
}

/**
  * @brief  处理TIM2输入捕获中断
  * @param  captureValue: TIM2捕获寄存器值
  * @retval None
  * @note   在TIM2捕获中断中调用
  */
void PLL_TIM2_CaptureCallback(uint32_t captureValue)
{
  uint32_t period;

  if (g_pll.capturePrev == 0)
  {
    /* 第一次捕获，只记录值 */
    g_pll.capturePrev = captureValue;
    return;
  }

  /* 计算周期 (处理溢出) */
  if (captureValue >= g_pll.capturePrev)
  {
    period = captureValue - g_pll.capturePrev;
  }
  else
  {
    period = (65536 - g_pll.capturePrev) + captureValue;
  }

  /* 保存周期 */
  g_pll.capturePeriod = period;
  g_pll.capturePrev = captureValue;
  g_pll.captureValid = 1;

  /* 计算输入频率 */
  if (period > 0)
  {
    g_pll.inputFreq = TIM2_CLK_FREQ / (float)period;
  }
}

/**
  * @brief  SOGI滤波器更新
  * @param  sogi: SOGI结构体指针
  * @param  input: 输入信号
  * @param  omega: 角频率
  * @param  dt: 采样周期
  * @retval None
  */
static void SOGI_Update(SOGI_Filter_t *sogi, float input, float omega, float dt)
{
  float alpha_err = input - sogi->alpha_prev;
  float k_omega_dt = sogi->k * omega * dt;

  /* SOGI差分方程 */
  sogi->alpha = sogi->alpha_prev + k_omega_dt * (alpha_err - sogi->beta_prev);
  sogi->beta = sogi->beta_prev + k_omega_dt * sogi->alpha;

  /* 保存历史值 */
  sogi->alpha_prev = sogi->alpha;
  sogi->beta_prev = sogi->beta;
}

/**
  * @brief  PI控制器更新
  * @param  pi: PI结构体指针
  * @param  error: 误差输入
  * @retval 控制器输出
  */
static float PI_Update(PI_Controller_t *pi, float error)
{
  /* 积分项 */
  pi->integral += error * pi->ki * (PLL_UPDATE_PERIOD_MS / 1000.0f);

  /* 积分限幅 */
  if (pi->integral > pi->outputMax)
  {
    pi->integral = pi->outputMax;
  }
  else if (pi->integral < pi->outputMin)
  {
    pi->integral = pi->outputMin;
  }

  /* PI输出 */
  pi->output = pi->kp * error + pi->integral;

  /* 输出限幅 */
  if (pi->output > pi->outputMax)
  {
    pi->output = pi->outputMax;
  }
  else if (pi->output < pi->outputMin)
  {
    pi->output = pi->outputMin;
  }

  return pi->output;
}

/**
  * @brief  更新PLL锁定状态
  * @param  None
  * @retval None
  */
static void PLL_UpdateState(void)
{
  /* 计算频率误差和相位误差 */
  g_pll.freqError = fabsf(g_pll.freq - g_pll.inputFreq);
  g_pll.phaseError = fabsf(g_pll.phaseError);

  switch (g_pll.state)
  {
    case PLL_STATE_UNLOCKED:
      /* 检查是否满足锁定条件 */
      if (g_pll.freqError < PLL_LOCK_THRESHOLD_HZ &&
          g_pll.phaseError < DEG_TO_RAD(PLL_LOCK_THRESHOLD_DEG))
      {
        g_pll.lockCount++;
        if (g_pll.lockCount >= PLL_LOCK_COUNT)
        {
          g_pll.state = PLL_STATE_LOCKED;
          strncpy(g_mqttData.inverter.syncState, "SYNCED", sizeof(g_mqttData.inverter.syncState));
        }
        else
        {
          g_pll.state = PLL_STATE_LOCKING;
        }
      }
      else
      {
        g_pll.lockCount = 0;
      }
      break;

    case PLL_STATE_LOCKING:
      if (g_pll.freqError < PLL_LOCK_THRESHOLD_HZ &&
          g_pll.phaseError < DEG_TO_RAD(PLL_LOCK_THRESHOLD_DEG))
      {
        g_pll.lockCount++;
        if (g_pll.lockCount >= PLL_LOCK_COUNT)
        {
          g_pll.state = PLL_STATE_LOCKED;
          strncpy(g_mqttData.inverter.syncState, "SYNCED", sizeof(g_mqttData.inverter.syncState));
        }
      }
      else
      {
        g_pll.lockCount = 0;
        g_pll.state = PLL_STATE_UNLOCKED;
      }
      break;

    case PLL_STATE_LOCKED:
      /* 检查是否失锁 */
      if (g_pll.freqError >= PLL_LOCK_THRESHOLD_HZ * 2 ||
          g_pll.phaseError >= DEG_TO_RAD(PLL_LOCK_THRESHOLD_DEG * 2))
      {
        g_pll.lockCount = 0;
        g_pll.state = PLL_STATE_UNLOCKED;
        strncpy(g_mqttData.inverter.syncState, "UNSYNC", sizeof(g_mqttData.inverter.syncState));
      }
      break;

    default:
      g_pll.state = PLL_STATE_UNLOCKED;
      break;
  }
}

/**
  * @brief  执行PLL更新 (SOGI-PLL算法)
  * @param  gridVoltage: 电网电压采样值 (V)
  * @retval None
  * @note   建议每10ms调用一次
  */
void PLL_Update(float gridVoltage)
{
  float dt = PLL_UPDATE_PERIOD_MS / 1000.0f;

  /* 保存输入 */
  g_pll.input = gridVoltage;

  /* SOGI滤波器更新 (生成正交信号) */
  SOGI_Update(&g_pll.sogi, gridVoltage, g_pll.omega, dt);

  /* 计算相位误差 (使用正交信号进行鉴相) */
  /* phase_error = alpha * cos(theta) + beta * sin(theta) */
  g_pll.phaseError = -(g_pll.sogi.alpha * g_pll.cosTheta + g_pll.sogi.beta * g_pll.sinTheta);

  /* PI控制器调节频率 */
  g_pll.omega = PI_Update(&g_pll.pi, g_pll.phaseError);

  /* 频率限幅 */
  float omega_nominal = 2.0f * PI * PLL_GRID_FREQ_NOMINAL;
  float omega_max = 2.0f * PI * PLL_GRID_FREQ_MAX;
  float omega_min = 2.0f * PI * PLL_GRID_FREQ_MIN;

  if (g_pll.omega > omega_max)
  {
    g_pll.omega = omega_max;
  }
  else if (g_pll.omega < omega_min)
  {
    g_pll.omega = omega_min;
  }

  /* 计算频率 */
  g_pll.freq = g_pll.omega / (2.0f * PI);

  /* 更新相位角 */
  g_pll.theta += g_pll.omega * dt;

  /* 相位角归一化到 0-2π */
  while (g_pll.theta >= TWO_PI)
  {
    g_pll.theta -= TWO_PI;
  }
  while (g_pll.theta < 0.0f)
  {
    g_pll.theta += TWO_PI;
  }

  /* 计算sin/cos */
  g_pll.sinTheta = sinf(g_pll.theta);
  g_pll.cosTheta = cosf(g_pll.theta);

  /* 更新状态 */
  PLL_UpdateState();

  /* 更新数据结构 */
  g_mqttData.inverter.gridFreq = g_pll.freq;
  g_mqttData.inverter.gridPhase = PLL_GetPhaseDegree();
}

/**
  * @brief  获取PLL输出相位
  * @param  None
  * @retval 相位角 (0-360度)
  */
float PLL_GetPhaseDegree(void)
{
  return RAD_TO_DEG(g_pll.theta);
}

/**
  * @brief  获取PLL输出频率
  * @param  None
  * @retval 频率 (Hz)
  */
float PLL_GetFrequency(void)
{
  return g_pll.freq;
}

/**
  * @brief  获取PLL输出sin值
  * @param  None
  * @retval sin(theta) (-1.0 ~ 1.0)
  */
float PLL_GetSinTheta(void)
{
  return g_pll.sinTheta;
}

/**
  * @brief  获取PLL输出cos值
  * @param  None
  * @retval cos(theta) (-1.0 ~ 1.0)
  */
float PLL_GetCosTheta(void)
{
  return g_pll.cosTheta;
}

/**
  * @brief  获取PLL锁定状态
  * @param  None
  * @retval 状态值
  */
uint8_t PLL_GetState(void)
{
  return g_pll.state;
}

/**
  * @brief  检查PLL是否已锁定
  * @param  None
  * @retval true: 已锁定, false: 未锁定
  */
bool PLL_IsLocked(void)
{
  return (g_pll.state == PLL_STATE_LOCKED);
}

/**
  * @brief  重置PLL控制器
  * @param  None
  * @retval None
  */
void PLL_Reset(void)
{
  /* 重置SOGI */
  g_pll.sogi.alpha = 0.0f;
  g_pll.sogi.beta = 0.0f;
  g_pll.sogi.alpha_prev = 0.0f;
  g_pll.sogi.beta_prev = 0.0f;

  /* 重置PI */
  g_pll.pi.integral = 0.0f;
  g_pll.pi.output = 0.0f;

  /* 重置PLL */
  g_pll.theta = 0.0f;
  g_pll.omega = 2.0f * PI * PLL_GRID_FREQ_NOMINAL;
  g_pll.freq = PLL_GRID_FREQ_NOMINAL;
  g_pll.phaseError = 0.0f;

  /* 重置状态 */
  g_pll.state = PLL_STATE_UNLOCKED;
  g_pll.lockCount = 0;
}

/**
  * @brief  从TIM2捕获计算电网频率
  * @param  None
  * @retval 频率 (Hz)
  */
float PLL_CalcFreqFromCapture(void)
{
  if (g_pll.captureValid && g_pll.capturePeriod > 0)
  {
    return TIM2_CLK_FREQ / (float)g_pll.capturePeriod;
  }
  return 0.0f;
}

/* USER CODE BEGIN 1 */

/**
  * @brief  TIM2输入捕获中断回调函数
  * @param  htim: TIM句柄
  * @retval None
  * @note   在stm32f4xx_it.c中调用
  */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if (htim == &htim2)
  {
    uint32_t capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
    PLL_TIM2_CaptureCallback(capture);
  }
}

/* USER CODE END 1 */
