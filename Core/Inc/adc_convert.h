/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : adc_convert.h
  * @brief          : ADC conversion functions for PV Micro-Inverter
  *                   Hardware: PA0-PA6 -> ADC1_IN0-IN6
  *                   DMA Mode: Scan mode, 7 channels, DMA2 Stream 0 (Circular)
  *                   Current Sensors: ACS712-5A (185mV/A)
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

#ifndef __ADC_CONVERT_H
#define __ADC_CONVERT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* ADC硬件配置参数 */
#define ADC_VREF                    3.3f        /* ADC参考电压 (V) */
#define ADC_RESOLUTION              4096.0f     /* 12位ADC分辨率 */
#define ADC_MAX_VALUE               4095        /* ADC最大原始值 */

/* DMA缓冲区大小 - 7个通道 */
#define ADC_CHANNEL_NUM             7           /* ADC通道数量 */
#define ADC_DMA_BUFFER_SIZE         (ADC_CHANNEL_NUM * 4)  /* 4组采样，用于滤波 */

/* 电压分压系数 (10kΩ + 1kΩ 电阻分压) */
#define VOLTAGE_DIVIDER_RATIO       11.0f

/* ACS712-5A电流传感器参数 */
#define ACS712_VOUT_ZERO            2.5f        /* 零电流输出电压 (V) */
#define ACS712_SENSITIVITY          0.185f      /* 灵敏度: 185mV/A (ACS712-5A) */
#define ACS712_CURRENT_RANGE        5.0f        /* 电流量程: ±5A */

/* NTC B3950热敏电阻参数 (10kΩ 3950) */
#define NTC_B_VALUE                 3950.0f     /* B值 (K) */
#define NTC_R25                     10000.0f    /* 25℃时阻值 10kΩ */
#define NTC_T25                     298.15f     /* 25℃绝对温度 (K) */
#define NTC_PULLUP_RESISTOR         10000.0f    /* 上拉电阻 10kΩ */

/* 数字滤波参数 */
#define ADC_FILTER_COEF             0.2f        /* 一阶低通滤波系数 (0-1) */
#define ADC_NOISE_THRESHOLD         0.02f       /* 电流噪声阈值 (A), 约20mA */

/* ACS712滑动窗口滤波参数 */
#define ACS712_WINDOW_SIZE          8           /* 滑动窗口大小 (2的幂次，便于计算) */

/* 温度限值 */
#define TEMP_MIN_VALID              -40.0f      /* 最低有效温度 (℃) */
#define TEMP_MAX_VALID              125.0f      /* 最高有效温度 (℃) */

/* ZMPT101B交流电压计算 */
#define ZMPT101B_SAMPLE_CYCLES      4           /* 4个周期用于计算RMS */

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/**
  * @brief  ADC通道索引 (对应DMA缓冲区顺序)
  * @note   顺序必须与CubeMX中ADC通道Rank配置一致
  */
typedef enum
{
  ADC_IDX_PV_VOLTAGE = 0,      /**< Rank 1: PA0 - ADC1_IN0 - 光伏输入电压 */
  ADC_IDX_PV_CURRENT,          /**< Rank 2: PA1 - ADC1_IN1 - 光伏输入电流 (ACS712-5A) */
  ADC_IDX_BUS_VOLTAGE,         /**< Rank 3: PA2 - ADC1_IN2 - 直流母线电压 */
  ADC_IDX_AC_CURRENT,          /**< Rank 4: PA3 - ADC1_IN3 - 后级输出电流 (ACS712-5A) */
  ADC_IDX_AC_VOLTAGE,          /**< Rank 5: PA4 - ADC1_IN4 - 交流输出电压 (ZMPT101B) */
  ADC_IDX_TEMP_FRONT,          /**< Rank 6: PA5 - ADC1_IN5 - 前级NTC温度 */
  ADC_IDX_TEMP_REAR,           /**< Rank 7: PA6 - ADC1_IN6 - 后级NTC温度 */
  ADC_IDX_COUNT
} ADC_Index_t;

/**
  * @brief  ADC校准参数结构体
  */
typedef struct
{
  float offset;                /**< 零点偏移电压 (V) */
  float gain;                  /**< 增益系数 */
  float ratio;                 /**< 分压比/变比/灵敏度 */
} ADC_Calibration_t;

/* USER CODE END ET */

/* Exported variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/**
  * @brief  DMA缓冲区
  * @note   大小为 ADC_DMA_BUFFER_SIZE = 7通道 × 4组
  * @note   由DMA循环写入，主程序读取
  */
extern volatile uint16_t g_adcDMABuffer[ADC_DMA_BUFFER_SIZE];

/**
  * @brief  DMA半传输完成标志
  */
extern volatile uint8_t g_adcDMAHalfFlag;

/**
  * @brief  DMA传输完成标志
  */
extern volatile uint8_t g_adcDMAFullFlag;

/**
  * @brief  ADC校准参数数组
  */
extern ADC_Calibration_t g_adcCal[ADC_IDX_COUNT];

/* USER CODE END EV */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/*================== DMA管理函数 ==================*/

/**
  * @brief  启动ADC DMA循环采样
  * @param  None
  * @retval None
  * @note   CubeMX已配置DMA，直接启动
  * @note   DMA为循环模式，持续采样7个通道
  */
