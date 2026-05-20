/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : task.c
  * @brief          : Task management for FreeRTOS (CMSIS-RTOS2)
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
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* 按键事件结构体 */
typedef struct
{
  uint8_t keyCode;
  uint8_t keyState;
} KeyEvent;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* 初始化任务句柄 */
osThreadId_t initTaskHandle;

/* 任务句柄 - 按优先级从高到低排列 */
osThreadId_t controlTaskHandle;
osThreadId_t wifiTaskHandle;
osThreadId_t rs485TaskHandle;
osThreadId_t hmiTaskHandle;
osThreadId_t keyTaskHandle;
osThreadId_t monitorTaskHandle;

/* 信号量、互斥量、队列句柄 */
osSemaphoreId_t adcDoneSem;
osMutexId_t globalDataMutex;
osMessageQueueId_t keyQueue;

/* 任务属性定义 */
const osThreadAttr_t initTask_attributes = {
  .name = "initTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

const osThreadAttr_t controlTask_attributes = {
  .name = "controlTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

const osThreadAttr_t wifiTask_attributes = {
  .name = "wifiTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

const osThreadAttr_t rs485Task_attributes = {
  .name = "rs485Task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t hmiTask_attributes = {
  .name = "hmiTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

const osThreadAttr_t keyTask_attributes = {
  .name = "keyTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

const osThreadAttr_t monitorTask_attributes = {
  .name = "monitorTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityIdle,
};

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

void InitTask(void *argument);
void ControlTask(void *argument);
void WiFiTask(void *argument);
void RS485Task(void *argument);
void HMITask(void *argument);
void KeyTask(void *argument);
void MonitorTask(void *argument);

static void Create_SyncObjects(void);
static void Create_AllTasks(void);

/* USER CODE END FunctionPrototypes */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  创建初始化任务
  * @param  None
  * @retval None
  */
void Create_InitTask(void)
{
  initTaskHandle = osThreadNew(InitTask, NULL, &initTask_attributes);
  
  if (initTaskHandle == NULL)
  {
    Error_Handler();
  }
}

/**
  * @brief  创建同步对象（信号量、互斥量、队列）
  * @param  None
  * @retval None
  */
static void Create_SyncObjects(void)
{
  /* 创建ADC完成二进制信号量 */
  adcDoneSem = osSemaphoreNew(1, 0, NULL);
  if (adcDoneSem == NULL)
  {
    Error_Handler();
  }
  
  /* 创建全局数据互斥量 */
  globalDataMutex = osMutexNew(NULL);
  if (globalDataMutex == NULL)
  {
    Error_Handler();
  }
  
  /* 创建按键事件队列（10个元素，每个大小为KeyEvent结构体） */
  keyQueue = osMessageQueueNew(10, sizeof(KeyEvent), NULL);
  if (keyQueue == NULL)
  {
    Error_Handler();
  }
}

/**
  * @brief  创建所有应用任务
  * @param  None
  * @retval None
  */
static void Create_AllTasks(void)
{
  /* 控制任务 - 最高优先级 */
  controlTaskHandle = osThreadNew(ControlTask, NULL, &controlTask_attributes);
  if (controlTaskHandle == NULL)
  {
    Error_Handler();
  }
  
  /* WiFi通信任务 */
  wifiTaskHandle = osThreadNew(WiFiTask, NULL, &wifiTask_attributes);
  if (wifiTaskHandle == NULL)
  {
    Error_Handler();
  }
  
  /* RS-485 Modbus任务 */
  rs485TaskHandle = osThreadNew(RS485Task, NULL, &rs485Task_attributes);
  if (rs485TaskHandle == NULL)
  {
    Error_Handler();
  }
  
  /* 人机交互任务 */
  hmiTaskHandle = osThreadNew(HMITask, NULL, &hmiTask_attributes);
  if (hmiTaskHandle == NULL)
  {
    Error_Handler();
  }
  
  /* 按键任务 */
  keyTaskHandle = osThreadNew(KeyTask, NULL, &keyTask_attributes);
  if (keyTaskHandle == NULL)
  {
    Error_Handler();
  }
  
  /* 监控任务 - 最低优先级 */
  monitorTaskHandle = osThreadNew(MonitorTask, NULL, &monitorTask_attributes);
  if (monitorTaskHandle == NULL)
  {
    Error_Handler();
  }
}

/**
  * @brief  初始化任务函数
  * @param  argument: 任务参数
  * @retval None
  */
void InitTask(void *argument)
{
  /* USER CODE BEGIN InitTask */
  
  /* 第1步：创建同步对象（信号量、互斥量、队列） */
  Create_SyncObjects();
  
  /* 第2步：创建所有应用任务 */
  Create_AllTasks();
  
  /* 第3步：初始化完成后删除自身 */
  osThreadTerminate(initTaskHandle);
  
  /* USER CODE END InitTask */
}

/**
  * @brief  控制任务 - 最高优先级
  * @param  argument: 任务参数
  * @retval None
  */
void ControlTask(void *argument)
{
  /* USER CODE BEGIN ControlTask */
  for (;;)
  {
    /* 控制算法实现 */
    
    osDelay(1);
  }
  /* USER CODE END ControlTask */
}

/**
  * @brief  WiFi通信任务
  * @param  argument: 任务参数
  * @retval None
  */
void WiFiTask(void *argument)
{
  /* USER CODE BEGIN WiFiTask */
  for (;;)
  {
    /* WiFi通信处理 */
    
    osDelay(10);
  }
  /* USER CODE END WiFiTask */
}

/**
  * @brief  RS-485 Modbus通信任务
  * @param  argument: 任务参数
  * @retval None
  */
void RS485Task(void *argument)
{
  /* USER CODE BEGIN RS485Task */
  for (;;)
  {
    /* RS-485 Modbus通信处理 */
    
    osDelay(10);
  }
  /* USER CODE END RS485Task */
}

/**
  * @brief  人机交互任务（HMI）
  * @param  argument: 任务参数
  * @retval None
  */
void HMITask(void *argument)
{
  /* USER CODE BEGIN HMITask */
  for (;;)
  {
    /* 人机交互界面处理 */
    
    osDelay(50);
  }
  /* USER CODE END HMITask */
}

/**
  * @brief  按键扫描任务
  * @param  argument: 任务参数
  * @retval None
  */
void KeyTask(void *argument)
{
  /* USER CODE BEGIN KeyTask */
  KeyEvent keyEvent;
  
  for (;;)
  {
    /* 按键扫描处理 */
    /* 检测到按键后发送队列：osMessageQueuePut(keyQueue, &keyEvent, 0, 0); */
    
    osDelay(20);
  }
  /* USER CODE END KeyTask */
}

/**
  * @brief  监控任务 - 最低优先级
  * @param  argument: 任务参数
  * @retval None
  */
void MonitorTask(void *argument)
{
  /* USER CODE BEGIN MonitorTask */
  for (;;)
  {
    /* 系统监控、看门狗喂狗等 */
    
    osDelay(100);
  }
  /* USER CODE END MonitorTask */
}

/* USER CODE BEGIN 1 */

/* 在此处添加其他辅助函数 */

/* USER CODE END 1 */
