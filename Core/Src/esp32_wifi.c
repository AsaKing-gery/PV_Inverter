/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : esp32_wifi.c
  * @brief          : ESP32-C3 WiFi and MQTT communication (Simplified Version)
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

/* Includes ------------------------------------------------------------------*/
#include "esp32_wifi.h"
#include "usart.h"
#include "data.h"
#include <string.h>
#include <stdio.h>
#include "cmsis_os2.h"

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
/* static uint8_t lineBuffer[AT_RSP_BUFFER_SIZE]; */ /* 未使用，暂时注释 */
static volatile uint16_t lineIndex = 0;

/* 旧缓冲区兼容 (逐步替换) */
static uint8_t rxBuffer[AT_RSP_BUFFER_SIZE];
static volatile uint16_t rxIndex = 0;
static volatile uint8_t rxComplete = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static void RingBuffer_Init(RingBuffer_t* rb);
static uint8_t RingBuffer_Write(RingBuffer_t* rb, uint8_t data);
static uint8_t RingBuffer_Read(RingBuffer_t* rb, uint8_t* data);
static uint16_t RingBuffer_Count(RingBuffer_t* rb);
static uint16_t RingBuffer_GetLine(RingBuffer_t* rb, uint8_t* line, uint16_t maxLen);

static void ESP32_ClearRxBuffer(void);
static void ESP32_SendRaw(const char* data, uint16_t len);
static bool ESP32_WaitForResponse(const char* expected, uint32_t timeoutMs);

/* CRC8计算 */
static uint8_t CRC8_Calculate(const uint8_t* data, uint16_t len);

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
}

/**
  * @brief  发送原始数据到ESP32
  * @param  data: 数据指针
  * @param  len: 数据长度
  * @retval None
  */
static void ESP32_SendRaw(const char* data, uint16_t len)
{
  if (len == 0 || data == NULL)
  {
    return;
  }
  
  HAL_UART_Transmit(&huart5, (uint8_t*)data, len, ESP32_UART_TIMEOUT_MS);
  g_esp32Status.txCount += len;
}

/**
  * @brief  等待ESP32响应
  * @param  expected: 期望的响应字符串
  * @param  timeoutMs: 超时时间(毫秒)
  * @retval true: 收到期望响应, false: 超时或错误
  */
static bool ESP32_WaitForResponse(const char* expected, uint32_t timeoutMs)
{
  uint32_t startTick = osKernelGetTickCount();
  uint8_t tempLine[AT_RSP_BUFFER_SIZE];
  
  while ((osKernelGetTickCount() - startTick) < timeoutMs)
  {
    /* 从环形缓冲区提取一行 */
    if (RingBuffer_GetLine(&rxRingBuffer, tempLine, sizeof(tempLine)) > 0)
    {
      /* 检查是否包含期望的响应 */
      if (strstr((char*)tempLine, expected) != NULL)
      {
        return true;
      }
      
      /* 检查错误响应 */
      if (strstr((char*)tempLine, "ERROR") != NULL ||
          strstr((char*)tempLine, "FAIL") != NULL)
      {
        return false;
      }
    }
    
    osDelay(10);
  }
  
  return false;  /* 超时 */
}

/**
  * @brief  CRC8校验计算
  * @param  data: 数据指针
  * @param  len: 数据长度
  * @retval CRC8值
  */
static uint8_t CRC8_Calculate(const uint8_t* data, uint16_t len)
{
  uint8_t crc = 0x00;
  uint8_t i;
  
  while (len--)
  {
    crc ^= *data++;
    for (i = 0; i < 8; i++)
    {
      if (crc & 0x80)
      {
        crc = (crc << 1) ^ 0x31;  /* CRC-8多项式 */
      }
      else
      {
        crc <<= 1;
      }
    }
  }
  
  return crc;
}

/* USER CODE END 0 */

