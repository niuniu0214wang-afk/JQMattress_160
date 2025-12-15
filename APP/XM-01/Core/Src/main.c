/* Includes ------------------------------------------------------------------*/
#include "main.h"

char Project_Version[16] = "V1.2.9";
unsigned short sensor_uart_rx_len = 0;
unsigned char sensor_uart_rx_buffer[SENSOR_RX_BUFFER_SIZE]; // DMA搬运目标缓存
unsigned short output_uart_rx_len = 0;
unsigned char output_uart_rx_buffer[OUTPUT_RX_BUFFER_SIZE]; // DMA搬运目标缓存

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/*------------------/-------------------/-------------------/------------------/
 * @brief   主函数
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
int main(void)
{
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART2_UART_Init();
    MX_USART3_UART_Init();
    SEGGER_RTT_Init();
//    MX_X_CUBE_AI_Init();
    /* USER CODE BEGIN 2 */

    SEGGER_RTT_WriteString(0, "DEBUG: Starting UART configuration...\n");

    // 使能串口IDLE中断
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    SEGGER_RTT_WriteString(0, "DEBUG: UART2 IDLE interrupt enabled\n");

    __HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
    SEGGER_RTT_WriteString(0, "DEBUG: UART3 IDLE interrupt enabled\n");

    // 开启串口接收DMA
    HAL_UART_Receive_DMA(&huart2, sensor_uart_rx_buffer,
                         SENSOR_RX_BUFFER_SIZE);
    SEGGER_RTT_WriteString(0, "DEBUG: UART2 DMA started\n");

    HAL_UART_Receive_DMA(&huart3, output_uart_rx_buffer,
                         OUTPUT_RX_BUFFER_SIZE);
    SEGGER_RTT_WriteString(0, "DEBUG: UART3 DMA started\n");

    /* 显示编译日期时间 */
    SEGGER_RTT_printf(0, "complie date time:%s %s\r\n", __DATE__, __TIME__);

    /* 打印程序版本 */
    SEGGER_RTT_printf(0, "FW version:%s\r\n", Project_Version);

    while (1)
    {
        HAL_GPIO_TogglePin(LED_B_GPIO_Port, LED_B_Pin);
        rt_thread_mdelay(500);
    }
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 6;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }

    /** Enables the Clock Security System
     */
    HAL_RCC_EnableCSS();
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   打印内存内容为十六进制和ASCII格式，每行16字节，8字节分隔。
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
void hex_dump(const void *data, size_t size)
{
    const uint8_t *byte_data = (const uint8_t *)data;
    size_t i, j;
    for (i = 0; i < size; i += 16)
    {
        // 打印16字节16进制，8字节后额外空格
        for (j = 0; j < 16; j++)
        {
            if (i + j < size)
                rt_kprintf("%02X ", byte_data[i + j]);
            else
                rt_kprintf("   ");

            if (j == 7)
                rt_kprintf(" "); // 每8字节后额外空格
        }

        rt_kprintf(" |");

        // 打印对应ASCII字符，非打印字符用'.'代替
        for (j = 0; j < 16; j++)
        {
            if (i + j < size)
            {
                uint8_t ch = byte_data[i + j];
                rt_kprintf("%c", isprint(ch) ? ch : '.');
            }
            else
            {
                rt_kprintf(" ");
            }
        }
        rt_kprintf("|\n");
    }
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   HEX打印
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
void hex_dump_simple(const void *data, size_t size)
{
    const uint8_t *byte_data = (const uint8_t *)data;
    size_t i;

    for (i = 0; i < size; i++)
    {
        rt_kprintf("%02X ", byte_data[i]);
        if ((i + 1) % 16 == 0)
        {
            rt_kprintf("\n");
        }
    }

    if (size % 16 != 0)
    {
        rt_kprintf("\n");
    }
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
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
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
