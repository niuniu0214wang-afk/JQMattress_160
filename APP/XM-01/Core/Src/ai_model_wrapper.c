/**
 * @file ai_model_wrapper.c
 * @brief X-CUBE-AI 模型包装函数实现
 *
 * 封装X-CUBE-AI生成的network模型
 * Wrapper for X-CUBE-AI generated network model
 *
 * 中文注释 (2026-03-03)
 */

#include "ai_model_wrapper.h"
#include "../../X-CUBE-AI-NEW/App/network.h"       /* X-CUBE-AI生成的模型头文件 (2026-04-07) */
#include "../../X-CUBE-AI-NEW/App/network_data.h"  /* X-CUBE-AI生成的权重数据 (2026-04-07) */
#include <string.h>

/* AI模型句柄 (2026-04-07) */
static ai_handle network_handle = AI_HANDLE_NULL;

/* 激活缓冲区 - 32字节对齐，与X-CUBE-AI库要求一致 (2026-04-07) */
AI_ALIGNED(32) static ai_u8 activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];

/* 输入缓冲区: uint8 260字节，模型训练时输入类型为tf.uint8 (2026-04-07) */
AI_ALIGNED(4) static ai_u8 in_data[AI_NETWORK_IN_1_SIZE_BYTES];

/* 输出缓冲区: float32 1个值（侧卧概率） (2026-04-07) */
AI_ALIGNED(4) static ai_float out_data[AI_NETWORK_OUT_1_SIZE];

/* 初始化标志，防止重复初始化 (2026-04-07) */
static uint8_t ai_initialized = 0;

/**
 * @brief 初始化AI模型 (2026-04-07)
 * 使用ai_network_create_and_init API，参考JQ704实现 (2026-04-07)
 */
int ai_wrapper_init(void)
{
    ai_error err;
    const ai_handle act_addr[] = { activations };

    /* 防止重复初始化 (2026-04-07) */
    if (ai_initialized) {
        return 0;
    }

    /* 一步完成创建和初始化 (2026-04-07) */
    err = ai_network_create_and_init(&network_handle, act_addr, NULL);
    if (err.type != AI_ERROR_NONE) {
        network_handle = AI_HANDLE_NULL;
        return -1;
    }

    ai_initialized = 1;
    return 0;
}

/**
 * @brief 运行AI睡姿分类 (2026-04-07)
 *
 * 预处理：无需额外处理，模型训练时输入为uint8 [0,255]，
 * 转换脚本设置 inference_input_type = tf.uint8，直接传入原始传感器数据。
 * 参考：train_cnn_binary_lite.py preprocess_for_cnn() 仅做 X/255.0，
 * 量化后等价于直接输入uint8。(2026-04-07)
 */
uint8_t ai_model_classify(const uint8_t* sensor_data, float* probability)
{
    ai_i32 batch;
    ai_buffer *ai_input;
    ai_buffer *ai_output;

    /* 检查模型是否已初始化 (2026-04-07) */
    if (!ai_initialized || network_handle == AI_HANDLE_NULL) {
        return 0;
    }

    /* 复制输入数据（uint8，260字节） (2026-04-07) */
    memcpy(in_data, sensor_data, AI_NETWORK_IN_1_SIZE_BYTES);

    /* 获取输入输出缓冲区 (2026-04-07) */
    ai_input  = ai_network_inputs_get(network_handle, NULL);
    ai_output = ai_network_outputs_get(network_handle, NULL);

    if (!ai_input || !ai_output) {
        return 0;
    }

    /* 设置数据指针 (2026-04-07) */
    ai_input->data  = AI_HANDLE_PTR(in_data);
    ai_output->data = AI_HANDLE_PTR(out_data);

    /* 执行推理 (2026-04-07) */
    batch = ai_network_run(network_handle, ai_input, ai_output);
    if (batch != 1) {
        return 0;
    }

    /* 输出为侧卧概率 float32 [0,1] (2026-04-07) */
    float prob = out_data[0];

    if (probability != NULL) {
        *probability = prob;
    }

    /* 阈值判断：>= AI_OPTIMAL_THRESHOLD 为侧卧 (2026-04-07) */
    return (prob >= AI_OPTIMAL_THRESHOLD) ? AI_POSTURE_SIDE : AI_POSTURE_SUPINE;
}

/**
 * @brief 释放AI资源 (2026-04-07)
 */
void ai_model_deinit(void)
{
    if (network_handle != AI_HANDLE_NULL) {
        ai_network_destroy(network_handle);
        network_handle = AI_HANDLE_NULL;
        ai_initialized = 0;
    }
}
