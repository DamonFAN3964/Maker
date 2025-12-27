#include "sr04.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h"  // for esp_rom_delay_us

static const char *TAG = "SR04";

esp_err_t sr04_init(const sr04_config_t *cfg)
{
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << cfg->trig_gpio,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << cfg->echo_gpio;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

    gpio_set_level(cfg->trig_gpio, 0);
    return ESP_OK;
}

esp_err_t sr04_measure_cm(const sr04_config_t *cfg, float *out_cm)
{
    if (!cfg || !out_cm) {
        return ESP_ERR_INVALID_ARG;
    }

    // Send 10us trigger pulse
    gpio_set_level(cfg->trig_gpio, 0);
    esp_rom_delay_us(2);
    gpio_set_level(cfg->trig_gpio, 1);
    esp_rom_delay_us(10);
    gpio_set_level(cfg->trig_gpio, 0);

    const uint32_t timeout = cfg->timeout_us ? cfg->timeout_us : 30000; // default ~5m
    const uint64_t t_start = esp_timer_get_time();

    // Wait for echo high
    while (gpio_get_level(cfg->echo_gpio) == 0) {
        if ((esp_timer_get_time() - t_start) > timeout) {
            ESP_LOGW(TAG, "Echo high timeout");
            return ESP_ERR_TIMEOUT;
        }
    }

    const uint64_t t_rise = esp_timer_get_time();

    // Measure high pulse width
    while (gpio_get_level(cfg->echo_gpio) == 1) {
        if ((esp_timer_get_time() - t_rise) > timeout) {
            ESP_LOGW(TAG, "Echo low timeout");
            return ESP_ERR_TIMEOUT;
        }
    }
    const uint64_t t_fall = esp_timer_get_time();

    uint64_t pulse_us = t_fall - t_rise;
    *out_cm = (float)pulse_us / 58.0f; // per SR04 formula
    return ESP_OK;
}