/*
 * ============================================================================
 * 简化后的ESP32通信接口
 * ESP32端负责: WiFi连接、MQTT连接、JSON打包
 * STM32端负责: 发送二进制数据帧
 * ============================================================================
 */

/**
  * @brief  初始化ESP32模块 (简化版)
  * @param  None
  * @retval true: 成功, false: 失败
  * @note   只等待ESP32发送READY信号，不进行WiFi/MQTT配置
  */
bool ESP32_Init(void)
{
  g_esp32Status.state = ESP32_STATE_INIT;
  
  /* 初始化环形缓冲区 */
  RingBuffer_Init(&rxRingBuffer);
  rxIndex = 0;
  rxLineComplete = 0;
  
  /* 启动UART接收中断 */
  HAL_UART_Receive_IT(&huart5, (uint8_t*)&huart5.Instance->DR, 1);
  
  /* 等待ESP32启动并发送READY信号 (最多10秒) */
  /* ESP32上电后会自动连接WiFi和MQTT，完成后发送"READY\n" */
  if (!ESP32_WaitForResponse("READY", 10000))
  {
    g_esp32Status.state = ESP32_STATE_ERROR;
    return false;
  }
  
  g_esp32Status.state = ESP32_STATE_CONNECTED;
  return true;
}

/**
  * @brief  发送二进制数据到ESP32
  * @param  data: 数据指针
  * @param  len: 数据长度
  * @retval true: 成功, false: 失败
  * @note   数据格式: [0xAA][0x55][LEN(2B)][DATA][CRC8]
  */
