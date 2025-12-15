

#include "stm32_sleep_posture.h"
#include "random_forest_model_generated.h"  // 使用Python生成的完整模型参数
#include "rf_features_complete.h"
#include "SEGGER_RTT.h"
#include "rtthread.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

// =============================================================================
// 配置参数
// =============================================================================

#define SENSOR_READ_INTERVAL_MS    1000    // 传感器读取间隔
#define CLASSIFICATION_THRESHOLD   0.5f    // 分类置信度阈值
#define RF_NUM_FEATURES            41      // 随机森林特征数量 (Python选中的41个特征)

// =============================================================================
// 私有变量
// =============================================================================

static uint32_t g_total_classifications = 0;
static uint32_t g_total_processing_time = 0;
//static uint8_t g_initialized = 0;

// 全局缓冲区 - 避免栈溢出 (Global buffers to prevent stack overflow)
// 这些数组在feature_engineering_model_run中用到，移到全局避免栈溢出
// 2025-11-04: Critical fix for MCU crash
//static float g_nonzero_values[260];     // 用于extract_distribution_features
//static uint8_t g_visited[260];          // 用于extract_texture_features (连通分量)

/* 导入直接输出变量 - 避免struct对齐问题 - 2025-11-06 */
extern uint8_t g_posture_0;
extern uint8_t g_posture_1;
extern uint8_t Origin_MattressData[260];

//static const char* VERSION_STRING = "STM32_SleepPosture_v2.0_Python_Match_2025-11-04";

// =============================================================================
// 私有函数声明
// =============================================================================

// Note: Feature extraction now uses extract_41_python_features() from rf_features_complete.h
// 注意: 特征提取现在使用rf_features_complete.h中的extract_41_python_features()函数

// 随机森林推理函数
static float rf_predict_tree(uint8_t tree_idx, const float* normalized_features);
static float rf_predict_ensemble(const float* normalized_features);
static void rf_normalize_features(const float* features, float* normalized);

// 工具函数
//static void sort_array(float* arr, int size);


// =============================================================================
// 主功能函数：特征工程模型推理
// Main Feature Engineering Model Inference Function
// =============================================================================

/**
 * @brief 运行特征工程模型推理 - 完全匹配Python实现
 * Run feature engineering model inference - Exact Python match
 *
 * @param ai_input: 输入传感器数据指针 (uint8_t array, 260 bytes = 26x10)
 * @param ai_output: 输出指针（可选，存储结果）
 * @return 睡姿结果: 1=仰卧(Supine), 2=侧卧(Side)
 */
uint32_t feature_engineering_model_run(void *ai_input, void *ai_output)
{
    const uint8_t *input_data = (const uint8_t *)ai_input;
    uint32_t start_time = rt_tick_get_millisecond();

    // 使用全局数据作为备选方案
    if (input_data == NULL) {
        input_data = Origin_MattressData;
    }

    // ========================================================================
    // 第一步：提取Python选中的41个特征
    // Step 1: Extract all 41 Python-selected features
    // ========================================================================

    float features[41];

    // 调用Python兼容的41特征提取函数
    if (extract_41_python_features(input_data, features) != 0) {
        SEGGER_RTT_printf(0, "ERROR: Feature extraction failed\r\n");
        return 1;  // Return Supine as default
    }

    // ========================================================================
    // 第二步：特征标准化
    // Step 2: Feature normalization using RobustScaler parameters
    // ========================================================================

    float normalized_features[41];
    rf_normalize_features(features, normalized_features);

    // ========================================================================
    // 第三步：使用随机森林进行分类
    // Step 3: Classification using Random Forest ensemble
    // ========================================================================

    float ensemble_prob = rf_predict_ensemble(normalized_features);

    // 决策：概率 > 0.5 表示侧卧(Side), 否则仰卧(Supine)
    uint8_t result = (ensemble_prob > CLASSIFICATION_THRESHOLD) ? 2 : 1;

    // ========================================================================
    // 调试输出
    // Debug output
    // ========================================================================

    uint32_t processing_time = rt_tick_get_millisecond() - start_time;

    // ========================================================================
    // 更新统计和输出
    // Update statistics and output
    // ========================================================================

    g_total_classifications++;
    g_total_processing_time += processing_time;

    if (ai_output != NULL) {
        *(uint8_t*)ai_output = result;
    }

    // 注意: 在Model()函数中调用此函数时，它会返回结果值
    // 由Model()函数负责将结果分配给g_posture_0或g_posture_1
    // Note: This function returns the result; Model() is responsible for assigning
    // it to g_posture_0 or g_posture_1 depending on upper/lower matrix

    return result;
}


// =============================================================================
// 随机森林推理函数
// Random Forest Inference Functions
// =============================================================================

/**
 * @brief 使用单棵决策树进行预测
 * Predict using single decision tree
 */
static float rf_predict_tree(uint8_t tree_idx, const float* normalized_features)
{
    if (tree_idx >= RF_NUM_TREES) return 0.5f;

    uint16_t tree_offset = RF_TREE_OFFSETS[tree_idx];
    int16_t current_node = 0;

    // 从根节点遍历到叶节点
    while (RF_TREE_NODES[tree_offset + current_node].left_child != -1) {
        const TreeNode_t* node = &RF_TREE_NODES[tree_offset + current_node];
        uint8_t feature_idx = node->feature_index;
        float threshold = node->threshold;

        // 导航决策树
        if (feature_idx < RF_NUM_FEATURES && normalized_features[feature_idx] <= threshold) {
            current_node = node->left_child;
        } else {
            current_node = node->right_child;
        }
    }

    // 返回叶节点的类概率
    return RF_TREE_NODES[tree_offset + current_node].leaf_value;
}

/**
 * @brief 使用所有树进行集合预测
 * Predict using all trees (ensemble voting)
 */
static float rf_predict_ensemble(const float* normalized_features)
{
    float sum_probabilities = 0.0f;

    for (uint8_t i = 0; i < RF_NUM_TREES; i++) {
        float tree_prob = rf_predict_tree(i, normalized_features);
        sum_probabilities += tree_prob;
    }

    // 返回所有树的平均概率
    return sum_probabilities / RF_NUM_TREES;
}

/**
 * @brief 特征标准化 (使用RobustScaler参数)
 * Normalize features using RobustScaler parameters
 */
static void rf_normalize_features(const float* features, float* normalized)
{
    for (int i = 0; i < RF_NUM_FEATURES; i++) {
        if (RF_FEATURE_SCALES[i] > 0.0f) {
            normalized[i] = (features[i] - RF_FEATURE_MEANS[i]) / RF_FEATURE_SCALES[i];
        } else {
            normalized[i] = 0.0f;
        }
    }
}

// =============================================================================
// 工具函数
// Utility Functions
// =============================================================================

#if 0
/**
 * @brief 简单的冒泡排序
 * Simple bubble sort for small arrays
 */
static void sort_array(float* arr, int size)
{
    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j < size; j++) {
            if (arr[i] > arr[j]) {
                float temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
    }
}
#endif

