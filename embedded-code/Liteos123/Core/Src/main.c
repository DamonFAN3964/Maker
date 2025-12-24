/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : LiteOS Temperature & Humidity Sensor Module (All-in-One)
  ******************************************************************************
  * @attention
  *
  * STM32F103C8T6 - Sensor Module with Bluetooth (AT09)
  * - UART2 (PA2/PA3): Debug @ 115200
  * - UART1 (PB6/PB7): AT09 Bluetooth @ 9600
  * - PB3: Green LED (Status)
  * - PA4: Light Sensor (ADC)
  * - PB5: DHT11 Temperature & Humidity Sensor
  * - PB8: Buzzer (Active Low)
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "los_sys.h"
#include "los_task.h"
#include "los_memory.h"
#include "stdio.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// ========== DHT11配置 ==========
#define DHT_PORT    GPIOB
#define DHT_PIN     GPIO_PIN_5

// ========== 温湿度阈值 ==========
#define TEMP_HIGH_THRESHOLD     35  // 温度过高阈值(°C)
#define TEMP_LOW_THRESHOLD      10  // 温度过低阈值(°C)
#define HUMI_LOW_THRESHOLD      30  // 湿度过低阈值(%)
#define HUMI_HIGH_THRESHOLD     80  // 湿度过高阈值(%)

// ========== LED和蜂鸣器控制宏 ==========
#define LED_STATUS_ON()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET)
#define LED_STATUS_OFF()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET)
#define BUZZER_ON()        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET)  // 低电平触发
#define BUZZER_OFF()       HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
// 传感器数据
volatile uint8_t g_temperature = 0;
volatile uint8_t g_humidity = 0;
volatile uint16_t g_light_level = 0;
volatile uint8_t g_alarm_flag = 0;  // 警报标志

// 蓝牙接收缓冲
uint8_t bt_rx_buffer[200];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ============================================================================
   DHT11驱动代码 (集成在main.c中)
   ============================================================================ */

// DWT微秒延时初始化
static void DWT_Delay_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

// 获取微秒时间戳
static inline uint32_t DWT_GetUS(void)
{
    return DWT->CYCCNT / (SystemCoreClock / 1000000U);
}

// 微秒延时
static void DWT_Delay_us(uint32_t us)
{
    uint32_t start = DWT_GetUS();
    while ((DWT_GetUS() - start) < us);
}

// DHT11引脚配置为输出
static void DHT_Pin_Output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT_PORT, &GPIO_InitStruct);
}

// DHT11引脚配置为输入
static void DHT_Pin_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT_PORT, &GPIO_InitStruct);
}

// DHT11初始化
HAL_StatusTypeDef DHT11_Init(void)
{
    DWT_Delay_Init();
    DHT_Pin_Input();
    return HAL_OK;
}

// DHT11读取温湿度
HAL_StatusTypeDef DHT11_Read(uint8_t *humidity, uint8_t *temperature)
{
    uint8_t data[5] = {0};
    
    if (humidity == NULL || temperature == NULL) return HAL_ERROR;

    // 发送起始信号：拉低至少18ms
    DHT_Pin_Output();
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);
    HAL_Delay(18);
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
    DWT_Delay_us(30);

    // 切换为输入模式接收响应
    DHT_Pin_Input();

    // 等待DHT11拉低(80us)
    uint32_t t_start = DWT_GetUS();
    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET)
    {
        if ((DWT_GetUS() - t_start) > 1000) return HAL_TIMEOUT;
    }

    // 等待DHT11拉高(80us)
    t_start = DWT_GetUS();
    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_RESET)
    {
        if ((DWT_GetUS() - t_start) > 1000) return HAL_TIMEOUT;
    }

    // 等待DHT11拉低，开始数据传输
    t_start = DWT_GetUS();
    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET)
    {
        if ((DWT_GetUS() - t_start) > 1000) return HAL_TIMEOUT;
    }

    // 读取40位数据
    for (uint8_t i = 0; i < 40; i++)
    {
        // 每个位以约50us的低电平开始
        t_start = DWT_GetUS();
        while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_RESET)
        {
            if ((DWT_GetUS() - t_start) > 1000) return HAL_TIMEOUT;
        }

        // 测量高电平持续时间
        uint32_t high_start = DWT_GetUS();
        while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET)
        {
            if ((DWT_GetUS() - high_start) > 1000) return HAL_TIMEOUT;
        }
        uint32_t high_us = DWT_GetUS() - high_start;

        // 高电平持续时间：1约65us，0约20-30us，阈值设为40us
        uint8_t bit = (high_us > 40) ? 1 : 0;
        data[i / 8] <<= 1;
        data[i / 8] |= bit;
    }

    // 校验和验证
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4])
    {
        return HAL_ERROR;
    }

    *humidity = data[0];
    *temperature = data[2];
    return HAL_OK;
}

