/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : adc_convert.c
  * @brief          : ADC conversion for PV Micro-Inverter
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

/* Includes ------------------------------------------------------------------*/
#include "adc_convert.h"
#include "data.h"
#include "adc.h"
#include "dma.h"
#include "cmsis_os2.h"
#include <math.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* ZMPT101B交流电压传感器参数 */
#define ZMPT101B_VREF               1.65f       /* 交流信号偏置电压中点 (Vref/2) */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* ADC句柄 - 由CubeMX生成 */
extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;

/* DMA缓冲区 - 7通道 × 4组 = 28个元素 */
/* 布局: [CH0,CH1,CH2,CH3,CH4,CH5,CH6] × 4组 */
volatile uint16_t g_adcDMABuffer[ADC_DMA_BUFFER_SIZE];

/* ADC原始值缓冲区 - 双缓冲处理后存储 */
volatile uint16_t g_adcRawValue[ADC_IDX_COUNT];

/* DMA传输标志 */
volatile uint8_t g_adcDMAHalfFlag = 0;
volatile uint8_t g_adcDMAFullFlag = 0;

/* 校准参数数组 */
ADC_Calibration_t g_adcCal[ADC_IDX_COUNT];

/* 滤波值缓存 */
static float s_filteredValue[ADC_IDX_COUNT] = {0};

/* ACS712滑动窗口缓冲区 */
static float s_acs712Window[ADC_IDX_COUNT][ACS712_WINDOW_SIZE];
static uint8_t s_acs712WindowIdx[ADC_IDX_COUNT] = {0};
static uint8_t s_acs712WindowFilled[ADC_IDX_COUNT] = {0};

/* ZMPT101B交流电压采样缓冲区 (用于RMS计算) */
static float s_acVoltageBuffer[ZMPT101B_SAMPLE_CYCLES];
static uint8_t s_acVoltageIndex = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static float ACS712_ConvertToCurrent(float voltage);
static float NTC_ConvertToTemp(uint16_t adcRaw);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  初始化ADC校准参数
  * @param  None
  * @retval None
  * @note   设置各通道默认校准值
  */
void ADC_CalibrationInit(void)
{
  /* PA0 - 光伏电压: 分压比11 */
  g_adcCal[ADC_IDX_PV_VOLTAGE].offset = 0.0f;
  g_adcCal[ADC_IDX_PV_VOLTAGE].gain = 1.0f;
  g_adcCal[ADC_IDX_PV_VOLTAGE].ratio = VOLTAGE_DIVIDER_RATIO;

  /* PA1 - 光伏电流: ACS712-5A, 185mV/A */
  g_adcCal[ADC_IDX_PV_CURRENT].offset = ACS712_VOUT_ZERO;
  g_adcCal[ADC_IDX_PV_CURRENT].gain = 1.0f;
  g_adcCal[ADC_IDX_PV_CURRENT].ratio = ACS712_SENSITIVITY;

  /* PA2 - 母线电压: 分压比11 */
  g_adcCal[ADC_IDX_BUS_VOLTAGE].offset = 0.0f;
  g_adcCal[ADC_IDX_BUS_VOLTAGE].gain = 1.0f;
  g_adcCal[ADC_IDX_BUS_VOLTAGE].ratio = VOLTAGE_DIVIDER_RATIO;

  /* PA3 - 交流电流: ACS712-5A, 185mV/A */
  g_adcCal[ADC_IDX_AC_CURRENT].offset = ACS712_VOUT_ZERO;
  g_adcCal[ADC_IDX_AC_CURRENT].gain = 1.0f;
  g_adcCal[ADC_IDX_AC_CURRENT].ratio = ACS712_SENSITIVITY;

  /* PA4 - 交流电压: ZMPT101B, 需要校准 */
  g_adcCal[ADC_IDX_AC_VOLTAGE].offset = ZMPT101B_VREF;
  g_adcCal[ADC_IDX_AC_VOLTAGE].gain = 1.0f;
  g_adcCal[ADC_IDX_AC_VOLTAGE].ratio = 220.0f;  /* 待校准 */

  /* PA5 - 前级温度: NTC B3950 */
  g_adcCal[ADC_IDX_TEMP_FRONT].offset = 0.0f;
  g_adcCal[ADC_IDX_TEMP_FRONT].gain = 1.0f;
  g_adcCal[ADC_IDX_TEMP_FRONT].ratio = 1.0f;

  /* PA6 - 后级温度: NTC B3950 */
  g_adcCal[ADC_IDX_TEMP_REAR].offset = 0.0f;
  g_adcCal[ADC_IDX_TEMP_REAR].gain = 1.0f;
  g_adcCal[ADC_IDX_TEMP_REAR].ratio = 1.0f;
}

