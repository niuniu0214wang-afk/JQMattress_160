#include "dma.h"

/*------------------/-------------------/-------------------/------------------/
 * @brief   
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);  // 最高优先级 - DMA UART3 (OTA) (2025-09-03)
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);  // 最高优先级 - DMA UART2 (传感器) (2025-09-03)
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

