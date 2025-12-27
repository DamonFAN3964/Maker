#ifndef _TTP223_H
#define _TTP223_H

#include "esp_err.h"
#include "driver/gpio.h"

// Default TTP223 signal pin (active high). Can be overridden before including.
#ifndef TTP223_GPIO
#define TTP223_GPIO GPIO_NUM_20
#endif

esp_err_t ttp223_init(gpio_num_t pin);
esp_err_t ttp223_read(gpio_num_t pin, int *level);
esp_err_t ttp223_enable_interrupt(gpio_num_t pin, gpio_int_type_t intr_type,
								  gpio_isr_t handler, void *arg);

#endif
