/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : esp32_wifi.c
  * @brief          : ESP32-C3 WiFi and MQTT communication implementation
  *                   AT command reference: ESP-AT User Guide
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
#include "esp32_wifi.h"
#include "usart.h"
#include "data.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

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

/* ESP32状态 */
ESP32_Status_t g_esp32Status;

/* UART句柄 - 由CubeMX生成 */
extern UART_HandleTypeDef huart5;  /* 使用UART5连接ESP32-C3 */

/* ========== 环形缓冲区定义 ========== */
#define RING_BUFFER_SIZE    1024    /* 环形缓冲区大小 */
#define RING_BUFFER_MASK    (RING_BUFFER_SIZE - 1)

typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;         /* 写入位置 */
    volatile uint16_t tail;         /* 读取位置 */
    volatile uint16_t count;        /* 当前数据量 */
} RingBuffer_t;

static RingBuffer_t rxRingBuffer;   /* 接收环形缓冲区 */
static volatile uint8_t rxLineComplete = 0;  /* 行接收完成标志 */

/* 行缓冲区 - 用于提取完整行 */
static uint8_t lineBuffer[AT_RSP_BUFFER_SIZE];
static volatile uint16_t lineIndex = 0;

/* 旧缓冲区兼容 (逐步替换) */
static uint8_t rxBuffer[AT_RSP_BUFFER_SIZE];
static volatile uint16_t rxIndex = 0;
static volatile uint8_t rxComplete = 0;

/* 发送缓冲区 */
static char txBuffer[AT_CMD_BUFFER_SIZE];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static bool ESP32_WaitForResponse(const char* expected, uint32_t timeoutMs);
static void ESP32_ClearRxBuffer(void);
static void ESP32_SendRaw(const char* data, uint16_t len);
static bool ESP32_SendATWithRetry(const char* cmd, const char* expected, uint32_t timeoutMs, uint8_t retryCount);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/**
  * @brief  初始化环形缓冲区
  */
static void RingBuffer_Init(RingBuffer_t* rb)
{
  memset(rb->buffer, 0, sizeof(rb->buffer));
  rb->head = 0;
  rb->tail = 0;
  rb->count = 0;
}

/**
  * @brief  写入一个字节到环形缓冲区
  * @retval 0:成功, 1:缓冲区满
  */
static uint8_t RingBuffer_Write(RingBuffer_t* rb, uint8_t data)
{
  if (rb->count >= RING_BUFFER_SIZE)
  {
    return 1;  /* 缓冲区满 */
  }
  
  rb->buffer[rb->head] = data;
  rb->head = (rb->head + 1) & RING_BUFFER_MASK;
  rb->count++;
  
  return 0;
}

/**
  * @brief  从环形缓冲区读取一个字节
  * @retval 0:成功, 1:缓冲区空
  */
static uint8_t RingBuffer_Read(RingBuffer_t* rb, uint8_t* data)
{
  if (rb->count == 0)
  {
    return 1;  /* 缓冲区空 */
  }
  
  *data = rb->buffer[rb->tail];
  rb->tail = (rb->tail + 1) & RING_BUFFER_MASK;
  rb->count--;
  
  return 0;
}

/**
  * @brief  获取环形缓冲区当前数据量
  */
static uint16_t RingBuffer_Count(RingBuffer_t* rb)
{
  return rb->count;
}

/**
  * @brief  从环形缓冲区提取一行数据 (以\n结尾)
  * @retval 0:未找到完整行, >0:行长度
  */