/* ============================================================================
   AT09蓝牙驱动代码 
   ============================================================================ */

// 发送AT命令并等待响应
HAL_StatusTypeDef AT09_SendCommand(char *cmd, uint16_t timeout_ms)
{
    uint8_t rx_buffer[128] = {0};
    uint16_t rx_len = 0;

    printf("AT09 Send: %s\r\n", cmd);

    HAL_StatusTypeDef tx = HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);
    if (tx != HAL_OK) return tx;

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (HAL_UART_Receive(&huart1, &rx_buffer[rx_len], 1, 10) == HAL_OK)
        {
            rx_len++;
            if (rx_len >= sizeof(rx_buffer) - 1) break;
        }
    }

    if (rx_len > 0)
    {
        rx_buffer[rx_len] = '\0';
        printf("AT09 Resp: %s\r\n\r\n", rx_buffer);
        return HAL_OK;
    }
    else
    {
        printf("AT09 Resp: (none)\r\n\r\n");
        return HAL_TIMEOUT;
    }
}

// 配置AT09蓝牙模块
void AT09_Configure(void)
{
    // 发送AT命令测试连接
    AT09_SendCommand("AT", 500);
    
    
    AT09_SendCommand("AT+NAMESTM32", 500);
    
  
    AT09_SendCommand("AT+PASS123456", 500);
}

// 启动蓝牙接收中断
void AT09_StartReceiveIT(uint8_t *buf, uint16_t len)
{
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, buf, len);
}

/* ============================================================================
   Printf重定向和蓝牙通信函数
   ============================================================================ */

// Printf重定向到UART2 (调试串口)
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

// 通过蓝牙发送字符串
void BT_SendString(char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

// 蓝牙接收中断回调
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        // 收到来自ESP32的命令
        if (Size < 200) bt_rx_buffer[Size] = '\0';
        else bt_rx_buffer[199] = '\0';
        
        printf("BT Received: %s\r\n", bt_rx_buffer);
        
        // 处理接收到的命令
        if (strstr((char*)bt_rx_buffer, "GET_DATA") != NULL)
        {
            // ESP32请求数据，立即发送
            char data_buf[50];
            sprintf(data_buf, "T:%d,H:%d,L:%d\n", g_temperature, g_humidity, g_light_level);
            BT_SendString(data_buf);
        }
        
        // 重新开启接收
        HAL_UARTEx_ReceiveToIdle_IT(&huart1, bt_rx_buffer, 200);
    }
}

/* ============================================================================
   LiteOS任务函数
   ============================================================================ */

/**
 * @brief 任务1: 传感器数据采集任务
 * 每2秒读取DHT11温湿度和光敏传感器数据
 */
