/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : rs485_modbus.h
  * @brief          : Minimal Modbus RTU Slave for SunSpec support
  *                   Supports Function Code 03 (Read Holding Registers)
  *                   and Function Code 06 (Write Single Register)
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

#ifndef __RS485_MODBUS_H
#define __RS485_MODBUS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* Modbus配置 */
#define MODBUS_SLAVE_ADDR           0x01    /* 从机地址 */
#define MODBUS_BAUDRATE             9600    /* Modbus标准波特率 */

/* RS485方向控制引脚 */
#define RS485_DE_GPIO_PORT          GPIOE
#define RS485_DE_GPIO_PIN           GPIO_PIN_5

/* 缓冲区大小 */
#define MODBUS_RX_BUFFER_SIZE       256
#define MODBUS_TX_BUFFER_SIZE       256

/* SunSpec模型定义 */
#define SUNSPEC_COMMON_MODEL_ID     1       /* 通用模型 */
#define SUNSPEC_INVERTER_MODEL_ID   103     /* 逆变器模型 */

/* 寄存器地址映射 */
#define REG_SUNSPEC_ID              0x0000  /* SunSpec ID (0x53756E53) */
#define REG_COMMON_MODEL_START      0x0002  /* 通用模型起始 */
#define REG_INVERTER_MODEL_START    0x0040  /* 逆变器模型起始 */

/* 功能码 */
#define FUNC_READ_HOLDING_REGS      0x03    /* 读保持寄存器 */
#define FUNC_WRITE_SINGLE_REG       0x06    /* 写单个寄存器 */

/* 异常码 */
#define EX_ILLEGAL_FUNCTION         0x01
#define EX_ILLEGAL_DATA_ADDR        0x02
#define EX_ILLEGAL_DATA_VALUE       0x03

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/**
  * @brief  SunSpec通用模型 (Model 1)
  */
typedef struct
{
  uint16_t modelId;           /* 模型ID = 1 */
  uint16_t modelLen;          /* 模型长度 */
  char manufacturer[32];      /* 制造商 */
  char model[32];             /* 型号 */
  char version[16];           /* 固件版本 */
  char serial[32];            /* 序列号 */
} SunSpecCommonModel_t;

/**
  * @brief  SunSpec逆变器模型 (Model 103)
  */
typedef struct
{
  uint16_t modelId;           /* 模型ID = 103 */
  uint16_t modelLen;          /* 模型长度 */
  uint16_t acPower;           /* 交流功率 (W) */
  uint16_t acVoltage;         /* 交流电压 (V) */
  uint16_t acCurrent;         /* 交流电流 (0.01A) */
  uint16_t dcPower;           /* 直流功率 (W) */
  uint16_t dcVoltage;         /* 直流电压 (0.1V) */
  uint16_t dcCurrent;         /* 直流电流 (0.01A) */
  uint16_t temp;              /* 温度 (0.1°C) */
  uint16_t status;            /* 运行状态 */
  uint16_t faultCode;         /* 故障码 */
} SunSpecInverterModel_t;

/**
  * @brief  Modbus寄存器表
  */
typedef struct
{
  /* SunSpec头部 */
  uint16_t sunspecId[2];      /* "Su" "nS" */
  
  /* 通用模型 (Model 1) */
  SunSpecCommonModel_t common;
  
  /* 逆变器模型 (Model 103) */
  SunSpecInverterModel_t inverter;
  
} ModbusRegisterMap_t;

/* USER CODE END ET */

/* Exported variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

extern ModbusRegisterMap_t g_modbusRegs;

/* USER CODE END EV */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/**
  * @brief  初始化Modbus从机
  * @param  None
  * @retval None
  */
void Modbus_Init(void);

/**
  * @brief  处理Modbus请求 (在任务中调用)
  * @param  None
  * @retval None
  */
void Modbus_Process(void);

/**
  * @brief  更新寄存器数据 (从g_mqttData同步)
  * @param  None
  * @retval None
  */
void Modbus_UpdateRegisters(void);

/**
  * @brief  设置RS485方向
  * @param  txMode: true=发送模式, false=接收模式
  * @retval None
  */
void RS485_SetDirection(bool txMode);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __RS485_MODBUS_H */
