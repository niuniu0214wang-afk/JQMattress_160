/**
 * @file person_detection_wrapper.c
 * @brief 人数检测AI模型包装函数实现
 * @date 2026-03-03
 *
 * 封装X-CUBE-AI生成的person_detection模型
 */

#include "person_detection_wrapper.h"
#include "../../X-CUBE-AI-NEW/App/person_detection.h"
#include "../../X-CUBE-AI-NEW/App/person_detection_data.h"
#include <string.h>

// AI模型句柄
static ai_handle person_detection_handle = AI_HANDLE_NULL;

// 输入/输出缓冲区
AI_ALIGNED(4) static ai_float in_data[AI_PERSON_DETECTION_IN_1_SIZE];
AI_ALIGNED(4) static ai_float out_data[AI_PERSON_DETECTION_OUT_1_SIZE];

// 激活缓冲区
AI_ALIGNED(32) static ai_u8 activations[AI_PERSON_DETECTION_DATA_ACTIVATIONS_SIZE];

/**
 * @brief 初始化人数检测AI模型
 */
int person_detection_init(void)
{
    ai_error err;

    // 创建模型实例
    err = ai_person_detection_create(&person_detection_handle,
                                     AI_PERSON_DETECTION_DATA_CONFIG);
    if (err.type != AI_ERROR_NONE) {
        return -1;
    }

    // 初始化模型
    ai_network_params params = {
        AI_PERSON_DETECTION_DATA_WEIGHTS(ai_person_detection_data_weights_get()),
        AI_PERSON_DETECTION_DATA_ACTIVATIONS(activations)
    };

    if (!ai_person_detection_init(person_detection_handle, &params)) {
        ai_person_detection_destroy(person_detection_handle);
        person_detection_handle = AI_HANDLE_NULL;
        return -1;
    }

    return 0;
}

/**
 * @brief 使用AI模型预测人数
 */
uint8_t person_detection_predict(const uint8_t* sensor_data, float* confidence)
{
    ai_i32 batch;
    ai_buffer *ai_input;
    ai_buffer *ai_output;

    // 检查模型是否已初始化
    if (person_detection_handle == AI_HANDLE_NULL) {
        return 0;
    }

    // 准备输入数据 (归一化到[0,1])
    for (int i = 0; i < AI_PERSON_DETECTION_IN_1_SIZE; i++) {
        in_data[i] = (ai_float)sensor_data[i] / 255.0f;
    }

    // 获取输入输出缓冲区
    ai_input = ai_person_detection_inputs_get(person_detection_handle, NULL);
    ai_output = ai_person_detection_outputs_get(person_detection_handle, NULL);

    // 设置输入数据
    ai_input[0].data = AI_HANDLE_PTR(in_data);

    // 设置输出数据
    ai_output[0].data = AI_HANDLE_PTR(out_data);

    // 运行推理
    batch = ai_person_detection_run(person_detection_handle, ai_input, ai_output);
    if (batch != 1) {
        return 0;  // 推理失败
    }

    // 解析输出
    // out_data[0] = 1人的概率 (类别0)
    // out_data[1] = 2人的概率 (类别1)
    uint8_t predicted_persons = (out_data[1] > out_data[0]) ? 2 : 1;

    // 返回置信度
    if (confidence != NULL) {
        *confidence = (predicted_persons == 2) ? out_data[1] : out_data[0];
    }

    return predicted_persons;
}

/**
 * @brief 释放AI模型资源
 */
void person_detection_deinit(void)
{
    if (person_detection_handle != AI_HANDLE_NULL) {
        ai_person_detection_destroy(person_detection_handle);
        person_detection_handle = AI_HANDLE_NULL;
    }
}