static void SensorReadTask(void)
{
    uint8_t read_err_count = 0;
    
    while(1)
    {
        // 读取DHT11温湿度
        uint8_t temp = 0, humi = 0;
        HAL_StatusTypeDef status = DHT11_Read(&humi, &temp);
        
        if (status == HAL_OK)
        {
            g_temperature = temp;
            g_humidity = humi;
            read_err_count = 0;
            printf("[Sensor] Temp: %d°C, Humi: %d%%\r\n", temp, humi);
        }
        else
        {
            read_err_count++;
            printf("[Sensor] DHT11 Read Error (count: %d)\r\n", read_err_count);
            if (read_err_count > 5)
            {
                // 连续读取失败，可能传感器故障
                g_alarm_flag |= 0x01;
            }
        }
        
        // 读取光敏传感器 (ADC) - 这里需要根据你的ADC配置调整
        // 如果已配置ADC，取消下面注释并实现ADC读取函数
        // g_light_level = ADC_ReadValue();
        g_light_level = 512;  // 临时固定值
        
        // 检查温湿度阈值
        if (temp > TEMP_HIGH_THRESHOLD || temp < TEMP_LOW_THRESHOLD ||
            humi < HUMI_LOW_THRESHOLD || humi > HUMI_HIGH_THRESHOLD)
        {
            g_alarm_flag |= 0x02;  // 设置温湿度异常标志
        }
        else
        {
            g_alarm_flag &= ~0x02; // 清除温湿度异常标志
        }
        
        // 延时2秒
        LOS_TaskDelay(2000);
    }
}

/**
 * @brief 任务2: 蓝牙数据发送任务
 * 每5秒通过蓝牙发送传感器数据
 */
static void BluetoothSendTask(void)
{
    char data_buffer[60];
    
    while(1)
    {
        // 组装数据包：格式 "T:xx,H:xx,L:xxxx,A:x\n"
        sprintf(data_buffer, "T:%d,H:%d,L:%d,A:%d\n", 
                g_temperature, g_humidity, g_light_level, g_alarm_flag);
        
        // 发送到AT09蓝牙模块
        BT_SendString(data_buffer);
        
        // 同时输出到调试串口
        printf("[BT Send] %s", data_buffer);
        
        // 延时5秒
        LOS_TaskDelay(5000);
    }
}

/**
 * @brief 任务3: LED状态指示任务
 * 正常：慢闪（1秒周期）
 * 警报：快闪（200ms周期）
 */
static void LedIndicatorTask(void)
{
    while(1)
    {
        LED_STATUS_ON();
        
        if (g_alarm_flag)
        {
            // 有警报：快闪
            LOS_TaskDelay(100);
        }
        else
        {
            // 正常：慢闪
            LOS_TaskDelay(500);
        }
        
        LED_STATUS_OFF();
        
        if (g_alarm_flag)
        {
            LOS_TaskDelay(100);
        }
        else
        {
            LOS_TaskDelay(500);
        }
    }
}

/**
 * @brief 任务4: 蜂鸣器警报任务
 * 当温湿度超出阈值时鸣叫
 */
static void BuzzerAlarmTask(void)
{
    while(1)
    {
        if (g_alarm_flag & 0x02)  // 温湿度异常
        {
            // 间歇鸣叫：响0.5秒，停0.5秒
            BUZZER_ON();
            LOS_TaskDelay(500);
            BUZZER_OFF();
            LOS_TaskDelay(500);
        }
        else if (g_alarm_flag & 0x01)  // 传感器故障
        {
            // 长鸣1秒
            BUZZER_ON();
            LOS_TaskDelay(1000);
            BUZZER_OFF();
            LOS_TaskDelay(2000);
        }
        else
        {
            // 无警报
            BUZZER_OFF();
            LOS_TaskDelay(1000);
        }
    }
}

/* ============================================================================
   LiteOS任务创建
   ============================================================================ */

UINT32 SensorTask_Handle;
UINT32 BTTask_Handle;
UINT32 LedTask_Handle;
UINT32 BuzzerTask_Handle;

