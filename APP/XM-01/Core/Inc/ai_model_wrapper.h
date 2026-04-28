/**
 * @file ai_model_wrapper.h
 * @brief X-CUBE-AI 模型包装函数头文件
 *
 * 封装X-CUBE-AI生成的network模型，提供简单的API接口
 * Wrapper for X-CUBE-AI generated network model
 *
 * 中文注释 (2026-03-03)
 */

#ifndef AI_MODEL_WRAPPER_H
#define AI_MODEL_WRAPPER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 分类结果定义 (2026-03-03)
#define AI_POSTURE_SUPINE      1   // 仰卧
#define AI_POSTURE_SIDE        2   // 侧卧

// 最优阈值（从训练脚本获取）(2026-03-03)
#define AI_OPTIMAL_THRESHOLD   0.32f

/**
 * @brief 初始化AI模型
 * Initialize AI model
 *
 * @return 0=成功, -1=失败
 */
int ai_wrapper_init(void);

/**
 * @brief 运行AI睡姿分类
 * Run AI sleep posture classification
 *
 * @param sensor_data 输入传感器数据 (uint8_t[260], 26x10矩阵)
 * @param probability 输出侧卧概率指针 (可选，可传NULL)
 * @return 睡姿结果: 1=仰卧(Supine), 2=侧卧(Side), 0=错误
 */
uint8_t ai_model_classify(const uint8_t* sensor_data, float* probability);

/**
 * @brief 释放AI资源
 * Deinitialize AI model
 */
void ai_model_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // AI_MODEL_WRAPPER_H
