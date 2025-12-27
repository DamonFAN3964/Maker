/**
 * ESP32-S3 Audio Streaming Player
 * 
 * Hardware:
 * - MCU: ESP32-S3
 * - Audio Codec: ES8311
 * - Power Amp: NS4150B
 * 
 * Features:
 * - WiFi connectivity
 * - HTTP audio streaming from cloud server
 * - I2S audio output
 * - I2C codec control
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "cJSON.h"

#include "wifi_manager.h"
#include "audio_player.h"
#include "http_client.h"
#include "sr04.h"
#include "humidifier.h"
#include "ttp223.h"
#include "ble_service.h"
#include "st7789_display.h"

#ifndef CONFIG_CLOUD_API_LOAD_URL
#define CONFIG_CLOUD_API_LOAD_URL CLOUD_API_LOAD_URL
#endif
#ifndef CONFIG_CLOUD_REPORT_INTERVAL_MS
#define CONFIG_CLOUD_REPORT_INTERVAL_MS 60000
#endif
#ifndef CONFIG_CLOUD_TIME_URL
#define CONFIG_CLOUD_TIME_URL "http://101.126.138.110:8000/api/time"
#endif
#ifndef CONFIG_MCU_COMMANDS_ENABLE
#define CONFIG_MCU_COMMANDS_ENABLE 1
#endif
#ifndef CONFIG_MCU_POWER_URL
#define CONFIG_MCU_POWER_URL "http://101.126.138.110:8000/api/mcu/power"
#endif
#ifndef CONFIG_MCU_LEVEL_URL
#define CONFIG_MCU_LEVEL_URL "http://101.126.138.110:8000/api/mcu/level"
#endif
#ifndef CONFIG_MCU_SCHEDULE_URL
#define CONFIG_MCU_SCHEDULE_URL "http://101.126.138.110:8000/api/mcu/schedule"
#endif
#ifndef CONFIG_MCU_COMMANDS_POLL_INTERVAL_MS
#define CONFIG_MCU_COMMANDS_POLL_INTERVAL_MS 2000
#endif
#ifndef CONFIG_MCU_SCHEDULE_DURATION_MIN
#define CONFIG_MCU_SCHEDULE_DURATION_MIN 30
#endif

static const char *TAG = "main";

static QueueHandle_t ttp223_evt_queue = NULL;
static QueueHandle_t s_display_evt_queue = NULL;

static portMUX_TYPE s_timer_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t s_timed_remaining_sec = 0;

static portMUX_TYPE s_time_lock = portMUX_INITIALIZER_UNLOCKED;
static char s_cloud_date[16] = "--";
static char s_cloud_time[16] = "--";
static uint32_t s_cloud_unix = 0;
static int64_t s_cloud_unix_set_us = 0;

static portMUX_TYPE s_sched_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_schedule_enabled = false;
static bool s_schedule_should_run = false;
static char s_schedule_start_time[8] = "--:--";
static char s_schedule_end_time[8] = "--:--";
static uint32_t s_schedule_end_epoch = 0;
static bool s_schedule_running = false;

static TaskHandle_t s_upload_task = NULL;
static gptimer_handle_t s_upload_gptimer = NULL;
static gptimer_handle_t s_sched_gptimer = NULL;

typedef enum {
    DISPLAY_EVT_SERVER_CMD_OK = 1,
} display_evt_type_t;

typedef struct {
    display_evt_type_t type;
} display_evt_t;

static inline void timed_mode_set_remaining_sec(uint32_t sec)
{
    portENTER_CRITICAL(&s_timer_lock);
    s_timed_remaining_sec = sec;
    portEXIT_CRITICAL(&s_timer_lock);
}

static inline uint32_t timed_mode_get_remaining_sec(void)
{
    uint32_t sec;
    portENTER_CRITICAL(&s_timer_lock);
    sec = s_timed_remaining_sec;
    portEXIT_CRITICAL(&s_timer_lock);
    return sec;
}

static void cloud_time_set(const char *date_str, const char *time_str, uint32_t unix_sec)
{
    if (!date_str || !time_str) {
        return;
    }
    portENTER_CRITICAL(&s_time_lock);
    strncpy(s_cloud_date, date_str, sizeof(s_cloud_date) - 1);
    s_cloud_date[sizeof(s_cloud_date) - 1] = '\0';
    strncpy(s_cloud_time, time_str, sizeof(s_cloud_time) - 1);
    s_cloud_time[sizeof(s_cloud_time) - 1] = '\0';
    s_cloud_unix = unix_sec;
    s_cloud_unix_set_us = esp_timer_get_time();
    portEXIT_CRITICAL(&s_time_lock);
}

static void cloud_time_get(char *out_date, size_t out_date_sz, char *out_time, size_t out_time_sz)
{
    if (!out_date || out_date_sz == 0 || !out_time || out_time_sz == 0) {
        return;
    }
    portENTER_CRITICAL(&s_time_lock);
    strncpy(out_date, s_cloud_date, out_date_sz - 1);
    out_date[out_date_sz - 1] = '\0';
    strncpy(out_time, s_cloud_time, out_time_sz - 1);
    out_time[out_time_sz - 1] = '\0';
    portEXIT_CRITICAL(&s_time_lock);
}

static void schedule_set(bool enabled, bool should_run, const char *start_time, const char *end_time, uint32_t end_epoch)
{
    portENTER_CRITICAL(&s_sched_lock);
    s_schedule_enabled = enabled;
    s_schedule_should_run = should_run;
    if (start_time) {
        strncpy(s_schedule_start_time, start_time, sizeof(s_schedule_start_time) - 1);
        s_schedule_start_time[sizeof(s_schedule_start_time) - 1] = '\0';
    }
    if (end_time) {
        strncpy(s_schedule_end_time, end_time, sizeof(s_schedule_end_time) - 1);
        s_schedule_end_time[sizeof(s_schedule_end_time) - 1] = '\0';
    }
    s_schedule_end_epoch = end_epoch;
    portEXIT_CRITICAL(&s_sched_lock);
}

static void schedule_get(bool *enabled, bool *should_run)
{
    portENTER_CRITICAL(&s_sched_lock);
    if (enabled) {
        *enabled = s_schedule_enabled;
    }
    if (should_run) {
        *should_run = s_schedule_should_run;
    }
    portEXIT_CRITICAL(&s_sched_lock);
}

static void schedule_times_get(char *out_start, size_t out_start_sz, char *out_end, size_t out_end_sz)
{
    if (!out_start || out_start_sz == 0 || !out_end || out_end_sz == 0) {
        return;
    }
    portENTER_CRITICAL(&s_sched_lock);
    strncpy(out_start, s_schedule_start_time, out_start_sz - 1);
    out_start[out_start_sz - 1] = '\0';
    strncpy(out_end, s_schedule_end_time, out_end_sz - 1);
    out_end[out_end_sz - 1] = '\0';
    portEXIT_CRITICAL(&s_sched_lock);
}

static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
    // Days since 1970-01-01, valid for Gregorian calendar.
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? (unsigned)-3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static bool parse_ymd(const char *s, int *y, int *m, int *d)
{
    if (!s || !y || !m || !d) {
        return false;
    }
    return sscanf(s, "%d-%d-%d", y, m, d) == 3;
}

static bool cjson_is_bool_item(const cJSON *item)
{
    if (!item) {
        return false;
    }
    return item->type == cJSON_True || item->type == cJSON_False;
}
typedef struct {
    float temp_c;
    float humi_percent;
    float distance_cm;
    float water_ml;
} system_status_t;

static system_status_t s_status = {
    .temp_c = 25.0f,
    .humi_percent = 55.0f,
    .distance_cm = 0.0f,
    .water_ml = 0.0f,
};

static const gpio_num_t s_humidifier_pins[3] = {
    HUMIDIFIER1_GPIO,
    HUMIDIFIER2_GPIO,
    HUMIDIFIER3_GPIO,
};

static uint8_t s_gear_level = 0; // 0~3
static uint8_t s_last_nonzero_gear = 1;

static void humidifier_apply_gear(uint8_t gear)
{
    if (gear > 3) gear = 3;
    s_gear_level = gear;
    if (gear > 0) {
        s_last_nonzero_gear = gear;
    }
    for (int i = 0; i < 3; ++i) {
        if (i < gear) {
            humidifier_on(s_humidifier_pins[i]);
        } else {
            humidifier_off(s_humidifier_pins[i]);
        }
    }
}

static uint8_t humidifier_get_last_nonzero_gear(void)
{
    return s_last_nonzero_gear == 0 ? 1 : s_last_nonzero_gear;
}

#define LED_PIN    GPIO_NUM_17
#define LED_PIN_SEL  (1ULL<<LED_PIN)

// SR04 pins (adjust if needed)
#define SR04_TRIG_GPIO  GPIO_NUM_6
#define SR04_ECHO_GPIO  GPIO_NUM_7

// TTP223 touch pin (input, active high)
#define TTP223_GPIO_PIN TTP223_GPIO

// Software debounce for touch key (ms)
#define TTP223_DEBOUNCE_MS 200

/* Operate LED on/off */
#define LED_OFF gpio_set_level(LED_PIN, 1)
#define LED_ON  gpio_set_level(LED_PIN, 0)

