

#ifndef STM32_SLEEP_POSTURE_H
#define STM32_SLEEP_POSTURE_H

#include <stdint.h>
#include <math.h>
#include <string.h>

// =============================================================================
// Configuration Parameters 配置参数
// =============================================================================

#define SENSOR_ROWS             26      // 传感器矩阵行数
#define SENSOR_COLS             10      // 传感器矩阵列数
//#define TOTAL_SENSORS           260     // 总传感器数量
#define NUM_FEATURES            41      // 提取的特征数量 (Python选中的41个特征)
#define NUM_REGIONS             9       // 区域数量 (3x3)

// Classification results 分类结果
#define POSTURE_SUPINE          0       // 仰卧
#define POSTURE_SIDE            1       // 侧卧
#define POSTURE_UNKNOWN         -1      // 未知

// =============================================================================
// Data Structures 数据结构
// =============================================================================

/**
 * @brief 传感器数据结构
 * Sensor data structure
 */
typedef struct {
    uint16_t data[SENSOR_ROWS][SENSOR_COLS];    // 传感器原始数据
    uint32_t timestamp;                         // 时间戳
    uint8_t valid;                             // 数据有效标志
} SensorData_t;

/**
 * @brief 提取的特征结构
 * Extracted features structure
 */
typedef struct {
    // Basic features 基础特征 (7)
    float active_area;          // 激活面积
    float active_ratio;         // 激活比例
    float total_pressure;       // 总压力
    float mean_pressure;        // 平均压力
    float max_pressure;         // 最大压力
    float pressure_std;         // 压力标准差
    float pressure_range;       // 压力范围

    // Geometric features 几何特征 (12)
    float centroid_x;           // 质心X坐标
    float centroid_y;           // 质心Y坐标
    float width;                // 宽度
    float height;               // 高度
    float aspect_ratio;         // 长宽比
    float orientation;          // 方向角
    float eccentricity;         // 偏心率
    float solidity;             // 实体度
    float convex_area;          // 凸包面积
    float major_axis;           // 主轴长度
    float minor_axis;           // 次轴长度
    float body_spread;          // 身体展开度

    // Distribution features 分布特征 (7)
    float pressure_skewness;    // 偏度
    float pressure_kurtosis;    // 峰度
    float p25, p50, p75, p90;   // 分位数
    float pressure_gini;        // 基尼系数

// Region features 区域特征 (20 = 9*2 + 2) 这里可以通过左右分区再进行细化，或者单区域的更多信息点
    float region_sums[NUM_REGIONS];         // 各区域压力和
    float region_ratios[NUM_REGIONS];       // 各区域激活比例
    float lr_symmetry;                      // 左右对称性
    float upper_lower_ratio;                // 上下比例

    // Texture features 纹理特征 (4)
    float gradient_x;           // X方向梯度
    float gradient_y;           // Y方向梯度
    float laplacian_variance;   // 拉普拉斯方差
    float connected_components; // 连通分量数
} Features_t;

/**
 * @brief 分类结果结构
 * Classification result structure
 */
typedef struct {
    int8_t posture;             // 分类结果
    float confidence;           // 置信度
    uint32_t processing_time_ms; // 处理时间(毫秒)
    uint8_t feature_valid;      // 特征提取是否成功
} ClassificationResult_t;

// =============================================================================
// Random Forest Model Parameters 随机森林模型参数
// =============================================================================

// Model configuration 模型配置
#define RF_NUM_TREES            20      // 决策树数量 (简化以适应STM32)
#define RF_MAX_DEPTH            8       // 最大深度
#define RF_NUM_FEATURES_PER_TREE 15     // 每棵树使用的特征数

// Decision tree node 决策树节点
typedef struct {
    uint8_t feature_index;      // 特征索引
    float threshold;            // 阈值
    int16_t left_child;         // 左子节点索引 (-1表示叶节点)
    int16_t right_child;        // 右子节点索引
    float leaf_value;           // 叶节点值 (仅叶节点有效)
} TreeNode_t;

// =============================================================================
// Function Declarations 函数声明
// =============================================================================



/**
 * @brief Run feature engineering model inference - Standalone tool function - 2025-11-04
 * 运行特征工程模型推理 - 独立工具函数
 *
 * @param ai_input: 输入传感器数据指针 (uint8_t array, 260 bytes)
 * @param ai_output: 输出指针，可选 (uint8_t*, 存储结果: 1=仰卧, 2=侧卧)
 * @return 睡姿结果: 1=仰卧(Supine), 2=侧卧(Side)
 * @note 这个函数完全独立，直接处理输入数据并返回睡姿结果 (1 or 2)
 *       This function is completely standalone, processes input and returns posture directly
 */
extern uint32_t feature_engineering_model_run(void *ai_input, void *ai_output);

#endif // STM32_SLEEP_POSTURE_H
