/**
 * @file lvgl.h
 * LVGL main header file
 */

#ifndef LVGL_H
#define LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

/* Configuration */
#include "src/lv_conf_internal.h"

/* Misc */
#include "src/misc/lv_log.h"
#include "src/misc/lv_timer.h"
#include "src/misc/lv_math.h"
#include "src/misc/lv_mem.h"
#include "src/misc/lv_async.h"
#include "src/misc/lv_anim.h"
#include "src/misc/lv_printf.h"

/* Hal */
#include "src/hal/lv_hal.h"

/* Core */
#include "src/core/lv_obj.h"
#include "src/core/lv_group.h"
#include "src/core/lv_indev.h"
#include "src/core/lv_disp.h"
#include "src/core/lv_theme.h"
#include "src/core/lv_refr.h"

/* Draw */
#include "src/draw/lv_draw.h"

/* Font */
#include "src/font/lv_font.h"
#include "src/font/lv_symbol_def.h"

/* Widgets */
#include "src/widgets/lv_arc.h"
#include "src/widgets/lv_bar.h"
#include "src/widgets/lv_btn.h"
#include "src/widgets/lv_btnmatrix.h"
#include "src/widgets/lv_canvas.h"
#include "src/widgets/lv_checkbox.h"
#include "src/widgets/lv_dropdown.h"
#include "src/widgets/lv_img.h"
#include "src/widgets/lv_label.h"
#include "src/widgets/lv_line.h"
#include "src/widgets/lv_roller.h"
#include "src/widgets/lv_slider.h"
#include "src/widgets/lv_switch.h"
#include "src/widgets/lv_table.h"
#include "src/widgets/lv_textarea.h"

/* Extra */
#include "src/extra/lv_extra.h"

/* API map */
#include "src/lv_api_map.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LVGL_H*/
