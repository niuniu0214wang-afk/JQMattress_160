#ifndef _MYSENSOR_DEAL_H_
#define _MYSENSOR_DEAL_H_

#include "stm32f4xx_hal.h"
#include "SEGGER_RTT.h"
#include "rtthread.h"
#include "myEdge_ai_app.h"
#include "myOutput_deal.h"
#include <ctype.h>
#include <rtthread.h>
#include <math.h>

extern struct rt_event uart_rx_event;
extern struct rt_event data_ready_event;
extern uint8_t Origin_MattressData_Resize[1024];  /* 原始1024字节帧缓冲 (2026-05-06) */
extern uint8_t Origin_MattressData[160];           /* 16x10矩阵尺寸，1024→160映射输出 (2026-05-06) */
#endif 