bool ESP32_SendBinary(const uint8_t* data, uint16_t len)
{
  if (len == 0 || data == NULL || len > 256)
  {
    return false;
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
  * @note   数据格式 (72字节):
  *         [ts(4B)][pv_voltage(4B)][pv_current(4B)][pv_power(4B)]
  *         [bus_voltage(4B)][ac_voltage(4B)][ac_current(4B)][ac_power(4B)]
  *         [ac_frequency(4B)][ac_pf(4B)][temp_front(4B)][temp_rear(4B)]
  *         [mppt_duty(4B)][mppt_efficiency(4B)][thd(4B)][grid_freq(4B)]
  *         [grid_phase(4B)][status(4B)][reserved(4B)]
  */
bool ESP32_SendDataFrame(void)
{
  /* 数据缓冲区 (72字节) */
  uint8_t dataFrame[72];
  uint8_t idx = 0;
  
  /* 获取互斥锁 */
  extern osMutexId_t globalDataMutex;
  extern MqttDataPacket_t g_mqttData;
  
  osMutexAcquire(globalDataMutex, osWaitForever);
  {
    /* [0-3] 时间戳 */
    uint32_t timestamp = (uint32_t)(osKernelGetTickCount() / 1000);
    memcpy(&dataFrame[idx], &timestamp, 4);
    idx += 4;
    
    /* [4-7] PV电压 */
    float pvV = g_mqttData.pv.voltage;
    memcpy(&dataFrame[idx], &pvV, 4);
    idx += 4;
    
    /* [8-11] PV电流 */
    float pvI = g_mqttData.pv.current;
    memcpy(&dataFrame[idx], &pvI, 4);
    idx += 4;
    
    /* [12-15] PV功率 */
    float pvP = g_mqttData.pv.power;
    memcpy(&dataFrame[idx], &pvP, 4);
    idx += 4;
    
    /* [16-19] 母线电压 */
    float busV = g_mqttData.bus.voltage;
    memcpy(&dataFrame[idx], &busV, 4);
    idx += 4;
    
    /* [20-23] 交流电压 */
    float acV = g_mqttData.ac.voltage;
    memcpy(&dataFrame[idx], &acV, 4);
    idx += 4;
    
    /* [24-27] 交流电流 */
    float acI = g_mqttData.ac.current;
    memcpy(&dataFrame[idx], &acI, 4);
    idx += 4;
    
    /* [28-31] 交流功率 */
    float acP = g_mqttData.ac.power;
    memcpy(&dataFrame[idx], &acP, 4);
    idx += 4;
    
    /* [32-35] 交流频率 */
    float acFreq = g_mqttData.ac.frequency;
    memcpy(&dataFrame[idx], &acFreq, 4);
    idx += 4;
    
    /* [36-39] 功率因数 */
    float acPf = g_mqttData.ac.pf;
    memcpy(&dataFrame[idx], &acPf, 4);
    idx += 4;
    
    /* [40-43] 前级温度 */
    float tempF = g_mqttData.temp.mosfet_front;
    memcpy(&dataFrame[idx], &tempF, 4);
    idx += 4;
    
    /* [44-47] 后级温度 */
    float tempR = g_mqttData.temp.mosfet_rear;
    memcpy(&dataFrame[idx], &tempR, 4);
    idx += 4;
    
    /* [48-51] MPPT占空比 */
    float mpptDuty = g_mqttData.mppt.duty;
    memcpy(&dataFrame[idx], &mpptDuty, 4);
    idx += 4;
    
    /* [52-55] MPPT效率 */
    float mpptEff = g_mqttData.mppt.efficiency;
    memcpy(&dataFrame[idx], &mpptEff, 4);
    idx += 4;
    
    /* [56-59] THD */
    float thd = g_mqttData.inverter.thd;
    memcpy(&dataFrame[idx], &thd, 4);
    idx += 4;
    
    /* [60-63] 电网频率 */
    float gridFreq = g_mqttData.inverter.gridFreq;
    memcpy(&dataFrame[idx], &gridFreq, 4);
    idx += 4;
    
    /* [64-67] 电网相位 */
    float gridPhase = g_mqttData.inverter.gridPhase;
    memcpy(&dataFrame[idx], &gridPhase, 4);
    idx += 4;
    
    /* [68-71] 状态字 (包含运行状态、MPPT状态、锁相状态、模式等) */
    uint32_t status = 0;
    /* Bit 0-1: 逆变器状态 (0=STOP, 1=RUN, 2=FAULT) */
    if (strcmp(g_mqttData.inverter.state, "RUN") == 0)
      status |= 0x01;
    else if (strcmp(g_mqttData.inverter.state, "FAULT") == 0)
      status |= 0x02;
    /* Bit 2-3: MPPT状态 (0=SCANNING, 1=TRACKING, 2=LOCKED) */
    if (strcmp(g_mqttData.mppt.state, "TRACKING") == 0)
      status |= (0x01 << 2);
    else if (strcmp(g_mqttData.mppt.state, "LOCKED") == 0)
      status |= (0x02 << 2);
    /* Bit 4: 锁相状态 (0=UNSYNC, 1=SYNCED) */
    if (strcmp(g_mqttData.inverter.syncState, "SYNCED") == 0)
      status |= (0x01 << 4);
    /* Bit 5: 运行模式 (0=ON_GRID, 1=OFF_GRID) */
    if (strcmp(g_mqttData.inverter.mode, "OFF_GRID") == 0)
      status |= (0x01 << 5);
    /* Bit 8-15: 故障代码 */
    status |= ((uint32_t)g_mqttData.inverter.faultCode << 8);
    
    memcpy(&dataFrame[idx], &status, 4);
    idx += 4;
    
    /* 保留字节 (填充到72字节) */
    while (idx < 72)
    {
      dataFrame[idx++] = 0;
    }
  }
  osMutexRelease(globalDataMutex);
  
  /* 发送数据帧 */
  return ESP32_SendBinary(dataFrame, 72);
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
    case ESP32_STATE_INIT:       return "INIT";
    case ESP32_STATE_CONNECTED:  return "CONNECTED";
    case ESP32_STATE_ERROR:      return "ERROR";
    default:                     return "UNKNOWN";
  }
}

/**
  * @brief  检查ESP32是否已连接
  * @param  None
  * @retval true: 已连接, false: 未连接
  */
bool ESP32_IsConnected(void)
{
  return (g_esp32Status.state == ESP32_STATE_CONNECTED);
}

/**
  * @brief  复位ESP32模块
  * @param  None
  * @retval true: 成功, false: 失败
  * @note   发送复位命令后等待READY信号
  */
bool ESP32_Reset(void)
{
  g_esp32Status.state = ESP32_STATE_INIT;
  
  /* 发送复位命令 */
  ESP32_SendRaw("RESET\n", 6);
  
  /* 等待READY信号 */
  if (ESP32_WaitForResponse("READY", 10000))
  {
    g_esp32Status.state = ESP32_STATE_CONNECTED;
    return true;
  }
  
  return false;
}

/**
  * @brief  UART接收中断回调 - 环形缓冲区版本
  * @param  huart: UART句柄
  * @retval None
  * @note   将接收到的字节写入环形缓冲区
  */
/* HAL_UART_RxCpltCallback 已合并到rs485_modbus.c中，此函数暴露给外部调用 */
void ESP32_UART_RxHandler(UART_HandleTypeDef *huart)
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

/* USER CODE BEGIN 1 */

/**
  * @brief  检查ESP32 MQTT是否已连接
  * @param  None
  * @retval true: 已连接, false: 未连接
  * @note   通过检查ESP32状态判断MQTT连接状态
  */
bool ESP32_IsMQTTConnected(void)
{
  /* 简化版：只要ESP32状态为CONNECTED，认为MQTT已连接 */
  /* ESP32在上电后会自动连接WiFi和MQTT，成功后发送READY */
  return (g_esp32Status.state == ESP32_STATE_CONNECTED);
}

/**
  * @brief  发送心跳包到ESP32
  * @param  None
  * @retval true: 成功, false: 失败
  * @note   发送简单的心跳字符串，ESP32可据此判断STM32在线状态
  */
bool ESP32_SendHeartbeat(void)
{
  /* 发送心跳命令，ESP32收到后可回复ACK */
  ESP32_SendRaw("HB\n", 3);
  return true;
}

/**
  * @brief  发送故障事件到ESP32
  * @param  faultCode: 故障代码
  * @param  faultData: 附加数据 (如故障时的电压/电流值，放大1000倍)
  * @retval true: 成功, false: 失败
  * @note   事件格式: [0xAA][0x55][LEN(2B)][EVENT_TYPE_FAULT][faultCode][faultData(4B)][CRC8]
  *         ESP32收到后解析并发布到MQTT event主题
  */
bool ESP32_SendEvent(uint8_t faultCode, uint32_t faultData)
{
  if (g_esp32Status.state != ESP32_STATE_CONNECTED)
  {
    return false;
  }
  
  uint8_t eventFrame[16];
  uint8_t idx = 0;
  
  /* 帧头 */
  eventFrame[idx++] = 0xAA;
  eventFrame[idx++] = 0x55;
  
  /* 长度 (1字节类型 + 1字节故障码 + 4字节数据 = 6字节) */
  eventFrame[idx++] = 0x06;
  eventFrame[idx++] = 0x00;
  
  /* 事件类型: 0x01 = 故障事件 */
  eventFrame[idx++] = 0x01;
  
  /* 故障代码 */
  eventFrame[idx++] = faultCode;
  
  /* 附加数据 (4字节) */
  memcpy(&eventFrame[idx], &faultData, 4);
  idx += 4;
  
  /* CRC8校验 */
  uint8_t crc = 0;
  for (uint8_t i = 4; i < idx; i++)
  {
    crc ^= eventFrame[i];
  }
  eventFrame[idx++] = crc;
  
  /* 发送事件帧 */
  ESP32_SendRaw((char*)eventFrame, idx);
  
  return true;
}

/* USER CODE END 1 */
