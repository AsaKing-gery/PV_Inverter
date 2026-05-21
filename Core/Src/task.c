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
#include "app_task.h"
#include "cmsis_os.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "adc_convert.h"
#include "mppt.h"
#include "spwm.h"
#include "pll_sogi.h"
#include "protection.h"
#include "esp32_wifi.h"
#include "rs485_modbus.h"
#include "data.h"
#include "display.h"
#include "display_ui.h"
#include <string.h>
#include <stdio.h>

/* 看门狗句柄声明 (在main.c中定义) */
extern IWDG_HandleTypeDef hiwdg;

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* 按键事件结构体 */
typedef struct
{
  uint8_t key_id;   /* 按键编号 1~5 */
  uint8_t event;    /* 0 = 短按, 1 = 长按 */
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
osSemaphoreId_t modbusTxCompleteSem;
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
  .stack_size = 2048 * 4,  /* LVGL需要较大堆栈 */
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
  
  /* 创建Modbus发送完成二进制信号量 */
  modbusTxCompleteSem = osSemaphoreNew(1, 0, NULL);
  if (modbusTxCompleteSem == NULL)
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
  * @brief  模块初始化函数
  * @note   集中初始化所有应用模块
  * @retval None
  */
static void Init_AllModules(void)
{
  /* 1. 数据管理初始化 */
  Data_Init();
  
  /* 2. ADC采集初始化 */
  ADC_DMA_Start();           /* 启动ADC DMA采集 */
  
  /* 3. 控制算法初始化 */
  MPPT_Init();               /* MPPT算法 */
  SPWM_Init();               /* SPWM生成 */
  PLL_Init();                /* 锁相环 */
  
  /* 4. 保护系统初始化 */
  Protection_Init();
  
  /* 5. 通信模块初始化 */
  ESP32_Init();              /* WiFi模块 */
  Modbus_Init();             /* RS485 Modbus */
  
  /* 6. 显示系统初始化 */
  Display_Init();            /* LCD+LVGL */
  DisplayUI_Init();          /* UI界面 */
}

/**
  * @brief  初始化任务函数
  * @param  argument: 任务参数
  * @retval None
  * @note   系统启动入口，完成所有初始化和任务创建
  */
void InitTask(void *argument)
{
  /* USER CODE BEGIN InitTask */
  
  /* 第1步：创建同步对象（信号量、互斥量、队列） */
  Create_SyncObjects();
  
  /* 第2步：初始化所有应用模块 */
  Init_AllModules();
  
  /* 第3步：创建所有应用任务 */
  Create_AllTasks();
  
  /* 第4步：启动SPWM输出（必须在任务创建后） */
  SPWM_Start();
  
  /* 第5步：初始化完成后删除自身 */
  osThreadTerminate(initTaskHandle);
  
  /* USER CODE END InitTask */
}

/**
  * @brief  控制任务 - 最高优先级
  * @param  argument: 任务参数
  * @retval None
  * @note   10ms周期，执行以下功能：
  *         1. 读取ADC数据
  *         2. 还原物理量
  *         3. MPPT算法
  *         4. SPWM更新 (在TIM8中断中执行)
  *         5. 锁相环SOGI-PLL
  *         6. 保护检测
  *         7. 更新全局数据
  */
void ControlTask(void *argument)
{
  /* USER CODE BEGIN ControlTask */
  
  /* 注意：所有模块已在InitTask中初始化 */
  
  /* 任务周期计数 */
  uint32_t cycleCount = 0;
  
  for (;;)
  {
    /* ========== 1. 读取ADC原始数据 ========== */
    uint16_t rawPV_V = ADC_GetRawValue(ADC_IDX_PV_VOLTAGE);
    uint16_t rawPV_I = ADC_GetRawValue(ADC_IDX_PV_CURRENT);
    uint16_t rawBUS_V = ADC_GetRawValue(ADC_IDX_BUS_VOLTAGE);
    uint16_t rawAC_I = ADC_GetRawValue(ADC_IDX_AC_CURRENT);
    uint16_t rawAC_V = ADC_GetRawValue(ADC_IDX_AC_VOLTAGE);
    uint16_t rawTEMP_F = ADC_GetRawValue(ADC_IDX_TEMP_FRONT);
    uint16_t rawTEMP_R = ADC_GetRawValue(ADC_IDX_TEMP_REAR);

    /* ========== 2. 还原物理量 (raw→校准→低通滤波) ========== */
    float pvVoltage = ADC_GetPVVoltage(rawPV_V);
    float pvCurrent = ADC_GetPVCurrent(rawPV_I);
    float busVoltage = ADC_GetBusVoltage(rawBUS_V);
    float acCurrent = ADC_GetACCurrent(rawAC_I);
    float acVoltage = ADC_GetACVoltage_RMS(rawAC_V);
    float tempFront = ADC_GetFrontTemperature(rawTEMP_F);
    float tempRear = ADC_GetRearTemperature(rawTEMP_R);
    
    /* 计算功率 */
    float pvPower = pvVoltage * pvCurrent;
    float acPower = acVoltage * acCurrent;
    
    /* ========== 3. MPPT算法 (每100ms执行一次) ========== */
    if (cycleCount % 10 == 0)  /* 10ms * 10 = 100ms */
    {
      /* 获取降功率系数 */
      float derateFactor = Protection_GetDerateFactor();
      
      /* 执行MPPT */
      MPPT_Execute(pvVoltage, pvCurrent);
      
      /* 应用降功率限制 */
      uint8_t targetDuty = (uint8_t)(MPPT_GetDuty() * derateFactor);
      MPPT_SetDuty(targetDuty);
    }
    
    /* ========== 4. SPWM更新 ========== */
    /* SPWM在TIM8中断中自动更新 (50µs周期) */
    /* 这里只更新调制比 */
    if (cycleCount % 10 == 0)
    {
      float derateFactor = Protection_GetDerateFactor();
      uint8_t modulation = (uint8_t)(80 * derateFactor);  /* 基础调制比80% */
      
      if (modulation < 10) modulation = 10;  /* 最小10% */
      SPWM_SetModulation(modulation);
    }
    
    /* ========== 5. 锁相环SOGI-PLL ========== */
    PLL_Update(acVoltage);
    
    /* ========== 6. 保护检测 ========== */
    uint8_t faultCode = Protection_Check(pvVoltage, acCurrent, tempFront, tempRear);
    
    if (faultCode != 0)
    {
      /* 有故障，执行保护动作 */
      Protection_ExecuteAction();
    }
    
    /* ========== 7. 准备状态字符串 (互斥量外准备) ========== */
    const char* invState = PLL_IsLocked() ? "RUN" : "STOP";
    
    /* ========== 8. 更新全局数据 (缩短互斥量持有时间) ========== */
    osMutexAcquire(globalDataMutex, osWaitForever);
    {
      /* 使用结构体赋值减少代码量 */
      g_mqttData.pv.voltage = pvVoltage;
      g_mqttData.pv.current = pvCurrent;
      g_mqttData.pv.power = pvPower;
      
      g_mqttData.bus.voltage = busVoltage;
      
      g_mqttData.ac.voltage = acVoltage;
      g_mqttData.ac.current = acCurrent;
      g_mqttData.ac.power = acPower;
      g_mqttData.ac.frequency = PLL_GetFrequency();
      
      g_mqttData.temp.mosfet_front = tempFront;
      g_mqttData.temp.mosfet_rear = tempRear;
      
      g_mqttData.mppt.duty = MPPT_GetDuty();
      g_mqttData.mppt.efficiency = MPPT_GetEfficiency();
      
      /* 直接赋值4字节状态码，比strncpy更快 */
      *(uint32_t*)g_mqttData.inverter.state = *(uint32_t*)invState;
    }
    osMutexRelease(globalDataMutex);
    
    /* 注意：ADC完成信号量adcDoneSem在HAL_ADC_ConvCpltCallback中释放 */
    /* 用于通知WiFi任务数据已准备好，可以发送 */
    
    /* 周期计数 */
    cycleCount++;
    
    /* 10ms周期 */
    osDelay(10);
  }
  /* USER CODE END ControlTask */
}

/**
  * @brief  WiFi通信任务
  * @param  argument: 任务参数
  * @retval None
  * @note   启动时初始化ESP32-C3，建立WiFi和MQTT连接
  *         正常运行时周期性发送数据到MQTT Broker
  */
void WiFiTask(void *argument)
{
  /* USER CODE BEGIN WiFiTask */
  
  /* 注意：ESP32已在InitTask中初始化，这里检查连接状态 */
  
  /* 等待系统稳定 */
  osDelay(1000);
  
  /* 检查ESP32连接状态 */
  if (ESP32_IsMQTTConnected())
  {
    strncpy(g_mqttStatus.state, "ONLINE", sizeof(g_mqttStatus.state));
    strncpy(g_mqttStatus.fw_version, "V1.0.0", sizeof(g_mqttStatus.fw_version));
  }
  else
  {
    /* 连接未建立，记录状态 */
    strncpy(g_mqttStatus.state, "OFFLINE", sizeof(g_mqttStatus.state));
  }
  
  /* 主循环 - 周期性发送数据 */
  uint32_t heartbeatCount = 0;
  uint32_t dataSendCount = 0;
  uint32_t taskStartTick = osKernelGetTickCount();  /* 记录任务启动时间 */

  for (;;)
  {
    if (ESP32_IsMQTTConnected())
    {
      /* 更新WiFi状态和运行时间 */
      strncpy(g_mqttStatus.state, "ONLINE", sizeof(g_mqttStatus.state));
      g_mqttStatus.uptime = (osKernelGetTickCount() - taskStartTick) / 1000;  /* 秒 */

      /* 等待ADC数据就绪信号量 (由ControlTask释放) */
      if (osSemaphoreAcquire(adcDoneSem, 1000) == osOK)
      {
        /* 发送二进制数据帧到ESP32
         * ESP32接收后解析并打包成JSON发送给MQTT Broker
         * 帧格式: [0xAA][0x55][LEN(2B)][DATA(32B)][CRC8]
         */
        if (ESP32_SendDataFrame())
        {
          dataSendCount++;
          heartbeatCount++;

          /* 更新发送计数 */
          g_esp32Status.txCount = dataSendCount;

          /* 每10次数据发送一次心跳保活 */
          if (heartbeatCount >= 10)
          {
            ESP32_SendHeartbeat();
            heartbeatCount = 0;
          }
        }
        else
        {
          /* 发送失败，增加错误计数 */
          g_esp32Status.errCount++;
        }
      }
    }
    else
    {
      /* 连接断开，尝试重连 */
      strncpy(g_mqttStatus.state, "OFFLINE", sizeof(g_mqttStatus.state));
      strncpy(g_mqttData.inverter.state, "STOP", sizeof(g_mqttData.inverter.state));

      /* 尝试重新初始化ESP32 */
      if (ESP32_Init())
      {
        strncpy(g_mqttStatus.state, "ONLINE", sizeof(g_mqttStatus.state));
        dataSendCount = 0;
        heartbeatCount = 0;
      }
      else
      {
        /* 重连失败，延时后再次尝试 */
        osDelay(5000);
      }
    }

    /* 1秒发送一次数据 */
    osDelay(1000);
  }
  
  /* USER CODE END WiFiTask */
}

/**
  * @brief  RS-485 Modbus任务
  * @param  argument: 任务参数
  * @retval None
  * @note   SunSpec协议支持，功能码03/06
  *         从g_mqttData同步数据到Modbus寄存器
  */
void RS485Task(void *argument)
{
  /* USER CODE BEGIN RS485Task */
  
  /* 注意：Modbus已在InitTask中初始化 */
  
  /* 主循环 - 快速响应Modbus请求 */
  for (;;)
  {
    /* 处理Modbus请求 (帧结束检测 + 响应发送) */
    /* 注意：数据接收在中断中完成，这里只处理完整的帧 */
    Modbus_Process();
    
    /* 每100ms更新一次寄存器数据 */
    static uint32_t lastUpdateTick = 0;
    uint32_t currentTick = osKernelGetTickCount();
    if (currentTick - lastUpdateTick >= 100)
    {
      lastUpdateTick = currentTick;
      
      /* 从g_mqttData同步到Modbus寄存器 */
      Modbus_UpdateRegisters();
    }
    
    /* 10ms周期 - 快速响应 */
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
  
  /* 注意：显示系统已在InitTask中初始化 */
  
  /* 主循环 - 人机交互任务 */
  uint32_t lastUpdateTick = 0;
  
  for (;;)
  {
    /* 1. 处理LVGL内部定时器（动画、刷新） */
    lv_timer_handler();
    
    /* 2. 每500ms更新界面数据 */
    uint32_t currentTick = osKernelGetTickCount();
    if (currentTick - lastUpdateTick >= 500)
    {
      lastUpdateTick = currentTick;
      
      /* 获取g_mqttData最新值（使用互斥量保护） */
      osMutexAcquire(globalDataMutex, osWaitForever);
      {
        /* 更新光伏电压/电流/功率显示 */
        /* 更新交流输出电压/频率/功率显示 */
        /* 更新MPPT状态指示 */
        /* 更新运行模式（离网/并网） */
        /* 更新温度显示 */
        
        /* 通过LVGL更新UI */
        DisplayUI_UpdateFromMQTT(&g_mqttData);
      }
      osMutexRelease(globalDataMutex);
    }
    
    /* 3. 处理触摸屏输入事件 */
    /* SPI2读取XPT2046坐标 - 在lv_timer_handler中自动处理 */
    /* LVGL输入设备驱动处理 - 已注册在Display_Init中 */
    
    /* 周期5ms运行 */
    osDelay(5);
  }
  
  /* USER CODE END HMITask */
}

/**
  * @brief  按键扫描任务
  * @param  argument: 任务参数
  * @retval None
  * @note   触发方式：周期10ms扫描
  *         主循环逻辑：
  *         - 轮询5个按键GPIO（PA7, PC4, PE2, PE3, PE4）的电平
  *         - 消抖处理：连续5次读到低电平确认为按下
  *         - 按下后发送按键事件到队列keyQueue
  */
void KeyTask(void *argument)
{
  /* USER CODE BEGIN KeyTask */
  /* 按键扫描处理 */
  /* 检测到按键后发送队列：osMessageQueuePut(keyQueue, &keyEvent, 0, 0); */
  /* 按键配置：GPIO已在gpio.c中配置为输入模式 */
  /* PA7, PC4, PE2, PE3, PE4 */
  
  /* 消抖计数器：连续检测到5次低电平才确认为按下 */
  #define KEY_DEBOUNCE_COUNT  5
  #define KEY_NUM             5
  
  /* 按键GPIO端口和引脚定义 */
  GPIO_TypeDef* keyPorts[KEY_NUM] = {GPIOA, GPIOC, GPIOE, GPIOE, GPIOE};
  uint16_t keyPins[KEY_NUM] = {GPIO_PIN_7, GPIO_PIN_4, GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4};
  
  /* 消抖计数器和状态 */
  uint8_t debounceCnt[KEY_NUM] = {0};
  uint8_t keyPressed[KEY_NUM] = {0};
  
  KeyEvent keyEvent;
  
  for (;;)
  {
    /* 轮询5个按键 */
    for (uint8_t i = 0; i < KEY_NUM; i++)
    {
      /* 读取按键电平（低电平有效） */
      GPIO_PinState pinState = HAL_GPIO_ReadPin(keyPorts[i], keyPins[i]);
      
      if (pinState == GPIO_PIN_RESET)
      {
        /* 按键被按下（低电平） */
        if (debounceCnt[i] < KEY_DEBOUNCE_COUNT)
        {
          debounceCnt[i]++;
          
          /* 消抖完成，确认为按下 */
          if (debounceCnt[i] >= KEY_DEBOUNCE_COUNT && !keyPressed[i])
          {
            keyPressed[i] = 1;
            
            /* 发送按键事件到队列 */
            keyEvent.key_id = i + 1;  /* 按键编号1~5 */
            keyEvent.event = 0;       /* 0 = 短按（暂不支持长按检测） */
            
            osMessageQueuePut(keyQueue, &keyEvent, 0, 0);
          }
        }
      }
      else
      {
        /* 按键释放（高电平） */
        debounceCnt[i] = 0;
        keyPressed[i] = 0;
      }
    }
    
    /* 周期10ms运行 */
    osDelay(10);
  }
  /* USER CODE END KeyTask */
}

/**
  * @brief  监控任务 - 最低优先级
  * @param  argument: 任务参数
  * @retval None
  * @note   触发方式：周期100ms运行
  *         主循环逻辑：
  *         - 调用HAL_IWDG_Refresh()喂狗，防止看门狗复位
  *         - 可选：系统状态监控、任务运行时间统计等
  */
void MonitorTask(void *argument)
{
  /* USER CODE BEGIN MonitorTask */
  
  /* 初始化看门狗（如已在main.c初始化则跳过） */
  /* HAL_IWDG_Init(&hiwdg); */
  
  for (;;)
  {
    /* 1. 喂狗操作 - 防止看门狗复位 */
    /* 假设看门狗超时时间 > 100ms (通常设置为500ms-1s) */
    HAL_IWDG_Refresh(&hiwdg);
    
    /* 2. 可选：系统状态监控 */
    /* - 检查任务运行状态 */
    /* - 记录系统运行时间 */
    /* - 检测死锁或任务阻塞 */
    
    /* 3. 可选：低优先级后台任务 */
    /* - 日志缓冲刷新 */
    /* - 非关键数据统计 */
    
    /* 周期100ms运行 */
    osDelay(100);
  }
  /* USER CODE END MonitorTask */
}

/* USER CODE BEGIN 1 */

/* 在此处添加其他辅助函数 */

/* USER CODE END 1 */
