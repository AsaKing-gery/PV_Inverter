/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : spwm.c
  * @brief          : SPWM (Sinusoidal PWM) generator implementation
  *                   Uses lookup table for sine wave generation
  *                   Updates TIM8 CCR1/CCR2 at 50µs interval
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
#include "spwm.h"
#include "tim.h"
#include "data.h"
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

/* TIM8 PWM周期值 (由CubeMX配置决定)
 * 当前配置: Period = 8399, CenterAligned1模式
 * 168MHz时钟下:
 *   - PWM频率 = 168MHz / (2 * 8400) = 10kHz
 *   - 更新中断频率 = 20kHz (上下计数都触发)
 *   - 50µs更新周期正好匹配!
 */
#define TIM8_PWM_PERIOD             8399
#define TIM8_PWM_HALF_PERIOD        4200    /* (8399 + 1) / 2 */

/* 相位计算常量 */
#define PHASE_BITS                  16      /* 相位累加器位数 */
#define PHASE_MASK                  0xFFFF  /* 相位掩码 */
#define SINE_TABLE_BITS             8       /* 正弦表地址位数 (256点) */
#define SINE_TABLE_MASK             0xFF    /* 正弦表掩码 */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* TIM8句柄 - 由CubeMX生成 */
extern TIM_HandleTypeDef htim8;

/* SPWM控制器实例 */
SPWM_Controller_t g_spwm;

/**
  * @brief  正弦表 (256点，Q15格式)
  * @note   数值范围: 0-32767 (对应正弦值 0-1.0)
  * @note   索引: 0-255 对应 0-359度
  */
const uint16_t spwm_sineTable[SPWM_SINE_TABLE_SIZE] = {
  32768, 33079, 33390, 33701, 34012, 34322, 34633, 34943,
  35253, 35563, 35872, 36181, 36490, 36798, 37106, 37414,
  37721, 38028, 38334, 38640, 38945, 39250, 39554, 39858,
  40161, 40463, 40765, 41066, 41367, 41666, 41965, 42263,
  42561, 42857, 43153, 43448, 43742, 44036, 44328, 44620,
  44911, 45201, 45490, 45778, 46065, 46351, 46636, 46920,
  47203, 47485, 47766, 48046, 48325, 48603, 48880, 49155,
  49430, 49703, 49975, 50246, 50516, 50785, 51052, 51319,
  51584, 51848, 52110, 52372, 52632, 52891, 53149, 53405,
  53660, 53914, 54167, 54418, 54668, 54916, 55163, 55409,
  55653, 55896, 56138, 56378, 56617, 56854, 57090, 57324,
  57557, 57789, 58019, 58247, 58474, 58699, 58923, 59145,
  59366, 59585, 59803, 60019, 60233, 60446, 60657, 60867,
  61075, 61281, 61486, 61689, 61891, 62091, 62289, 62486,
  62681, 62874, 63066, 63256, 63444, 63631, 63816, 63999,
  64180, 64360, 64538, 64714, 64888, 65061, 65232, 65401,
  65568, 65734, 65897, 66059, 66219, 66378, 66534, 66689,
  66842, 66993, 67143, 67290, 67436, 67580, 67722, 67862,
  68000, 68136, 68270, 68403, 68533, 68662, 68788, 68913,
  69036, 69157, 69276, 69393, 69508, 69621, 69732, 69841,
  69948, 70053, 70156, 70257, 70356, 70453, 70548, 70641,
  70732, 70821, 70908, 70993, 71076, 71157, 71236, 71313,
  71388, 71461, 71532, 71601, 71668, 71733, 71796, 71857,
  71916, 71973, 72028, 72081, 72132, 72181, 72228, 72273,
  72316, 72357, 72396, 72433, 72468, 72501, 72532, 72561,
  72588, 72613, 72636, 72657, 72676, 72693, 72708, 72721,
  72732, 72741, 72748, 72753, 72756, 72757, 72756, 72753,
  72748, 72741, 72732, 72721, 72708, 72693, 72676, 72657,
  72636, 72613, 72588, 72561, 72532, 72501, 72468, 72433,
  72396, 72357, 72316, 72273, 72228, 72181, 72132, 72081,
  72028, 71973, 71916, 71857, 71796, 71733, 71668, 71601,
  71532, 71461, 71388, 71313, 71236, 71157, 71076, 70993
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static uint16_t SPWM_GetSineValue(uint16_t phase);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  从正弦表获取正弦值
  * @param  phase: 相位值 (0-65535对应0-360度)
  * @retval 正弦值 (Q15格式: 0-32767)
  */
static uint16_t SPWM_GetSineValue(uint16_t phase)
{
  /* 取高8位作为正弦表索引 */
  uint8_t index = (phase >> (PHASE_BITS - SINE_TABLE_BITS)) & SINE_TABLE_MASK;
  return spwm_sineTable[index];
}

/**
  * @brief  初始化SPWM控制器
  * @param  None
  * @retval None
  */
void SPWM_Init(void)
{
  /* 清零结构体 */
  memset(&g_spwm, 0, sizeof(SPWM_Controller_t));

  /* 计算相位步进
   * 目标: 50Hz正弦波，每50µs更新一次
   * 每周期点数: 20ms / 50µs = 400点
   * 相位步进: 65536 / 400 = 163.84 ≈ 164
   * 
   * 注意: TIM8为CenterAligned1模式，更新中断频率 = 2 * PWM频率 = 20kHz
   * 正好对应50µs周期，无需额外分频
   */
  g_spwm.phaseStep = (uint16_t)((65536.0f * SPWM_MODULATION_FREQ * SPWM_UPDATE_PERIOD_US) / 1000000.0f);

  /* 默认调制比 */
  g_spwm.modulationIndex = 80;  /* 80% */

  /* 初始化TIM8 PWM */
  HAL_TIM_PWM_Init(&htim8);
}

/**
  * @brief  启动SPWM输出
  * @param  None
  * @retval None
  */
void SPWM_Start(void)
{
  /* 使能TIM8 PWM输出 */
  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);

  /* 使能互补输出 */
  HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_2);

  /* 使能TIM8更新中断 */
  __HAL_TIM_ENABLE_IT(&htim8, TIM_IT_UPDATE);

  g_spwm.isRunning = 1;

  /* 更新数据结构 */
  strncpy(g_mqttData.inverter.state, "RUN", sizeof(g_mqttData.inverter.state));
}