static uint16_t RingBuffer_GetLine(RingBuffer_t* rb, uint8_t* line, uint16_t maxLen)
{
  uint16_t i;
  uint16_t count = rb->count;
  uint16_t tempTail = rb->tail;
  
  /* 查找\n */
  for (i = 0; i < count && i < maxLen - 1; i++)
  {
    uint8_t ch = rb->buffer[tempTail];
    tempTail = (tempTail + 1) & RING_BUFFER_MASK;
    
    if (ch == '\n')
    {
      /* 找到行尾，提取数据 */
      uint16_t lineLen = i + 1;
      for (uint16_t j = 0; j < lineLen; j++)
      {
        RingBuffer_Read(rb, &line[j]);
      }
      line[lineLen] = '\0';  /* 添加字符串结束符 */
      return lineLen;
    }
  }
  
  return 0;  /* 未找到完整行 */
}

/* USER CODE END 0 */

/**
  * @brief  清除接收缓冲区
  * @param  None
  * @retval None
  */
static void ESP32_ClearRxBuffer(void)
{
  rxIndex = 0;
  rxComplete = 0;
  memset(rxBuffer, 0, sizeof(rxBuffer));
  
  /* 同时清除环形缓冲区 */
  RingBuffer_Init(&rxRingBuffer);
  lineIndex = 0;
  rxLineComplete = 0;
}

/**
  * @brief  发送原始数据
  * @param  data: 数据指针
  * @param  len: 数据长度
  * @retval None
  */
static void ESP32_SendRaw(const char* data, uint16_t len)
{
  HAL_UART_Transmit(&huart5, (uint8_t*)data, len, ESP32_UART_TIMEOUT_MS);
  g_esp32Status.txCount += len;
}

/**
  * @brief  等待期望的响应
  * @param  expected: 期望的响应字符串
  * @param  timeoutMs: 超时时间(ms)
  * @retval true: 收到期望响应, false: 超时或错误
  */
static bool ESP32_WaitForResponse(const char* expected, uint32_t timeoutMs)
{
  uint32_t startTick = HAL_GetTick();
  
  while ((HAL_GetTick() - startTick) < timeoutMs)
  {
    /* 检查是否收到数据 */
    if (rxComplete)
    {
      rxComplete = 0;
      
      /* 检查是否包含期望的响应 */
      if (strstr((char*)rxBuffer, expected) != NULL)
      {
        return true;
      }
      
      /* 检查是否收到ERROR */
      if (strstr((char*)rxBuffer, "ERROR") != NULL)
      {
        return false;
      }
    }
    
    HAL_Delay(10);
  }
  
  return false;  /* 超时 */
}

/**
  * @brief  发送AT命令（带重试）
  * @param  cmd: AT命令
  * @param  expected: 期望响应
  * @param  timeoutMs: 超时时间
  * @param  retryCount: 重试次数
  * @retval true: 成功, false: 失败
  */
static bool ESP32_SendATWithRetry(const char* cmd, const char* expected, uint32_t timeoutMs, uint8_t retryCount)
{
  for (uint8_t i = 0; i < retryCount; i++)
  {
    ESP32_ClearRxBuffer();
    
    /* 发送命令 */
    ESP32_SendRaw(cmd, strlen(cmd));
    ESP32_SendRaw("\r\n", 2);
    
    /* 等待响应 */
    if (ESP32_WaitForResponse(expected, timeoutMs))
    {
      return true;
    }
    
    /* 重试前延时 */
    HAL_Delay(100);
  }
  
  g_esp32Status.errCount++;
  return false;
}

/**
  * @brief  发送AT命令
  * @param  cmd: AT命令字符串
  * @param  expectedRsp: 期望的响应字符串
  * @param  timeoutMs: 超时时间(ms)
  * @retval true: 成功, false: 失败
  */
bool ESP32_SendATCommand(const char* cmd, const char* expectedRsp, uint32_t timeoutMs)
{
  return ESP32_SendATWithRetry(cmd, expectedRsp, timeoutMs, AT_CMD_RETRY_COUNT);
}

/**
  * @brief  初始化ESP32模块
  * @param  None
  * @retval true: 成功, false: 失败
  */
