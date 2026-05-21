/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : rs485_modbus.c
  * @brief          : Minimal Modbus RTU Slave implementation
  *                   - Function 03: Read Holding Registers
  *                   - Function 06: Write Single Register
  *                   - SunSpec Model 1 (Common) & Model 103 (Inverter)
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
#include "rs485_modbus.h"
#include "usart.h"
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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* UART句柄 */
extern UART_HandleTypeDef huart3;

/* 寄存器映射表 */
ModbusRegisterMap_t g_modbusRegs;

/* 接收缓冲区 */
static uint8_t rxBuffer[MODBUS_RX_BUFFER_SIZE];
static volatile uint16_t rxIndex = 0;
static volatile uint8_t frameReady = 0;
static uint32_t lastRxTick = 0;  /* 上次接收时间戳 */

/* 发送缓冲区 */
static uint8_t txBuffer[MODBUS_TX_BUFFER_SIZE];

/* 字节超时时间 (3.5字符间隔 @ 9600bps ≈ 3.6ms) */
#define MODBUS_BYTE_TIMEOUT_MS      5

/* CRC16查找表 (Modbus标准多项式 0xA001) */
static const uint16_t crc16Table[256] = {
  0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
  0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
  0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
  0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
  0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
  0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
  0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
  0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
  0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
  0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
  0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
  0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
  0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
  0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
  0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
  0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
  0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
  0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
  0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
  0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
  0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
  0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
  0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
  0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
  0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
  0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
  0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
  0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
  0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
  0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
  0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
  0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static uint16_t CRC16_Calculate(const uint8_t* data, uint16_t len);
static bool Modbus_ValidateFrame(void);
static void Modbus_SendResponse(uint8_t* data, uint16_t len);
static void Modbus_SendException(uint8_t funcCode, uint8_t exceptionCode);
static void Modbus_HandleReadHoldingRegs(uint16_t addr, uint16_t count);
static void Modbus_HandleWriteSingleReg(uint16_t addr, uint16_t value);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/**
  * @brief  计算CRC16 (Modbus标准)
  */
static uint16_t CRC16_Calculate(const uint8_t* data, uint16_t len)
{
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++)
  {
    crc = (crc >> 8) ^ crc16Table[(crc ^ data[i]) & 0xFF];
  }
  return crc;
}

/**
  * @brief  设置RS485方向
  */
