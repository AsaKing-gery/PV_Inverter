/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : protection.c
  * @brief          : Protection system implementation for PV Micro-Inverter
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
#include "protection.h"
#include "mppt.h"
#include "spwm.h"
#include "tim.h"
#include "data.h"
#include <string.h>
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 降功率变化速率 (每周期变化量) */
#define DERATE_RAMP_RATE            0.05f

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* 保护系统实例 */
Protection_System_t g_protection;

/* 故障代码字符串映射 */
static const char* faultStrings[] = {
  "NONE",                     /* 0x00 */
  "PV UNDERVOLTAGE",          /* 0x01 */
  "OUTPUT OVERCURRENT",       /* 0x02 */
  "PV UV + OC",               /* 0x03 */
  "FRONT OVERTEMP",           /* 0x04 */
  "PV UV + OT FRONT",         /* 0x05 */
  "OC + OT FRONT",            /* 0x06 */
  "PV UV + OC + OT FRONT",    /* 0x07 */
  "REAR OVERTEMP",            /* 0x08 */
  "PV UV + OT REAR",          /* 0x09 */
  "OC + OT REAR",             /* 0x0A */
  "PV UV + OC + OT REAR",     /* 0x0B */
  "BOTH OVERTEMP",            /* 0x0C */
  "PV UV + BOTH OT",          /* 0x0D */
  "OC + BOTH OT",             /* 0x0E */
  "MULTI FAULT"               /* 0x0F */
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static uint8_t Protection_CheckPV_UV(float pvVoltage);
static uint8_t Protection_CheckOC(float acCurrent);
static uint8_t Protection_CheckOT(float tempFront, float tempRear);
static void Protection_UpdateDerateFactor(void);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  初始化保护系统
  * @param  None
  * @retval None
  */
void Protection_Init(void)
{
  /* 清零结构体 */
  memset(&g_protection, 0, sizeof(Protection_System_t));

  /* 初始化光伏欠压保护 */
  g_protection.pvUV.threshold = PROT_PV_UV_THRESHOLD;
  g_protection.pvUV.recovery = PROT_PV_UV_RECOVERY;
  g_protection.pvUV.delayMs = PROT_PV_UV_DELAY_MS;

  /* 初始化输出过流保护 */
  g_protection.outOC.threshold = PROT_OC_THRESHOLD;
  g_protection.outOC.delayMs = PROT_OC_DELAY_MS;

  /* 初始化过温保护 */
  g_protection.tempFront.threshold = PROT_OT_THRESHOLD;
  g_protection.tempFront.recovery = PROT_OT_RECOVERY;
  g_protection.tempFront.delayMs = PROT_OT_DELAY_MS;

  g_protection.tempRear.threshold = PROT_OT_THRESHOLD;
  g_protection.tempRear.recovery = PROT_OT_RECOVERY;
  g_protection.tempRear.delayMs = PROT_OT_DELAY_MS;

  /* 初始状态 */
  g_protection.state = PROT_STATE_NORMAL;
  g_protection.derateFactor = 1.0f;
  g_protection.targetDerateFactor = 1.0f;
}

/**
  * @brief  检查光伏欠压保护
  * @param  pvVoltage: 光伏电压 (V)
  * @retval 故障位 (0或PROT_FAULT_PV_UV)
  */
static uint8_t Protection_CheckPV_UV(float pvVoltage)
{
  uint8_t fault = 0;

  if (pvVoltage < g_protection.pvUV.threshold)
  {
    /* 欠压检测 */
    g_protection.pvUV.counter++;

    if (g_protection.pvUV.counter >= (g_protection.pvUV.delayMs / 10))
    {
      /* 延时满足，触发保护 */
      g_protection.pvUV.isTriggered = 1;
      g_protection.pvUV.isLatched = 1;
      fault = PROT_FAULT_PV_UV;
    }
  }
  else if (pvVoltage > g_protection.pvUV.recovery && g_protection.pvUV.isLatched)
  {
    /* 恢复检测 (滞环) */
    g_protection.pvUV.isTriggered = 0;
    g_protection.pvUV.counter = 0;

    /* 清除锁存 */
    g_protection.pvUV.isLatched = 0;
  }
  else
  {
    /* 正常状态 */
    g_protection.pvUV.counter = 0;
  }

  return fault;
}

/**
  * @brief  检查输出过流保护
  * @param  acCurrent: 交流输出电流 (A)
  * @retval 故障位 (0或PROT_FAULT_OC)
  */
static uint8_t Protection_CheckOC(float acCurrent)
{
  uint8_t fault = 0;

  /* 取绝对值 (峰值电流) */
  float currentAbs = (acCurrent < 0) ? -acCurrent : acCurrent;

  if (currentAbs > g_protection.outOC.threshold)
  {
    /* 过流检测 */
    g_protection.outOC.counter++;

    if (g_protection.outOC.counter >= (g_protection.outOC.delayMs / 10))
    {
      /* 延时满足，触发保护 */
      g_protection.outOC.isTriggered = 1;
      g_protection.outOC.isLatched = 1;
      fault = PROT_FAULT_OC;
    }
  }
  else if (g_protection.outOC.isLatched)
  {
    /* 检查恢复计时 */
    g_protection.recoveryTimer++;

    if (g_protection.recoveryTimer >= (PROT_OC_RECOVERY_MS / 10))
    {
      /* 恢复时间到，清除锁存 */
      g_protection.outOC.isTriggered = 0;
      g_protection.outOC.isLatched = 0;
      g_protection.outOC.counter = 0;
      g_protection.recoveryTimer = 0;
    }
  }
  else
  {
    /* 正常状态 */
    g_protection.outOC.counter = 0;
    g_protection.recoveryTimer = 0;
  }

  return fault;
}

/**
  * @brief  检查过温保护
  * @param  tempFront: 前级温度 (°C)
  * @param  tempRear: 后级温度 (°C)
  * @retval 故障位 (0, PROT_FAULT_OT_FRONT, PROT_FAULT_OT_REAR, 或 PROT_FAULT_OT_BOTH)
  */
static uint8_t Protection_CheckOT(float tempFront, float tempRear)
{
  uint8_t fault = 0;

  /* 前级温度检查 */
  if (tempFront > g_protection.tempFront.threshold)
  {
    g_protection.tempFront.counter++;

    if (g_protection.tempFront.counter >= (g_protection.tempFront.delayMs / 10))
    {
      g_protection.tempFront.isTriggered = 1;
      g_protection.tempFront.isLatched = 1;
      fault |= PROT_FAULT_OT_FRONT;
    }
  }
  else if (tempFront < g_protection.tempFront.recovery && g_protection.tempFront.isLatched)
  {
    /* 恢复 */
    g_protection.tempFront.isTriggered = 0;
    g_protection.tempFront.isLatched = 0;
    g_protection.tempFront.counter = 0;
  }
  else
  {
    g_protection.tempFront.counter = 0;
  }

  /* 后级温度检查 */
  if (tempRear > g_protection.tempRear.threshold)
  {
    g_protection.tempRear.counter++;

    if (g_protection.tempRear.counter >= (g_protection.tempRear.delayMs / 10))
    {
      g_protection.tempRear.isTriggered = 1;
      g_protection.tempRear.isLatched = 1;
      fault |= PROT_FAULT_OT_REAR;
    }
  }
  else if (tempRear < g_protection.tempRear.recovery && g_protection.tempRear.isLatched)
  {
    /* 恢复 */
    g_protection.tempRear.isTriggered = 0;
    g_protection.tempRear.isLatched = 0;
    g_protection.tempRear.counter = 0;
  }
  else
  {
    g_protection.tempRear.counter = 0;
  }

  return fault;
}

/**
  * @brief  更新降功率系数
  * @param  None
  * @retval None
  */
static void Protection_UpdateDerateFactor(void)
{
  /* 根据温度计算目标降功率系数 */
  if (g_protection.tempFront.isTriggered || g_protection.tempRear.isTriggered)
  {
    g_protection.targetDerateFactor = PROT_DERATE_FACTOR;
  }
  else
  {
    g_protection.targetDerateFactor = 1.0f;
  }

  /* 平滑过渡 */
  if (g_protection.derateFactor > g_protection.targetDerateFactor)
  {
    g_protection.derateFactor -= DERATE_RAMP_RATE;
    if (g_protection.derateFactor < g_protection.targetDerateFactor)
    {
      g_protection.derateFactor = g_protection.targetDerateFactor;
    }
  }
  else if (g_protection.derateFactor < g_protection.targetDerateFactor)
  {
    g_protection.derateFactor += DERATE_RAMP_RATE;
    if (g_protection.derateFactor > g_protection.targetDerateFactor)
    {
      g_protection.derateFactor = g_protection.targetDerateFactor;
    }
  }
}

/**
  * @brief  执行保护检测
  * @param  pvVoltage: 光伏电压 (V)
  * @param  acCurrent: 交流输出电流 (A)
  * @param  tempFront: 前级温度 (°C)
  * @param  tempRear: 后级温度 (°C)
  * @retval 故障代码
  */
uint8_t Protection_Check(float pvVoltage, float acCurrent, float tempFront, float tempRear)
{
  uint8_t fault = 0;

  /* 各项保护检查 */
  fault |= Protection_CheckPV_UV(pvVoltage);
  fault |= Protection_CheckOC(acCurrent);
  fault |= Protection_CheckOT(tempFront, tempRear);

  /* 更新降功率系数 */
  Protection_UpdateDerateFactor();

  /* 更新故障代码 */
  g_protection.faultCode = fault;

  /* 更新状态 */
  if (fault == 0)
  {
    if (g_protection.state == PROT_STATE_FAULT)
    {
      g_protection.state = PROT_STATE_RECOVERING;
    }
    else if (g_protection.state == PROT_STATE_RECOVERING)
    {
      /* 所有保护都恢复后回到正常状态 */
      if (!g_protection.pvUV.isLatched &&
          !g_protection.outOC.isLatched &&
          !g_protection.tempFront.isLatched &&
          !g_protection.tempRear.isLatched)
      {
        g_protection.state = PROT_STATE_NORMAL;
      }
    }
    else
    {
      g_protection.state = PROT_STATE_NORMAL;
    }
  }
  else
  {
    g_protection.state = PROT_STATE_FAULT;
  }

  /* 更新数据结构 */
  g_mqttData.inverter.faultCode = fault;
  strncpy(g_mqttData.inverter.faultDesc,
          Protection_GetFaultString(fault),
          sizeof(g_mqttData.inverter.faultDesc));

  return fault;
}

/**
  * @brief  执行保护动作
  * @param  None
  * @retval None
  */
void Protection_ExecuteAction(void)
{
  uint8_t action = PROT_ACTION_NONE;

  /* 确定动作 */
  if (g_protection.pvUV.isLatched)
  {
    /* 光伏欠压 → 关闭TIM1 (MPPT) */
    action = PROT_ACTION_STOP_TIM1;
  }

  if (g_protection.outOC.isLatched)
  {
    /* 输出过流 → 关闭TIM8 (SPWM) */
    if (action == PROT_ACTION_STOP_TIM1)
    {
      action = PROT_ACTION_STOP_ALL;
    }
    else
    {
      action = PROT_ACTION_STOP_TIM8;
    }
  }

  if (g_protection.tempFront.isLatched || g_protection.tempRear.isLatched)
  {
    /* 过温 → 降功率 */
    if (action == PROT_ACTION_NONE)
    {
      action = PROT_ACTION_DERATE;
    }
  }

  g_protection.action = action;

  /* 执行动作 */
  switch (action)
  {
    case PROT_ACTION_STOP_TIM1:
      /* 关闭TIM1 PWM (MPPT) */
      HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
      strncpy(g_mqttData.inverter.state, "PV UV STOP", sizeof(g_mqttData.inverter.state));
      break;

    case PROT_ACTION_STOP_TIM8:
      /* 关闭TIM8 PWM (SPWM) */
      SPWM_Stop();
      strncpy(g_mqttData.inverter.state, "OC STOP", sizeof(g_mqttData.inverter.state));
      break;

    case PROT_ACTION_STOP_ALL:
      /* 全部关闭 */
      HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
      SPWM_Stop();
      strncpy(g_mqttData.inverter.state, "FAULT STOP", sizeof(g_mqttData.inverter.state));
      break;

    case PROT_ACTION_DERATE:
      /* 降功率运行 */
      /* MPPT和SPWM会自动使用降功率系数 */
      strncpy(g_mqttData.inverter.state, "DERATE", sizeof(g_mqttData.inverter.state));
      break;

    case PROT_ACTION_NONE:
    default:
      /* 正常运行 */
      break;
  }
}

/**
  * @brief  清除故障 (手动恢复)
  * @param  None
  * @retval None
  */
void Protection_ClearFault(void)
{
  /* 清除所有锁存 */
  g_protection.pvUV.isLatched = 0;
  g_protection.pvUV.isTriggered = 0;
  g_protection.pvUV.counter = 0;

  g_protection.outOC.isLatched = 0;
  g_protection.outOC.isTriggered = 0;
  g_protection.outOC.counter = 0;
  g_protection.recoveryTimer = 0;

  g_protection.tempFront.isLatched = 0;
  g_protection.tempFront.isTriggered = 0;
  g_protection.tempFront.counter = 0;

  g_protection.tempRear.isLatched = 0;
  g_protection.tempRear.isTriggered = 0;
  g_protection.tempRear.counter = 0;

  /* 清除故障码 */
  g_protection.faultCode = PROT_FAULT_NONE;
  g_protection.state = PROT_STATE_NORMAL;
  g_protection.action = PROT_ACTION_NONE;

  /* 恢复满功率 */
  g_protection.derateFactor = 1.0f;
  g_protection.targetDerateFactor = 1.0f;
}

/**
  * @brief  获取当前故障代码
  * @param  None
  * @retval 故障代码
  */
uint8_t Protection_GetFaultCode(void)
{
  return g_protection.faultCode;
}

/**
  * @brief  获取保护状态字符串
  * @param  None
  * @retval 状态字符串
  */
const char* Protection_GetStateString(void)
{
  switch (g_protection.state)
  {
    case PROT_STATE_NORMAL:
      return "NORMAL";
    case PROT_STATE_WARNING:
      return "WARNING";
    case PROT_STATE_FAULT:
      return "FAULT";
    case PROT_STATE_RECOVERING:
      return "RECOVERING";
    default:
      return "UNKNOWN";
  }
}

/**
  * @brief  获取故障字符串
  * @param  faultCode: 故障代码
  * @retval 故障描述字符串
  */
const char* Protection_GetFaultString(uint8_t faultCode)
{
  if (faultCode < sizeof(faultStrings) / sizeof(faultStrings[0]))
  {
    return faultStrings[faultCode];
  }
  return "UNKNOWN";
}

/**
  * @brief  检查是否有故障
  * @param  None
  * @retval true: 有故障, false: 无故障
  */
bool Protection_HasFault(void)
{
  return (g_protection.faultCode != PROT_FAULT_NONE);
}

/**
  * @brief  获取降功率系数
  * @param  None
  * @retval 降功率系数 (0.0-1.0)
  */
float Protection_GetDerateFactor(void)
{
  return g_protection.derateFactor;
}

/**
  * @brief  设置降功率系数
  * @param  factor: 降功率系数 (0.0-1.0)
  * @retval None
  */
void Protection_SetDerateFactor(float factor)
{
  if (factor < 0.0f) factor = 0.0f;
  if (factor > 1.0f) factor = 1.0f;

  g_protection.targetDerateFactor = factor;
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