bool ESP32_Init(void)
{
  g_esp32Status.state = ESP32_STATE_INIT;
  
  /* 初始化环形缓冲区 */
  RingBuffer_Init(&rxRingBuffer);
  lineIndex = 0;
  rxLineComplete = 0;
  
  /* 启动UART接收中断 */
  HAL_UART_Receive_IT(&huart5, (uint8_t*)&huart5.Instance->DR, 1);
  
  /* 第1步：等待ESP32启动 (2-3秒) */
  HAL_Delay(ESP32_BOOT_DELAY_MS);
  
  /* 第2步：发送AT测试命令 */
  if (!ESP32_SendATCommand("AT", "OK", AT_CMD_TIMEOUT_MS))
  {
    g_esp32Status.state = ESP32_STATE_ERROR;
    return false;
  }
  g_esp32Status.state = ESP32_STATE_READY;
  
  /* 关闭回显 */
  ESP32_SendATCommand("ATE0", "OK", AT_CMD_TIMEOUT_MS);
  
  /* 第3步：连接WiFi */
  if (!ESP32_ConnectWiFi(WIFI_SSID, WIFI_PASSWORD))
  {
    g_esp32Status.state = ESP32_STATE_ERROR;
    return false;
  }
  g_esp32Status.state = ESP32_STATE_WIFI_CONNECTED;
  
  /* 第4步：配置MQTT */
  if (!ESP32_ConfigMQTT(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD))
  {
    g_esp32Status.state = ESP32_STATE_ERROR;
    return false;
  }
  
  /* 第5步：连接MQTT Broker */
  if (!ESP32_ConnectMQTT(MQTT_BROKER_IP, MQTT_BROKER_PORT))
  {
    g_esp32Status.state = ESP32_STATE_ERROR;
    return false;
  }
  g_esp32Status.state = ESP32_STATE_MQTT_CONNECTED;
  
  /* 订阅控制主题 */
  ESP32_MQTTSubscribe(MQTT_TOPIC_CONTROL, 1);
  
  return true;
}

/**
  * @brief  连接WiFi热点
  * @param  ssid: WiFi名称
  * @param  password: WiFi密码
  * @retval true: 成功, false: 失败
  */
