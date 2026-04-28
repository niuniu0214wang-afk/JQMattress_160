#ifndef _MYEDGE_AI_APP_H_
#define _MYEDGE_AI_APP_H_

#include "SEGGER_RTT.h"
#include "rtthread.h"
#include "usart.h"
#include "mySensor_deal.h"
//#include "app_x-cube-ai.h"
//#include "mcu_body_analyzer.h"

#define SENSOR_ROWS 26
#define SENSOR_COLS 10
#define SENSOR_DATA_SIZE (SENSOR_ROWS * SENSOR_COLS)

/* 质心 (Center of Mass) 结构体 - 使用浮点坐标以保持精度 - 2025-11-04 */
typedef struct
{
    float row; // CoM行坐标(0-25)
    float col; // CoM列坐标(0-9)
} CoM_t;

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
extern uint8_t Origin_MattressData[260];

/* 直接输出变量 - 避免struct对齐问题 - 2025-11-06 */
extern uint8_t g_person_count;
extern uint8_t g_posture_0;
extern uint8_t g_waist_x_0;
extern uint8_t g_waist_y_0;
extern uint8_t g_posture_1;
extern uint8_t g_waist_x_1;
extern uint8_t g_waist_y_1;


/* 计算260个元素的和 - 2025-11-04 */
uint32_t calc_260_sum(const uint8_t *data);

/* 计算260个元素中非零值的个数 - 2025-11-04 */
uint16_t sensor_number(const uint8_t *data);

/* Model处理函数 - 集成所有检测逻辑，返回人数(0=空床, 1=一人, 2=两人) - 2025-11-06 */
int Model(const uint8_t *input);
rt_bool_t ai_should_schedule_model(const uint8_t *input);

#endif // MYEDGE_AI_APP_H
