#ifndef _WIFI_MANAGER_H
#define _WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

// WiFi configuration - modify these for your network
#define WIFI_SSID      "iPhone"
#define WIFI_PASS      "42358773"
#define WIFI_MAX_RETRY 5

esp_err_t wifi_init_sta(void);
bool wifi_is_connected(void);

#endif