/******************************************************************************
 * func name   : bsp_led_init
 * para        : NULL
 * return      : led init result
 * description : LED init,LED->IO17 (status indicator)
******************************************************************************/
void bsp_led_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = LED_PIN_SEL,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    LED_OFF;
}

void status_led_task(void *pvParameters)
{
    while (1) {
        if (wifi_is_connected()) {
            // WiFi connected - slow blink
            LED_ON;
            vTaskDelay(pdMS_TO_TICKS(100));
            LED_OFF;
            vTaskDelay(pdMS_TO_TICKS(1900));
        } else {
            // WiFi not connected - fast blink
            LED_ON;
            vTaskDelay(pdMS_TO_TICKS(200));
            LED_OFF;
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

static void sr04_task(void *pvParameters)
{
    sr04_config_t cfg = {
        .trig_gpio = SR04_TRIG_GPIO,
        .echo_gpio = SR04_ECHO_GPIO,
        .timeout_us = 30000,
    };

    if (sr04_init(&cfg) != ESP_OK) {
        ESP_LOGE(TAG, "SR04 init failed");
        vTaskDelete(NULL);
    }

    while (1) {
        float dist_cm = 0.0f;
        esp_err_t ret = sr04_measure_cm(&cfg, &dist_cm);
        if (ret == ESP_OK) {
            s_status.distance_cm = dist_cm;
            float height_cm = 7.0f - dist_cm;
            if (height_cm < 0) height_cm = 0;
            s_status.water_ml = height_cm * 205.2507f; // 1 cm^3 ~ 1 mL
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void IRAM_ATTR ttp223_isr(void *arg)
{
    (void)arg;
    int evt = 1;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (ttp223_evt_queue) {
        // Queue is length 1; overwrite to avoid bounce flooding.
        xQueueOverwriteFromISR(ttp223_evt_queue, &evt, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void ttp223_task(void *pvParameters)
{
    int evt = 0;
    int64_t last_touch_us = 0;
    const int64_t debounce_us = (int64_t)TTP223_DEBOUNCE_MS * 1000;

    while (xQueueReceive(ttp223_evt_queue, &evt, portMAX_DELAY)) {
        (void)evt;
        int64_t now_us = esp_timer_get_time();
        if (last_touch_us != 0 && (now_us - last_touch_us) < debounce_us) {
            continue;
        }

        // Wait a bit and confirm the pin is still high (filters noise/glitches).
        vTaskDelay(pdMS_TO_TICKS(30));
        if (gpio_get_level(TTP223_GPIO_PIN) == 0) {
            continue;
        }

        last_touch_us = now_us;
        {
            uint8_t next = (s_gear_level + 1) % 4;
            // Manual touch cancels timed mode
            timed_mode_set_remaining_sec(0);
            humidifier_apply_gear(next);
            ESP_LOGI(TAG, "Touch -> gear %u", next);
        }
    }
    vTaskDelete(NULL);
}

static void update_simulated_th(void)
{
    int64_t us = esp_timer_get_time();
    float t = (us % 60000000) / 1000000.0f; // 0-60s
    s_status.temp_c = 24.0f + 2.0f * sinf(t * 0.1f);
    s_status.humi_percent = 55.0f + 5.0f * cosf(t * 0.07f);
}

static void display_task(void *pvParameters)
{
    int64_t last_th_update_us = 0;
    int64_t last_full_draw_us = 0;
    int64_t cmd_anim_start_us = 0;
    int64_t cmd_anim_end_us = 0;

    for (;;) {
        int64_t now_us = esp_timer_get_time();

        if (s_display_evt_queue) {
            display_evt_t evt;
            while (xQueueReceive(s_display_evt_queue, &evt, 0) == pdTRUE) {
                if (evt.type == DISPLAY_EVT_SERVER_CMD_OK) {
                    cmd_anim_start_us = now_us;
                    cmd_anim_end_us = now_us + 1200 * 1000;
                    last_full_draw_us = 0; // force refresh immediately
                }
            }
        }

        if (last_th_update_us == 0 || (now_us - last_th_update_us) >= 1000 * 1000) {
            update_simulated_th();
            last_th_update_us = now_us;
        }

        bool need_full_draw = (last_full_draw_us == 0) || ((now_us - last_full_draw_us) >= 1000 * 1000);
        if (need_full_draw) {
            uint32_t remaining = timed_mode_get_remaining_sec();
            bool timer_mode = remaining > 0;
            char date_str[16];
            char time_str[16];
            cloud_time_get(date_str, sizeof(date_str), time_str, sizeof(time_str));
            bool sch_en = false;
            bool sch_run = false;
            schedule_get(&sch_en, &sch_run);
            char sch_st[8];
            char sch_ed[8];
            schedule_times_get(sch_st, sizeof(sch_st), sch_ed, sizeof(sch_ed));
            st7789_display_show_status(s_status.temp_c, s_status.humi_percent, s_gear_level, s_status.water_ml,
                                      timer_mode, remaining, date_str, time_str, sch_en, sch_run, sch_st, sch_ed);
            last_full_draw_us = now_us;
        }

        if (cmd_anim_end_us != 0 && now_us < cmd_anim_end_us) {
            uint32_t frame = (uint32_t)((now_us - cmd_anim_start_us) / (100 * 1000));
            st7789_display_draw_cmd_anim((uint8_t)frame);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// 简单 1kHz 正弦测试音播放（约 1 秒），用于验证 MAX98357 输出
typedef struct {
    TaskHandle_t task;
} gptimer_notify_ctx_t;

static gptimer_notify_ctx_t s_upload_timer_ctx = {0};

static bool IRAM_ATTR upload_timer_cb(gptimer_handle_t timer,
                                      const gptimer_alarm_event_data_t *edata,
                                      void *user_ctx)
{
    (void)timer;
    (void)edata;
    BaseType_t hp = pdFALSE;
    gptimer_notify_ctx_t *ctx = (gptimer_notify_ctx_t *)user_ctx;
    if (ctx && ctx->task) {
        vTaskNotifyGiveFromISR(ctx->task, &hp);
    }
    return hp == pdTRUE;
}

static bool IRAM_ATTR sched_timer_cb(gptimer_handle_t timer,
                                     const gptimer_alarm_event_data_t *edata,
                                     void *user_ctx)
{
    (void)timer;
    (void)edata;

    portENTER_CRITICAL_ISR(&s_timer_lock);
    if (s_timed_remaining_sec > 0) {
        s_timed_remaining_sec--;
    }
    portEXIT_CRITICAL_ISR(&s_timer_lock);

    (void)user_ctx;
    return false;
}

static void cloud_upload_task(void *pvParameters)
{
    (void)pvParameters;
    const char *url = CONFIG_CLOUD_API_LOAD_URL;

    // Backward-compatible fallback: older sdkconfig may still contain /api/load.
    if (!url || url[0] == '\0' || strcmp(url, "http://101.126.138.110:8000/api/load") == 0) {
        url = CLOUD_API_LOAD_URL;
    }

    // Initial time sync for display/logging
    if (wifi_is_connected()) {
        char resp[512];
        int http_status = 0;
        if (http_client_get_json(CONFIG_CLOUD_TIME_URL, resp, sizeof(resp), &http_status) == ESP_OK) {
            cJSON *root = cJSON_Parse(resp);
            if (root) {
                cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
                cJSON *date = cJSON_GetObjectItemCaseSensitive(root, "date");
                cJSON *time = cJSON_GetObjectItemCaseSensitive(root, "time");
                cJSON *unixv = cJSON_GetObjectItemCaseSensitive(root, "unix");
                if (cJSON_IsTrue(success) && cJSON_IsString(date) && date->valuestring &&
                    cJSON_IsString(time) && time->valuestring && cJSON_IsNumber(unixv)) {
                    cloud_time_set(date->valuestring, time->valuestring, (uint32_t)unixv->valuedouble);
                    ESP_LOGI(TAG, "Cloud time: %s %s", date->valuestring, time->valuestring);
                }
                cJSON_Delete(root);
            }
        }
    }

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (wifi_is_connected()) {
            // Time sync (date/time shown on screen + UART log)
            {
                char resp[512];
                int http_status = 0;
                if (http_client_get_json(CONFIG_CLOUD_TIME_URL, resp, sizeof(resp), &http_status) == ESP_OK) {
                    cJSON *root = cJSON_Parse(resp);
                    if (root) {
                        cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
                        cJSON *date = cJSON_GetObjectItemCaseSensitive(root, "date");
                        cJSON *time = cJSON_GetObjectItemCaseSensitive(root, "time");
                        cJSON *unixv = cJSON_GetObjectItemCaseSensitive(root, "unix");
                        if (cJSON_IsTrue(success) && cJSON_IsString(date) && date->valuestring &&
                            cJSON_IsString(time) && time->valuestring && cJSON_IsNumber(unixv)) {
                            cloud_time_set(date->valuestring, time->valuestring, (uint32_t)unixv->valuedouble);
                            ESP_LOGI(TAG, "Cloud time: %s %s", date->valuestring, time->valuestring);
                        }
                        cJSON_Delete(root);
                    }
                }
            }

            http_client_post_temp_humi(url, s_status.temp_c, s_status.humi_percent);
        }
    }
}

static esp_err_t mcu_timers_start(void)
{
    const uint64_t upload_interval_us = (uint64_t)CONFIG_CLOUD_REPORT_INTERVAL_MS * 1000ULL;

    gptimer_config_t tcfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };

    // Upload timer (periodic, e.g. 60s)
    ESP_RETURN_ON_ERROR(gptimer_new_timer(&tcfg, &s_upload_gptimer), TAG, "new upload gptimer failed");
    gptimer_event_callbacks_t upload_cbs = {
        .on_alarm = upload_timer_cb,
    };
    s_upload_timer_ctx.task = s_upload_task;
    ESP_RETURN_ON_ERROR(gptimer_register_event_callbacks(s_upload_gptimer, &upload_cbs, &s_upload_timer_ctx), TAG, "reg upload cb failed");
    gptimer_alarm_config_t upload_alarm = {
        .reload_count = 0,
        .alarm_count = upload_interval_us,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_RETURN_ON_ERROR(gptimer_set_alarm_action(s_upload_gptimer, &upload_alarm), TAG, "set upload alarm failed");
    ESP_RETURN_ON_ERROR(gptimer_enable(s_upload_gptimer), TAG, "enable upload gptimer failed");
    ESP_RETURN_ON_ERROR(gptimer_start(s_upload_gptimer), TAG, "start upload gptimer failed");

    // Schedule countdown timer (1s tick)
    ESP_RETURN_ON_ERROR(gptimer_new_timer(&tcfg, &s_sched_gptimer), TAG, "new sched gptimer failed");
    gptimer_event_callbacks_t sched_cbs = {
        .on_alarm = sched_timer_cb,
    };
    ESP_RETURN_ON_ERROR(gptimer_register_event_callbacks(s_sched_gptimer, &sched_cbs, NULL), TAG, "reg sched cb failed");
    gptimer_alarm_config_t sched_alarm = {
        .reload_count = 0,
        .alarm_count = 1000000ULL,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_RETURN_ON_ERROR(gptimer_set_alarm_action(s_sched_gptimer, &sched_alarm), TAG, "set sched alarm failed");
    ESP_RETURN_ON_ERROR(gptimer_enable(s_sched_gptimer), TAG, "enable sched gptimer failed");
    ESP_RETURN_ON_ERROR(gptimer_start(s_sched_gptimer), TAG, "start sched gptimer failed");

    return ESP_OK;
}

static void display_notify_cmd_ok(void)
{
    if (!s_display_evt_queue) {
        return;
    }
    display_evt_t evt = {.type = DISPLAY_EVT_SERVER_CMD_OK};
    xQueueOverwrite(s_display_evt_queue, &evt);
}

static void mcu_apply_power_action(const char *action)
{
    if (!action) {
        return;
    }

    if (strcmp(action, "off") == 0) {
        timed_mode_set_remaining_sec(0);
        humidifier_apply_gear(0);
        display_notify_cmd_ok();
        return;
    }
    if (strcmp(action, "on") == 0) {
        uint8_t gear = humidifier_get_last_nonzero_gear();
        if (gear == 0) {
            gear = 1;
        }
        humidifier_apply_gear(gear);
        display_notify_cmd_ok();
        return;
    }

    ESP_LOGW(TAG, "Unknown power action: %s", action);
}

static void mcu_apply_level_cmd(int level)
{
    if (level < 0) {
        level = 0;
    }
    if (level > 3) {
        level = 3;
    }
    if (level == 0) {
        timed_mode_set_remaining_sec(0);
    }
    humidifier_apply_gear((uint8_t)level);
    display_notify_cmd_ok();
}

static uint32_t build_epoch_from_date_time(const char *date_ymd, const char *time_hms_or_hm)
{
    int y, m, d;
    if (!parse_ymd(date_ymd, &y, &m, &d)) {
        return 0;
    }
    int hh = 0, mm = 0, ss = 0;
    int n = sscanf(time_hms_or_hm, "%d:%d:%d", &hh, &mm, &ss);
    if (n < 2) {
        return 0;
    }
    if (n == 2) {
        ss = 0;
    }
    if (m < 1 || m > 12 || d < 1 || d > 31 || hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) {
        return 0;
    }
    int64_t days = days_from_civil(y, (unsigned)m, (unsigned)d);
    if (days < 0) {
        return 0;
    }
    uint64_t sec = (uint64_t)days * 86400ULL + (uint64_t)hh * 3600ULL + (uint64_t)mm * 60ULL + (uint64_t)ss;
    if (sec > 0xFFFFFFFFULL) {
        return 0;
    }
    return (uint32_t)sec;
}

static void mcu_apply_schedule_state(bool enabled,
                                    bool should_run,
                                    const char *start_date,
                                    const char *start_time,
                                    const char *end_date,
                                    const char *end_time)
{
    uint32_t end_epoch = 0;
    if (enabled && end_date && end_time) {
        end_epoch = build_epoch_from_date_time(end_date, end_time);
    }
    schedule_set(enabled, should_run, start_time, end_time, end_epoch);

    if (!enabled) {
        timed_mode_set_remaining_sec(0);
        s_schedule_running = false;
        return;
    }

    if (!should_run) {
        timed_mode_set_remaining_sec(0);
        if (s_gear_level != 0) {
            humidifier_apply_gear(0);
            display_notify_cmd_ok();
        }
        s_schedule_running = false;
        return;
    }

    // should_run == true
    if (!s_schedule_running) {
        char now_date[16];
        char now_time[16];
        cloud_time_get(now_date, sizeof(now_date), now_time, sizeof(now_time));
        uint32_t now_epoch = build_epoch_from_date_time(now_date, now_time);
        uint32_t remaining = 0;
        if (end_epoch != 0 && now_epoch != 0 && end_epoch > now_epoch) {
            remaining = end_epoch - now_epoch;
        } else {
            remaining = (uint32_t)CONFIG_MCU_SCHEDULE_DURATION_MIN * 60U;
        }
        timed_mode_set_remaining_sec(remaining);
    }

    if (s_gear_level == 0) {
        humidifier_apply_gear(humidifier_get_last_nonzero_gear());
        display_notify_cmd_ok();
    }
    s_schedule_running = true;
}

static void mcu_commands_task(void *pvParameters)
{
    (void)pvParameters;

    const TickType_t interval = pdMS_TO_TICKS(CONFIG_MCU_COMMANDS_POLL_INTERVAL_MS);
    char resp[768];

    for (;;) {
        if (wifi_is_connected()) {
            // 1) Power command (marks executed by server)
            {
                int http_status = 0;
                if (http_client_get_json(CONFIG_MCU_POWER_URL, resp, sizeof(resp), &http_status) == ESP_OK) {
                    cJSON *root = cJSON_Parse(resp);
                    if (root) {
                        cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
                        cJSON *has_cmd = cJSON_GetObjectItemCaseSensitive(root, "has_command");
                        cJSON *action = cJSON_GetObjectItemCaseSensitive(root, "action");
                        if (cJSON_IsTrue(success) && cJSON_IsTrue(has_cmd) && cJSON_IsString(action) && action->valuestring) {
                            ESP_LOGI(TAG, "Cloud power cmd: %s", action->valuestring);
                            mcu_apply_power_action(action->valuestring);
                        }
                        cJSON_Delete(root);
                    }
                }
            }

            // 2) Level command (marks executed by server)
            {
                int http_status = 0;
                if (http_client_get_json(CONFIG_MCU_LEVEL_URL, resp, sizeof(resp), &http_status) == ESP_OK) {
                    cJSON *root = cJSON_Parse(resp);
                    if (root) {
                        cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
                        cJSON *has_cmd = cJSON_GetObjectItemCaseSensitive(root, "has_command");
                        cJSON *level = cJSON_GetObjectItemCaseSensitive(root, "level");
                        if (cJSON_IsTrue(success) && cJSON_IsTrue(has_cmd) && cJSON_IsNumber(level)) {
                            ESP_LOGI(TAG, "Cloud level cmd: %d", (int)level->valuedouble);
                            mcu_apply_level_cmd((int)level->valuedouble);
                        }
                        cJSON_Delete(root);
                    }
                }
            }

            // 3) Schedule config (should_run polled continuously; not marked executed)
            {
                int http_status = 0;
                if (http_client_get_json(CONFIG_MCU_SCHEDULE_URL, resp, sizeof(resp), &http_status) == ESP_OK) {
                    cJSON *root = cJSON_Parse(resp);
                    if (root) {
                        cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
                        cJSON *enabled = cJSON_GetObjectItemCaseSensitive(root, "enabled");
                        cJSON *should_run = cJSON_GetObjectItemCaseSensitive(root, "should_run");
                        cJSON *start_date = cJSON_GetObjectItemCaseSensitive(root, "start_date");
                        cJSON *start_time = cJSON_GetObjectItemCaseSensitive(root, "start_time");
                        cJSON *end_date = cJSON_GetObjectItemCaseSensitive(root, "end_date");
                        cJSON *end_time = cJSON_GetObjectItemCaseSensitive(root, "end_time");
                        if (cJSON_IsTrue(success) &&
                            cjson_is_bool_item(enabled) &&
                            cjson_is_bool_item(should_run) &&
                            cJSON_IsString(start_date) && cJSON_IsString(start_time) &&
                            cJSON_IsString(end_date) && cJSON_IsString(end_time)) {
                            bool en = cJSON_IsTrue(enabled);
                            bool run = cJSON_IsTrue(should_run);
                            mcu_apply_schedule_state(en, run,
                                                    start_date->valuestring, start_time->valuestring,
                                                    end_date->valuestring, end_time->valuestring);
                        }
                        cJSON_Delete(root);
                    }
                }
            }
        }

        vTaskDelay(interval);
    }
}

static void play_test_tone(void)
{
    const int sample_rate = SAMPLE_RATE;
    const float freq = 1000.0f;
    const float amplitude = 0.4f; // 避免削顶
    const int duration_ms = 1000;
    const int samples = sample_rate * duration_ms / 1000;
    // 一次写入一小块，避免大栈占用
    int16_t buf[512]; // 256 stereo frames
    int frames_per_chunk = sizeof(buf) / (2 * sizeof(int16_t));
    int remaining = samples;
    int idx = 0;

    while (remaining > 0) {
        int frames = remaining < frames_per_chunk ? remaining : frames_per_chunk;
        for (int i = 0; i < frames; ++i, ++idx) {
            float s = amplitude * sinf(2.0f * (float)M_PI * freq * ((float)idx / sample_rate));
            int16_t v = (int16_t)(s * 32767);
            buf[2 * i] = v;     // L
            buf[2 * i + 1] = v; // R
        }
        audio_player_play((uint8_t *)buf, frames * 2 * sizeof(int16_t));
        remaining -= frames;
    }
    ESP_LOGI(TAG, "Test tone played (1kHz, 1s)");
}

void app_main(void)
{
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "ESP32-S3 Audio Streaming Player");
    ESP_LOGI(TAG, "=================================");
    for (int i = 0; i < 3; ++i) {
        humidifier_init(s_humidifier_pins[i]);
    }
    // Initialize LED for status indication
    bsp_led_init();

    if (st7789_display_init() == ESP_OK) {
        st7789_display_run_self_test();
        // Single-slot queue: latest display event wins (prevents flooding)
        s_display_evt_queue = xQueueCreate(1, sizeof(display_evt_t));
        xTaskCreate(display_task, "lcd_display", 4096, NULL, 4, NULL);
    } else {
        ESP_LOGE(TAG, "LCD init failed");
    }
    
    // Initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    ret = wifi_init_sta();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi initialization failed!");
        LED_ON; // Keep LED on to indicate error
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "WiFi connected successfully");
    
    // Initialize audio player
    ESP_LOGI(TAG, "Initializing audio player...");
    ret = audio_player_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio player initialization failed!");
        // while (1) {
        //     vTaskDelay(pdMS_TO_TICKS(1000));
        // }
        for(uint8_t i=0;i<10;i++)vTaskDelay(pdMS_TO_TICKS(1000));
    }
    // ESP_LOGI(TAG, "Audio player initialized successfully");
    ESP_LOGI(TAG, "audio player init end");

    // Initialize BLE (receive temp/humi from external MCU)
    if (ble_service_init() != ESP_OK) {
        ESP_LOGE(TAG, "BLE init failed");
    }
    
    // Set volume to 70%
    // audio_player_set_volume(70);
    
    // Start status LED task
    xTaskCreate(status_led_task, "status_led", 2048, NULL, 3, NULL);

    // Start SR04 distance task
    // Increase stack to avoid overflow from logging and timing calls
    xTaskCreate(sr04_task, "sr04_task", 4096, NULL, 4, NULL);

    // Setup TTP223 touch interrupt + event task
    if (ttp223_init(TTP223_GPIO_PIN) != ESP_OK) {
        ESP_LOGE(TAG, "TTP223 init failed");
    } else {
        // Use length 1 queue; ISR overwrites to avoid bounce flooding.
        ttp223_evt_queue = xQueueCreate(1, sizeof(int));
        if (!ttp223_evt_queue) {
            ESP_LOGE(TAG, "TTP223 queue alloc failed");
        } else if (ttp223_enable_interrupt(TTP223_GPIO_PIN, GPIO_INTR_POSEDGE, ttp223_isr, (void *)TTP223_GPIO_PIN) != ESP_OK) {
            ESP_LOGE(TAG, "TTP223 interrupt enable failed");
        } else {
            xTaskCreate(ttp223_task, "ttp223_task", 2048, NULL, 4, NULL);
        }
    }
    
        humidifier_apply_gear(0);
    
    // Initialize HTTP client
    http_client_init();

    // Timer-driven cloud upload + timed-mode countdown
    xTaskCreate(cloud_upload_task, "cloud_upload", 4096, NULL, 4, &s_upload_task);
    if (mcu_timers_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MCU timers");
    }

#if CONFIG_MCU_COMMANDS_ENABLE
    // Poll cloud for pending control commands (humidifier on/off/level)
    xTaskCreate(mcu_commands_task, "mcu_commands", 4096, NULL, 4, NULL);
#endif
    
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "System ready!");
    ESP_LOGI(TAG, "=================================");
    
    // Example: Start streaming audio from server
    // Configure CONFIG_SERVER_URL via menuconfig (Audio Player Configuration)
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Starting PCM HTTP stream...");
    ESP_LOGI(TAG, "Ensure CONFIG_SERVER_URL points to raw PCM (s16le, stereo, %d Hz)", SAMPLE_RATE);
    http_client_stream_audio(CONFIG_SERVER_URL);
    
    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
