/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : esp32_wifi.h
  * @brief          : ESP32-C3 WiFi and MQTT communication driver
  *                   Uses AT commands via UART
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

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* ESP32 UART配置 */
#define ESP32_UART_BAUDRATE         115200
#define ESP32_UART_TIMEOUT_MS       1000

/* 启动等待时间 */
#define ESP32_BOOT_DELAY_MS         3000    /* 等待ESP32启动 3秒 */

/* AT命令超时 */
#define AT_CMD_TIMEOUT_MS           5000
#define AT_CMD_RETRY_COUNT          3

/* WiFi配置 */
#define WIFI_SSID                   "YourWiFiSSID"
#define WIFI_PASSWORD               "YourWiFiPassword"

/* MQTT配置 */
#define MQTT_BROKER_IP              "192.168.1.100"
#define MQTT_BROKER_PORT            1883
#define MQTT_CLIENT_ID              "PV_Inverter_001"
#define MQTT_USERNAME               "inverter"
#define MQTT_PASSWORD               "password"
#define MQTT_KEEPALIVE              60

/* 主题定义 */
#define MQTT_TOPIC_STATUS           "pv/inverter/status"
#define MQTT_TOPIC_DATA             "pv/inverter/data"
#define MQTT_TOPIC_CONTROL          "pv/inverter/control"
#define MQTT_TOPIC_WILL             "pv/inverter/will"

/* 缓冲区大小 */
#define AT_CMD_BUFFER_SIZE          256
#define AT_RSP_BUFFER_SIZE          512
#define MQTT_PAYLOAD_SIZE           1024

/* 连接状态 */
#define ESP32_STATE_INIT            0
#define ESP32_STATE_READY           1
#define ESP32_STATE_WIFI_CONNECTING 2
#define ESP32_STATE_WIFI_CONNECTED  3
#define ESP32_STATE_MQTT_CONNECTING 4
#define ESP32_STATE_MQTT_CONNECTED  5
#define ESP32_STATE_ERROR           6

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/**
  * @brief  ESP32状态结构体
  */
typedef struct
{
  uint8_t state;               /* 当前状态 */
  uint8_t wifiRssi;            /* WiFi信号强度 */
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
  * @brief  初始化ESP32模块
  * @param  None
  * @retval true: 成功, false: 失败
  * @note   执行完整初始化流程：
  *         1. 等待ESP32启动
  *         2. 发送AT测试
  *         3. 连接WiFi
  *         4. 配置MQTT
  *         5. 连接MQTT Broker
  */
bool ESP32_Init(void);

/**
  * @brief  发送AT命令并等待响应
  * @param  cmd: AT命令字符串
  * @param  expectedRsp: 期望的响应字符串
  * @param  timeoutMs: 超时时间(ms)
  * @retval true: 成功, false: 失败
  */
bool ESP32_SendATCommand(const char* cmd, const char* expectedRsp, uint32_t timeoutMs);

/**
  * @brief  连接WiFi热点
  * @param  ssid: WiFi名称
  * @param  password: WiFi密码
  * @retval true: 成功, false: 失败
  */
bool ESP32_ConnectWiFi(const char* ssid, const char* password);

/**
  * @brief  配置MQTT客户端
  * @param  clientId: 客户端ID
  * @param  username: 用户名
  * @param  password: 密码
  * @retval true: 成功, false: 失败
  */
bool ESP32_ConfigMQTT(const char* clientId, const char* username, const char* password);

/**
  * @brief  连接MQTT Broker
  * @param  brokerIp: Broker IP地址
  * @param  port: 端口号
  * @retval true: 成功, false: 失败
  */
bool ESP32_ConnectMQTT(const char* brokerIp, uint16_t port);

/**
  * @brief  发布MQTT消息
  * @param  topic: 主题
  * @param  payload: 消息内容
  * @param  qos: QoS等级 (0/1/2)
  * @retval true: 成功, false: 失败
  */
bool ESP32_MQTTPublish(const char* topic, const char* payload, uint8_t qos);

/**
  * @brief  订阅MQTT主题
  * @param  topic: 主题
  * @param  qos: QoS等级 (0/1/2)
  * @retval true: 成功, false: 失败
  */
bool ESP32_MQTTSubscribe(const char* topic, uint8_t qos);

/**
  * @brief  检查ESP32状态
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
  * @brief  检查WiFi是否连接
  * @param  None
  * @retval true: 已连接, false: 未连接
  */
bool ESP32_IsWiFiConnected(void);

/**
  * @brief  检查MQTT是否连接
  * @param  None
  * @retval true: 已连接, false: 未连接
  */
bool ESP32_IsMQTTConnected(void);

/**
  * @brief  发送心跳保活
  * @param  None
  * @retval true: 成功, false: 失败
  */
bool ESP32_SendHeartbeat(void);

/**
  * @brief  处理接收到的数据
  * @param  None
  * @retval None
  * @note   在UART接收中断中调用
  */
void ESP32_ProcessRxData(void);

/**
  * @brief  复位ESP32模块
  * @param  None
  * @retval None
  */
void ESP32_Reset(void);

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
  */
bool ESP32_SendBinary(const uint8_t* data, uint16_t len);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __ESP32_WIFI_H */
