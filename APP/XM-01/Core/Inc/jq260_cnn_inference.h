/**
 * @file jq260_cnn_inference.h
 * @brief JQ260 CNN Lite Inference API for STM32F405RG
 *
 * 中文注释 (2025-12-22)
 */

#ifndef JQ260_CNN_INFERENCE_H
#define JQ260_CNN_INFERENCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 常量定义在 jq260_cnn_model.h 中 (2025-12-23)
// CNN_POSTURE_SUPINE, CNN_POSTURE_SIDE, CNN_OPTIMAL_THRESHOLD

// =============================================================================
// API函数 (2025-12-22)
// =============================================================================

/**
 * @brief 初始化CNN模型
 * @return 0成功, -1失败
 */
int cnn_posture_init(void);

/**
 * @brief 运行CNN睡姿分类
 *
 * @param sensor_data 输入传感器数据 (uint8_t[260], 26x10矩阵)
 * @param probability 输出侧卧概率指针 (可选，可传NULL)
 * @return 睡姿结果: 1=仰卧(Supine), 2=侧卧(Side)
 */
uint8_t cnn_posture_classify(const uint8_t* sensor_data, float* probability);

/**
 * @brief 释放CNN资源
 */
void cnn_posture_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // JQ260_CNN_INFERENCE_H
