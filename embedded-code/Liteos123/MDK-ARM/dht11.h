#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"

/* DHT11 GPIO配置 */
#define DHT_PORT    GPIOB
#define DHT_PIN     GPIO_PIN_5

/**
 * @brief 初始化DHT11传感器
 * @retval HAL_OK: 初始化成功
 */
HAL_StatusTypeDef DHT11_Init(void);

/**
 * @brief 读取DHT11温湿度数据
 * @param humidity: 湿度指针 (0-100%)
 * @param temperature: 温度指针 (0-50°C)
 * @retval HAL_OK: 读取成功
 *         HAL_TIMEOUT: 通信超时
 *         HAL_ERROR: 校验和错误
 */
HAL_StatusTypeDef DHT11_Read(uint8_t *humidity, uint8_t *temperature);

#endif /* __DHT11_H */