/**
  * @brief  设置指定通道的校准参数
  * @param  index: 通道索引
  * @param  offset: 零点偏移
  * @param  gain: 增益系数
  * @param  ratio: 分压比/变比/灵敏度
  * @retval None
  */
void ADC_SetCalibration(ADC_Index_t index, float offset, float gain, float ratio)
{
  if (index < ADC_IDX_COUNT)
  {
    g_adcCal[index].offset = offset;
    g_adcCal[index].gain = gain;
    g_adcCal[index].ratio = ratio;
  }
}

/**
  * @brief  零点校准 (电流传感器用)
  * @param  index: 通道索引
  * @retval None
  * @note   校准时确保无电流通过传感器！
  */
void ADC_ZeroCalibration(ADC_Index_t index)
{
  if (index >= ADC_IDX_COUNT)
  {
    return;
  }

  /* 采样多次取平均 */
  uint32_t sum = 0;
  const uint8_t sampleCount = 16;

  for (uint8_t i = 0; i < sampleCount; i++)
  {
    sum += ADC_GetRawValue(index);
    HAL_Delay(1);
  }

  uint16_t avgRaw = sum / sampleCount;
  float voltage = ((float)avgRaw * ADC_VREF) / ADC_RESOLUTION;

  g_adcCal[index].offset = voltage;
}

/**
  * @brief  启动ADC DMA循环采样
  * @param  None
  * @retval None
  * @note   CubeMX已配置DMA，直接启动
  */
void ADC_DMA_Start(void)
{
  /* 清空缓冲区 */
  memset((void*)g_adcDMABuffer, 0, sizeof(g_adcDMABuffer));

  /* 启动DMA循环传输 */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_adcDMABuffer, ADC_DMA_BUFFER_SIZE);
}

/**
  * @brief  停止ADC DMA采样
  * @param  None
  * @retval None
  */
void ADC_DMA_Stop(void)
{
  HAL_ADC_Stop_DMA(&hadc1);
}

/**
  * @brief  处理DMA半传输完成 - 双缓冲前半部分
  * @param  None
  * @retval None
  * @note   在半传输完成回调中调用
  *         前半缓冲区: g_adcDMABuffer[0] ~ g_adcDMABuffer[13]
  *         此时DMA正在写入后半缓冲区，可以安全读取前半
  */
void ADC_DMA_ProcessHalf(void)
{
  /* 处理前半缓冲区数据 (7通道 × 2组 = 14个样本) */
  /* 注意：只处理2组而不是4组，减少单次处理时间 */
  for (uint8_t ch = 0; ch < ADC_IDX_COUNT; ch++)
  {
    uint32_t sum = 0;
    /* 前半缓冲区布局: [CH0,CH1,CH2,CH3,CH4,CH5,CH6] × 2组 */
    for (uint8_t i = 0; i < 2; i++)
    {
      sum += g_adcDMABuffer[ch + i * ADC_IDX_COUNT];
    }
    g_adcRawValue[ch] = sum / 2;  /* 取平均值 */
  }
  
  g_adcDMAHalfFlag = 1;
}

/**
  * @brief  处理DMA传输完成 - 双缓冲后半部分
  * @param  None
  * @retval None
  * @note   在传输完成回调中调用
  *         后半缓冲区: g_adcDMABuffer[14] ~ g_adcDMABuffer[27]
  *         此时DMA正在写入前半缓冲区(循环模式)，可以安全读取后半
  */
void ADC_DMA_ProcessFull(void)
{
  /* 处理后半缓冲区数据 (7通道 × 2组 = 14个样本) */
  for (uint8_t ch = 0; ch < ADC_IDX_COUNT; ch++)
  {
    uint32_t sum = 0;
    /* 后半缓冲区布局: [CH0,CH1,CH2,CH3,CH4,CH5,CH6] × 2组 */
    for (uint8_t i = 0; i < 2; i++)
    {
      sum += g_adcDMABuffer[ch + 14 + i * ADC_IDX_COUNT];  /* 偏移14 */
    }
    g_adcRawValue[ch] = sum / 2;  /* 取平均值 */
  }
  
  g_adcDMAFullFlag = 1;
}

