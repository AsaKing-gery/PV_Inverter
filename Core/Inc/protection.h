/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : protection.h
  * @brief          : Protection system for PV Micro-Inverter
  *                   Monitors and protects against fault conditions
  *                   - PV input undervoltage
  *                   - Output overcurrent
  *                   - Overtemperature (front/rear)
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

#ifndef __PROTECTION_H
#define __PROTECTION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* 保护阈值 */
/* 光伏输入欠压保护 */
#define PROT_PV_UV_THRESHOLD        1.0f        /* 欠压阈值 1.0V */
#define PROT_PV_UV_RECOVERY         1.2f        /* 恢复阈值 1.2V (滞环) */
#define PROT_PV_UV_DELAY_MS         100         /* 欠压延时 100ms */

/* 输出过流保护 */
#define PROT_OC_THRESHOLD           3.0f        /* 过流阈值 3.0A (峰值) */
#define PROT_OC_DELAY_MS            50          /* 过流延时 50ms (快速保护) */
#define PROT_OC_RECOVERY_MS         5000        /* 恢复时间 5s */

/* 过温保护 */
#define PROT_OT_THRESHOLD           85.0f       /* 过温阈值 85°C */
#define PROT_OT_WARNING             75.0f       /* 温度警告 75°C */
#define PROT_OT_RECOVERY            70.0f       /* 恢复阈值 70°C (滞环) */
#define PROT_OT_DELAY_MS            200         /* 过温延时 200ms */

/* 降功率系数 */
#define PROT_DERATE_FACTOR          0.5f        /* 过温时降功率至50% */

/* 故障代码 */
#define PROT_FAULT_NONE             0x00        /* 无故障 */
#define PROT_FAULT_PV_UV            0x01        /* 光伏欠压 */
#define PROT_FAULT_OC               0x02        /* 输出过流 */
#define PROT_FAULT_OT_FRONT         0x04        /* 前级过温 */
#define PROT_FAULT_OT_REAR          0x08        /* 后级过温 */
#define PROT_FAULT_OT_BOTH          0x0C        /* 前后级过温 */

/* 保护状态 */
#define PROT_STATE_NORMAL           0           /* 正常运行 */
#define PROT_STATE_WARNING          1           /* 警告状态 */
#define PROT_STATE_FAULT            2           /* 故障状态 */
#define PROT_STATE_RECOVERING       3           /* 恢复中 */

/* 动作类型 */
#define PROT_ACTION_NONE            0           /* 无动作 */
#define PROT_ACTION_DERATE          1           /* 降功率 */
#define PROT_ACTION_STOP_TIM1       2           /* 关闭TIM1 (MPPT) */
#define PROT_ACTION_STOP_TIM8       3           /* 关闭TIM8 (SPWM) */
#define PROT_ACTION_STOP_ALL        4           /* 全部关闭 */

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/**
  * @brief  保护通道结构体
  */
typedef struct
{
  float threshold;             /* 保护阈值 */
  float recovery;              /* 恢复阈值 */
  uint32_t delayMs;            /* 延时时间 (ms) */
  uint32_t counter;            /* 延时计数器 */
  uint8_t isTriggered;         /* 触发标志 */
  uint8_t isLatched;           /* 锁存标志 */
} Protection_Channel_t;

/**
  * @brief  保护系统结构体
  */
typedef struct
{
  /* 保护通道 */
  Protection_Channel_t pvUV;           /* 光伏欠压 */
  Protection_Channel_t outOC;          /* 输出过流 */
  Protection_Channel_t tempFront;      /* 前级温度 */
  Protection_Channel_t tempRear;       /* 后级温度 */

  /* 系统状态 */
  uint8_t faultCode;                   /* 故障代码 */
  uint8_t state;                       /* 保护状态 */
  uint8_t action;                      /* 执行动作 */

  /* 降功率控制 */
  float derateFactor;                  /* 当前降功率系数 (0.0-1.0) */
  float targetDerateFactor;            /* 目标降功率系数 */

  /* 恢复计时 */
  uint32_t recoveryTimer;              /* 恢复计时器 */

} Protection_System_t;

/* USER CODE END ET */

/* Exported variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/**
  * @brief  保护系统实例
  */
extern Protection_System_t g_protection;

/* USER CODE END EV */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/**
  * @brief  初始化保护系统
  * @param  None
  * @retval None
  */
void Protection_Init(void);

/**
  * @brief  执行保护检测 (在控制任务中周期性调用)
  * @param  pvVoltage: 光伏电压 (V)
  * @param  acCurrent: 交流输出电流 (A)
  * @param  tempFront: 前级温度 (°C)
  * @param  tempRear: 后级温度 (°C)
  * @retval 故障代码
  * @note   建议每10-20ms调用一次
  */
uint8_t Protection_Check(float pvVoltage, float acCurrent, float tempFront, float tempRear);

/**
  * @brief  执行保护动作
  * @param  None
  * @retval None
  * @note   根据检测结果执行相应动作
  */
void Protection_ExecuteAction(void);

/**
  * @brief  清除故障 (手动恢复)
  * @param  None
  * @retval None
  */
void Protection_ClearFault(void);

/**
  * @brief  获取当前故障代码
  * @param  None
  * @retval 故障代码
  */
uint8_t Protection_GetFaultCode(void);

/**
  * @brief  获取保护状态字符串
  * @param  None
  * @retval 状态字符串
  */
const char* Protection_GetStateString(void);

/**
  * @brief  获取故障字符串
  * @param  faultCode: 故障代码
  * @retval 故障描述字符串
  */
const char* Protection_GetFaultString(uint8_t faultCode);

/**
  * @brief  检查是否有故障
  * @param  None
  * @retval true: 有故障, false: 无故障
  */
bool Protection_HasFault(void);

/**
  * @brief  获取降功率系数
  * @param  None
  * @retval 降功率系数 (0.0-1.0)
  * @note   用于MPPT和SPWM调节输出功率
  */
float Protection_GetDerateFactor(void);

/**
  * @brief  设置降功率系数
  * @param  factor: 降功率系数 (0.0-1.0)
  * @retval None
  */
void Protection_SetDerateFactor(float factor);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __PROTECTION_H */