bool ESP32_ConnectWiFi(const char* ssid, const char* password)
{
  g_esp32Status.state = ESP32_STATE_WIFI_CONNECTING;
  
  /* 设置WiFi模式为STA */
  if (!ESP32_SendATCommand("AT+CWMODE=1", "OK", AT_CMD_TIMEOUT_MS))
  {
    return false;
  }
  
  /* 连接WiFi */
  snprintf(txBuffer, sizeof(txBuffer), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
  
  if (!ESP32_SendATWithRetry(txBuffer, "OK", 20000, AT_CMD_RETRY_COUNT))
  {
    return false;
  }
  
  /* 获取IP地址 */
  ESP32_SendATCommand("AT+CIFSR", "OK", AT_CMD_TIMEOUT_MS);
  
  g_esp32Status.state = ESP32_STATE_WIFI_CONNECTED;
  return true;
}

/**
  * @brief  配置MQTT客户端
  * @param  clientId: 客户端ID
  * @param  username: 用户名
  * @param  password: 密码
  * @retval true: 成功, false: 失败
  */
bool ESP32_ConfigMQTT(const char* clientId, const char* username, const char* password)
{
  /* 配置MQTT用户属性 */
  snprintf(txBuffer, sizeof(txBuffer), 
           "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
           clientId, username, password);
  
  return ESP32_SendATCommand(txBuffer, "OK", AT_CMD_TIMEOUT_MS);
}

/**
  * @brief  连接MQTT Broker
  * @param  brokerIp: Broker IP地址
  * @param  port: 端口号
  * @retval true: 成功, false: 失败
  */
bool ESP32_ConnectMQTT(const char* brokerIp, uint16_t port)
{
  g_esp32Status.state = ESP32_STATE_MQTT_CONNECTING;
  
  /* 连接MQTT Broker */
  snprintf(txBuffer, sizeof(txBuffer),
           "AT+MQTTCONN=0,\"%s\",%d,%d",
           brokerIp, port, MQTT_KEEPALIVE);
  
  if (!ESP32_SendATWithRetry(txBuffer, "OK", 10000, AT_CMD_RETRY_COUNT))
  {
    return false;
  }
  
  g_esp32Status.state = ESP32_STATE_MQTT_CONNECTED;
  return true;
}

/**
  * @brief  发布MQTT消息
  * @param  topic: 主题
  * @param  payload: 消息内容
  * @param  qos: QoS等级
  * @retval true: 成功, false: 失败
  */
bool ESP32_MQTTPublish(const char* topic, const char* payload, uint8_t qos)
{
  /* 计算payload长度 */
  uint16_t payloadLen = strlen(payload);
  
  /* 发送发布命令 */
  snprintf(txBuffer, sizeof(txBuffer),
           "AT+MQTTPUB=0,\"%s\",\"%s\",%d,0",
           topic, payload, qos);
  
  return ESP32_SendATCommand(txBuffer, "OK", AT_CMD_TIMEOUT_MS);
}

/**
  * @brief  订阅MQTT主题
  * @param  topic: 主题
  * @param  qos: QoS等级
  * @retval true: 成功, false: 失败
  */
bool ESP32_MQTTSubscribe(const char* topic, uint8_t qos)
{
  snprintf(txBuffer, sizeof(txBuffer),
           "AT+MQTTSUB=0,\"%s\",%d",
           topic, qos);
  
  return ESP32_SendATCommand(txBuffer, "OK", AT_CMD_TIMEOUT_MS);
}

/**
  * @brief  获取ESP32状态
  * @param  None
  * @retval 状态值
  */
uint8_t ESP32_GetState(void)
{
  return g_esp32Status.state;
}

/**
  * @brief  获取状态字符串
  * @param  None
  * @retval 状态描述字符串
  */
const char* ESP32_GetStateString(void)
{
  switch (g_esp32Status.state)
  {
    case ESP32_STATE_INIT:            return "INIT";
    case ESP32_STATE_READY:           return "READY";
    case ESP32_STATE_WIFI_CONNECTING: return "WIFI_CONNECTING";
    case ESP32_STATE_WIFI_CONNECTED:  return "WIFI_CONNECTED";
    case ESP32_STATE_MQTT_CONNECTING: return "MQTT_CONNECTING";
    case ESP32_STATE_MQTT_CONNECTED:  return "MQTT_CONNECTED";
    case ESP32_STATE_ERROR:           return "ERROR";
    default:                          return "UNKNOWN";
  }
}

/**
  * @brief  检查WiFi是否连接
  * @param  None
  * @retval true: 已连接, false: 未连接
  */
bool ESP32_IsWiFiConnected(void)
{
  return (g_esp32Status.state >= ESP32_STATE_WIFI_CONNECTED);
}

/**
  * @brief  检查MQTT是否连接
  * @param  None
  * @retval true: 已连接, false: 未连接
  */
bool ESP32_IsMQTTConnected(void)
{
  return (g_esp32Status.state == ESP32_STATE_MQTT_CONNECTED);
}

/**
  * @brief  发送心跳保活
  * @param  None
  * @retval true: 成功, false: 失败
  */
bool ESP32_SendHeartbeat(void)
{
  if (!ESP32_IsMQTTConnected())
  {
    return false;
  }
  
  const char* heartbeat = "{\"status\":\"alive\"}";
  return ESP32_MQTTPublish(MQTT_TOPIC_STATUS, heartbeat, 0);
}

/**
  * @brief  从环形缓冲区读取一行数据
  * @param  line: 行缓冲区
  * @param  maxLen: 最大长度
  * @param  timeoutMs: 超时时间
  * @retval >0: 读取到的长度, 0: 超时无数据
  * @note   使用环形缓冲区，非阻塞读取
  */
uint16_t ESP32_ReadLine(uint8_t* line, uint16_t maxLen, uint32_t timeoutMs)
{
  uint32_t tickStart = HAL_GetTick();
  
  while (1)
  {
    /* 尝试从环形缓冲区提取一行 */
    uint16_t len = RingBuffer_GetLine(&rxRingBuffer, line, maxLen);
    if (len > 0)
    {
      return len;
    }
    
    /* 检查超时 */
    if (HAL_GetTick() - tickStart >= timeoutMs)
    {
      return 0;
    }
    
    /* 短暂延时，让出CPU */
    osDelay(1);
  }
}

/**
  * @brief  检查环形缓冲区是否有完整行
  * @retval true: 有完整行, false: 无
  */
bool ESP32_HasLine(void)
{
  uint16_t count = RingBuffer_Count(&rxRingBuffer);
  uint16_t tempTail = rxRingBuffer.tail;
  
  for (uint16_t i = 0; i < count; i++)
  {
    if (rxRingBuffer.buffer[tempTail] == '\n')
    {
      return true;
    }
    tempTail = (tempTail + 1) & RING_BUFFER_MASK;
  }
  
  return false;
}

/**
  * @brief  处理接收到的数据 - 环形缓冲区版本
  * @param  None
  * @retval None
  * @note   在主循环中调用，使用环形缓冲区处理MQTT消息
  */
void ESP32_ProcessRxData(void)
{
  uint8_t tempLine[256];
  
  /* 处理环形缓冲区中的所有完整行 */
  while (ESP32_HasLine())
  {
    if (ESP32_ReadLine(tempLine, sizeof(tempLine), 0) > 0)
    {
      /* 处理接收到的MQTT消息 */
      if (strstr((char*)tempLine, "+MQTTSUBRECV") != NULL)
      {
        /* 解析接收到的消息 */
        /* TODO: 实现消息解析和命令处理 */
      }
      
      /* 可以在这里添加其他消息处理 */
      /* 例如: +MQTTCONN, +MQTTPUB等 */
    }
  }
  
  /* 兼容旧代码 - 清除旧缓冲区 */
  ESP32_ClearRxBuffer();
}

/**
  * @brief  复位ESP32模块
  * @param  None
  * @retval None
  */
void ESP32_Reset(void)
{
  ESP32_SendATCommand("AT+RST", "OK", AT_CMD_TIMEOUT_MS);
  HAL_Delay(ESP32_BOOT_DELAY_MS);
}

/* USER CODE BEGIN 1 */

/**
  * @brief  计算CRC8校验
  * @param  data: 数据指针
  * @param  len: 数据长度
  * @retval CRC8值
  */
static uint8_t CRC8_Calculate(const uint8_t* data, uint16_t len)
{
  uint8_t crc = 0x00;
  for (uint16_t i = 0; i < len; i++)
  {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++)
    {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x31;  /* CRC-8-Dallas/Maxim多项式 */
      else
        crc <<= 1;
    }
  }
  return crc;
}

