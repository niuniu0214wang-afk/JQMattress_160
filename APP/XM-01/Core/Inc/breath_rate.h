/* breath_rate.h — STM32 呼吸率 + 心率计算模块头文件
 * 算法移植自 InDevelop/software/src/breath_signal_analyzer.py
 *                  InDevelop/software/src/live_prediction.py
 * 流程：ROI融合 → 去趋势 → 带通滤波 → FFT → 历史追踪 + 卡尔曼平滑
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

/* 心率参数（BCG，移植自 live_prediction.py）(2026-05-06) */
#define HR_BAND_LO_HZ       0.8f    /* 心率带下限 Hz */
#define HR_BAND_HI_HZ       6.0f    /* 心率带上限 Hz（Nyquist=6.5 Hz）*/
#define HR_PRQ_LO           3.0f    /* PRQ约束下限：HR >= RR × 3.0 */
#define HR_PRQ_HI           6.0f    /* PRQ约束上限：HR <= RR × 6.0 */
#define HR_FALLBACK_LO_HZ   0.8f    /* PRQ无效时回退范围下限 */
#define HR_FALLBACK_HI_HZ   3.0f    /* PRQ无效时回退范围上限 */

/* 运动检测阈值（总载荷逐帧变化量，需按硬件校准）(2026-05-06) */
#define MOTION_DELTA_THRESH 200.0f

/* 有人检测最小总压力（80点之和）(2026-05-06) */
#define PRESENCE_MIN        250.0f

/* SNR 阈值 (2026-05-06) */
#define BR_SNR_MIN          3.0f    /* 与 live_prediction.py BREATH_SNR_MIN 一致 */
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
    float freq;
    float bpm;
    float amp;
} BRCandidate;

/* 单次呼吸率分析结果 (2026-05-06) */
typedef struct {
    float         bpm;
    float         snr;
    float         raw_bpm;
    float         track_bpm;
    BreathQuality quality;
    uint8_t       candidate_n;
    BRCandidate   candidates[BR_MAX_CANDIDATES];
} BRResult;

/* 卡尔曼滤波器（心率/呼吸率平滑，移植自 live_prediction.py KalmanHR/KalmanBR）(2026-05-06) */
typedef struct {
    float x;   /* 状态估计 */
    float p;   /* 误差协方差 */
    float q;   /* 过程噪声 */
    float r;   /* 测量噪声 */
} KalmanFilter;

/* 呼吸率 + 心率联合分析器（每个半区一个实例）(2026-05-06) */
typedef struct {
    /* 滑动窗口缓冲区：390帧 × 80点 */
    float     buf[BR_N_WIN][BR_HALF_SIZE];
    uint16_t  buf_head;
    uint16_t  buf_count;
    uint16_t  frame_cnt;

    /* 呼吸率历史追踪 (2026-05-06) */
    BRResult  hist[BR_TRACK_HISTORY];
    uint8_t   hist_head;
    uint8_t   hist_count;
    float     last_good_bpm;
    uint8_t   hold_count;

    /* 卡尔曼平滑器 (2026-05-06) */
    KalmanFilter kalman_br;   /* 呼吸率：Q=0.5, R=4.0, init=15 */
    KalmanFilter kalman_hr;   /* 心率：  Q=2.0, R=16.0, init=65 */

    /* 最新输出结果 (2026-05-06) */
    BRResult  br_result;
    float     hr_bpm;         /* 卡尔曼平滑后心率，0=无效 */
    float     hr_snr;
    uint8_t   motion;         /* 1=检测到运动 */
    uint8_t   present;        /* 1=有人在床 */
    uint8_t   result_valid;
} BreathAnalyzer;

/* ── 公开接口 ─────────────────────────────────────────────── */

/* 初始化分析器 (2026-05-06) */
void breath_analyzer_init(BreathAnalyzer *ba);

/* 推入一帧 80 点半区数据，每 BR_FS 帧自动触发分析 (2026-05-06) */
void breath_analyzer_push(BreathAnalyzer *ba, const uint8_t *half80);

/* 读取呼吸率 BPM（0=无效）(2026-05-06) */
static inline float breath_get_bpm(const BreathAnalyzer *ba)
{
    return (ba->result_valid && ba->present) ? ba->br_result.bpm : 0.0f;
}

/* 读取心率 BPM（0=无效）(2026-05-06) */
static inline float breath_get_hr_bpm(const BreathAnalyzer *ba)
{
    return (ba->result_valid && ba->present && !ba->motion) ? ba->hr_bpm : 0.0f;
}

#ifdef __cplusplus
}
#endif

#endif /* BREATH_RATE_H */

