#include "protocol_parser.h"

#define XOR_KEY 0x5A

/*------------------/-------------------/-------------------/------------------/
 * @brief
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static void xor_crypt(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        data[i] ^= XOR_KEY;
    }
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   计算16位累加校验和（严格按照communication_protocol.md规范）
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
uint16_t Protocol_CRC16_CCITT(uint16_t pre_crc, const uint8_t *data, uint16_t len)
{
    // 严格按照协议规范：16位累加校验从0开始，忽略pre_crc (2025-09-03)
    uint16_t crc16 = 0;
    uint16_t i;

    // 严格按照协议规范实现16位累加校验
    for (i = 0; i < len; i++)
    {
        crc16 += data[i];
    }

    return crc16;
}

/*------------------/-------------------/-------------------/------------------/
 * @brief
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
void Protocol_BuildFrame(ProtocolFrame *frame, uint8_t frame_ctl, uint16_t seq, uint32_t time,
                         const uint8_t *payload, uint16_t payload_len, bool enable_crc)
{
    frame->head[0] = FRAME_HEAD_1;
    frame->head[1] = FRAME_HEAD_2;
    frame->version = VERSION;
    frame->frame_ctl = (frame_ctl);
    frame->seq_num = (seq);
    frame->timeStamp = (time);
    frame->payload_len = (payload_len);

    rt_memcpy(frame->payload, payload, payload_len);

    // 若设置加密标志位，则加密
    if ((frame_ctl & 0x30) >> 4 == 0x01)
    {
        xor_crypt(frame->payload, payload_len);
    }
    if (enable_crc)
    {
        frame->crc = Protocol_CRC16_CCITT(0xFFFF, (uint8_t *)frame, 12 + payload_len);
    }
    else
    {
        frame->crc = 0;
    }
}

/*------------------/-------------------/-------------------/------------------/
 * @brief
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
int Protocol_ParseFrame(const uint8_t *data, uint16_t len, ProtocolFrame *frame)
{
    if((data[0] != FRAME_HEAD_1) || (data[1] != FRAME_HEAD_2))
    {
        return -1;
    }

    if (len < 12)
    {
        return 0;
    }

    rt_memcpy(frame->head, data, 2);
    frame->version = data[2];
    frame->frame_ctl = data[3];
    frame->seq_num = (data[4] << 8) | data[5];                                       // 大端序
    frame->timeStamp = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9]; // 大端序时间戳
    frame->payload_len = (data[10] << 8) | data[11];                                 // 大端序

    uint16_t total_len = 12 + frame->payload_len + 2;
    uint16_t with_crc_len = total_len + ((frame->frame_ctl & 0x80) ? 2 : 0);
    
    if(with_crc_len >= 1500)
    {
        return -1;
    }

    if (len < with_crc_len)
    {
        return 0;
    }

    rt_memcpy(frame->payload, &data[12], frame->payload_len);
    // 设置状态标志
    frame->need_ack = (frame->frame_ctl & 0x40) ? 1 : 0;
    frame->is_ack = (frame->frame_ctl & 0x01) ? 1 : 0;
    frame->crc_enabled = (frame->frame_ctl & 0x80) ? 1 : 0;
    frame->encrypted = ((frame->frame_ctl & 0x30) >> 4) == 0x01;
    frame->decrypted = 0;

    // CRC 校验 - 临时调试：验证CRC计算是否匹配 (2025-09-03)
    if (frame->crc_enabled)
    {
        frame->crc = data[12 + frame->payload_len];
        frame->crc <<= 8;
        frame->crc |= data[13 + frame->payload_len];

        uint16_t crc_calc = Protocol_CRC16_CCITT(0xFFFF, data, 12 + frame->payload_len);
        SEGGER_RTT_printf(0, "receive crc:0x%x, calc crc:0x%x\r\n", frame->crc, crc_calc);

        if (frame->crc != crc_calc)
        {
            SEGGER_RTT_WriteString(0, "[PARSER] CRC mismatch - continuing for debug\n");
            return -1;
        }
    }

    // 加密解密（仅支持 XOR）
    if (frame->encrypted)
    {
        xor_crypt(frame->payload, frame->payload_len);
        frame->decrypted = 1;
    }
    
    return with_crc_len;
}

/*------------------/-------------------/-------------------/------------------/
 * @brief  将ProtocolFrame结构体的值序列化到字节数组
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
uint16_t Protocol_SerializeFrame(uint8_t *buffer, uint16_t buffer_size, const ProtocolFrame *frame)
{
    uint16_t offset = 0;
    uint16_t required_size = sizeof(frame->head) + sizeof(frame->version) +
                             sizeof(frame->frame_ctl) + sizeof(frame->seq_num) + sizeof(frame->timeStamp) +
                             sizeof(frame->payload_len) + frame->payload_len +
                             sizeof(frame->crc);

    // 检查缓冲区是否足够大
    if (buffer_size < required_size)
    {
        return 0; // 缓冲区不足
    }

    // 复制头部
    rt_memcpy(buffer + offset, frame->head, sizeof(frame->head));
    offset += sizeof(frame->head);

    // 复制版本号
    buffer[offset++] = frame->version;

    // 复制帧控制字段
    buffer[offset++] = frame->frame_ctl;

    // 复制序列号（按大端格式写入）
    buffer[offset++] = (frame->seq_num >> 8) & 0xFF;
    buffer[offset++] = frame->seq_num & 0xFF;

    // 复制时间戳（按大端格式写入）
    buffer[offset++] = (frame->timeStamp >> 24) & 0xFF;
    buffer[offset++] = (frame->timeStamp >> 16) & 0xFF;
    buffer[offset++] = (frame->timeStamp >> 8) & 0xFF;
    buffer[offset++] = frame->timeStamp & 0xFF;

    // 复制负载长度（按大端格式写入）
    buffer[offset++] = (frame->payload_len >> 8) & 0xFF;
    buffer[offset++] = frame->payload_len & 0xFF;

    // 复制负载数据
    rt_memcpy(buffer + offset, frame->payload, frame->payload_len);
    offset += frame->payload_len;

    // 复制CRC校验值（按大端格式写入）
    buffer[offset++] = (frame->crc >> 8) & 0xFF;
    buffer[offset++] = frame->crc & 0xFF;

    // 帧尾
    buffer[offset++] = 0x0d;
    buffer[offset++] = 0x0a;

    return offset; // 返回实际写入的字节数
}
