#include "ttp223.h"
#include "esp_check.h"

static bool s_isr_service_installed = false;

esp_err_t ttp223_init(gpio_num_t pin)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << pin,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    return gpio_config(&io_conf);
}

esp_err_t ttp223_read(gpio_num_t pin, int *level)
{
    if (!level) return ESP_ERR_INVALID_ARG;
    *level = gpio_get_level(pin);
    return ESP_OK;
}

esp_err_t ttp223_enable_interrupt(gpio_num_t pin, gpio_int_type_t intr_type,
                                  gpio_isr_t handler, void *arg)
{
    ESP_RETURN_ON_FALSE(handler != NULL, ESP_ERR_INVALID_ARG, "ttp223", "handler null");

    ESP_RETURN_ON_ERROR(gpio_set_intr_type(pin, intr_type), "ttp223", "set intr type fail");

    if (!s_isr_service_installed) {
        esp_err_t err = gpio_install_isr_service(0);
        if (err == ESP_ERR_INVALID_STATE) {
            // already installed by other code
            s_isr_service_installed = true;
        } else {
            ESP_RETURN_ON_ERROR(err, "ttp223", "install isr service fail");
            s_isr_service_installed = true;
        }
    }

    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(pin, handler, arg), "ttp223", "add isr handler fail");
    return ESP_OK;
}
