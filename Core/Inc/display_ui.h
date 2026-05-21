/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    display_ui.h
  * @brief   光伏逆变器显示UI界面
  *          参考微信小程序"PV Inspector"设计
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __DISPLAY_UI_H
#define __DISPLAY_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "lvgl.h"
#include "data.h"

/* 颜色定义 ------------------------------------------------------------------*/
#define UI_COLOR_PRIMARY        lv_color_hex(0x4CAF50)  /* 绿色主题 */
#define UI_COLOR_SECONDARY      lv_color_hex(0x388E3C)  /* 深绿 */
#define UI_COLOR_BG             lv_color_hex(0xF5F5F5)  /* 浅灰背景 */
#define UI_COLOR_CARD           lv_color_hex(0xFFFFFF)  /* 白色卡片 */
#define UI_COLOR_TEXT           lv_color_hex(0x333333)  /* 深灰文字 */
#define UI_COLOR_TEXT_LIGHT     lv_color_hex(0x757575)  /* 浅灰文字 */
#define UI_COLOR_ACCENT         lv_color_hex(0xFF9800)  /* 橙色强调 */
#define UI_COLOR_ERROR          lv_color_hex(0xF44336)  /* 红色错误 */
#define UI_COLOR_ONLINE         lv_color_hex(0x4CAF50)  /* 在线绿色 */
#define UI_COLOR_OFFLINE        lv_color_hex(0x9E9E9E)  /* 离线灰色 */

/* 页面枚举 ------------------------------------------------------------------*/
typedef enum {
    UI_PAGE_HOME = 0,       /* 首页概览 */
    UI_PAGE_DETAIL,         /* 详情页 */
    UI_PAGE_COUNT
} UI_Page_t;

/* UI数据更新结构 -----------------------------------------------------------*/
typedef struct {
    float pv_voltage;       /* 光伏电压 */
    float pv_current;       /* 光伏电流 */
    float pv_power;         /* 光伏功率 */
    float ac_voltage;       /* 交流电压 */
    float ac_current;       /* 交流电流 */
    float ac_power;         /* 交流功率 */
    float ac_freq;          /* 电网频率 */
    float bus_voltage;      /* 母线电压 */
    float temp_front;       /* 前级温度 */
    float temp_rear;        /* 后级温度 */
    uint8_t mppt_duty;      /* MPPT占空比 */
    uint8_t mppt_eff;       /* MPPT效率 */
    char state[16];         /* 运行状态 */
    uint8_t is_online;      /* 在线状态 */
} UI_Data_t;

/* 函数声明 ------------------------------------------------------------------*/

/**
  * @brief  UI初始化
  * @param  None
  * @retval None
  */
void DisplayUI_Init(void);

/**
  * @brief  更新UI数据
  * @param  data: 数据指针
  * @retval None
  */
void DisplayUI_UpdateData(const UI_Data_t* data);

/**
  * @brief  从MQTT数据结构更新UI
  * @param  mqttData: MQTT数据指针
  * @retval None
  */
void DisplayUI_UpdateFromMQTT(const MqttDataPacket_t* mqttData);

/**
  * @brief  切换页面
  * @param  page: 页面枚举
  * @retval None
  */
void DisplayUI_SwitchPage(UI_Page_t page);

/**
  * @brief  获取当前页面
  * @param  None
  * @retval 当前页面
  */
UI_Page_t DisplayUI_GetCurrentPage(void);

/**
  * @brief  UI任务处理，需在任务中循环调用
  * @param  None
  * @retval None
  */
void DisplayUI_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_UI_H */
