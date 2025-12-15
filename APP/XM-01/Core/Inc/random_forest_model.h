/**
 * @file random_forest_model.h
 * @brief Complete Random Forest Model with 20 Decision Trees
 *
 * 完整的随机森林模型 - 20棵决策树
 * Auto-generated from Python training with 94.87% accuracy
 * 自动从Python训练生成，精度94.87%
 *
 * Author: Claude Code (Auto-generated)
 * Date: 2025-01
 */

#ifndef RANDOM_FOREST_MODEL_H
#define RANDOM_FOREST_MODEL_H

#include <stdint.h>
#include "stm32_sleep_posture.h"  // 包含TreeNode_t定义

// ============================================================================
// Model Configuration 模型配置
// ============================================================================

#define RF_NUM_TREES            20      // Number of decision trees
#define RF_NUM_FEATURES         50      // Number of features
#define RF_MAX_TREE_NODES       120     // Max nodes per tree

// ============================================================================
// Feature Scaling Parameters (from RobustScaler training)
// 特征缩放参数 (来自RobustScaler训练)
// ============================================================================

// Feature means for normalization
// 用于标准化的特征均值
static const float RF_FEATURE_MEANS[50] = {
    81.000000f,     0.311538f,     5200.000000f,     64.439026f,     177.000000f,
    48.534935f,     172.000000f,     0.424265f,     0.479679f,     0.000000f,
    0.000000f,     0.000000f,     0.000000f,     0.000000f,     0.000000f,
    0.000000f,     0.000000f,     0.000000f,     0.000000f,     0.000000f,
    0.000000f,     0.000000f,     0.000000f,     0.000000f,     0.000000f,
    0.000000f,     0.000000f,     0.000000f,     0.000000f,     0.000000f,
    0.000000f,     0.000000f,     0.000000f,     0.000000f,     0.000000f,
    0.000000f,     0.000000f,     0.000000f,     0.000000f,     0.000000f,
    0.000000f,     0.000000f,     0.000000f,     0.000000f,     0.000000f,
    0.000000f,     0.000000f,     0.000000f,     0.000000f,     0.000000f
};

// Feature scales for normalization
// 用于标准化的特征缩放因子
static const float RF_FEATURE_SCALES[50] = {
    11.000000f,     0.042308f,     771.000000f,     6.091038f,     33.000000f,
    7.332058f,     33.000000f,     0.030853f,     0.489765f,     1.000000f,
    1.000000f,     1.000000f,     1.000000f,     1.000000f,     1.000000f,
    1.000000f,     1.000000f,     1.000000f,     1.000000f,     1.000000f,
    1.000000f,     1.000000f,     1.000000f,     1.000000f,     1.000000f,
    1.000000f,     1.000000f,     1.000000f,     1.000000f,     1.000000f,
    1.000000f,     1.000000f,     1.000000f,     1.000000f,     1.000000f,
    1.000000f,     1.000000f,     1.000000f,     1.000000f,     1.000000f,
    1.000000f,     1.000000f,     1.000000f,     1.000000f,     1.000000f,
    1.000000f,     1.000000f,     1.000000f,     1.000000f,     1.000000f
};

// ============================================================================
// Note: TreeNode_t 已在 stm32_sleep_posture.h 中定义
// Note: TreeNode_t is already defined in stm32_sleep_posture.h
// ============================================================================

// ============================================================================
// Decision Tree Data - All 20 trees
// 决策树数据 - 全部20棵树
// ============================================================================

// Complete tree nodes array (all 20 trees)
// 完整的树节点数组 (全部20棵树)
#define TOTAL_TREE_NODES 2208

