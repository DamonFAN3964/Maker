#ifndef _SR04_H
#define _SR04_H

#include "esp_err.h"
#include "driver/gpio.h"

// Typical SR04 timing: distance_cm = pulse_us / 58.0

// Default pins (can be overridden in code before including this header)
#ifndef SR04_TRIG_GPIO
#define SR04_TRIG_GPIO  GPIO_NUM_6
#endif

#ifndef SR04_ECHO_GPIO
#define SR04_ECHO_GPIO  GPIO_NUM_7
#endif

typedef struct {
    gpio_num_t trig_gpio;
    gpio_num_t echo_gpio;
    uint32_t timeout_us; // maximum wait time for echo, e.g., 30000 us (~5 m)
} sr04_config_t;

esp_err_t sr04_init(const sr04_config_t *cfg);
esp_err_t sr04_measure_cm(const sr04_config_t *cfg, float *out_cm);

#endif
