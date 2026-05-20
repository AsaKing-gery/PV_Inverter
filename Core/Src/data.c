/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : data.c
  * @brief          : MQTT data management implementation
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
#include "data.h"
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* MQTT数据包实例 */
MqttDataPacket_t g_mqttData;     /* 实时数据 */
MqttStatusPacket_t g_mqttStatus; /* 状态数据 */
MqttEventPacket_t g_mqttEvent;   /* 事件数据 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  初始化MQTT数据结构
  * @param  None
  * @retval None
  */
void Data_Init(void)
{
  /* 清空数据结构 */
  memset(&g_mqttData, 0, sizeof(MqttDataPacket_t));
  memset(&g_mqttStatus, 0, sizeof(MqttStatusPacket_t));
  memset(&g_mqttEvent, 0, sizeof(MqttEventPacket_t));
  
  /* 初始化状态数据 */
  strncpy(g_mqttStatus.state, "ONLINE", DEVICE_STATE_LEN - 1);
  strncpy(g_mqttStatus.fw_version, "v1.0.2", FW_VERSION_LEN - 1);
  
  /* 初始化MPPT状态 */
  strncpy(g_mqttData.mppt.state, "SCANNING", MPPT_STATE_LEN - 1);
  
  /* 初始化逆变器状态 */
  strncpy(g_mqttData.inverter.state, "STOP", INVERTER_STATE_LEN - 1);
  strncpy(g_mqttData.inverter.mode, "ON_GRID", INVERTER_MODE_LEN - 1);
}

/* USER CODE BEGIN 1 */

/* 
 * ============================================================================
 * 以下代码为JSON打包功能，当前在STM32上不使用
 * 数据打包将在另一颗芯片（如ESP32）上完成
 * ============================================================================
 */

#if 0  /* 打包功能已禁用 - 移至外部芯片处理 */

#include "cmsis_os.h"
#include <stdio.h>

/* 互斥量保护数据访问 - 由task.c定义 */
extern osMutexId_t globalDataMutex;

/**
  * @brief  安全更新数据（带互斥锁）- 打包用
  * @param  None
  * @retval None
  * @note   此函数用于外部芯片打包时同步数据
  */
void Data_UpdateSafe(void)
{
  /* 获取互斥量 */
  if (osMutexAcquire(globalDataMutex, 100) == osOK)
  {
    /* 更新时间戳 */
    g_mqttData.ts = osKernelGetTickCount() / 1000;
    g_mqttStatus.ts = g_mqttData.ts;
    
    /* 更新运行时间 */
    g_mqttStatus.uptime = g_mqttData.ts;
    
    /* 释放互斥量 */
    osMutexRelease(globalDataMutex);
  }
}

/**
  * @brief  生成实时数据主题JSON - 打包用
  * @param  buffer: 输出缓冲区
  * @param  len: 缓冲区长度
  * @retval 实际生成的长度
  * @note   此函数用于外部芯片打包data主题数据
  */
int Data_GenerateDataJson(char *buffer, int len)
{
  int n = 0;
  
  /* 获取互斥量 */
  if (osMutexAcquire(globalDataMutex, 100) != osOK)
  {
    return 0;
  }
  
  n = snprintf(buffer, len,
    "{"
    "\"ts\":%lu,"
    "\"pv\":{\"voltage\":%.2f,\"current\":%.3f,\"power\":%.2f},"
    "\"bus\":{\"voltage\":%.2f},"
    "\"ac\":{\"voltage\":%.2f,\"current\":%.3f,\"power\":%.2f,\"frequency\":%.2f,\"pf\":%.2f},"
    "\"temp\":{\"mosfet_front\":%.1f,\"mosfet_rear\":%.1f},"
    "\"mppt\":{\"state\":\"%s\",\"duty\":%.1f,\"efficiency\":%.1f},"
    "\"inverter\":{\"state\":\"%s\",\"mode\":\"%s\",\"thd\":%.1f}"
    "}",
    g_mqttData.ts,
    g_mqttData.pv.voltage,
    g_mqttData.pv.current,
    g_mqttData.pv.power,
    g_mqttData.bus.voltage,
    g_mqttData.ac.voltage,
    g_mqttData.ac.current,
    g_mqttData.ac.power,
    g_mqttData.ac.frequency,
    g_mqttData.ac.pf,
    g_mqttData.temp.mosfet_front,
    g_mqttData.temp.mosfet_rear,
    g_mqttData.mppt.state,
    g_mqttData.mppt.duty,
    g_mqttData.mppt.efficiency,
    g_mqttData.inverter.state,
    g_mqttData.inverter.mode,
    g_mqttData.inverter.thd
  );
  
  osMutexRelease(globalDataMutex);
  
  return n;
}

/**
  * @brief  生成状态主题JSON - 打包用
  * @param  buffer: 输出缓冲区
  * @param  len: 缓冲区长度
  * @retval 实际生成的长度
  * @note   此函数用于外部芯片打包status主题数据
  */
int Data_GenerateStatusJson(char *buffer, int len)
{
  int n = 0;
  
  /* 获取互斥量 */
  if (osMutexAcquire(globalDataMutex, 100) != osOK)
  {
    return 0;
  }
  
  n = snprintf(buffer, len,
    "{"
    "\"ts\":%lu,"
    "\"state\":\"%s\","
    "\"uptime\":%lu,"
    "\"fw_version\":\"%s\","
    "\"wifi_rssi\":%d"
    "}",
    g_mqttStatus.ts,
    g_mqttStatus.state,
    g_mqttStatus.uptime,
    g_mqttStatus.fw_version,
    g_mqttStatus.wifi_rssi
  );
  
  osMutexRelease(globalDataMutex);
  
  return n;
}

/**
  * @brief  生成事件主题JSON - 打包用
  * @param  buffer: 输出缓冲区
  * @param  len: 缓冲区长度
  * @retval 实际生成的长度
  * @note   此函数用于外部芯片打包event主题数据
  */
int Data_GenerateEventJson(char *buffer, int len)
{
  int n = 0;
  
  /* 获取互斥量 */
  if (osMutexAcquire(globalDataMutex, 100) != osOK)
  {
    return 0;
  }
  
  n = snprintf(buffer, len,
    "{"
    "\"ts\":%lu,"
    "\"level\":\"%s\","
    "\"code\":\"%s\","
    "\"message\":\"%s\","
    "\"detail\":{}"
    "}",
    g_mqttEvent.ts,
    g_mqttEvent.level,
    g_mqttEvent.code,
    g_mqttEvent.message
  );
  
  osMutexRelease(globalDataMutex);
  
  return n;
}

#endif /* 打包功能已禁用 - 移至外部芯片处理 */

/* USER CODE END 1 */
