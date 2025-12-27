#pragma once
#include "esp_err.h"

// 初始化 BLE GATT Client：主动去连接外部温湿度采集端（该端为 GATT Server）。
esp_err_t ble_service_init(void);

// 获取最近一次接收到的温湿度（x100，例：2534 表示 25.34）。
void ble_get_latest_th(int16_t *temp_x100, int16_t *humi_x100);