/**
  * @brief  发送二进制数据到ESP32
  * @param  data: 数据指针
  * @param  len: 数据长度
  * @retval true: 成功, false: 失败
  */
bool ESP32_SendBinary(const uint8_t* data, uint16_t len)
{
  if (len > 256)
  {
    return false;  /* 数据太长 */
  }

  /* 帧头 */
  uint8_t frameHead[2] = {0xAA, 0x55};
  ESP32_SendRaw((char*)frameHead, 2);

  /* 长度 (2字节，小端) */
  uint8_t lenBytes[2] = {len & 0xFF, (len >> 8) & 0xFF};
  ESP32_SendRaw((char*)lenBytes, 2);

  /* 数据 */
  ESP32_SendRaw((char*)data, len);

  /* CRC8校验 */
  uint8_t crc = CRC8_Calculate(data, len);
  ESP32_SendRaw((char*)&crc, 1);

  return true;
}

/**
  * @brief  发送数据帧到ESP32 (二进制格式)
  * @param  None
  * @retval true: 成功, false: 失败
  * @note   数据格式 (32字节):
  *         [pv_voltage(4B)][pv_current(4B)][ac_voltage(4B)][ac_current(4B)]
  *         [temp_front(4B)][temp_rear(4B)][mppt_duty(1B)][fault_code(1B)]
  *         [status(2B)][reserved(4B)]
  */
