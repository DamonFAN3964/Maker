#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// Initialize ST7789 display using pre-defined GPIO mapping
esp_err_t st7789_display_init(void);

// Draw basic color bars and a gradient to validate panel wiring
esp_err_t st7789_display_run_self_test(void);

// Draw a single digit (0-9) centered on the screen
esp_err_t st7789_display_show_digit(uint8_t digit);

// Create a FreeRTOS任务循环显示指定数字（0-9），一直刷新显示
esp_err_t st7789_display_start_digit_task(uint8_t digit);

// 清屏为单色
esp_err_t st7789_display_clear(uint16_t color);

// 显示温度/湿度/档位/水量
esp_err_t st7789_display_show_status(float temp_c, float humi_percent, uint8_t gear, float water_ml,
                                     bool timer_mode, uint32_t timer_remaining_sec,
                                     const char *date_str, const char *time_str,
                                     bool schedule_enabled, bool schedule_should_run,
                                     const char *sched_start_time, const char *sched_end_time);

// Draw a small command-received animation overlay (call periodically while active)
esp_err_t st7789_display_draw_cmd_anim(uint8_t frame);