/**
  * @brief  获取指定通道的原始值 (双缓冲处理后)
  * @param  index: 通道索引
  * @retval ADC原始值 (0-4095)
  * @note   返回双缓冲处理后的值，无需再计算平均值
  *         数据在DMA半传输/全传输中断中已处理
  */
uint16_t ADC_GetRawValue(ADC_Index_t index)
{
  if (index >= ADC_IDX_COUNT)
  {
    return 0;
  }

  /* 直接返回双缓冲处理后的值 */
  /* 数据在ADC_DMA_ProcessHalf/ProcessFull中已计算平均值 */
  return g_adcRawValue[index];
}

/**
  * @brief  获取指定通道的电压值
  * @param  index: 通道索引
  * @retval 电压值 (V)
  */
float ADC_GetChannelVoltage(ADC_Index_t index)
{
  uint16_t raw = ADC_GetRawValue(index);
  return ((float)raw * ADC_VREF) / ADC_RESOLUTION;
}

/**
  * @brief  一阶低通滤波器
  * @param  newValue: 新采样值
  * @param  oldValue: 上次滤波值
  * @param  coef: 滤波系数
  * @retval 滤波后的值
  */
float ADC_LowPassFilter(float newValue, float oldValue, float coef)
{
  return (coef * newValue) + ((1.0f - coef) * oldValue);
}

/**
  * @brief  ACS712滑动窗口滤波器
  * @param  index: 电流通道索引 (ADC_IDX_PV_CURRENT 或 ADC_IDX_AC_CURRENT)
  * @param  newValue: 新采样值
  * @retval 滤波后的值
  * @note   滑动窗口取平均，对WiFi瞬态干扰等脉冲噪声抑制效果优于低通滤波
  *         窗口大小 ACS712_WINDOW_SIZE=8，约80ms延迟 (10ms×8)
  */
float ACS712_SlidingWindowFilter(ADC_Index_t index, float newValue)
{
  if (index >= ADC_IDX_COUNT)
  {
    return newValue;
  }

  /* 用新值替换最旧的值 (环形缓冲) */
  s_acs712Window[index][s_acs712WindowIdx[index]] = newValue;
  s_acs712WindowIdx[index] = (s_acs712WindowIdx[index] + 1) % ACS712_WINDOW_SIZE;

  /* 窗口未填满时标记 */
  if (s_acs712WindowFilled[index] < ACS712_WINDOW_SIZE)
  {
    s_acs712WindowFilled[index]++;
  }

  /* 计算窗口平均值 */
  float sum = 0.0f;
  for (uint8_t i = 0; i < s_acs712WindowFilled[index]; i++)
  {
    sum += s_acs712Window[index][i];
  }

  return sum / (float)s_acs712WindowFilled[index];
}

/**
  * @brief  ACS712电压转电流 (内部函数)
  * @param  voltage: ACS712输出电压 (V)
  * @retval 电流值 (A)
  */
static float ACS712_ConvertToCurrent(float voltage)
{
  float current = (voltage - ACS712_VOUT_ZERO) / ACS712_SENSITIVITY;

  /* 噪声抑制 */
  if (fabsf(current) < ADC_NOISE_THRESHOLD)
  {
    current = 0.0f;
  }

  /* 限幅 */
  if (current > ACS712_CURRENT_RANGE)
  {
    current = ACS712_CURRENT_RANGE;
  }
  else if (current < -ACS712_CURRENT_RANGE)
  {
    current = -ACS712_CURRENT_RANGE;
  }

  return current;
}

/**
  * @brief  NTC温度转换 (内部函数)
  * @param  adcRaw: ADC原始值
  * @retval 温度值 (℃)
  */
