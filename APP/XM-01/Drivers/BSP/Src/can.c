/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    can.c
 * @brief   This file provides code for the configuration
 *          of the CAN instances.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "can.h"

/* USER CODE BEGIN 0 */
CAN_TxHeaderTypeDef TxHeader; // 发送
CAN_RxHeaderTypeDef RxHeader; // 接收

uint8_t RxData[8] = {0}; // 数据接收数组，can的数据帧只有8帧

/* USER CODE END 0 */

CAN_HandleTypeDef hcan1;

/* CAN1 init function */
void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 21;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_2TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**CAN1 GPIO Configuration
    PB8     ------> CAN1_RX
    PB9     ------> CAN1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* CAN1 interrupt Init */
    HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 1, 0);  // 高优先级 - CAN总线通信 (2025-09-03)
    HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN1 GPIO Configuration
    PB8     ------> CAN1_RX
    PB9     ------> CAN1_TX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8|GPIO_PIN_9);

    /* CAN1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/*CAN过滤器初始化*/
void CAN_Filter_Init(void)
{
  // 不进行过滤
  CAN_FilterTypeDef sFilterConfig;

  sFilterConfig.FilterBank = 0;                      /* 过滤器组0 */
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;  /* 屏蔽位模式 */
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT; /* 32位。*/

  sFilterConfig.FilterIdHigh = 0;                    // /* 要过滤的ID高位  */ (((uint32_t)CAN_RxExtId << 3) & 0xFFFF0000) >> 16;
  sFilterConfig.FilterIdLow = 0;                     // /* 要过滤的ID低位 */ (((uint32_t)CAN_RxExtId << 3) | CAN_ID_EXT | CAN_RTR_DATA) & 0xFFFF;
  sFilterConfig.FilterMaskIdHigh = 0;                // /* 过滤器高16位每位必须匹配 */ 0xFFFF;
  sFilterConfig.FilterMaskIdLow = 0;                 // /* 过滤器低16位每位必须匹配 */ 0xFFFF;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0; /* 过滤器被关联到FIFO 0 */
  sFilterConfig.FilterActivation = ENABLE;           /* 使能过滤器 */
  sFilterConfig.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK)
  {
    /* Filter configuration Error */
    Error_Handler();
  }

  if (HAL_CAN_Start(&hcan1) != HAL_OK)
  {
    /* Start Error */
    Error_Handler();
  }

  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
    /* Start Error */
    Error_Handler();
  }
}

/*CAN发送数据，入口参数为要发送的数组指针，数据长度，返回0代表发送数据无异常，返回1代表传输异常*/
uint8_t CAN_Send_Msg(uint8_t *msg, uint8_t len)
{
  uint8_t i = 0;
  uint8_t message[8];
  uint32_t TxMailbox;
  CAN_TxHeaderTypeDef CAN_TxHeader;

  // 设置发送参数
  CAN_TxHeader.StdId = 0x12;       // 标准标识符(12bit)
  CAN_TxHeader.ExtId = 0x12;       // 扩展标识符(29bit)
  CAN_TxHeader.IDE = CAN_ID_STD;   // 使用标准帧
  CAN_TxHeader.RTR = CAN_RTR_DATA; // 数据帧
  CAN_TxHeader.DLC = len;          // 发送长度
  CAN_TxHeader.TransmitGlobalTime = DISABLE;

  // 装载数据
  for (i = 0; i < len; i++)
  {
    message[i] = msg[i];
  }

  // 发送CAN消息
  if (HAL_CAN_AddTxMessage(&hcan1, &CAN_TxHeader, message, &TxMailbox) != HAL_OK)
  {
    return 1;
  }
  while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) != 3)
  {
  }
  return 0;
}

/*CAN接收中断函数*/
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *CanNum)
{
  uint32_t i;

  CAN_RX_EVENT = 1; // 接收标志位
  HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxHeader, RxData);
  for (i = 0; i < RxHeader.DLC; i++)
    CAN_RX_BUFFER[i] = RxData[i]; // 用RxBuf转存RxData的数据
}
/* USER CODE END 1 */