void RS485_SetDirection(bool txMode)
{
  HAL_GPIO_WritePin(RS485_DE_GPIO_PORT, RS485_DE_GPIO_PIN, 
                    txMode ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
  * @brief  验证Modbus帧
  */
static bool Modbus_ValidateFrame(void)
{
  if (rxIndex < 4) return false;  /* 最小帧长度 */
  
  /* 检查从机地址 */
  if (rxBuffer[0] != MODBUS_SLAVE_ADDR) return false;
  
  /* 计算并验证CRC */
  uint16_t rxCrc = (rxBuffer[rxIndex - 1] << 8) | rxBuffer[rxIndex - 2];
  uint16_t calcCrc = CRC16_Calculate(rxBuffer, rxIndex - 2);
  
  return (rxCrc == calcCrc);
}

/**
  * @brief  发送响应帧
  */
static void Modbus_SendResponse(uint8_t* data, uint16_t len)
{
  /* 切换到发送模式 */
  RS485_SetDirection(true);
  
  /* 发送数据 */
  HAL_UART_Transmit(&huart3, data, len, 100);
  
  /* 切换回接收模式 */
  RS485_SetDirection(false);
}

/**
  * @brief  发送异常响应
  */
static void Modbus_SendException(uint8_t funcCode, uint8_t exceptionCode)
{
  txBuffer[0] = MODBUS_SLAVE_ADDR;
  txBuffer[1] = funcCode | 0x80;  /* 设置异常标志 */
  txBuffer[2] = exceptionCode;
  
  /* 计算CRC */
  uint16_t crc = CRC16_Calculate(txBuffer, 3);
  txBuffer[3] = crc & 0xFF;
  txBuffer[4] = (crc >> 8) & 0xFF;
  
  Modbus_SendResponse(txBuffer, 5);
}

/**
  * @brief  处理功能码03 - 读保持寄存器
  */
static void Modbus_HandleReadHoldingRegs(uint16_t addr, uint16_t count)
{
  if (count == 0 || count > 125)  /* 最大125个寄存器 */
  {
    Modbus_SendException(FUNC_READ_HOLDING_REGS, EX_ILLEGAL_DATA_VALUE);
    return;
  }
  
  /* 检查地址范围 */
  if (addr + count > 0x0100)  /* 最大256个寄存器 */
  {
    Modbus_SendException(FUNC_READ_HOLDING_REGS, EX_ILLEGAL_DATA_ADDR);
    return;
  }
  
  /* 构建响应帧 */
  txBuffer[0] = MODBUS_SLAVE_ADDR;
  txBuffer[1] = FUNC_READ_HOLDING_REGS;
  txBuffer[2] = count * 2;  /* 字节数 */
  
  /* 填充寄存器数据 */
  uint16_t* regPtr = (uint16_t*)&g_modbusRegs;
  for (uint16_t i = 0; i < count; i++)
  {
    uint16_t value = regPtr[addr + i];
    txBuffer[3 + i * 2] = (value >> 8) & 0xFF;  /* 高字节 */
    txBuffer[4 + i * 2] = value & 0xFF;          /* 低字节 */
  }
  
  /* 计算CRC */
  uint16_t dataLen = 3 + count * 2;
  uint16_t crc = CRC16_Calculate(txBuffer, dataLen);
  txBuffer[dataLen] = crc & 0xFF;
  txBuffer[dataLen + 1] = (crc >> 8) & 0xFF;
  
  Modbus_SendResponse(txBuffer, dataLen + 2);
}

/**
  * @brief  处理功能码06 - 写单个寄存器
  */
static void Modbus_HandleWriteSingleReg(uint16_t addr, uint16_t value)
{
  /* 检查地址范围 */
  if (addr >= 0x0100)
  {
    Modbus_SendException(FUNC_WRITE_SINGLE_REG, EX_ILLEGAL_DATA_ADDR);
    return;
  }
  
  /* 写入寄存器 */
  uint16_t* regPtr = (uint16_t*)&g_modbusRegs;
  regPtr[addr] = value;
  
  /* 回显响应 */
  txBuffer[0] = MODBUS_SLAVE_ADDR;
  txBuffer[1] = FUNC_WRITE_SINGLE_REG;
  txBuffer[2] = (addr >> 8) & 0xFF;
  txBuffer[3] = addr & 0xFF;
  txBuffer[4] = (value >> 8) & 0xFF;
  txBuffer[5] = value & 0xFF;
  
  /* 计算CRC */
  uint16_t crc = CRC16_Calculate(txBuffer, 6);
  txBuffer[6] = crc & 0xFF;
  txBuffer[7] = (crc >> 8) & 0xFF;
  
  Modbus_SendResponse(txBuffer, 8);
}

/**
  * @brief  解析并处理Modbus帧
  */
static void Modbus_ParseFrame(void)
{
  uint8_t slaveAddr = rxBuffer[0];
  uint8_t funcCode = rxBuffer[1];
  
  /* 只处理本机地址 */
  if (slaveAddr != MODBUS_SLAVE_ADDR) return;
  
  switch (funcCode)
  {
    case FUNC_READ_HOLDING_REGS:
    {
      uint16_t addr = (rxBuffer[2] << 8) | rxBuffer[3];
      uint16_t count = (rxBuffer[4] << 8) | rxBuffer[5];
      Modbus_HandleReadHoldingRegs(addr, count);
      break;
    }
    
    case FUNC_WRITE_SINGLE_REG:
    {
      uint16_t addr = (rxBuffer[2] << 8) | rxBuffer[3];
      uint16_t value = (rxBuffer[4] << 8) | rxBuffer[5];
      Modbus_HandleWriteSingleReg(addr, value);
      break;
    }
    
    default:
      Modbus_SendException(funcCode, EX_ILLEGAL_FUNCTION);
      break;
  }
}

/* USER CODE END 0 */

/**
  * @brief  初始化Modbus从机
  */
void Modbus_Init(void)
{
  /* 初始化DE引脚 */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = RS485_DE_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RS485_DE_GPIO_PORT, &GPIO_InitStruct);
  
  /* 初始状态：接收模式 */
  RS485_SetDirection(false);
  
  /* 初始化寄存器表 */
  memset(&g_modbusRegs, 0, sizeof(g_modbusRegs));
  
  /* SunSpec ID: "Su" "nS" = 0x5375 0x6E53 */
  g_modbusRegs.sunspecId[0] = 0x5375;
  g_modbusRegs.sunspecId[1] = 0x6E53;
  
  /* 通用模型 (Model 1) */
  g_modbusRegs.common.modelId = SUNSPEC_COMMON_MODEL_ID;
  g_modbusRegs.common.modelLen = 65;  /* 32+32+16+32 words / 2 */
  strncpy(g_modbusRegs.common.manufacturer, "PV-Inverter", 32);
  strncpy(g_modbusRegs.common.model, "Micro-300W", 32);
  strncpy(g_modbusRegs.common.version, "V1.0.0", 16);
  strncpy(g_modbusRegs.common.serial, "SN20260001", 32);
  
  /* 逆变器模型 (Model 103) */
  g_modbusRegs.inverter.modelId = SUNSPEC_INVERTER_MODEL_ID;
  g_modbusRegs.inverter.modelLen = 20;
  
  /* 启用UART空闲中断 (帧结束检测) */
  __HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
  
  /* 启动接收中断 */
  HAL_UART_Receive_IT(&huart3, &rxBuffer[rxIndex], 1);
}

/**
  * @brief  检查字节超时 (备用帧结束检测)
  * @note   当空闲中断不可用时，使用字节超时判断帧结束
  *         Modbus RTU标准：3.5字符间隔
  */
static void Modbus_CheckByteTimeout(void)
{
  if (rxIndex > 0 && !frameReady)
  {
    uint32_t elapsed = HAL_GetTick() - lastRxTick;
    if (elapsed >= MODBUS_BYTE_TIMEOUT_MS)
    {
      /* 字节超时，帧结束 */
      frameReady = 1;
    }
  }
}

/**
  * @brief  处理Modbus请求
  */
void Modbus_Process(void)
{
  /* 检查字节超时 (备用方案) */
  Modbus_CheckByteTimeout();
  
  if (frameReady)
  {
    frameReady = 0;
    
    if (Modbus_ValidateFrame())
    {
      Modbus_ParseFrame();
    }
    
    /* 重置接收索引 */
    rxIndex = 0;
    
    /* 重新启动接收 */
    HAL_UART_Receive_IT(&huart3, &rxBuffer[rxIndex], 1);
  }
}

/**
  * @brief  更新寄存器数据
  * @note   从g_mqttData同步到Modbus寄存器
  *         数据格式与data.h定义保持一致
  */
void Modbus_UpdateRegisters(void)
{
  extern MqttDataPacket_t g_mqttData;
  
  /* 从g_mqttData同步到Modbus寄存器 */
  
  /* 交流侧数据 */
  g_modbusRegs.inverter.acPower = (uint16_t)g_mqttData.ac.power;              /* W */
  g_modbusRegs.inverter.acVoltage = (uint16_t)(g_mqttData.ac.voltage * 10);   /* 0.1V */
  g_modbusRegs.inverter.acCurrent = (uint16_t)(g_mqttData.ac.current * 1000); /* 0.001A */
  
  /* 直流侧数据 */
  g_modbusRegs.inverter.dcPower = (uint16_t)g_mqttData.pv.power;              /* W */
  g_modbusRegs.inverter.dcVoltage = (uint16_t)(g_mqttData.pv.voltage * 10);   /* 0.1V */
  g_modbusRegs.inverter.dcCurrent = (uint16_t)(g_mqttData.pv.current * 1000); /* 0.001A */
  
  /* 温度数据 (使用mosfet_front字段) */
  g_modbusRegs.inverter.temp = (uint16_t)(g_mqttData.temp.mosfet_front * 10); /* 0.1°C */
  
  /* 运行状态 (与data.h的inverter.state字段对应) */
  if (strncmp(g_mqttData.inverter.state, "RUN", 3) == 0)
  {
    g_modbusRegs.inverter.status = 1;  /* 运行 */
  }
  else if (strncmp(g_mqttData.inverter.state, "FAULT", 5) == 0)
  {
    g_modbusRegs.inverter.status = 2;  /* 故障 */
  }
  else
  {
    g_modbusRegs.inverter.status = 0;  /* 停止 */
  }
  
  /* 故障码 (从protection模块获取) */
  extern uint8_t Protection_GetFaultCode(void);
  g_modbusRegs.inverter.faultCode = Protection_GetFaultCode();
}

/**
  * @brief  UART空闲中断回调 (帧结束检测)
  */
void HAL_UART_IdleCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart3)
  {
    frameReady = 1;
  }
}

/**
  * @brief  UART接收中断回调
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart3)
  {
    /* 更新接收时间戳 */
    lastRxTick = HAL_GetTick();
    
    rxIndex++;
    
    if (rxIndex < MODBUS_RX_BUFFER_SIZE)
    {
      HAL_UART_Receive_IT(&huart3, &rxBuffer[rxIndex], 1);
    }
    else
    {
      /* 缓冲区溢出，重置 */
      rxIndex = 0;
      HAL_UART_Receive_IT(&huart3, &rxBuffer[rxIndex], 1);
    }
  }
}

/**
  * @brief  UART空闲中断回调 (帧结束检测)
  * @note   当接收到一帧数据后，总线空闲时触发
  */
void HAL_UART_IdleCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart3)
  {
    /* 清除空闲中断标志 */
    __HAL_UART_CLEAR_IDLEFLAG(&huart3);
    
    /* 标记帧就绪 */
    if (rxIndex > 0)
    {
      frameReady = 1;
    }
  }
}
