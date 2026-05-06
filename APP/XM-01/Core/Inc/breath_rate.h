/* breath_rate.h — STM32 呼吸率计算模块头文件
 * 算法移植自 InDevelop/software/src/breath_signal_analyzer.py
 * 流程：ROI融合 → 线性去趋势 → 带通滤波 → FFT峰值检测 → 历史追踪
 * 采样率：13 Hz，分析窗口：30 s（390帧），FFT：2048点
 * (2026-05-06)
 */
#ifndef BREATH_RATE_H
#define BREATH_RATE_H

#include <stdint.h>
#include "rtthread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 配置参数 ─────────────────────────────────────────────── */
#define BR_FS               13      /* 采样率 Hz (2026-05-06) */
#define BR_WINDOW_SEC       30      /* 分析窗口秒数 (2026-05-06) */
#define BR_N_WIN            (BR_FS * BR_WINDOW_SEC)  /* 390帧 */
#define BR_FFT_NFFT         2048    /* FFT点数 (2026-05-06) */
#define BR_HALF_SIZE        80      /* 单人半区点数 8×10 (2026-05-06) */
#define BR_TRACK_HISTORY    6       /* 历史追踪窗口数 (2026-05-06) */
#define BR_HOLD_MAX         2       /* 保持机制最大窗口数 (2026-05-06) */
#define BR_MAX_CANDIDATES   5       /* 最多候选峰数 (2026-05-06) */

/* 呼吸率有效范围 6–30 bpm = 0.1–0.5 Hz (2026-05-06) */
#define BR_LO_HZ            0.1f
#define BR_HI_HZ            0.5f
#define BR_LO_BPM           6.0f
#define BR_HI_BPM           30.0f

/* SNR 阈值 (2026-05-06) */
#define BR_SNR_MIN          1.5f
#define BR_SNR_GOOD         2.2f

/* 质量分类 (2026-05-06) */
typedef enum {
    BR_QUALITY_LOW_SNR  = 0,
    BR_QUALITY_AMBIGUOUS,
    BR_QUALITY_USABLE,
    BR_QUALITY_GOOD,
} BreathQuality;

/* 单个候选峰 (2026-05-06) */
typedef struct {
    float freq;   /* Hz */
    float bpm;
    float amp;
} BRCandidate;

/* 单次分析结果 (2026-05-06) */
typedef struct {
    float         bpm;          /* 最终输出 BPM，0=无效 */
    float         snr;
    float         raw_bpm;      /* 原始 FFT BPM */
    float         track_bpm;    /* 追踪后 BPM */
    BreathQuality quality;
    uint8_t       candidate_n;
    BRCandidate   candidates[BR_MAX_CANDIDATES];
} BRResult;

/* 呼吸率分析器状态（每个半区一个实例）(2026-05-06) */
typedef struct {
    /* 滑动窗口缓冲区：390帧 × 80点 */
    float     buf[BR_N_WIN][BR_HALF_SIZE];
    uint16_t  buf_head;     /* 环形缓冲写指针 */
    uint16_t  buf_count;    /* 已填充帧数 */

    /* 帧计数，每 BR_FS 帧触发一次分析 (2026-05-06) */
    uint16_t  frame_cnt;

    /* 历史追踪 (2026-05-06) */
    BRResult  hist[BR_TRACK_HISTORY];
    uint8_t   hist_head;
    uint8_t   hist_count;

    /* 保持机制 (2026-05-06) */
    float     last_good_bpm;
    uint8_t   hold_count;

    /* 最新结果 (2026-05-06) */
    BRResult  result;
    uint8_t   result_valid;
} BreathAnalyzer;

/* ── 公开接口 ─────────────────────────────────────────────── */

/* 初始化分析器（清零所有状态）(2026-05-06) */
void breath_analyzer_init(BreathAnalyzer *ba);

/* 推入一帧 80 点半区数据（uint8_t → float 转换在内部完成）
 * 每 BR_FS 帧自动触发一次分析，结果写入 ba->result (2026-05-06) */
void breath_analyzer_push(BreathAnalyzer *ba, const uint8_t *half80);

/* 读取最新结果（线程安全：调用方需在临界区外读取 result_valid）(2026-05-06) */
static inline float breath_get_bpm(const BreathAnalyzer *ba)
{
    return ba->result_valid ? ba->result.bpm : 0.0f;
}

#ifdef __cplusplus
}
#endif

#endif /* BREATH_RATE_H */
