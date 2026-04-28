/**
 * @file person_detection_wrapper.h
 * @brief 人数检测AI模型包装函数
 * @date 2026-03-03
 *
 * 使用X-CUBE-AI生成的person_detection模型
 * 输入: 260字节传感器数据
 * 输出: 人数预测 (1或2)
 */

#ifndef PERSON_DETECTION_WRAPPER_H
#define PERSON_DETECTION_WRAPPER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化人数检测AI模型
 * @return 0=成功, -1=失败
 */
int person_detection_init(void);

/**
 * @brief 使用AI模型预测人数
 * @param sensor_data 260字节传感器数据 (26x10矩阵)
 * @param confidence 输出置信度指针 (可选，可传NULL)
 * @return 预测的人数 (1或2)，错误返回0
 */
uint8_t person_detection_predict(const uint8_t* sensor_data, float* confidence);

/**
 * @brief 释放AI模型资源
 */
void person_detection_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // PERSON_DETECTION_WRAPPER_H
