/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    display_ui.c
  * @brief   光伏逆变器显示UI实现
  *          参考微信小程序"PV Inspector"设计
  *          屏幕尺寸: 240x320 (竖屏)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "display_ui.h"
#include <stdio.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define UI_HEADER_HEIGHT        40
#define UI_NAV_HEIGHT           50
#define UI_CARD_RADIUS          8
#define UI_PADDING              10
#define UI_GAP                  8

/* Private variables ---------------------------------------------------------*/
static lv_obj_t* screen_main = NULL;
static lv_obj_t* page_home = NULL;
static lv_obj_t* page_detail = NULL;
static lv_obj_t* nav_bar = NULL;

/* 首页控件 */
static lv_obj_t* lbl_power = NULL;
static lv_obj_t* lbl_efficiency = NULL;
static lv_obj_t* lbl_pv_volt = NULL;
static lv_obj_t* lbl_pv_curr = NULL;
static lv_obj_t* lbl_ac_volt = NULL;
static lv_obj_t* lbl_freq = NULL;
static lv_obj_t* lbl_temp_front = NULL;
static lv_obj_t* lbl_temp_rear = NULL;
static lv_obj_t* lbl_status = NULL;
static lv_obj_t* led_online = NULL;

/* 详情页控件 */
static lv_obj_t* cont_pv = NULL;
static lv_obj_t* cont_bus = NULL;
static lv_obj_t* cont_ac = NULL;
static lv_obj_t* cont_temp = NULL;

/* 当前页面 */
static UI_Page_t current_page = UI_PAGE_HOME;

/* Private function prototypes -----------------------------------------------*/
static void CreateHeader(lv_obj_t* parent);
static void CreateHomePage(lv_obj_t* parent);
static void CreateDetailPage(lv_obj_t* parent);
static void CreateNavBar(lv_obj_t* parent);
static void NavBtnEventHandler(lv_event_t* e);
static lv_obj_t* CreateParamCard(lv_obj_t* parent, const char* label, const char* unit, lv_coord_t w, lv_coord_t h);
static void ExpandPanelEventHandler(lv_event_t* e);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  创建顶部标题栏
  */
static void CreateHeader(lv_obj_t* parent)
{
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_size(header, 240, UI_HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    
    /* 标题 */
    lv_obj_t* lbl_title = lv_label_create(header);
    lv_label_set_text(lbl_title, "光伏监控");
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_title);
    
    /* 在线状态指示 */
    led_online = lv_led_create(header);
    lv_obj_set_size(led_online, 8, 8);
    lv_obj_set_pos(led_online, 210, 16);
    lv_led_set_color(led_online, UI_COLOR_ONLINE);
    lv_led_on(led_online);
}

/**
  * @brief  创建参数卡片
  */
static lv_obj_t* CreateParamCard(lv_obj_t* parent, const char* label, const char* unit, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, UI_COLOR_CARD, 0);
    lv_obj_set_style_radius(card, UI_CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x20000000), 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 数值 */
    lv_obj_t* lbl_value = lv_label_create(card);
    lv_label_set_text(lbl_value, "--");
    lv_obj_set_style_text_font(lbl_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_value, UI_COLOR_TEXT, 0);
    lv_obj_align(lbl_value, LV_ALIGN_TOP_LEFT, 0, 0);
    
    /* 单位 */
    lv_obj_t* lbl_unit = lv_label_create(card);
    lv_label_set_text(lbl_unit, unit);
    lv_obj_set_style_text_font(lbl_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_unit, UI_COLOR_TEXT_LIGHT, 0);
    lv_obj_align_to(lbl_unit, lbl_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, 0);
    
    /* 标签 */
    lv_obj_t* lbl_name = lv_label_create(card);
    lv_label_set_text(lbl_name, label);
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_name, UI_COLOR_TEXT_LIGHT, 0);
    lv_obj_align(lbl_name, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    return lbl_value;
}

/**
  * @brief  创建首页
  */