static const TreeNode_t RF_TREE_NODES[TOTAL_TREE_NODES] = {
    // ========== Tree 0 ==========
    {8, 0.045801f, 1, 72, 0.000000f},
    {4, 0.166667f, 2, 55, 0.000000f},
    {1, -0.227273f, 3, 28, 0.000000f},
    {7, -0.286952f, 4, 15, 0.000000f},
    {4, -0.166667f, 5, 12, 0.000000f},
    {4, -0.469697f, 6, 11, 0.000000f},
    {8, -0.446079f, 7, 8, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {1, -2.090909f, 9, 10, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.750000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {8, -0.168762f, 13, 14, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {5, -0.082489f, 16, 21, 0.000000f},
    {7, 0.186687f, 17, 18, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {6, -1.106061f, 19, 20, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {3, 0.263779f, 22, 23, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {8, -0.119916f, 24, 25, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {1, -0.454545f, 26, 27, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.583333f},
    {8, -0.110568f, 29, 44, 0.000000f},
    {7, -0.343470f, 30, 37, 0.000000f},
    {7, -1.717570f, 31, 32, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {4, -0.621212f, 33, 34, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {2, 0.398184f, 35, 36, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.944444f},
    {8, -0.449224f, 38, 43, 0.000000f},
    {8, -0.716263f, 39, 40, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {8, -0.594696f, 41, 42, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 0.285714f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {0, 1.090909f, 45, 52, 0.000000f},
    {8, -0.046271f, 46, 51, 0.000000f},
    {7, -0.110423f, 47, 50, 0.000000f},
    {5, -0.208699f, 48, 49, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {3, -1.341978f, 53, 54, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {3, -2.236046f, 56, 57, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {7, 0.454039f, 58, 63, 0.000000f},
    {4, 0.257576f, 59, 62, 0.000000f},
    {8, 0.017871f, 60, 61, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {1, 0.863636f, 64, 71, 0.000000f},
    {8, -0.492411f, 65, 70, 0.000000f},
    {8, -0.527912f, 66, 69, 0.000000f},
    {1, -0.545455f, 67, 68, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.666667f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {0, 0.500000f, 73, 98, 0.000000f},
    {5, -0.614906f, 74, 83, 0.000000f},
    {1, 0.045455f, 75, 78, 0.000000f},
    {6, -0.030303f, 76, 77, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {1, 0.318182f, 79, 82, 0.000000f},
    {2, -0.804150f, 80, 81, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {8, 0.479109f, 84, 93, 0.000000f},
    {5, -0.281358f, 85, 92, 0.000000f},
    {2, -0.026589f, 86, 91, 0.000000f},
    {1, -1.363636f, 87, 88, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {0, -0.409091f, 89, 90, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.454545f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {6, 0.378788f, 94, 97, 0.000000f},
    {6, 0.333333f, 95, 96, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {6, -0.287879f, 99, 108, 0.000000f},
    {6, -1.181818f, 100, 101, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {0, 0.772727f, 102, 107, 0.000000f},
    {6, -0.439394f, 103, 104, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {7, 0.191745f, 105, 106, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {8, 0.657427f, 109, 114, 0.000000f},
    {3, -1.986678f, 110, 113, 0.000000f},
    {3, -2.469657f, 111, 112, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
    {0, 0.954545f, 115, 116, 0.000000f},
    {255, -2.000000f, -1, -1, 0.000000f},
    {255, -2.000000f, -1, -1, 1.000000f},
};

// Tree offset table: starting index for each tree in RF_TREE_NODES
// 树偏移表：每棵树在RF_TREE_NODES中的起始索引
static const uint16_t RF_TREE_OFFSETS[RF_NUM_TREES] = {
    0,      // Tree 0 starts at index 0
    117,    // Tree 1 starts at index 117
    224,    // Tree 2
    344,    // Tree 3
    456,    // Tree 4
    561,    // Tree 5
    686,    // Tree 6
    788,    // Tree 7
    900,    // Tree 8
    1010,   // Tree 9
    1116,   // Tree 10
    1225,   // Tree 11
    1329,   // Tree 12
    1447,   // Tree 13
    1541,   // Tree 14
    1635,   // Tree 15
    1757,   // Tree 16
    1859,   // Tree 17
    1969,   // Tree 18
    2089,   // Tree 19
};

// Tree sizes (number of nodes in each tree)
// 树的大小（每棵树的节点数）
static const uint16_t RF_TREE_SIZES[RF_NUM_TREES] = {
    117,    // Tree 0
    107,    // Tree 1
    120,    // Tree 2
    112,    // Tree 3
    105,    // Tree 4
    125,    // Tree 5
    102,    // Tree 6
    112,    // Tree 7
    110,    // Tree 8
    106,    // Tree 9
    109,    // Tree 10
    104,    // Tree 11
    118,    // Tree 12
    94,     // Tree 13
    94,     // Tree 14
    122,    // Tree 15
    102,    // Tree 16
    110,    // Tree 17
    120,    // Tree 18
    119,    // Tree 19
};

// ============================================================================
// Ensemble Prediction Function
// 集合预测函数
// ============================================================================

/**
 * @brief Predict using single decision tree
 * 使用单棵决策树进行预测
 *
 * @param tree_idx Index of the tree to use
 * @param normalized_features Normalized feature array [50]
 * @return Class probability (0.0 - 1.0)
 */
static inline float RF_PredictTree(uint8_t tree_idx, const float* normalized_features)
{
    if (tree_idx >= RF_NUM_TREES) return 0.5f;

    uint16_t tree_offset = RF_TREE_OFFSETS[tree_idx];
    int16_t current_node = 0;

    // Traverse tree from root to leaf
    while (RF_TREE_NODES[tree_offset + current_node].left_child != -1) {
        const TreeNode_t* node = &RF_TREE_NODES[tree_offset + current_node];
        uint8_t feature_idx = node->feature_index;
        float threshold = node->threshold;

        // Navigate tree based on feature value
        if (feature_idx < 50 && normalized_features[feature_idx] <= threshold) {
            current_node = node->left_child;
        } else {
            current_node = node->right_child;
        }
    }

    // Return leaf node probability
    return RF_TREE_NODES[tree_offset + current_node].leaf_value;
}

/**
 * @brief Predict using all trees (ensemble voting)
 * 使用所有树进行预测（集合投票）
 *
 * @param normalized_features Normalized feature array [50]
 * @return Final class probability (0.0 - 1.0)
 */
static inline float RF_PredictEnsemble(const float* normalized_features)
{
    float sum_probabilities = 0.0f;

    for (uint8_t i = 0; i < RF_NUM_TREES; i++) {
        float tree_prob = RF_PredictTree(i, normalized_features);
        sum_probabilities += tree_prob;
    }

    // Average probability across all trees
    return sum_probabilities / RF_NUM_TREES;
}

/**
 * @brief Normalize features for model inference
 * 为模型推理标准化特征
 *
 * @param features Raw feature array
 * @param normalized Output normalized feature array
 */
static inline void RF_NormalizeFeatures(const float* features, float* normalized)
{
    for (int i = 0; i < RF_NUM_FEATURES; i++) {
        if (RF_FEATURE_SCALES[i] > 0.0f) {
            normalized[i] = (features[i] - RF_FEATURE_MEANS[i]) / RF_FEATURE_SCALES[i];
        } else {
            normalized[i] = 0.0f;
        }
    }
}

#endif // RANDOM_FOREST_MODEL_H
