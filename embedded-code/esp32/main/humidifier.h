#ifndef _HUMIDIFIER_H
#define _HUMIDIFIER_H

#include "esp_err.h"
#include "driver/gpio.h"

// Default control pins for three humidifier modules (override before include if needed)
#ifndef HUMIDIFIER1_GPIO
#define HUMIDIFIER1_GPIO GPIO_NUM_12
#endif
#ifndef HUMIDIFIER2_GPIO
#define HUMIDIFIER2_GPIO GPIO_NUM_38
#endif
#ifndef HUMIDIFIER3_GPIO
#define HUMIDIFIER3_GPIO GPIO_NUM_39
#endif

esp_err_t humidifier_init(gpio_num_t ctrl_pin);
void humidifier_on(gpio_num_t ctrl_pin);
void humidifier_off(gpio_num_t ctrl_pin);

#endif