void ADC_DMA_Start(void);

/**
  * @brief  停止ADC DMA采样
  * @param  None
  * @retval None
  */
void ADC_DMA_Stop(void);

/**
  * @brief  处理DMA半传输完成中断
  * @param  None
  * @retval None
  * @note   在半传输完成回调中调用
  */
void ADC_DMA_ProcessHalf(void);

/**
  * @brief  处理DMA传输完成中断
  * @param  None
  * @retval None
  * @note   在传输完成回调中调用
  */
void ADC_DMA_ProcessFull(void);

/*================== 数据获取函数 ==================*/

/**
  * @brief  获取指定通道的原始值 (从DMA缓冲区)
  * @param  index: 通道索引 (ADC_Index_t)
  * @retval ADC原始值 (0-4095)
  * @note   返回最近4次采样的平均值
  */
uint16_t ADC_GetRawValue(ADC_Index_t index);

/**
  * @brief  获取指定通道的电压值
  * @param  index: 通道索引
  * @retval 电压值 (V), 范围 0-3.3V
  */
float ADC_GetChannelVoltage(ADC_Index_t index);

/*================== 物理量测量函数 ==================*/

/**
  * @brief  获取光伏输入电压
  * @param  raw: ADC原始值 (0-4095)
  * @retval 光伏电压 (V), 范围 0-36.3V
  * @note   硬件: PA0, 分压比 11
  */
float ADC_GetPVVoltage(uint16_t raw);

/**
  * @brief  获取直流母线电压
  * @param  raw: ADC原始值 (0-4095)
  * @retval 母线电压 (V), 范围 0-36.3V
  * @note   硬件: PA2, 分压比 11
  */
float ADC_GetBusVoltage(uint16_t raw);

/**
  * @brief  获取交流输出电压有效值
  * @param  raw: ADC原始值 (0-4095)
  * @retval 交流电压有效值 (V)
  * @note   硬件: PA4, ZMPT101B
  * @note   使用4个周期的采样数据计算RMS
  */
float ADC_GetACVoltage_RMS(uint16_t raw);

/**
  * @brief  获取光伏输入电流
  * @param  raw: ADC原始值 (0-4095)
  * @retval 光伏电流 (A), 范围 ±5A
  * @note   硬件: PA1, ACS712-5A (185mV/A)
  */
float ADC_GetPVCurrent(uint16_t raw);

/**
  * @brief  获取交流输出电流
  * @param  raw: ADC原始值 (0-4095)
  * @retval 交流电流 (A), 范围 ±5A
  * @note   硬件: PA3, ACS712-5A (185mV/A)
  */
float ADC_GetACCurrent(uint16_t raw);

/**
  * @brief  获取前级MOSFET温度
  * @param  raw: ADC原始值 (0-4095)
  * @retval 温度值 (℃), 范围 -40~125℃
  * @note   硬件: PA5, NTC B3950
  */
float ADC_GetFrontTemperature(uint16_t raw);

/**
  * @brief  获取后级MOSFET温度
  * @param  raw: ADC原始值 (0-4095)
  * @retval 温度值 (℃), 范围 -40~125℃
  * @note   硬件: PA6, NTC B3950
  */
float ADC_GetRearTemperature(uint16_t raw);

/*================== 校准与滤波函数 ==================*/

/**
  * @brief  初始化ADC校准参数
  * @retval None
  */
void ADC_CalibrationInit(void);

/**
  * @brief  设置指定通道的校准参数
  * @param  index: 通道索引
  * @param  offset: 零点偏移
  * @param  gain: 增益系数
  * @param  ratio: 分压比/变比/灵敏度
  */
void ADC_SetCalibration(ADC_Index_t index, float offset, float gain, float ratio);

/**
  * @brief  零点校准 (电流传感器用)
  * @param  index: 通道索引
  * @note   校准时确保无电流通过传感器！
  */
void ADC_ZeroCalibration(ADC_Index_t index);

/**
  * @brief  一阶低通滤波器
  * @param  newValue: 新采样值
  * @param  oldValue: 上次滤波值
  * @param  coef: 滤波系数
  * @retval 滤波后的值
  */
float ADC_LowPassFilter(float newValue, float oldValue, float coef);

/**
  * @brief  ACS712滑动窗口滤波器
  * @param  index: 电流通道索引 (ADC_IDX_PV_CURRENT 或 ADC_IDX_AC_CURRENT)
  * @param  newValue: 新采样值
  * @retval 滤波后的值
  * @note   滑动窗口取平均，对WiFi瞬态干扰等脉冲噪声抑制效果优于低通滤波
  */
float ACS712_SlidingWindowFilter(ADC_Index_t index, float newValue);

/*================== 批量更新函数 ==================*/

/**
  * @brief  批量更新所有ADC数据到MQTT结构
  * @retval None
  * @note   建议在控制任务中定期调用
  */
void ADC_UpdateAllData(void);

/**
  * @brief  更新电压电流数据
  * @retval None
  */
void ADC_UpdateVoltageCurrent(void);

/**
  * @brief  更新温度数据
  * @retval None
  */
void ADC_UpdateTemperature(void);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_CONVERT_H */
