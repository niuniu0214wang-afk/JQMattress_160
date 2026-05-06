#ifndef _MYEDGE_AI_APP_H_
#define _MYEDGE_AI_APP_H_

#include "SEGGER_RTT.h"
#include "rtthread.h"
#include "usart.h"
#include "mySensor_deal.h"
#include "breath_rate.h"  /* 呼吸率计算模块 (2026-05-06) */

/* 传感器矩阵配置 - 16x10 = 160传感器点（1024→160映射输出）(2026-05-06) */
#define SENSOR_ROWS 16
#define SENSOR_COLS 10
#define SENSOR_DATA_SIZE (SENSOR_ROWS * SENSOR_COLS)  /* 160 = 16 * 10 */

/* 上下分割配置 - 每侧8x10 = 80传感器点 (2026-05-06) */
#define SENSOR_HALF_ROWS 8
#define SENSOR_HALF_SIZE (SENSOR_HALF_ROWS * SENSOR_COLS)  /* 80 = 8 * 10 */

typedef enum
{
    AI_STATUS_IDLE = 0,  /* 空闲状态 */
    AI_STATUS_COMPLETED, /* 完成推理 */
    AI_STATUS_ERROR      /* 发生错误 */
} ai_status_t;

typedef struct model_T
{
    char result;
    char status;
    float confidence;
    float ai_output[3];
} model_t;

extern model_t model;
extern uint8_t Origin_MattressData[160];  /* 16x10矩阵，1024→160映射输出 (2026-05-06) */

/* 睡姿输出变量 (2026-05-06) */
extern uint8_t g_person_count;
extern uint8_t g_posture_0;
extern uint8_t g_posture_1;

/* 呼吸率输出变量（0xFF = 无效/未就绪）(2026-05-06) */
extern uint8_t g_breath_rate_0;  /* 下半区呼吸率 bpm，0xFF=无效 */
extern uint8_t g_breath_rate_1;  /* 上半区呼吸率 bpm，0xFF=无效 */

/* 呼吸率分析器实例（供外部读取状态）(2026-05-06) */
extern BreathAnalyzer g_breath_upper;
extern BreathAnalyzer g_breath_lower;

/* 工具函数 (2026-05-06) */
uint32_t calc_160_sum(const uint8_t *data);
uint16_t sensor_number_160(const uint8_t *data);
void     split_upper_lower_matrix(const uint8_t *input_data, uint8_t *upper_matrix, uint8_t *lower_matrix);
uint32_t calc_sum_upper_half(const uint8_t *input_data);
uint32_t calc_sum_lower_half(const uint8_t *input_data);
uint16_t count_nonzero_in_upper_half(const uint8_t *input_data);
uint16_t count_nonzero_in_lower_half(const uint8_t *input_data);

/* Model处理函数 (2026-05-06) */
int Model(const uint8_t *input);

#endif /* MYEDGE_AI_APP_H */

