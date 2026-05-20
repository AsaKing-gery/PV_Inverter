/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : data.h
  * @brief          : MQTT data structure for PV Inverter (Based on AI发布指令.md)
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

#ifndef __DATA_H
#define __DATA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

#define FW_VERSION_LEN          16
#define MPPT_STATE_LEN          16
#define INVERTER_STATE_LEN      16
#define INVERTER_MODE_LEN       16
#define EVENT_CODE_LEN          8
#define EVENT_MESSAGE_LEN       64
#define DEVICE_STATE_LEN        16

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/**
  * @brief  PV输入数据组
  */
typedef struct
{
  float voltage;      /* 光伏输入电压 (V), 范围 0-18V */
  float current;      /* 光伏输入电流 (A), 范围 0-2.5A */
  float power;        /* 光伏输入功率 (W), voltage × current */
} PV_Data_t;

/**
  * @brief  母线数据组
  */
typedef struct
{
  float voltage;      /* 母线电压 (V), 略低于pv电压, 范围 0-16V */
} Bus_Data_t;

/**
  * @brief  交流输出数据组
  */
typedef struct
{
  float voltage;      /* 交流输出电压 (V), 典型 1.8-2.5V */
  float current;      /* 交流输出电流 (A), 范围 0-0.7A */
  float power;        /* 交流输出功率 (W) */
  float frequency;    /* 电网频率 (Hz), 典型 49.8-50.2Hz */
  float pf;           /* 功率因数, 典型 0.95-0.99 */
} AC_Data_t;

/**
  * @brief  温度数据组
  */
typedef struct
{
  float mosfet_front; /* 前级MOSFET温度 (℃), 范围 25-80℃ */
  float mosfet_rear;  /* 后级MOSFET温度 (℃), 比前级高2-8℃, 范围 25-85℃ */
} Temp_Data_t;

/**
  * @brief  MPPT数据组
  */
typedef struct
{
  char state[MPPT_STATE_LEN];     /* 状态: TRACKING / LOCKED / SCANNING */
  float duty;                     /* 占空比 (%), 范围 10-90% */
  float efficiency;               /* 转换效率 (%), 范围 75-92% */
} MPPT_Data_t;

/**
  * @brief  逆变器状态数据组
  */
typedef struct
{
  char state[INVERTER_STATE_LEN]; /* 状态: RUN / STOP / FAULT */
  char mode[INVERTER_MODE_LEN];   /* 模式: ON_GRID / OFF_GRID */
  float thd;                      /* 谐波失真 (%), 范围 2.0-5.0% */
} Inverter_Data_t;

/**
  * @brief  实时数据包 (Topic: pv/devices/PV-INV-001/data)
  */
typedef struct
{
  uint32_t ts;              /* Unix时间戳 (秒) */
  PV_Data_t pv;             /* PV输入数据 */
  Bus_Data_t bus;           /* 母线数据 */
  AC_Data_t ac;             /* 交流输出数据 */
  Temp_Data_t temp;         /* 温度数据 */
  MPPT_Data_t mppt;         /* MPPT数据 */
  Inverter_Data_t inverter; /* 逆变器状态 */
} MqttDataPacket_t;

/**
  * @brief  设备状态数据 (Topic: pv/devices/PV-INV-001/status)
  */
typedef struct
{
  uint32_t ts;                      /* Unix时间戳 (秒) */
  char state[DEVICE_STATE_LEN];     /* 设备状态: ONLINE */
  uint32_t uptime;                  /* 运行时间 (秒) */
  char fw_version[FW_VERSION_LEN];  /* 固件版本 */
  int8_t wifi_rssi;                 /* WiFi信号强度 (dBm), 范围 -40 ~ -75 */
} MqttStatusPacket_t;

/**
  * @brief  事件详情数据 (动态字段，根据事件类型变化)
  */
typedef struct
{
  int8_t wifi_rssi;     /* WiFi信号强度 */
  int8_t threshold;     /* 阈值 */
  float temp_value;     /* 温度值 */
  float temp_threshold; /* 温度阈值 */
  float voltage;        /* 电压值 */
  float voltage_max;    /* 电压上限 */
} EventDetail_t;

/**
  * @brief  事件数据 (Topic: pv/devices/PV-INV-001/event)
  */
typedef struct
{
  uint32_t ts;                      /* Unix时间戳 (秒) */
  char level[8];                    /* 事件级别: INFO / WARN / ERROR / FAULT */
  char code[EVENT_CODE_LEN];        /* 事件编码 */
  char message[EVENT_MESSAGE_LEN];  /* 事件描述 */
  EventDetail_t detail;             /* 事件详情 */
} MqttEventPacket_t;

/* USER CODE END ET */

/* Exported variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* MQTT数据包实例 - 全局访问 */
extern MqttDataPacket_t g_mqttData;     /* 实时数据 */
extern MqttStatusPacket_t g_mqttStatus; /* 状态数据 */
extern MqttEventPacket_t g_mqttEvent;   /* 事件数据 */

/* USER CODE END EV */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

void Data_Init(void);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __DATA_H */
