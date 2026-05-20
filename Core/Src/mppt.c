/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : mppt.c
  * @brief          : MPPT (Perturb and Observe) algorithm implementation
  *                   Controls Buck converter via TIM1 PWM
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
#include "mppt.h"
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

/* TIM1 PWM周期值 (由CubeMX配置决定)
 * 当前配置: Period = 8399, 对应168MHz时钟下 20kHz PWM频率
 * 占空比计算公式: CCR = (duty * 8400) / 100
 */
#define TIM1_PWM_PERIOD             8399

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* MPPT控制器实例 */
MPPT_Controller_t g_mppt;

/* TIM句柄 - 由CubeMX生成 */
extern TIM_HandleTypeDef htim1;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static void MPPT_ScanningMode(void);
static void MPPT_TrackingMode(void);
static void MPPT_UpdateState(void);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  初始化MPPT控制器
  * @param  None
  * @retval None
  */
void MPPT_Init(void)
{
  /* 清零结构体 */
  memset(&g_mppt, 0, sizeof(MPPT_Controller_t));

  /* 初始状态为扫描模式 */
  g_mppt.state = MPPT_STATE_SCANNING;
  g_mppt.scanDuty = MPPT_SCAN_START_DUTY;
  g_mppt.duty = MPPT_SCAN_START_DUTY;
  g_mppt.perturbDirection = 1;

  /* 初始化PWM */
  MPPT_SetDuty(MPPT_SCAN_START_DUTY);

  /* 启动TIM1 PWM */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
}

/**
  * @brief  设置PWM占空比
  * @param  duty: 占空比值 (0-100)
  * @retval None
  * @note   自动限幅到 5%-90%
  * @note   更新TIM1 CCR1寄存器
  */
void MPPT_SetDuty(uint8_t duty)
{
  /* 限幅 */
  if (duty < MPPT_DUTY_MIN)
  {
    duty = MPPT_DUTY_MIN;
  }
  else if (duty > MPPT_DUTY_MAX)
  {
    duty = MPPT_DUTY_MAX;
  }

  /* 保存占空比 */
  g_mppt.duty = duty;

  /* 计算CCR值: CCR = (duty * 8400) / 100 = duty * 84 */
  uint32_t ccr = (duty * (TIM1_PWM_PERIOD + 1)) / 100;

  /* 更新TIM1 CCR1寄存器 */
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr);
}

/**
  * @brief  获取当前PWM占空比
  * @param  None
  * @retval 占空比值 (0-100)
  */
uint8_t MPPT_GetDuty(void)
{
  return g_mppt.duty;
}

/**
  * @brief  获取当前MPPT状态
  * @param  None
  * @retval 状态值
  */
uint8_t MPPT_GetState(void)
{
  return g_mppt.state;
}

/**
  * @brief  获取当前功率
  * @param  None
  * @retval 功率值 (W)
  */
float MPPT_GetPower(void)
{
  return g_mppt.power;
}

/**
  * @brief  获取MPPT效率
  * @param  None
  * @retval 效率值 (0-100)
  * @note   基于当前功率与扫描到的最大功率的比值
  */
float MPPT_GetEfficiency(void)
{
  if (g_mppt.maxPower > 0.1f)
  {
    return (g_mppt.power / g_mppt.maxPower) * 100.0f;
  }
  return 0.0f;
}

/**
  * @brief  强制进入扫描模式
  * @param  None
  * @retval None
  */
void MPPT_ForceScan(void)
{
  g_mppt.state = MPPT_STATE_SCANNING;
  g_mppt.scanDuty = MPPT_SCAN_START_DUTY;
  g_mppt.maxPower = 0.0f;
  g_mppt.maxPowerDuty = MPPT_SCAN_START_DUTY;
  g_mppt.lockCount = 0;
}

/**
  * @brief  扫描模式处理
  * @param  None
  * @retval None
  * @note   大范围扫描寻找最大功率点
  */
static void MPPT_ScanningMode(void)
{
  /* 记录当前功率 */
  if (g_mppt.power > g_mppt.maxPower)
  {
    g_mppt.maxPower = g_mppt.power;
    g_mppt.maxPowerDuty = g_mppt.scanDuty;
  }

  /* 增加扫描占空比 */
  g_mppt.scanDuty += MPPT_SCAN_STEP;

  /* 扫描完成判断 */
  if (g_mppt.scanDuty > MPPT_DUTY_MAX)
  {
    /* 扫描完成，切换到跟踪模式 */
    g_mppt.state = MPPT_STATE_TRACKING;
    g_mppt.duty = g_mppt.maxPowerDuty;
    g_mppt.perturbDirection = 1;
    MPPT_SetDuty(g_mppt.duty);

    /* 更新数据结构 */
    strncpy(g_mqttData.mppt.state, "TRACKING", sizeof(g_mqttData.mppt.state));
  }
  else
  {
    /* 继续扫描 */
    MPPT_SetDuty(g_mppt.scanDuty);
  }
}

