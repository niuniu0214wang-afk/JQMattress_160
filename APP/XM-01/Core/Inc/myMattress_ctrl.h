#ifndef MYMATTRESS_CTRL_H
#define MYMATTRESS_CTRL_H

#ifdef __cplusplus
extern "C"
{
#endif
#include "main.h"


/*
 * 数据包格式说明（十六进制表示）：
 *
 * ┌────────┬─────┬────────────────────────────┬────────────┬────────┬──────┬───────┐
 * │ 0xAA   │ LEN │      0x01 ~ 0x06           │   STATUS   │ p1~p12 │ CRC  │ 0D 0A │
 * └────────┴─────┴────────────────────────────┴────────────┴────────┴──────┴───────┘
 *   ↑        ↑               ↑                       ↑           ↑      ↑      ↑
 * 起始字节   数据长度        设备标识（6字节）        状态位   12字节参数  CRC校验 尾部结束符
 *
 * 字段说明：
 *   0xAA         ：帧头，固定为 0xAA
 *   LEN          ：总数据长度 全部数据
 *   0x01~0x06    ：设备地址/ID（共6字节）
 *   STATUS       ：状态码（功能/命令编号）
 *   p1~p12       ：参数字段（此处均为 0x00，共12字节）
 *   CRC          ：CRC16-Modbus 校验码（高字节在前，低字节在后）起始字节 ~ 数据区
 *   0x0D 0x0A    ：帧尾，表示结束
 *
 * 示例数据帧（全部字段展开）：
 *   AA | 19 | 01 02 03 04 05 06 | XX | 00 00 00 00 00 00 00 00 00 00 00 00 | CRC_H CRC_L | 0D 0A
 *
 * 说明：
 *   - XX 为具体的状态码（例如 0xA1 表示防褥疮模式等）
 *   - CRC_H CRC_L 由 CRC16-Modbus 算法计算得出
 */



// Mattress control modes
#define MODE_MANUAL 0xA0     // 手动压力控制
#define MODE_PREVENTION 0xA1 // 防褥疮
#define MODE_ADAPTIVE 0xA2   // 自适应模式
#define MODE_ABAB 0xA3       // ABAB 交替充放气
#define MODE_TURNING 0xA4    // 左右翻身
#define MODE_FLOATING 0xA5   // 悬浮模式

// Softness settings
#define SOFTNESS_SOFT 0xB1   // 软
#define SOFTNESS_MEDIUM 0xB2 // 中
#define SOFTNESS_HARD 0xB3   // 硬

#ifdef __cplusplus
}
#endif

#endif // MYEDGE_AI_APP_H
