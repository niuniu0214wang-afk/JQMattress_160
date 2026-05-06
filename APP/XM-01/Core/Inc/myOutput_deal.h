#ifndef _MYOUTPUT_DEAL_H_
#define _MYOUTPUT_DEAL_H_

#include "protocol_parser.h"
#include "myEdge_ai_app.h"
#include "myFlash.h"
#include <string.h>
#include <stdio.h>

typedef enum
{
    STATE_WAIT_RECV = 0,
    STATE_PARSE_FRAME,
    STATE_HANDLE_CMD,
    STATE_BACK_WAIT,
} CommState;

extern unsigned char UploadSrcMattressData[160];  /* 上传缓冲区，与160点输出一致 (2026-05-06) */
extern struct rt_event output_uart_rx_event;

#endif