/**
  * @brief  跟踪模式处理 (P&O算法)
  * @param  None
  * @retval None
  * @note   扰动观察法跟踪最大功率点
  */
static void MPPT_TrackingMode(void)
{
  float deltaPower = g_mppt.power - g_mppt.lastPower;
  float deltaVoltage = g_mppt.voltage - g_mppt.lastVoltage;

  /* P&O算法核心逻辑 */
  if (fabsf(deltaPower) > 0.01f)  /* 功率变化足够大 */
  {
    if (deltaPower > 0)
    {
      /* 功率增加，保持扰动方向 */
      /* 继续同方向扰动 */
    }
    else
    {
      /* 功率减小，反转扰动方向 */
      g_mppt.perturbDirection = -g_mppt.perturbDirection;
    }

    /* 应用扰动 */
    int16_t newDuty = (int16_t)g_mppt.duty + (g_mppt.perturbDirection * MPPT_DUTY_STEP);

    /* 限幅检查 */
    if (newDuty < MPPT_DUTY_MIN)
    {
      newDuty = MPPT_DUTY_MIN;
      g_mppt.perturbDirection = 1;  /* 到达下限，改为增加 */
    }
    else if (newDuty > MPPT_DUTY_MAX)
    {
      newDuty = MPPT_DUTY_MAX;
      g_mppt.perturbDirection = -1;  /* 到达上限，改为减小 */
    }

    MPPT_SetDuty((uint8_t)newDuty);
  }
  /* 如果功率变化很小，保持当前占空比 */
}

/**
  * @brief  更新MPPT状态
  * @param  None
  * @retval None
  * @note   根据功率稳定性判断是否进入锁定状态
  */
static void MPPT_UpdateState(void)
{
  float deltaPower = fabsf(g_mppt.power - g_mppt.lastPower);

  if (g_mppt.state == MPPT_STATE_TRACKING)
  {
    if (deltaPower < MPPT_POWER_LOCK_THRESHOLD)
    {
      /* 功率稳定 */
      g_mppt.lockCount++;

      if (g_mppt.lockCount >= MPPT_LOCK_COUNT_THRESHOLD)
      {
        /* 进入锁定状态 */
        g_mppt.state = MPPT_STATE_LOCKED;
        strncpy(g_mqttData.mppt.state, "LOCKED", sizeof(g_mqttData.mppt.state));
      }
    }
    else
    {
      /* 功率不稳定，重置计数器 */
      g_mppt.lockCount = 0;
    }
  }
  else if (g_mppt.state == MPPT_STATE_LOCKED)
  {
    if (deltaPower >= MPPT_POWER_LOCK_THRESHOLD)
    {
      /* 功率突变，退出锁定状态 */
      g_mppt.state = MPPT_STATE_TRACKING;
      g_mppt.lockCount = 0;
      strncpy(g_mqttData.mppt.state, "TRACKING", sizeof(g_mqttData.mppt.state));
    }
  }
}

/**
  * @brief  执行MPPT算法
  * @param  voltage: 当前光伏电压 (V)
  * @param  current: 当前光伏电流 (A)
  * @retval None
  * @note   每100ms调用一次
  */
void MPPT_Execute(float voltage, float current)
{
  /* 保存历史值 */
  g_mppt.lastVoltage = g_mppt.voltage;
  g_mppt.lastCurrent = g_mppt.current;
  g_mppt.lastPower = g_mppt.power;
  g_mppt.lastDuty = g_mppt.duty;

  /* 更新当前值 */
  g_mppt.voltage = voltage;
  g_mppt.current = current;
  g_mppt.power = voltage * current;

  /* 更新数据结构 */
  g_mqttData.mppt.duty = g_mppt.duty;
  g_mqttData.mppt.efficiency = MPPT_GetEfficiency();

  /* 根据状态执行相应算法 */
  switch (g_mppt.state)
  {
    case MPPT_STATE_SCANNING:
      MPPT_ScanningMode();
      strncpy(g_mqttData.mppt.state, "SCANNING", sizeof(g_mqttData.mppt.state));
      break;

    case MPPT_STATE_TRACKING:
      MPPT_TrackingMode();
      MPPT_UpdateState();
      break;

    case MPPT_STATE_LOCKED:
      /* 锁定状态，定期检查是否需要重新跟踪 */
      MPPT_UpdateState();
      break;

    default:
      g_mppt.state = MPPT_STATE_SCANNING;
      break;
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
