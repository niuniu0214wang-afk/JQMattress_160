/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "SEGGER_RTT.h"
#include "rtthread.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include "myEdge_ai_app.h"
#include <ctype.h>

#define SENSOR_RX_BUFFER_SIZE  2048
#define OUTPUT_RX_BUFFER_SIZE  (2048 + 128)  // 增大缓冲区以支持OTA协议 (2025-09-02)

extern uint16_t sensor_uart_rx_len;
extern uint8_t sensor_uart_rx_buffer[SENSOR_RX_BUFFER_SIZE]; 
extern uint16_t output_uart_rx_len;
extern uint8_t output_uart_rx_buffer[OUTPUT_RX_BUFFER_SIZE];  
extern char Project_Version[16];

/* Exported functions prototypes ---------------------------------------------*/
extern void Error_Handler(void);
extern void hex_dump(const void *data, size_t size);
extern void hex_dump_simple(const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