static void CreateHomePage(lv_obj_t* parent)
{
    page_home = lv_obj_create(parent);
    lv_obj_set_size(page_home, 240, 320 - UI_HEADER_HEIGHT - UI_NAV_HEIGHT);
    lv_obj_set_pos(page_home, 0, UI_HEADER_HEIGHT);
    lv_obj_set_style_bg_color(page_home, UI_COLOR_BG, 0);
    lv_obj_set_style_pad_all(page_home, UI_PADDING, 0);
    lv_obj_set_flex_flow(page_home, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(page_home, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(page_home, UI_GAP, 0);
    
    /* 设备信息卡片 */
    lv_obj_t* card_info = lv_obj_create(page_home);
    lv_obj_set_size(card_info, 220, 60);
    lv_obj_set_style_bg_color(card_info, UI_COLOR_CARD, 0);
    lv_obj_set_style_radius(card_info, UI_CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(card_info, 10, 0);
    lv_obj_clear_flag(card_info, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 在线状态 */
    lv_obj_t* lbl_online = lv_label_create(card_info);
    lv_label_set_text(lbl_online, "  在线");
    lv_obj_set_style_text_color(lbl_online, UI_COLOR_ONLINE, 0);
    lv_obj_set_style_text_font(lbl_online, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_online, LV_ALIGN_TOP_LEFT, 0, 0);
    
    /* 设备名称 */
    lv_obj_t* lbl_dev = lv_label_create(card_info);
    lv_label_set_text(lbl_dev, "智能光伏微逆变器");
    lv_obj_set_style_text_font(lbl_dev, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_dev, UI_COLOR_TEXT, 0);
    lv_obj_align(lbl_dev, LV_ALIGN_TOP_LEFT, 0, 18);
    
    /* 设备ID */
    lv_obj_t* lbl_id = lv_label_create(card_info);
    lv_label_set_text(lbl_id, "设备ID: PV-INV-001");
    lv_obj_set_style_text_font(lbl_id, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_id, UI_COLOR_TEXT_LIGHT, 0);
    lv_obj_align(lbl_id, LV_ALIGN_TOP_LEFT, 0, 36);
    
    /* 功率卡片 */
    lv_obj_t* card_power = lv_obj_create(page_home);
    lv_obj_set_size(card_power, 220, 100);
    lv_obj_set_style_bg_color(card_power, UI_COLOR_CARD, 0);
    lv_obj_set_style_radius(card_power, UI_CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(card_power, 10, 0);
    lv_obj_clear_flag(card_power, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 功率标题 */
    lv_obj_t* lbl_power_title = lv_label_create(card_power);
    lv_label_set_text(lbl_power_title, "当前发电功率");
    lv_obj_set_style_text_font(lbl_power_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_power_title, UI_COLOR_TEXT_LIGHT, 0);
    lv_obj_align(lbl_power_title, LV_ALIGN_TOP_MID, 0, 0);
    
    /* 功率值 */
    lbl_power = lv_label_create(card_power);
    lv_label_set_text(lbl_power, "--");
    lv_obj_set_style_text_font(lbl_power, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_power, UI_COLOR_PRIMARY, 0);
    lv_obj_align(lbl_power, LV_ALIGN_TOP_MID, -15, 20);
    
    /* 单位 W */
    lv_obj_t* lbl_w = lv_label_create(card_power);
    lv_label_set_text(lbl_w, "W");
    lv_obj_set_style_text_font(lbl_w, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_w, UI_COLOR_PRIMARY, 0);
    lv_obj_align_to(lbl_w, lbl_power, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, 0);
    
    /* 效率 */
    lbl_efficiency = lv_label_create(card_power);
    lv_label_set_text(lbl_efficiency, "--% 效率");
    lv_obj_set_style_text_font(lbl_efficiency, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_efficiency, UI_COLOR_TEXT_LIGHT, 0);
    lv_obj_align(lbl_efficiency, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    /* 参数网格 */
    lv_obj_t* grid = lv_obj_create(page_home);
    lv_obj_set_size(grid, 220, 120);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(grid, 8, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 6个参数卡片 */
    lbl_pv_volt = CreateParamCard(grid, "光伏电压", "V", 68, 55);
    lbl_pv_curr = CreateParamCard(grid, "光伏电流", "A", 68, 55);
    lbl_ac_volt = CreateParamCard(grid, "交流电压", "V", 68, 55);
    lbl_freq = CreateParamCard(grid, "电网频率", "Hz", 68, 55);
    lbl_temp_front = CreateParamCard(grid, "前级温度", "°C", 68, 55);
    lbl_temp_rear = CreateParamCard(grid, "后级温度", "°C", 68, 55);
    
    /* 状态栏 */
    lv_obj_t* card_status = lv_obj_create(page_home);
    lv_obj_set_size(card_status, 220, 40);
    lv_obj_set_style_bg_color(card_status, UI_COLOR_CARD, 0);
    lv_obj_set_style_radius(card_status, UI_CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(card_status, 8, 0);
    lv_obj_clear_flag(card_status, LV_OBJ_FLAG_SCROLLABLE);
    
    lbl_status = lv_label_create(card_status);
    lv_label_set_text(lbl_status, "逆变器: 停止");
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_status, UI_COLOR_TEXT, 0);
    lv_obj_align(lbl_status, LV_ALIGN_LEFT_MID, 0, 0);
}

/**
  * @brief  创建可展开面板
  */
static lv_obj_t* CreateExpandPanel(lv_obj_t* parent, const char* title, const char* icon)
{
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_width(panel, 220);
    lv_obj_set_style_bg_color(panel, UI_COLOR_CARD, 0);
    lv_obj_set_style_radius(panel, UI_CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 标题栏 */
    lv_obj_t* header = lv_obj_create(panel);
    lv_obj_set_size(header, 200, 30);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 图标和标题 */
    lv_obj_t* lbl_title = lv_label_create(header);
    lv_label_set_text_fmt(lbl_title, "%s %s", icon, title);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 0, 0);
    
    /* 展开箭头 */
    lv_obj_t* lbl_arrow = lv_label_create(header);
    lv_label_set_text(lbl_arrow, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(lbl_arrow, UI_COLOR_TEXT_LIGHT, 0);
    lv_obj_align(lbl_arrow, LV_ALIGN_RIGHT_MID, 0, 0);
    
    /* 内容区域 (初始隐藏) */
    lv_obj_t* content = lv_obj_create(panel);
    lv_obj_set_size(content, 200, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_style_height(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 保存引用 */
    lv_obj_set_user_data(header, content);
    lv_obj_add_event_cb(header, ExpandPanelEventHandler, LV_EVENT_CLICKED, NULL);
    
    return content;
}

/**
  * @brief  展开面板事件处理
  */
static void ExpandPanelEventHandler(lv_event_t* e)
{
    lv_obj_t* header = lv_event_get_target(e);
    lv_obj_t* content = (lv_obj_t*)lv_obj_get_user_data(header);
    
    if (content)
    {
        lv_coord_t h = lv_obj_get_height(content);
        if (h == 0)
        {
            lv_obj_set_height(content, LV_SIZE_CONTENT);
        }
        else
        {
            lv_obj_set_height(content, 0);
        }
    }
}

/**
  * @brief  创建详情页
  */
static void CreateDetailPage(lv_obj_t* parent)
{
    page_detail = lv_obj_create(parent);
    lv_obj_set_size(page_detail, 240, 320 - UI_HEADER_HEIGHT - UI_NAV_HEIGHT);
    lv_obj_set_pos(page_detail, 0, UI_HEADER_HEIGHT);
    lv_obj_set_style_bg_color(page_detail, UI_COLOR_BG, 0);
    lv_obj_set_style_pad_all(page_detail, UI_PADDING, 0);
    lv_obj_set_flex_flow(page_detail, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(page_detail, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(page_detail, UI_GAP, 0);
    lv_obj_add_flag(page_detail, LV_OBJ_FLAG_HIDDEN);  /* 初始隐藏 */
    
    /* 在线状态提示 */
    lv_obj_t* lbl_mqtt = lv_label_create(page_detail);
    lv_label_set_text(lbl_mqtt, "  MQTT已连接");
    lv_obj_set_style_text_color(lbl_mqtt, UI_COLOR_ONLINE, 0);
    lv_obj_set_style_text_font(lbl_mqtt, &lv_font_montserrat_12, 0);
    lv_obj_set_width(lbl_mqtt, 220);
    lv_obj_set_style_text_align(lbl_mqtt, LV_TEXT_ALIGN_LEFT, 0);
    
    /* 光伏输入模块 */
    cont_pv = CreateExpandPanel(page_detail, "光伏输入模块", "☀");
    lv_obj_set_height(cont_pv, LV_SIZE_CONTENT);  /* 默认展开 */
    
    lv_obj_t* lbl_pv_v = lv_label_create(cont_pv);
    lv_label_set_text(lbl_pv_v, "输入电压: -- V");
    lv_obj_set_style_text_font(lbl_pv_v, &lv_font_montserrat_12, 0);
    
    lv_obj_t* lbl_pv_i = lv_label_create(cont_pv);
    lv_label_set_text(lbl_pv_i, "输入电流: -- A");
    lv_obj_set_style_text_font(lbl_pv_i, &lv_font_montserrat_12, 0);
    
    lv_obj_t* lbl_pv_p = lv_label_create(cont_pv);
    lv_label_set_text(lbl_pv_p, "输入功率: -- W");
    lv_obj_set_style_text_font(lbl_pv_p, &lv_font_montserrat_12, 0);
    
    /* 母线模块 */
    cont_bus = CreateExpandPanel(page_detail, "母线模块", "🔋");
    
    lv_obj_t* lbl_bus_v = lv_label_create(cont_bus);
    lv_label_set_text(lbl_bus_v, "母线电压: -- V");
    lv_obj_set_style_text_font(lbl_bus_v, &lv_font_montserrat_12, 0);
    
    /* 交流输出模块 */
    cont_ac = CreateExpandPanel(page_detail, "交流输出模块", "⚡");
    
    lv_obj_t* lbl_ac_v = lv_label_create(cont_ac);
    lv_label_set_text(lbl_ac_v, "输出电压: -- V");
    lv_obj_set_style_text_font(lbl_ac_v, &lv_font_montserrat_12, 0);
    
    lv_obj_t* lbl_ac_i = lv_label_create(cont_ac);
    lv_label_set_text(lbl_ac_i, "输出电流: -- A");
    lv_obj_set_style_text_font(lbl_ac_i, &lv_font_montserrat_12, 0);
    
    lv_obj_t* lbl_ac_p = lv_label_create(cont_ac);
    lv_label_set_text(lbl_ac_p, "输出功率: -- W");
    lv_obj_set_style_text_font(lbl_ac_p, &lv_font_montserrat_12, 0);
    
    lv_obj_t* lbl_ac_f = lv_label_create(cont_ac);
    lv_label_set_text(lbl_ac_f, "电网频率: -- Hz");
    lv_obj_set_style_text_font(lbl_ac_f, &lv_font_montserrat_12, 0);
    
    /* 温度与状态模块 */
    cont_temp = CreateExpandPanel(page_detail, "温度与状态", "🌡");
    
    lv_obj_t* lbl_t_f = lv_label_create(cont_temp);
    lv_label_set_text(lbl_t_f, "前级温度: -- °C");
    lv_obj_set_style_text_font(lbl_t_f, &lv_font_montserrat_12, 0);
    
    lv_obj_t* lbl_t_r = lv_label_create(cont_temp);
    lv_label_set_text(lbl_t_r, "后级温度: -- °C");
    lv_obj_set_style_text_font(lbl_t_r, &lv_font_montserrat_12, 0);
    
    lv_obj_t* lbl_mppt = lv_label_create(cont_temp);
    lv_label_set_text(lbl_mppt, "MPPT状态: --");
    lv_obj_set_style_text_font(lbl_mppt, &lv_font_montserrat_12, 0);
}

/**
  * @brief  导航按钮事件处理
  */
static void NavBtnEventHandler(lv_event_t* e)
{
    lv_obj_t* btn = lv_event_get_target(e);
    UI_Page_t page = (UI_Page_t)(intptr_t)lv_obj_get_user_data(btn);
    
    DisplayUI_SwitchPage(page);
}

/**
  * @brief  创建底部导航栏
  */
static void CreateNavBar(lv_obj_t* parent)
{
    nav_bar = lv_obj_create(parent);
    lv_obj_set_size(nav_bar, 240, UI_NAV_HEIGHT);
    lv_obj_set_pos(nav_bar, 0, 320 - UI_NAV_HEIGHT);
    lv_obj_set_style_bg_color(nav_bar, UI_COLOR_CARD, 0);
    lv_obj_set_style_radius(nav_bar, 0, 0);
    lv_obj_set_style_pad_all(nav_bar, 5, 0);
    lv_obj_set_style_border_width(nav_bar, 1, 0);
    lv_obj_set_style_border_color(nav_bar, UI_COLOR_BG, 0);
    lv_obj_clear_flag(nav_bar, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 首页按钮 */
    lv_obj_t* btn_home = lv_btn_create(nav_bar);
    lv_obj_set_size(btn_home, 100, 40);
    lv_obj_set_style_bg_color(btn_home, UI_COLOR_PRIMARY, 0);
    lv_obj_set_user_data(btn_home, (void*)UI_PAGE_HOME);
    lv_obj_add_event_cb(btn_home, NavBtnEventHandler, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn_home, LV_ALIGN_LEFT_MID, 10, 0);
    
    lv_obj_t* lbl_home = lv_label_create(btn_home);
    lv_label_set_text(lbl_home, "首页");
    lv_obj_set_style_text_color(lbl_home, lv_color_white(), 0);
    lv_obj_center(lbl_home);
    
    /* 详情按钮 */
    lv_obj_t* btn_detail = lv_btn_create(nav_bar);
    lv_obj_set_size(btn_detail, 100, 40);
    lv_obj_set_style_bg_color(btn_detail, UI_COLOR_BG, 0);
    lv_obj_set_user_data(btn_detail, (void*)UI_PAGE_DETAIL);
    lv_obj_add_event_cb(btn_detail, NavBtnEventHandler, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn_detail, LV_ALIGN_RIGHT_MID, -10, 0);
    
    lv_obj_t* lbl_detail = lv_label_create(btn_detail);
    lv_label_set_text(lbl_detail, "详情");
    lv_obj_set_style_text_color(lbl_detail, UI_COLOR_TEXT, 0);
    lv_obj_center(lbl_detail);
}

/* Public functions ----------------------------------------------------------*/

/**
  * @brief  UI初始化
  */
void DisplayUI_Init(void)
{
    /* 创建主屏幕 */
    screen_main = lv_scr_act();
    lv_obj_set_style_bg_color(screen_main, UI_COLOR_BG, 0);
    
    /* 创建标题栏 */
    CreateHeader(screen_main);
    
    /* 创建页面 */
    CreateHomePage(screen_main);
    CreateDetailPage(screen_main);
    
    /* 创建导航栏 */
    CreateNavBar(screen_main);
    
    /* 默认首页 */
    current_page = UI_PAGE_HOME;
}

/**
  * @brief  更新UI数据
  */
void DisplayUI_UpdateData(const UI_Data_t* data)
{
    char buf[32];
    
    /* 更新功率 */
    if (data->pv_power < 0.1f)
    {
        lv_label_set_text(lbl_power, "--");
    }
    else
    {
        snprintf(buf, sizeof(buf), "%.1f", data->pv_power);
        lv_label_set_text(lbl_power, buf);
    }
    
    /* 更新效率 */
    if (data->mppt_eff > 0)
    {
        snprintf(buf, sizeof(buf), "%d%% 效率", data->mppt_eff);
        lv_label_set_text(lbl_efficiency, buf);
    }
    
    /* 更新参数卡片 */
    snprintf(buf, sizeof(buf), "%.1f", data->pv_voltage);
    lv_label_set_text(lbl_pv_volt, buf);
    
    snprintf(buf, sizeof(buf), "%.2f", data->pv_current);
    lv_label_set_text(lbl_pv_curr, buf);
    
    snprintf(buf, sizeof(buf), "%.1f", data->ac_voltage);
    lv_label_set_text(lbl_ac_volt, buf);
    
    snprintf(buf, sizeof(buf), "%.2f", data->ac_freq);
    lv_label_set_text(lbl_freq, buf);
    
    snprintf(buf, sizeof(buf), "%.1f", data->temp_front);
    lv_label_set_text(lbl_temp_front, buf);
    
    snprintf(buf, sizeof(buf), "%.1f", data->temp_rear);
    lv_label_set_text(lbl_temp_rear, buf);
    
    /* 更新状态 */
    snprintf(buf, sizeof(buf), "逆变器: %s", data->state);
    lv_label_set_text(lbl_status, buf);
    
    /* 更新在线状态 */
    if (data->is_online)
    {
        lv_led_set_color(led_online, UI_COLOR_ONLINE);
    }
    else
    {
        lv_led_set_color(led_online, UI_COLOR_OFFLINE);
    }
}

/**
  * @brief  从MQTT数据结构更新UI
  * @note   关联data.h中的全局变量:
  *         - g_mqttData: 实时数据 (PV/AC/温度/MPPT等)
  *         - g_mqttStatus: 设备状态 (ONLINE/OFFLINE)
  */
void DisplayUI_UpdateFromMQTT(const MqttDataPacket_t* mqttData)
{
    UI_Data_t uiData;
    extern MqttStatusPacket_t g_mqttStatus;
    
    /* 转换实时数据 (MqttDataPacket_t) */
    uiData.pv_voltage = mqttData->pv.voltage;
    uiData.pv_current = mqttData->pv.current;
    uiData.pv_power = mqttData->pv.power;
    uiData.ac_voltage = mqttData->ac.voltage;
    uiData.ac_current = mqttData->ac.current;
    uiData.ac_power = mqttData->ac.power;
    uiData.ac_freq = mqttData->ac.frequency;
    uiData.bus_voltage = mqttData->bus.voltage;
    uiData.temp_front = mqttData->temp.mosfet_front;
    uiData.temp_rear = mqttData->temp.mosfet_rear;
    uiData.mppt_duty = (uint8_t)mqttData->mppt.duty;
    uiData.mppt_eff = (uint8_t)mqttData->mppt.efficiency;
    strncpy(uiData.state, mqttData->inverter.state, sizeof(uiData.state) - 1);
    uiData.state[sizeof(uiData.state) - 1] = '\0';
    
    /* 从状态数据获取在线状态 (MqttStatusPacket_t) */
    uiData.is_online = (strncmp(g_mqttStatus.state, "ONLINE", 6) == 0);
    
    DisplayUI_UpdateData(&uiData);
}

/**
  * @brief  切换页面
  */
void DisplayUI_SwitchPage(UI_Page_t page)
{
    if (page == current_page) return;
    
    current_page = page;
    
    if (page == UI_PAGE_HOME)
    {
        lv_obj_clear_flag(page_home, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_detail, LV_OBJ_FLAG_HIDDEN);
    }
    else if (page == UI_PAGE_DETAIL)
    {
        lv_obj_add_flag(page_home, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(page_detail, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
  * @brief  获取当前页面
  */
UI_Page_t DisplayUI_GetCurrentPage(void)
{
    return current_page;
}

/**
  * @brief  UI任务处理
  */
void DisplayUI_Handler(void)
{
    /* LVGL任务处理在Display_LvglHandler中 */
}