/**
  * @brief  停止SPWM输出
  * @param  None
  * @retval None
  */
void SPWM_Stop(void)
{
  /* 关闭TIM8 PWM输出 */
  HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_1);
  HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_2);

  /* 关闭互补输出 */
  HAL_TIMEx_PWMN_Stop(&htim8, TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Stop(&htim8, TIM_CHANNEL_2);

  /* 禁用TIM8更新中断 */
  __HAL_TIM_DISABLE_IT(&htim8, TIM_IT_UPDATE);

  /* 清零CCR值 */
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 0);

  g_spwm.isRunning = 0;

  /* 更新数据结构 */
  strncpy(g_mqttData.inverter.state, "STOP", sizeof(g_mqttData.inverter.state));
}

/**
  * @brief  设置调制比
  * @param  modulation: 调制比值 (0-100)
  * @retval None
  */
void SPWM_SetModulation(uint8_t modulation)
{
  /* 限幅 */
  if (modulation > SPWM_MODULATION_MAX)
  {
    modulation = SPWM_MODULATION_MAX;
  }

  g_spwm.modulationIndex = modulation;
}

/**
  * @brief  获取当前调制比
  * @param  None
  * @retval 调制比值 (0-100)
  */
uint8_t SPWM_GetModulation(void)
{
  return g_spwm.modulationIndex;
}

/**
  * @brief  获取当前相位
  * @param  None
  * @retval 相位值 (0-359度)
  */
uint16_t SPWM_GetPhase(void)
{
  return (uint16_t)((g_spwm.phaseAccumulator * 360UL) >> PHASE_BITS);
}

/**
  * @brief  获取输出频率
  * @param  None
  * @retval 频率值 (Hz)
  */
float SPWM_GetFrequency(void)
{
  return (float)(g_spwm.phaseStep * 1000000.0f) / (65536.0f * SPWM_UPDATE_PERIOD_US);
}

/**
  * @brief  SPWM更新函数 (在TIM8更新中断中调用)
  * @param  None
  * @retval None
  * @note   每50µs执行一次
  * @note   查正弦表，更新TIM8 CCR1/CCR2
  */
void SPWM_Update(void)
{
  if (!g_spwm.isRunning)
  {
    return;
  }

  /* 获取当前相位对应的正弦值 (Q15格式) */
  uint16_t sineValue = SPWM_GetSineValue(g_spwm.phaseAccumulator);

  /* 计算调制后的占空比
   * CCR = 半周期 + (半周期 * 调制比 * 正弦值) / (100 * 32768)
   * 简化: CCR = 4200 + (4200 * modulation * sineValue) / 3276800
   */
  int32_t modulation = (int32_t)g_spwm.modulationIndex;
  int32_t temp = (TIM8_PWM_HALF_PERIOD * modulation * sineValue) / 3276800L;

  /* CH1: 正弦波 */
  g_spwm.ccr1 = (uint16_t)(TIM8_PWM_HALF_PERIOD + temp);

  /* CH2: 互补正弦波 (180度相位差) */
  g_spwm.ccr2 = (uint16_t)(TIM8_PWM_HALF_PERIOD - temp);

  /* 限幅检查 */
  if (g_spwm.ccr1 > TIM8_PWM_PERIOD)
  {
    g_spwm.ccr1 = TIM8_PWM_PERIOD;
  }
  if (g_spwm.ccr2 > TIM8_PWM_PERIOD)
  {
    g_spwm.ccr2 = TIM8_PWM_PERIOD;
  }

  /* 更新TIM8 CCR寄存器 */
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, g_spwm.ccr1);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, g_spwm.ccr2);

  /* 更新相位累加器 */
  g_spwm.phaseAccumulator += g_spwm.phaseStep;

  /* 周期计数 */
  g_spwm.cycleCount++;
}

/* USER CODE BEGIN 1 */

/**
  * @brief  TIM8更新中断回调函数
  * @param  htim: TIM句柄
  * @retval None
  * @note   在stm32f4xx_it.c中调用
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == &htim8)
  {
    SPWM_Update();
  }
}

/* USER CODE END 1 */