static UINT32 AppTaskCreate(void)
{
    UINT32 uwRet = LOS_OK;
    TSK_INIT_PARAM_S task_init_param;

    // 任务1: 传感器读取任务 (优先级3，最高)
    task_init_param.usTaskPrio = 3;
    task_init_param.pcName = "SensorRead";
    task_init_param.pfnTaskEntry = (TSK_ENTRY_FUNC)SensorReadTask;
    task_init_param.uwStackSize = 1024;
    uwRet = LOS_TaskCreate(&SensorTask_Handle, &task_init_param);
    if (uwRet != LOS_OK)
    {
        printf("SensorTask create failed, 0x%X\r\n", uwRet);
        return uwRet;
    }

    // 任务2: 蓝牙发送任务 (优先级4)
    task_init_param.usTaskPrio = 4;
    task_init_param.pcName = "BTSend";
    task_init_param.pfnTaskEntry = (TSK_ENTRY_FUNC)BluetoothSendTask;
    task_init_param.uwStackSize = 1024;
    uwRet = LOS_TaskCreate(&BTTask_Handle, &task_init_param);
    if (uwRet != LOS_OK)
    {
        printf("BTTask create failed, 0x%X\r\n", uwRet);
        return uwRet;
    }

    // 任务3: LED指示任务 (优先级5)
    task_init_param.usTaskPrio = 5;
    task_init_param.pcName = "LED";
    task_init_param.pfnTaskEntry = (TSK_ENTRY_FUNC)LedIndicatorTask;
    task_init_param.uwStackSize = 512;
    uwRet = LOS_TaskCreate(&LedTask_Handle, &task_init_param);
    if (uwRet != LOS_OK)
    {
        printf("LedTask create failed, 0x%X\r\n", uwRet);
        return uwRet;
    }

    // 任务4: 蜂鸣器警报任务 (优先级5)
    task_init_param.usTaskPrio = 5;
    task_init_param.pcName = "Buzzer";
    task_init_param.pfnTaskEntry = (TSK_ENTRY_FUNC)BuzzerAlarmTask;
    task_init_param.uwStackSize = 512;
    uwRet = LOS_TaskCreate(&BuzzerTask_Handle, &task_init_param);
    if (uwRet != LOS_OK)
    {
        printf("BuzzerTask create failed, 0x%X\r\n", uwRet);
        return uwRet;
    }

    return LOS_OK;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* USER CODE BEGIN 1 */
    UINT32 uwRet = LOS_OK;
    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */
    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */
    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART1_UART_Init();  // AT09蓝牙 @ 9600
    MX_USART2_UART_Init();  // 调试串口 @ 115200

    /* USER CODE BEGIN 2 */
    
    // 初始化外设
    printf("\r\n========================================\r\n");
    printf("STM32 Sensor Module with LiteOS\r\n");
    printf("========================================\r\n");
    
    // 初始化DHT11
    if (DHT11_Init() == HAL_OK)
    {
        printf("[Init] DHT11 initialized successfully\r\n");
    }
    else
    {
        printf("[Init] DHT11 initialization failed\r\n");
    }
    
    // 配置AT09蓝牙模块
    printf("[Init] Configuring AT09 Bluetooth...\r\n");
    AT09_Configure();
    
    // 启动蓝牙接收中断
    AT09_StartReceiveIT(bt_rx_buffer, 200);
    printf("[Init] AT09 Bluetooth ready\r\n");
    
    // 初始化GPIO（蜂鸣器默认关闭）
    BUZZER_OFF();
    LED_STATUS_OFF();
    
    // 初始化LiteOS内核
    printf("[Init] Initializing LiteOS kernel...\r\n");
    uwRet = LOS_KernelInit();
    if (uwRet != LOS_OK)
    {
        printf("[Error] LiteOS kernel init failed: 0x%X\r\n", uwRet);
        while(1);
    }
    
    // 创建应用任务
    printf("[Init] Creating tasks...\r\n");
    uwRet = AppTaskCreate();
    if (uwRet != LOS_OK)
    {
        printf("[Error] Task creation failed: 0x%X\r\n", uwRet);
        while(1);
    }
    
    // 启动LiteOS调度器
    printf("[Init] Starting LiteOS scheduler...\r\n");
    printf("========================================\r\n\r\n");
    LOS_Start();
    
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    // 注意：代码不应该执行到这里，因为LOS_Start()后调度器接管
    while (1)
    {
        /* USER CODE END WHILE */
        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Initializes the CPU, AHB and APB busses clocks */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
    
    /** Initializes the CPU, AHB and APB busses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/