static float NTC_ConvertToTemp(uint16_t adcRaw)
{
  if (adcRaw == 0 || adcRaw >= ADC_MAX_VALUE)
  {
    return 25.0f;
  }

  float r_ntc = NTC_PULLUP_RESISTOR * ((ADC_RESOLUTION - (float)adcRaw) / (float)adcRaw);

  if (r_ntc <= 0)
  {
    return 25.0f;
  }

  float ln_r = logf(r_ntc / NTC_R25);
  float invT = (1.0f / NTC_T25) + (ln_r / NTC_B_VALUE);
  float tempK = 1.0f / invT;
  float tempC = tempK - 273.15f;

  if (tempC < TEMP_MIN_VALID || tempC > TEMP_MAX_VALID)
  {
    return 25.0f;
  }

  return tempC;
}

/**
  * @brief  获取光伏输入电压
  * @retval 光伏电压 (V)
  */
float ADC_GetPVVoltage(uint16_t raw)
{
  float voltage = ((float)raw * ADC_VREF) / ADC_RESOLUTION;
  voltage = (voltage - g_adcCal[ADC_IDX_PV_VOLTAGE].offset) * g_adcCal[ADC_IDX_PV_VOLTAGE].gain;
  float result = voltage * g_adcCal[ADC_IDX_PV_VOLTAGE].ratio;

  s_filteredValue[ADC_IDX_PV_VOLTAGE] = ADC_LowPassFilter(result, s_filteredValue[ADC_IDX_PV_VOLTAGE], ADC_FILTER_COEF);
  return s_filteredValue[ADC_IDX_PV_VOLTAGE];
}

/**
  * @brief  获取直流母线电压
  * @retval 母线电压 (V)
  */
float ADC_GetBusVoltage(uint16_t raw)
{
  float voltage = ((float)raw * ADC_VREF) / ADC_RESOLUTION;
  voltage = (voltage - g_adcCal[ADC_IDX_BUS_VOLTAGE].offset) * g_adcCal[ADC_IDX_BUS_VOLTAGE].gain;
  float result = voltage * g_adcCal[ADC_IDX_BUS_VOLTAGE].ratio;

  s_filteredValue[ADC_IDX_BUS_VOLTAGE] = ADC_LowPassFilter(result, s_filteredValue[ADC_IDX_BUS_VOLTAGE], ADC_FILTER_COEF);
  return s_filteredValue[ADC_IDX_BUS_VOLTAGE];
}

/**
  * @brief  获取光伏输入电流
  * @retval 光伏电流 (A)
  */
float ADC_GetPVCurrent(uint16_t raw)
{
  float voltage = ((float)raw * ADC_VREF) / ADC_RESOLUTION;
  float current = ACS712_ConvertToCurrent(voltage);

  s_filteredValue[ADC_IDX_PV_CURRENT] = ACS712_SlidingWindowFilter(ADC_IDX_PV_CURRENT, current);
  return s_filteredValue[ADC_IDX_PV_CURRENT];
}

/**
  * @brief  获取交流输出电流
  * @retval 交流电流 (A)
  */
float ADC_GetACCurrent(uint16_t raw)
{
  float voltage = ((float)raw * ADC_VREF) / ADC_RESOLUTION;
  float current = ACS712_ConvertToCurrent(voltage);

  s_filteredValue[ADC_IDX_AC_CURRENT] = ACS712_SlidingWindowFilter(ADC_IDX_AC_CURRENT, current);
  return s_filteredValue[ADC_IDX_AC_CURRENT];
}

/**
  * @brief  获取前级MOSFET温度
  * @retval 温度值 (℃)
  */
float ADC_GetFrontTemperature(uint16_t raw)
{
  float temp = NTC_ConvertToTemp(raw);

  s_filteredValue[ADC_IDX_TEMP_FRONT] = ADC_LowPassFilter(temp, s_filteredValue[ADC_IDX_TEMP_FRONT], ADC_FILTER_COEF);
  return s_filteredValue[ADC_IDX_TEMP_FRONT];
}

/**
  * @brief  获取后级MOSFET温度
  * @retval 温度值 (℃)
  */
float ADC_GetRearTemperature(uint16_t raw)
{
  float temp = NTC_ConvertToTemp(raw);

  s_filteredValue[ADC_IDX_TEMP_REAR] = ADC_LowPassFilter(temp, s_filteredValue[ADC_IDX_TEMP_REAR], ADC_FILTER_COEF);
  return s_filteredValue[ADC_IDX_TEMP_REAR];
}

