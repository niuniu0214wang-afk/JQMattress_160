/* posture_classifier.h — STM32 裸机姿态分类器头文件
 * 由 export_rf_to_c.py 自动生成，请勿手动修改
 * Stage1: 200 棵树，共 890 个节点
 * Stage2: 200 棵树，共 9532 个节点
 * 生成日期：2026-05-06
 */
#ifndef POSTURE_CLASSIFIER_H
#define POSTURE_CLASSIFIER_H

#include <stdint.h>

/* 传感器矩阵尺寸（每侧 8 行 × 10 列）(2026-05-06) */
#define POSTURE_ROWS   8
#define POSTURE_COLS   10

/* 自适应激活阈值参数，与 posture_features.py 保持一致 (2026-05-06) */
#define ACTIVE_RATIO   0.05f
#define ACTIVE_FLOOR   5.0f

/* 姿态标签枚举 (2026-05-06) */
typedef enum {
    POSTURE_UNKNOWN = 0,
    POSTURE_SUPINE  = 1,   /* 仰卧 */
    POSTURE_LEFT    = 2,   /* 左侧卧 */
    POSTURE_RIGHT   = 3,   /* 右侧卧 */
} PostureLabel;

/* 随机森林节点结构体，#pragma pack(1) 避免 STM32 Flash 浪费 (2026-05-06) */
#pragma pack(push, 1)
typedef struct {
    int8_t  feature;     /* 分裂特征索引，叶节点为 -1 */
    float   threshold;   /* 分裂阈值，叶节点为 0.0f */
    int16_t left;        /* 左子节点索引，叶节点为 -1 */
    int16_t right;       /* 右子节点索引，叶节点为 -1 */
    int8_t  leaf_class;  /* 叶节点预测类别，内部节点为 -1 */
} RFNode;
#pragma pack(pop)

/* Stage1 节点数组与偏移量（仰卧 vs 侧卧）(2026-05-06) */
extern const RFNode    stage1_nodes[];
extern const int16_t   stage1_offsets[];
#define STAGE1_N_TREES 200

/* Stage2 节点数组与偏移量（左侧卧 vs 右侧卧）(2026-05-06) */
extern const RFNode    stage2_nodes[];
extern const int16_t   stage2_offsets[];
#define STAGE2_N_TREES 200

/* Stage2 特征索引（从 17 维全特征中选 11 维，0-based）(2026-05-06) */
#define STAGE2_N_FEATURES 11
extern const int8_t stage2_feature_idx[STAGE2_N_FEATURES];

/* 特征提取：从 80 个压力值提取 17 维特征 (2026-05-06) */
void posture_extract_features(const float *pressure_80, float *features_out);

/* 主分类接口：输入 80 个压力值，返回 PostureLabel (2026-05-06) */
int8_t posture_classify(const float *pressure_80);

#endif /* POSTURE_CLASSIFIER_H */
