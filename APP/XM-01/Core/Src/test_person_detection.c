/**
 * @file test_person_detection.c
 * @brief 测试人数检测AI模型
 * @date 2026-03-03
 */

#include "person_detection_wrapper.h"
#include <stdio.h>

// 测试数据 (从parsed_sensor_data.csv中提取)
// 单人样本
static const uint8_t test_single_person[260] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x07,
    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x0A, 0x00, 0x15, 0x00,
    // ... (完整的260字节数据)
};

// 双人样本
static const uint8_t test_double_person[260] = {
    0x00, 0x00, 0x00, 0x00, 0x16, 0x00, 0x17, 0x0E, 0x1B, 0x20,
    0x0B, 0x00, 0x17, 0x00, 0x0F, 0x09, 0x2E, 0x22, 0x43, 0x60,
    // ... (完整的260字节数据)
};

/**
 * @brief 测试人数检测模型
 */
void test_person_detection(void)
{
    float confidence = 0.0f;
    uint8_t result;

    printf("\n=== 人数检测AI模型测试 ===\n");

    // 初始化模型
    printf("初始化模型...\n");
    if (person_detection_init() != 0) {
        printf("错误: 模型初始化失败!\n");
        return;
    }
    printf("模型初始化成功!\n");

    // 测试单人样本
    printf("\n测试1: 单人样本\n");
    result = person_detection_predict(test_single_person, &confidence);
    printf("  预测结果: %d人\n", result);
    printf("  置信度: %.2f%%\n", confidence * 100.0f);
    printf("  期望: 1人\n");
    printf("  结果: %s\n", (result == 1) ? "✓ 正确" : "✗ 错误");

    // 测试双人样本
    printf("\n测试2: 双人样本\n");
    result = person_detection_predict(test_double_person, &confidence);
    printf("  预测结果: %d人\n", result);
    printf("  置信度: %.2f%%\n", confidence * 100.0f);
    printf("  期望: 2人\n");
    printf("  结果: %s\n", (result == 2) ? "✓ 正确" : "✗ 错误");

    printf("\n=== 测试完成 ===\n");
}
