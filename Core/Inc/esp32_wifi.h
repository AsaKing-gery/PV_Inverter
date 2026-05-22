/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : esp32_wifi.h
  * @brief          : ESP32-C3 WiFi and MQTT communication driver (Simplified)
  *                   ESP32 handles WiFi/MQTT connection and JSON packing
  *                   STM32 only sends binary data frames
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

#ifndef __ESP32_WIFI_H
#define __ESP32_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* ESP32 UART配置 */
#define ESP32_UART_BAUDRATE         115200
#define ESP32_UART_TIMEOUT_MS       1000

/* 启动等待时间 */
#define ESP32_BOOT_DELAY_MS         3000    /* 等待ESP32启动 3秒 */

/* 缓冲区大小 */
#define AT_CMD_BUFFER_SIZE          256
#define AT_RSP_BUFFER_SIZE          512
#define MQTT_PAYLOAD_SIZE           1024

/* 连接状态 (简化版) */
#define ESP32_STATE_INIT            0
#define ESP32_STATE_CONNECTED       1
#define ESP32_STATE_ERROR           2

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/**
  * @brief  ESP32状态结构体
  */
typedef struct
{
  uint8_t state;               /* 当前状态 */
  uint8_t wifiRssi;            /* WiFi信号强度 (由ESP32上报) */
  uint32_t lastHeartbeat;      /* 上次心跳时间 */
  uint32_t txCount;            /* 发送计数 */
  uint32_t rxCount;            /* 接收计数 */
  uint32_t errCount;           /* 错误计数 */
} ESP32_Status_t;

/* USER CODE END ET */

/* Exported variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

extern ESP32_Status_t g_esp32Status;

/* USER CODE END EV */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/**
  * @brief  初始化ESP32模块 (简化版)
  * @param  None
  * @retval true: 成功, false: 失败
  * @note   只等待ESP32发送READY信号，不进行WiFi/MQTT配置
  *         ESP32上电后自动连接WiFi和MQTT，完成后发送"READY\n"
  */
bool ESP32_Init(void);

/**
  * @brief  获取ESP32状态
  * @param  None
  * @retval 状态值
  */
uint8_t ESP32_GetState(void);

/**
  * @brief  获取状态字符串
  * @param  None
  * @retval 状态描述字符串
  */
const char* ESP32_GetStateString(void);

/**
  * @brief  检查ESP32是否已连接
  * @param  None
  * @retval true: 已连接, false: 未连接
  */
bool ESP32_IsConnected(void);

/**
  * @brief  复位ESP32模块
  * @param  None
  * @retval true: 成功, false: 失败
  * @note   发送复位命令后等待READY信号
  */
bool ESP32_Reset(void);

/**
  * @brief  ESP32 UART接收中断处理
  * @param  huart: UART句柄
  * @retval None
  * @note   由HAL_UART_RxCpltCallback调用
  */
void ESP32_UART_RxHandler(UART_HandleTypeDef *huart);

/**
  * @brief  发送数据帧到ESP32 (二进制格式)
  * @param  None
  * @retval true: 成功, false: 失败
  * @note   帧格式: [HEAD(0xAA55)][LEN(2B)][DATA(32B)][CRC(1B)]
  *         ESP32接收后解析并打包成JSON发送给MQTT Broker
  */
bool ESP32_SendDataFrame(void);

/**
  * @brief  发送二进制数据到ESP32
  * @param  data: 数据指针
  * @param  len: 数据长度
  * @retval true: 成功, false: 失败
  * @note   数据格式: [0xAA][0x55][LEN(2B)][DATA][CRC8]
  */
bool ESP32_SendBinary(const uint8_t* data, uint16_t len);

/**
  * @brief  检查ESP32 MQTT是否已连接
  * @param  None
  * @retval true: 已连接, false: 未连接
  */
bool ESP32_IsMQTTConnected(void);

/**
  * @brief  发送心跳包到ESP32
  * @param  None
  * @retval true: 成功, false: 失败
  */
bool ESP32_SendHeartbeat(void);

/**
  * @brief  发送故障事件到ESP32 (由ESP32转发到MQTT event主题)
  * @param  faultCode: 故障代码 (与protection.h中定义一致)
  * @param  faultData: 附加数据 (如故障时的电压/电流值)
  * @retval true: 成功, false: 失败
  * @note   事件格式: [0xAA][0x55][0x01][0x00][EVENT_TYPE_FAULT][faultCode][faultData(4B)][CRC8]
  */
bool ESP32_SendEvent(uint8_t faultCode, uint32_t faultData);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __ESP32_WIFI_H */