bool ESP32_SendDataFrame(void)
{
  /* 数据缓冲区 (32字节) */
  uint8_t dataFrame[32];
  uint8_t idx = 0;

  /* 获取互斥锁 */
  extern osMutexId_t globalDataMutex;
  extern MqttDataPacket_t g_mqttData;

  osMutexAcquire(globalDataMutex, osWaitForever);
  {
    /* PV电压 (float转4字节) */
    float pvV = g_mqttData.pv.voltage;
    memcpy(&dataFrame[idx], &pvV, 4);
    idx += 4;

    /* PV电流 */
    float pvI = g_mqttData.pv.current;
    memcpy(&dataFrame[idx], &pvI, 4);
    idx += 4;

    /* 交流电压 */
    float acV = g_mqttData.ac.voltage;
    memcpy(&dataFrame[idx], &acV, 4);
    idx += 4;

    /* 交流电流 */
    float acI = g_mqttData.ac.current;
    memcpy(&dataFrame[idx], &acI, 4);
    idx += 4;

    /* 前级温度 */
    float tempF = g_mqttData.temp.mosfet_front;
    memcpy(&dataFrame[idx], &tempF, 4);
    idx += 4;

    /* 后级温度 */
    float tempR = g_mqttData.temp.mosfet_rear;
    memcpy(&dataFrame[idx], &tempR, 4);
    idx += 4;

    /* MPPT占空比 */
    dataFrame[idx++] = (uint8_t)g_mqttData.mppt.duty;

    /* 故障代码 */
    dataFrame[idx++] = 0;  /* TODO: 从protection获取 */

    /* 状态 (2字节) */
    uint16_t status = 0;
    if (strcmp(g_mqttData.inverter.state, "RUN") == 0)
      status |= 0x01;
    if (strcmp(g_mqttData.mppt.state, "LOCKED") == 0)
      status |= 0x02;
    dataFrame[idx++] = status & 0xFF;
    dataFrame[idx++] = (status >> 8) & 0xFF;

    /* 保留字节 (填充到32字节) */
    while (idx < 32)
    {
      dataFrame[idx++] = 0;
    }
  }
  osMutexRelease(globalDataMutex);

  /* 发送数据帧 */
  return ESP32_SendBinary(dataFrame, 32);
}

/**
  * @brief  UART接收中断回调 - 环形缓冲区版本
  * @param  huart: UART句柄
  * @retval None
  * @note   将接收到的字节写入环形缓冲区
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart5)
  {
    /* 读取接收到的字节 */
    uint8_t rxByte = (uint8_t)(huart->Instance->DR & 0xFF);
    
    /* 写入环形缓冲区 */
    if (RingBuffer_Write(&rxRingBuffer, rxByte) == 0)
    {
      /* 同时写入旧缓冲区兼容 */
      if (rxIndex < AT_RSP_BUFFER_SIZE - 1)
      {
        rxBuffer[rxIndex++] = rxByte;
        rxBuffer[rxIndex] = '\0';
      }
      
      /* 检查是否接收到完整行 (以\n结尾) */
      if (rxByte == '\n')
      {
        rxLineComplete = 1;
        rxComplete = 1;  /* 兼容旧代码 */
      }
    }
    
    /* 重新启动接收 - 继续接收下一个字节 */
    HAL_UART_Receive_IT(&huart5, (uint8_t*)&huart->Instance->DR, 1);
  }
}

/* USER CODE END 1 */
