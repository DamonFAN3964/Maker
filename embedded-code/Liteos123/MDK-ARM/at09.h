#ifndef __AT09_H
#define __AT09_H

#include "main.h"

/* AT09通过UART1通信 */
extern UART_HandleTypeDef huart1;

/**
 * @brief 发送AT命令到AT09模块并获取响应
 * @param cmd: AT命令字符串 (例如: "AT", "AT+NAME", "AT+BAUD4")
 * @param timeout_ms: 超时时间(毫秒)
 * @retval HAL_OK: 发送成功并收到响应
 *         HAL_TIMEOUT: 超时未收到响应
 */
HAL_StatusTypeDef AT09_SendCommand(char *cmd, uint16_t timeout_ms);

/**
 * @brief 配置AT09蓝牙模块
 * @note 默认配置: 波特率9600, 可修改模块名称和PIN码
 */
void AT09_Configure(void);

/**
 * @brief 启动AT09接收中断
 * @param buf: 接收缓冲区
 * @param len: 缓冲区长度
 */
void AT09_StartReceiveIT(uint8_t *buf, uint16_t len);

#endif /* __AT09_H */