/**
  * @brief  获取交流输出电压有效值
  * @retval 交流电压有效值 (V)
  * @note   使用4个周期的采样数据计算RMS
  */
float ADC_GetACVoltage_RMS(uint16_t raw)
{
  /* 获取当前交流电压采样值 */
  float voltage = ((float)raw * ADC_VREF) / ADC_RESOLUTION;

  /* 减去偏置 */
  float acVoltage = voltage - ZMPT101B_VREF;

  /* 存入缓冲区 */
  s_acVoltageBuffer[s_acVoltageIndex] = acVoltage;
  s_acVoltageIndex = (s_acVoltageIndex + 1) % ZMPT101B_SAMPLE_CYCLES;

  /* 计算RMS */
  float sumSquares = 0.0f;
  for (uint8_t i = 0; i < ZMPT101B_SAMPLE_CYCLES; i++)
  {
    sumSquares += s_acVoltageBuffer[i] * s_acVoltageBuffer[i];
  }

  float rms = sqrtf(sumSquares / ZMPT101B_SAMPLE_CYCLES);

  /* 应用变比 */
  rms = rms * g_adcCal[ADC_IDX_AC_VOLTAGE].ratio;

  return rms;
}

/**
  * @brief  更新电压电流数据到MQTT结构
  * @retval None
  */
void ADC_UpdateVoltageCurrent(void)
{
  uint16_t rawPV_V  = ADC_GetRawValue(ADC_IDX_PV_VOLTAGE);
  uint16_t rawPV_I  = ADC_GetRawValue(ADC_IDX_PV_CURRENT);
  uint16_t rawBUS_V = ADC_GetRawValue(ADC_IDX_BUS_VOLTAGE);
  uint16_t rawAC_I  = ADC_GetRawValue(ADC_IDX_AC_CURRENT);
  uint16_t rawAC_V  = ADC_GetRawValue(ADC_IDX_AC_VOLTAGE);

  /* PV输入 */
  g_mqttData.pv.voltage = ADC_GetPVVoltage(rawPV_V);
  g_mqttData.pv.current = ADC_GetPVCurrent(rawPV_I);
  g_mqttData.pv.power = g_mqttData.pv.voltage * g_mqttData.pv.current;

  /* 母线 */
  g_mqttData.bus.voltage = ADC_GetBusVoltage(rawBUS_V);

  /* 交流输出 */
  g_mqttData.ac.current = ADC_GetACCurrent(rawAC_I);
  g_mqttData.ac.voltage = ADC_GetACVoltage_RMS(rawAC_V);
  g_mqttData.ac.power = g_mqttData.ac.voltage * g_mqttData.ac.current;
  g_mqttData.ac.pf = 0.95f;
}

/**
  * @brief  更新温度数据到MQTT结构
  * @retval None
  */
void ADC_UpdateTemperature(void)
{
  uint16_t rawTEMP_F = ADC_GetRawValue(ADC_IDX_TEMP_FRONT);
  uint16_t rawTEMP_R = ADC_GetRawValue(ADC_IDX_TEMP_REAR);
  g_mqttData.temp.mosfet_front = ADC_GetFrontTemperature(rawTEMP_F);
  g_mqttData.temp.mosfet_rear = ADC_GetRearTemperature(rawTEMP_R);
}

/**
  * @brief  批量更新所有ADC数据到MQTT结构
  * @retval None
  */
void ADC_UpdateAllData(void)
{
  ADC_UpdateVoltageCurrent();
  ADC_UpdateTemperature();
}

/* USER CODE BEGIN 1 */

/**
  * @brief  DMA半传输完成回调
  * @param  hadc: ADC句柄
  * @retval None
  */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
  if (hadc == &hadc1)
  {
    ADC_DMA_ProcessHalf();
  }
}

/**
  * @brief  DMA传输完成回调
  * @param  hadc: ADC句柄
  * @retval None
  * @note   ADC DMA完成中断，释放信号量adcDoneSem，唤醒控制任务
  */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  if (hadc == &hadc1)
  {
    ADC_DMA_ProcessFull();
    
    /* 释放ADC完成信号量，唤醒控制任务 */
    extern osSemaphoreId_t adcDoneSem;
    if (adcDoneSem != NULL)
    {
      osSemaphoreRelease(adcDoneSem);
    }
  }
}

/* USER CODE END 1 */
