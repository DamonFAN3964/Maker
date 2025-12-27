#include "humidifier.h"

esp_err_t humidifier_init(gpio_num_t ctrl_pin)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << ctrl_pin,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }
    // default off
    gpio_set_level(ctrl_pin, 0);
    return ESP_OK;
}

void humidifier_on(gpio_num_t ctrl_pin)
{
    gpio_set_level(ctrl_pin, 1);
}

void humidifier_off(gpio_num_t ctrl_pin)
{
    gpio_set_level(ctrl_pin, 0);
}
