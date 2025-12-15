#include "myMattress_ctrl.h"
#include "myEdge_ai_app.h"
#include <rtthread.h>

/**
 * 计算 Modbus CRC16 校验值
 * @param data     指向数据的指针
 * @param length   数据长度（字节数）
 * @return         16位 CRC 值（低字节在前）
 */
uint16_t crc16_modbus(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= data[i]; // 将数据字节与 CRC 的低字节异或
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001; // 多项式为 0xA001（即 0x8005 反转）
            else
                crc >>= 1;
        }
    }

    return crc;
}
/**
 * Send command to the mattress controller
 */
bool send_mattress_command(uint8_t mode_byte, uint8_t *args, size_t args_length)
{
    rt_uint8_t len = 0;
    uint16_t crc16 = 0;
    // Create command packet
    uint8_t packet[20]; // Allocate enough space for command
    size_t packet_index = 0;

    // Start byte
    packet[packet_index++] = 0xAA;

    // Add Length
    len = 1 + 1 + 6 + 13 + 2 + 2;
    packet[packet_index++] = len;

    // Add ID bytes
    for (int i = 1; i < 7; i++)
    {
        packet[packet_index++] = i;
    }

    // Mode byte
    packet[packet_index++] = mode_byte;

    // Parameter bytes
    for (size_t i = 0; i < args_length; i++)
    {
        packet[packet_index++] = args[i];
    }

    // CRC16 增加
    crc16 = crc16_modbus(packet, (1 + 1 + 6 + 13));
    packet[packet_index++] = crc16 >> 8;
    packet[packet_index++] = crc16 & 0xff;

    // End byte
    packet[packet_index++] = 0x0D;
    packet[packet_index++] = 0x0A;

    rt_kprintf("--------------------------------\n");
    for (int i = 0; i < packet_index; i++)
        rt_kprintf("packet[%d] = %x\n", i, packet[i]);
    return true;

    // Send packet
    // TODO:增加串口发送
}
/// @brief 漂浮模式
/// @param duration_sec
/// @return 成功 失败
int run_floating_mode(void)
{
    if (!(model.status == AI_STATUS_COMPLETED && model.result == POSTURE_SUPINE && model.confidence >= 70.0f))
    {
        return 0; // 条件不满足
    }
    // 发送“漂浮模式”指令
    uint8_t parm[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return send_mattress_command(MODE_FLOATING, parm, 12);
}

/// @brief 翻转模式
/// @param duration_sec
/// @return 成功 失败
int run_turning_cycle(void)
{
    if (!(model.status == AI_STATUS_COMPLETED && model.result == POSTURE_SUPINE && model.confidence >= 70.0f))
    {
        return 0; // 条件不满足
    }
    // 发送 MODE_TURNING 命令
    uint8_t parm[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return send_mattress_command(MODE_TURNING, parm, 12);
}

/// @brief 运行防褥疮模式
/// @return 1=成功，0=失败
int run_prevention_mode(void)
{
    uint8_t parm[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return send_mattress_command(MODE_PREVENTION, parm, 12);
}

/// @brief 运行 ABAB 交替充气循环
/// @return 1=成功，0=失败
int run_abab_cycle(void)
{
    uint8_t parm[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return send_mattress_command(MODE_ABAB, parm, 12);
}

/// @brief 运行自适应模式
/// @return 1=成功，0=失败
int run_adaptive_mode(void)
{
    uint8_t parm[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return send_mattress_command(MODE_ADAPTIVE, parm, 12);
}

/// @brief 设置床垫软模式
/// @return 1=成功，0=失败
int run_soft_mode(void)
{
    uint8_t parm[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return send_mattress_command(SOFTNESS_SOFT, parm, 12);
}

/// @brief 设置床垫中等模式
/// @return 1=成功，0=失败
int run_medium_mode(void)
{
    uint8_t parm[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return send_mattress_command(SOFTNESS_MEDIUM, parm, 12);
}

/// @brief 设置床垫硬模式
/// @return 1=成功，0=失败
int run_hard_mode(void)
{
    uint8_t parm[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return send_mattress_command(SOFTNESS_HARD, parm, 12);
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/* 定义线程栈与控制块（静态分配） */
#define REMOTE_THREAD_STACK_SIZE 1024
static struct rt_thread remote_thread;
static rt_uint8_t remote_thread_stack[REMOTE_THREAD_STACK_SIZE];

/* 线程入口函数 */
static void remote_thread_entry(void *parameter)
{
    while (1)
    {
        // 等待接收433数据
        // 如果接收到执行相关指令
        rt_thread_mdelay(100); // 可调整周期
    }
}

/* 初始化函数，使用静态线程启动遥控处理 */
int remote_thread_init(void)
{
    rt_thread_init(&remote_thread,              // 线程控制块
                   "remote_task",               // 名称
                   remote_thread_entry,         // 入口函数
                   RT_NULL,                     // 参数
                   &remote_thread_stack[0],     // 栈起始地址
                   sizeof(remote_thread_stack), // 栈大小
                   18,                          // 优先级（比 ai_thread 高一点）
                   10);                         // 时间片
    rt_thread_startup(&remote_thread);          // 启动线程
    return 0;
}
// INIT_APP_EXPORT(remote_thread_init); // 启动自动初始化
