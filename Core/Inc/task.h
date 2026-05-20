/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : task.h
  * @brief          : Header for task.c file
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

#ifndef __TASK_H
#define __TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "cmsis_os.h"
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* 按键事件结构体 */
typedef struct
{
  uint8_t keyCode;
  uint8_t keyState;
} KeyEvent;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* 任务句柄 - 外部可访问 */
extern osThreadId_t controlTaskHandle;
extern osThreadId_t wifiTaskHandle;
extern osThreadId_t rs485TaskHandle;
extern osThreadId_t hmiTaskHandle;
extern osThreadId_t keyTaskHandle;
extern osThreadId_t monitorTaskHandle;

/* 同步对象句柄 - 外部可访问 */
extern osSemaphoreId_t adcDoneSem;        /* ADC完成信号量 */
extern osMutexId_t globalDataMutex;       /* 全局数据互斥量 */
extern osMessageQueueId_t keyQueue;       /* 按键事件队列 */

/* USER CODE END EV */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

void Create_InitTask(void);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __TASK_H */
