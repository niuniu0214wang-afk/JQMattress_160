#include "main.h"
#include "myFlash.h"
#include "stm32f4xx_hal_flash.h"
#include "myFlash.h"
#include <string.h>

typedef void (*pFunction)(void);
pFunction Jump_To_Application;
void SystemClock_Config(void);

/*------------------/-------------------/-------------------/------------------/
 * @brief   跳转到APP
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
void jump_to_app(void)
{
    uint32_t JumpAddress = 0;
    uint32_t JumpAddr = APP_ADDR;

    JumpAddress = *(__IO uint32_t *)(JumpAddr + 4);

    if (((*(uint32_t *)(JumpAddr)) & 0x2FFE0000) == 0x20000000)
    {
        // 建议关全局中断
        Jump_To_Application = (pFunction)JumpAddress;
        __set_MSP(*(__IO uint32_t *)JumpAddr); // 初始化APP堆栈指针(用户代码区的第一个字用于存放栈顶地址)
        Jump_To_Application();                 // 设置PC指针为新程序复位中断函数的地址
    }
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   主函数
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
int main(void)
{
    t_flash_data flash_data;
    int i;

    memset((unsigned char *)&flash_data, 0, sizeof(flash_data));
    STM_Flash_Read(DATA_ADDR, (unsigned int *)&flash_data, sizeof(flash_data));

    for(i=0; i<3; i++)
    {
        if(flash_data.version[i] != (char)0xFF)
        {
            break;
        }
    }

    if(i == 3)
    {
        flash_data.version[0] = 0x01;
        flash_data.version[1] = 0x00;
        flash_data.version[2] = 0x00;
        flash_data.count = 0;
        data_flash_write(DATA_ADDR, (unsigned char *)&flash_data, sizeof(flash_data));
    }
    
    if (flash_data.flag != INTO_BOOT_FLASH)
    {
        jump_to_app();
    }
    else
    {
        HAL_Init();
        SystemClock_Config();
        app_flash_erase();

        unsigned int buf[1024];
        unsigned int read_addr = BIN_ADDR;
        unsigned int write_addr = APP_ADDR;

        for (;;)
        {
            STM_Flash_Read(read_addr, buf, sizeof(buf));
            app_flash_write(write_addr, buf, sizeof(buf));
            read_addr += sizeof(buf);
            write_addr += sizeof(buf);

            if (write_addr >= (APP_ADDR + APP_SIZE))
            {
                flash_data.flag = IAP_PRE_OK;
                flash_data.count++;
                data_flash_write(DATA_ADDR, (unsigned char *)&flash_data, sizeof(flash_data));
                NVIC_SystemReset();
            }
        }
    }
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   时钟配置
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
