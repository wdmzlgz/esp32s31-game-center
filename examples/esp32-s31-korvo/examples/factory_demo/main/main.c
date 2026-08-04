/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "bsp/esp32_s31_korvo.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ui.h"
#include "ui_app.h"

#define UI_DISPLAY_WIDTH              480
#define UI_DISPLAY_HEIGHT             480
#define UI_DISPLAY_BUFFER_HEIGHT      160
#define UI_DISPLAY_TASK_STACK_SIZE    (16 * 1024)
#define UI_DISPLAY_ENABLE_PPA_ACCEL   true
#define UI_INIT_TASK_STACK_SIZE       (12 * 1024)
#define UI_INIT_TASK_PRIORITY         4
#define UI_INIT_TASK_CORE_ID          1

static const char *TAG = "factory_demo";

void app_main(void)
{
    bsp_display_config_t display_cfg = BSP_DISPLAY_DEFAULT_CONFIG();
    display_cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL;
    display_cfg.buffer_height = UI_DISPLAY_BUFFER_HEIGHT;
    display_cfg.task_stack_size = UI_DISPLAY_TASK_STACK_SIZE;
    display_cfg.enable_ppa_accel = UI_DISPLAY_ENABLE_PPA_ACCEL;

    lv_disp_t *disp = bsp_display_start_with_config(&display_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "Display start failed");
        return;
    }
    ESP_LOGI(TAG, "Display started");
    
    if (bsp_display_lock(-1)) {
        lv_display_set_resolution(disp, UI_DISPLAY_WIDTH, UI_DISPLAY_HEIGHT);
        bsp_display_unlock();
        ESP_LOGI(TAG, "LVGL logical display size set to %d x %d", UI_DISPLAY_WIDTH, UI_DISPLAY_HEIGHT);
    } else {
        ESP_LOGE(TAG, "LVGL lock failed while setting display resolution");
        return;
    }

    if (bsp_display_lock(-1)) {
        ui_init();
        ui_app_init();
        bsp_display_unlock();
    } else {
        ESP_LOGE(TAG, "LVGL lock failed");
    }

    ESP_LOGI(TAG, "Factory demo display init done");
}
