/**
 * @file stm32_sleep_posture.c
 * @brief 睡姿分类 - CNN实现版本
 *
 * 使用CNN模型进行睡姿二分类 (仰卧/侧卧)
 * CNN-based binary sleep posture classification (Supine/Side)
 *
 * 中文注释 (2025-12-22)
 */

#include "stm32_sleep_posture.h"
#include "jq260_cnn_inference.h"
#include "SEGGER_RTT.h"
#include "rtthread.h"
#include <string.h>

// =============================================================================
// 外部变量声明 (2025-12-22)
// =============================================================================

extern uint8_t g_posture_0;
extern uint8_t g_posture_1;
extern uint8_t Origin_MattressData[260];

// =============================================================================
// 主功能函数：CNN模型推理 (2025-12-22)
// Main Function: CNN Model Inference
// =============================================================================

/**
 * @brief 运行CNN睡姿分类
 * Run CNN sleep posture classification
 *
 * @param ai_input: 输入传感器数据指针 (uint8_t array, 260 bytes = 26x10)
 * @param ai_output: 输出指针（可选，存储结果）
 * @return 睡姿结果: 1=仰卧(Supine), 2=侧卧(Side)
 *
 * 中文注释 (2025-12-22)
 */
uint32_t feature_engineering_model_run(void *ai_input, void *ai_output)
{
    const uint8_t *input_data = (const uint8_t *)ai_input;

    // 使用全局数据作为备选 (2025-12-22)
    if (input_data == NULL) {
        input_data = Origin_MattressData;
    }

    // 调用CNN分类函数 (2025-12-22)
    float probability;
    uint8_t result = cnn_posture_classify(input_data, &probability);

    // 输出结果 (2025-12-22)
    if (ai_output != NULL) {
        *(uint8_t*)ai_output = result;
    }

    return result;
}

