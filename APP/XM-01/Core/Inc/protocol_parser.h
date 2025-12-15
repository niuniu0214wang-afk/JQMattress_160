#ifndef _PROTOCOL_PARSER_H_
#define _PROTOCOL_PARSER_H_

#include <stdint.h>
#include <stdbool.h>
#include "SEGGER_RTT.h"
#include "rtthread.h"

#define FRAME_HEAD_1        0xA5
#define FRAME_HEAD_2        0xA5
#define VERSION             0x01
#define MAX_PAYLOAD_LEN     2048

// FRAME_CTL 标志位宏定义（1字节，共8位）
// Bit7 - CRC 校验
#define FRAME_CTL_CRC_EN    (1 << 7)
// Bit6 - 是否需要ACK
#define FRAME_CTL_ACK_REQ   (1 << 6)
#define FRAME_CTL_NACK_REQ  (0 << 6)
// Bit5~4 - 加密方式
#define FRAME_CTL_ENC_NONE  (0 << 4)  // 不加密
#define FRAME_CTL_ENC_XOR   (1 << 4)  // XOR 加密
#define FRAME_CTL_ENC_AES   (2 << 4)  // AES 加密（预留）
// Bit0 - 帧类型
#define FRAME_CTL_TYPE_DATA (0 << 0)
#define FRAME_CTL_TYPE_ACK  (1 << 0)

typedef struct __attribute__((packed))
{
    uint8_t head[2];
    uint8_t version;
    uint8_t frame_ctl;
    uint16_t seq_num;
    uint32_t timeStamp;
    uint16_t payload_len;
    uint8_t payload[MAX_PAYLOAD_LEN];
    uint16_t crc;
    uint8_t need_ack : 1;          
    uint8_t is_ack : 1;   
    uint8_t crc_enabled : 1;
    uint8_t encrypted : 1;
    uint8_t decrypted : 1;
    uint8_t reserved : 3;
}ProtocolFrame;

extern void Protocol_BuildFrame(ProtocolFrame *frame, uint8_t frame_ctl, uint16_t seq, uint32_t time,
                         const uint8_t *payload, uint16_t payload_len, bool enable_crc);
extern uint16_t Protocol_SerializeFrame(uint8_t *buffer, uint16_t buffer_size, const ProtocolFrame *frame);
extern int Protocol_ParseFrame(const uint8_t *data, uint16_t len,  ProtocolFrame *frame);
extern uint16_t Protocol_CRC16_CCITT(uint16_t pre_crc, const uint8_t *data, uint16_t len);

#endif
