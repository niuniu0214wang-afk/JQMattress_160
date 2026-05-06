#ifndef _MYEDGE_AI_APP_H_
#define _MYEDGE_AI_APP_H_

#include "SEGGER_RTT.h"
#include "rtthread.h"
#include "usart.h"
#include "mySensor_deal.h"
//#include "app_x-cube-ai.h"
//#include "mcu_body_analyzer.h"

/* 传感器矩阵配置 - 16x10 = 160传感器点（1024→160映射输出）(2026-05-06) */
#define SENSOR_ROWS 16
#define SENSOR_COLS 10
#define SENSOR_DATA_SIZE (SENSOR_ROWS * SENSOR_COLS)  /* 160 = 16 * 10 */

/* 上下分割配置 - 每侧8x10 = 80传感器点 (2026-05-06) */
#define SENSOR_HALF_ROWS 8
#define SENSOR_HALF_SIZE (SENSOR_HALF_ROWS * SENSOR_COLS)  /* 80 = 8 * 10 */

typedef enum
{
    AI_STATUS_IDLE = 0,  // 空闲状态
    AI_STATUS_COMPLETED, // 完成推理
    AI_STATUS_ERROR      // 发生错误
} ai_status_t;

typedef struct model_T
{
    char result;        // 结果 观察枚举类型 0 平躺 1 左侧 2 右侧
    char status;        // 状态 0 没有运算 1 运算完成
    float confidence;   // 结果的置信率
    float ai_output[3]; // 结果数组
} model_t;

extern model_t model;
extern uint8_t Origin_MattressData[160];  /* 16x10矩阵，1024→160映射输出 (2026-05-06) */

/* 直接输出变量 (2026-05-06) */
extern uint8_t g_person_count;
extern uint8_t g_posture_0;
extern uint8_t g_posture_1;

/* 计算160个元素的和 (2026-05-06) */
uint32_t calc_160_sum(const uint8_t *data);

/* 计算160个元素中非零值的个数 (2026-05-06) */
uint16_t sensor_number_160(const uint8_t *data);

/* 分割16x10矩阵为上下两部分(各8x10) (2026-05-06) */
void split_upper_lower_matrix(const uint8_t *input_data, uint8_t *upper_matrix, uint8_t *lower_matrix);

/* 计算上半部分(output[80:160])的压力和 (2026-05-06) */
uint32_t calc_sum_upper_half(const uint8_t *input_data);

/* 计算下半部分(output[0:80])的压力和 (2026-05-06) */
uint32_t calc_sum_lower_half(const uint8_t *input_data);

/* 计算上半部分的非零传感器点数 (2026-05-06) */
uint16_t count_nonzero_in_upper_half(const uint8_t *input_data);

/* 计算下半部分的非零传感器点数 (2026-05-06) */
uint16_t count_nonzero_in_lower_half(const uint8_t *input_data);

/* Model处理函数 (2026-05-06) */
int Model(const uint8_t *input);

#endif // MYEDGE_AI_APP_H
