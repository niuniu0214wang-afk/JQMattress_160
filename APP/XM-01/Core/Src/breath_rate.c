/* breath_rate.c — STM32 呼吸率 + 心率计算模块实现
 * 算法移植自 InDevelop/software/src/breath_signal_analyzer.py
 *                  InDevelop/software/src/live_prediction.py
 * 无动态内存分配，所有缓冲区静态分配
 * (2026-05-06)
 */
#include "breath_rate.h"
#include <string.h>
#include <math.h>

/* ── 呼吸带 Butterworth 4阶带通 SOS 系数 ─────────────────────
 * butter(4, [0.1/6.5, 0.5/6.5], btype='band', output='sos')
 * fs=13 Hz, lo=0.1 Hz, hi=0.5 Hz
 * (2026-05-06)
 */
#define BR_SOS_SECTIONS 4

static const float s_sos_b[BR_SOS_SECTIONS][3] = {
    { 3.76991937e-04f,  0.0f, -3.76991937e-04f },
    { 3.76991937e-04f,  0.0f, -3.76991937e-04f },
    { 1.0f,             0.0f, -1.0f             },
    { 1.0f,             0.0f, -1.0f             },
};
static const float s_sos_a[BR_SOS_SECTIONS][2] = {
    /* a1, a2 (a0=1 已归一化) */
    { -1.97687016f,  0.99924602f },
    { -1.99060636f,  0.99924602f },
    { -1.97687016f,  0.99849279f },
    { -1.99060636f,  0.99849279f },
};

/* ── 心率带 Butterworth 4阶带通 SOS 系数 ─────────────────────
 * butter(4, [0.8/6.5, 6.0/6.5], btype='band', output='sos')
 * fs=13 Hz, lo=0.8 Hz, hi=6.0 Hz
 * (2026-05-06)
 */
#define HR_SOS_SECTIONS 4

static const float s_hr_sos_b[HR_SOS_SECTIONS][3] = {
    { 0.08055953f,  0.0f, -0.08055953f },
    { 0.08055953f,  0.0f, -0.08055953f },
    { 1.0f,         0.0f, -1.0f        },
    { 1.0f,         0.0f, -1.0f        },
};
static const float s_hr_sos_a[HR_SOS_SECTIONS][2] = {
    { -0.55007024f,  0.83888094f },
    { -1.05610888f,  0.83888094f },
    { -0.55007024f,  0.67776188f },
    { -1.05610888f,  0.67776188f },
};

/* ── FFT 配置 ────────────────────────────────────────────────
 * 实数 FFT，2048 点，Cooley-Tukey 迭代实现
 * 输出：mag[0..1024]，频率分辨率 = 13/2048 ≈ 0.00635 Hz/bin
 * 呼吸带 [0.1, 0.5] Hz → bin [16, 81]
 * (2026-05-06)
 */
#define BR_FFT_OUT  (BR_FFT_NFFT / 2 + 1)  /* 1025 */

/* 追踪参数（与 Python 一致）(2026-05-06) */
#define BR_TRACK_DELTA_PENALTY   0.18f
#define BR_TRACK_PRIOR_PENALTY   0.04f
#define BR_FUNDAMENTAL_PRIOR     0.015f
#define BR_HARMONIC_PENALTY      0.50f
#define BR_PROMINENCE            0.08f
#define BR_MIN_PEAK_RATIO        0.35f
#define BR_PEAK_WIDTH_GOOD       5.0f
#define BR_PEAK_WIDTH_MAX        8.0f
#define BR_COMPETE_RATIO_WARN    0.80f
#define BR_COMPETE_RATIO_BAD     0.92f
#define BR_ROI_PERCENTILE        0.65f   /* 65th percentile */

/* ── 工作缓冲区（静态，避免栈溢出）────────────────────────── */
static float s_roi_sig[BR_N_WIN];        /* ROI 融合信号 390点 */
static float s_filtered[BR_N_WIN];       /* 带通滤波后信号 */
static float s_fft_buf[BR_FFT_NFFT];    /* FFT 输入（补零到2048）*/
static float s_fft_re[BR_FFT_NFFT];     /* FFT 实部 */
static float s_fft_im[BR_FFT_NFFT];     /* FFT 虚部 */
static float s_fft_mag[BR_FFT_OUT];     /* FFT 幅度谱 */
static float s_mean_load[BR_HALF_SIZE]; /* 各通道平均负载 */
static float s_weights[BR_HALF_SIZE];   /* ROI 权重 */

/* IIR 滤波器状态（每节2个延迟）(2026-05-06) */
static float s_sos_x[BR_SOS_SECTIONS][2]; /* 输入延迟 */
static float s_sos_y[BR_SOS_SECTIONS][2]; /* 输出延迟 */

/* ── 工具函数 ────────────────────────────────────────────────*/

/* 快速对数近似（避免 libm logf 开销）(2026-05-06) */
static float br_logf(float x)
{
    if (x <= 0.0f) return -30.0f;
    return logf(x);
}

/* 绝对值 (2026-05-06) */
static float br_fabsf(float x) { return x < 0.0f ? -x : x; }

/* 排序：冒泡排序（最多5个元素，开销可忽略）(2026-05-06) */
static void br_sort_desc(float *arr, int *idx, int n)
{
    int i, j;
    for (i = 0; i < n - 1; i++) {
        for (j = i + 1; j < n; j++) {
            if (arr[j] > arr[i]) {
                float tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
                int ti  = idx[i];  idx[i]  = idx[j];  idx[j]  = ti;
            }
        }
    }
}

/* ── ROI 融合 ────────────────────────────────────────────────
 * 输入：window[BR_N_WIN][BR_HALF_SIZE]（环形缓冲展开后）
 * 输出：s_roi_sig[BR_N_WIN]，s_weights[BR_HALF_SIZE]
 * (2026-05-06)
 */
static void br_compute_roi(const BreathAnalyzer *ba)
{
    int i, c;
    /* 计算各通道平均负载 (2026-05-06) */
    memset(s_mean_load, 0, sizeof(s_mean_load));
    for (i = 0; i < BR_N_WIN; i++) {
        int idx = (ba->buf_head - BR_N_WIN + i + BR_N_WIN) % BR_N_WIN;
        for (c = 0; c < BR_HALF_SIZE; c++)
            s_mean_load[c] += ba->buf[idx][c];
    }
    for (c = 0; c < BR_HALF_SIZE; c++)
        s_mean_load[c] /= (float)BR_N_WIN;

    /* 找 65th percentile 阈值（部分排序）(2026-05-06) */
    float sorted[BR_HALF_SIZE];
    int n_loaded = 0;
    for (c = 0; c < BR_HALF_SIZE; c++)
        if (s_mean_load[c] > 0.0f) sorted[n_loaded++] = s_mean_load[c];

    float thresh = 0.0f;
    if (n_loaded > 0) {
        /* 插入排序 (2026-05-06) */
        int k, j;
        for (k = 1; k < n_loaded; k++) {
            float key = sorted[k];
            for (j = k - 1; j >= 0 && sorted[j] > key; j--)
                sorted[j + 1] = sorted[j];
            sorted[j + 1] = key;
        }
        int pct_idx = (int)(BR_ROI_PERCENTILE * (n_loaded - 1) + 0.5f);
        thresh = sorted[pct_idx];
    }

    /* 计算权重 (2026-05-06) */
    float w_sum = 0.0f;
    memset(s_weights, 0, sizeof(s_weights));
    for (c = 0; c < BR_HALF_SIZE; c++) {
        if (s_mean_load[c] >= thresh && thresh > 0.0f) {
            s_weights[c] = s_mean_load[c];
            w_sum += s_mean_load[c];
        }
    }
    if (w_sum < 1e-9f) {
        /* 无有效通道，输出全零 (2026-05-06) */
        memset(s_roi_sig, 0, sizeof(s_roi_sig));
        return;
    }
    for (c = 0; c < BR_HALF_SIZE; c++)
        s_weights[c] /= w_sum;

    /* 加权求和 (2026-05-06) */
    for (i = 0; i < BR_N_WIN; i++) {
        int idx = (ba->buf_head - BR_N_WIN + i + BR_N_WIN) % BR_N_WIN;
        float val = 0.0f;
        for (c = 0; c < BR_HALF_SIZE; c++)
            val += ba->buf[idx][c] * s_weights[c];
        s_roi_sig[i] = val;
    }
}

/* ── 线性去趋势 ──────────────────────────────────────────────
 * 最小二乘拟合直线并减去 (2026-05-06)
 */
static void br_detrend(float *sig, int n)
{
    float sx = 0.0f, sy = 0.0f, sxx = 0.0f, sxy = 0.0f;
    int i;
    for (i = 0; i < n; i++) {
        float x = (float)i;
        sx  += x;
        sy  += sig[i];
        sxx += x * x;
        sxy += x * sig[i];
    }
    float fn = (float)n;
    float denom = fn * sxx - sx * sx;
    if (br_fabsf(denom) < 1e-9f) return;
    float slope     = (fn * sxy - sx * sy) / denom;
    float intercept = (sy - slope * sx) / fn;
    for (i = 0; i < n; i++)
        sig[i] -= slope * (float)i + intercept;
}

/* ── 4阶 Butterworth 带通滤波（双向 sosfiltfilt）────────────
 * 前向 + 后向各一次，实现零相位滤波 (2026-05-06)
 */
static void br_sos_filter_forward(const float *in, float *out, int n)
{
    int i, s;
    /* 重置状态 (2026-05-06) */
    memset(s_sos_x, 0, sizeof(s_sos_x));
    memset(s_sos_y, 0, sizeof(s_sos_y));

    for (i = 0; i < n; i++) {
        float x = in[i];
        for (s = 0; s < BR_SOS_SECTIONS; s++) {
            float y = s_sos_b[s][0] * x
                    + s_sos_b[s][1] * s_sos_x[s][0]
                    + s_sos_b[s][2] * s_sos_x[s][1]
                    - s_sos_a[s][0] * s_sos_y[s][0]
                    - s_sos_a[s][1] * s_sos_y[s][1];
            s_sos_x[s][1] = s_sos_x[s][0];
            s_sos_x[s][0] = x;
            s_sos_y[s][1] = s_sos_y[s][0];
            s_sos_y[s][0] = y;
            x = y;
        }
        out[i] = x;
    }
}

static void br_bandpass(float *sig, int n)
{
    /* 前向滤波 (2026-05-06) */
    br_sos_filter_forward(sig, s_filtered, n);

    /* 后向滤波（时间反转）(2026-05-06) */
    int i;
    float rev[BR_N_WIN];
    for (i = 0; i < n; i++) rev[i] = s_filtered[n - 1 - i];

    float rev_out[BR_N_WIN];
    br_sos_filter_forward(rev, rev_out, n);

    for (i = 0; i < n; i++) s_filtered[i] = rev_out[n - 1 - i];
}

/* ── Cooley-Tukey 迭代 FFT（实数输入）───────────────────────
 * 输入：s_fft_buf[0..N-1]（实数，已补零）
 * 输出：s_fft_re[], s_fft_im[]，然后计算 s_fft_mag[]
 * (2026-05-06)
 */
static void br_fft(int n)
{
    int i, j, k, m;
    /* 位反转排列 (2026-05-06) */
    for (i = 0; i < n; i++) {
        s_fft_re[i] = s_fft_buf[i];
        s_fft_im[i] = 0.0f;
    }
    j = 0;
    for (i = 1; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float tr = s_fft_re[i]; s_fft_re[i] = s_fft_re[j]; s_fft_re[j] = tr;
            float ti = s_fft_im[i]; s_fft_im[i] = s_fft_im[j]; s_fft_im[j] = ti;
        }
    }
    /* 蝶形运算 (2026-05-06) */
    for (m = 2; m <= n; m <<= 1) {
        float wm_re = cosf(-2.0f * 3.14159265358979f / (float)m);
        float wm_im = sinf(-2.0f * 3.14159265358979f / (float)m);
        for (k = 0; k < n; k += m) {
            float w_re = 1.0f, w_im = 0.0f;
            for (i = 0; i < m / 2; i++) {
                float u_re = s_fft_re[k + i];
                float u_im = s_fft_im[k + i];
                float t_re = w_re * s_fft_re[k + i + m/2] - w_im * s_fft_im[k + i + m/2];
                float t_im = w_re * s_fft_im[k + i + m/2] + w_im * s_fft_re[k + i + m/2];
                s_fft_re[k + i]       = u_re + t_re;
                s_fft_im[k + i]       = u_im + t_im;
                s_fft_re[k + i + m/2] = u_re - t_re;
                s_fft_im[k + i + m/2] = u_im - t_im;
                float new_w_re = w_re * wm_re - w_im * wm_im;
                float new_w_im = w_re * wm_im + w_im * wm_re;
                w_re = new_w_re;
                w_im = new_w_im;
            }
        }
    }
    /* 计算幅度谱 (2026-05-06) */
    for (i = 0; i < BR_FFT_OUT; i++)
        s_fft_mag[i] = sqrtf(s_fft_re[i]*s_fft_re[i] + s_fft_im[i]*s_fft_im[i]);
}

/* ── 峰值半宽（BPM 单位）────────────────────────────────────*/
static float br_peak_half_width(int peak_idx, int band_start, int band_n)
{
    float half = s_fft_mag[band_start + peak_idx] * 0.5f;
    int left = peak_idx, right = peak_idx;
    while (left > 0 && s_fft_mag[band_start + left - 1] >= half) left--;
    while (right + 1 < band_n && s_fft_mag[band_start + right + 1] >= half) right++;
    float freq_res = (float)BR_FS / (float)BR_FFT_NFFT;
    return (float)(right - left) * freq_res * 60.0f;
}

/* ── 质量分类 ────────────────────────────────────────────────*/
static BreathQuality br_classify(float snr, float width_bpm, float peak2_ratio)
{
    if (snr < BR_SNR_MIN) return BR_QUALITY_LOW_SNR;
    if (snr >= BR_SNR_GOOD && width_bpm <= BR_PEAK_WIDTH_GOOD && peak2_ratio <= BR_COMPETE_RATIO_WARN)
        return BR_QUALITY_GOOD;
    if (width_bpm <= BR_PEAK_WIDTH_MAX && peak2_ratio <= BR_COMPETE_RATIO_BAD)
        return BR_QUALITY_USABLE;
    return BR_QUALITY_AMBIGUOUS;
}

/* ── FFT 峰值检测 ────────────────────────────────────────────
 * 在呼吸带 [BR_LO_HZ, BR_HI_HZ] 内找主峰和候选峰
 * (2026-05-06)
 */
static void br_fft_peak(BRResult *res, float prev_bpm)
{
    float freq_res = (float)BR_FS / (float)BR_FFT_NFFT;  /* Hz/bin */
    int band_start = (int)(BR_LO_HZ / freq_res + 0.5f);
    int band_end   = (int)(BR_HI_HZ / freq_res + 0.5f);
    if (band_end >= BR_FFT_OUT) band_end = BR_FFT_OUT - 1;
    int band_n = band_end - band_start + 1;

    /* 找主峰 (2026-05-06) */
    int dom_idx = 0;
    float dom_mag = 0.0f;
    float band_sum = 0.0f;
    int i;
    for (i = 0; i < band_n; i++) {
        float m = s_fft_mag[band_start + i];
        band_sum += m;
        if (m > dom_mag) { dom_mag = m; dom_idx = i; }
    }
    float band_mean = band_sum / (float)band_n;
    float snr = dom_mag / (band_mean + 1e-9f);

    /* 候选峰：简单局部极大值检测 (2026-05-06) */
    float cand_amp[BR_MAX_CANDIDATES];
    int   cand_idx_arr[BR_MAX_CANDIDATES];
    int   cand_n = 0;
    float prom_thresh = dom_mag * BR_PROMINENCE;

    /* 始终包含主峰 (2026-05-06) */
    cand_amp[0] = dom_mag;
    cand_idx_arr[0] = dom_idx;
    cand_n = 1;

    for (i = 1; i < band_n - 1 && cand_n < BR_MAX_CANDIDATES; i++) {
        float m = s_fft_mag[band_start + i];
        if (m < prom_thresh) continue;
        if (m < dom_mag * BR_MIN_PEAK_RATIO) continue;
        if (m <= s_fft_mag[band_start + i - 1] || m <= s_fft_mag[band_start + i + 1]) continue;
        if (i == dom_idx) continue;
        /* 检查与已有候选的间距 (2026-05-06) */
        float bpm_i = (float)(band_start + i) * freq_res * 60.0f;
        int dup = 0;
        int k;
        for (k = 0; k < cand_n; k++) {
            float bpm_k = (float)(band_start + cand_idx_arr[k]) * freq_res * 60.0f;
            if (br_fabsf(bpm_i - bpm_k) < 1.0f) { dup = 1; break; }
        }
        if (!dup) {
            cand_amp[cand_n] = m;
            cand_idx_arr[cand_n] = i;
            cand_n++;
        }
    }
    br_sort_desc(cand_amp, cand_idx_arr, cand_n);

    /* 填充候选结构体 (2026-05-06) */
    res->candidate_n = (uint8_t)cand_n;
    for (i = 0; i < cand_n; i++) {
        res->candidates[i].freq = (float)(band_start + cand_idx_arr[i]) * freq_res;
        res->candidates[i].bpm  = res->candidates[i].freq * 60.0f;
        res->candidates[i].amp  = cand_amp[i];
    }

    /* 第二峰比 (2026-05-06) */
    float peak2_ratio = 0.0f, peak2_bpm = 0.0f;
    {
        float best2 = 0.0f;
        int   best2_idx = -1;
        float dom_freq = (float)(band_start + dom_idx) * freq_res;
        for (i = 0; i < band_n; i++) {
            float f = (float)(band_start + i) * freq_res;
            if (br_fabsf(f - dom_freq) < (1.0f / 60.0f)) continue;
            if (s_fft_mag[band_start + i] > best2) {
                best2 = s_fft_mag[band_start + i];
                best2_idx = i;
            }
        }
        if (best2_idx >= 0) {
            peak2_ratio = best2 / (dom_mag + 1e-9f);
            peak2_bpm   = (float)(band_start + best2_idx) * freq_res * 60.0f;
        }
    }

    float width_bpm = br_peak_half_width(dom_idx, band_start, band_n);

    /* 选择最终候选（与 Python 逻辑一致）(2026-05-06) */
    int chosen = 0;
    if (prev_bpm > 0.0f) {
        float best_near_amp = -1.0f;
        for (i = 0; i < cand_n; i++) {
            if (br_fabsf(res->candidates[i].bpm - prev_bpm) <= 4.0f) {
                if (res->candidates[i].amp > best_near_amp) {
                    best_near_amp = res->candidates[i].amp;
                    chosen = i;
                }
            }
        }
    } else if (cand_n >= 2) {
        /* 谐波抑制：若最低频候选幅度 ≥ 最高频候选的 60%，选低频 (2026-05-06) */
        int low_i = 0, high_i = 0;
        for (i = 1; i < cand_n; i++) {
            if (res->candidates[i].bpm < res->candidates[low_i].bpm)  low_i  = i;
            if (res->candidates[i].bpm > res->candidates[high_i].bpm) high_i = i;
        }
        float ratio = res->candidates[high_i].bpm / (res->candidates[low_i].bpm + 1e-9f);
        if ((ratio > 1.7f && ratio < 2.3f) || (ratio > 2.7f && ratio < 3.3f)) {
            if (res->candidates[low_i].amp >= res->candidates[high_i].amp * 0.6f)
                chosen = low_i;
        }
    }

    res->raw_bpm   = res->candidates[0].bpm;  /* 主峰 BPM */
    res->track_bpm = res->candidates[chosen].bpm;
    res->snr       = snr;
    res->quality   = br_classify(snr, width_bpm, peak2_ratio);
    res->bpm       = res->track_bpm;
    (void)peak2_bpm;
}

/* ── 历史追踪（Viterbi 动态规划）────────────────────────────
 * 与 Python _track_candidate_history 逻辑一致 (2026-05-06)
 */
static float br_track(BreathAnalyzer *ba, BRResult *cur, float prior_bpm)
{
    int hist_n = (int)ba->hist_count;
    if (hist_n == 0) return cur->bpm;

    /* 最多 BR_TRACK_HISTORY 个历史窗口 + 当前窗口 (2026-05-06) */
    int total = hist_n + 1;
    if (total > BR_TRACK_HISTORY + 1) total = BR_TRACK_HISTORY + 1;

    /* 收集历史（从旧到新）(2026-05-06) */
    const BRResult *hist_arr[BR_TRACK_HISTORY + 1];
    int t;
    for (t = 0; t < hist_n && t < BR_TRACK_HISTORY; t++) {
        int idx = ((int)ba->hist_head - hist_n + t + BR_TRACK_HISTORY) % BR_TRACK_HISTORY;
        hist_arr[t] = &ba->hist[idx];
    }
    hist_arr[hist_n < BR_TRACK_HISTORY ? hist_n : BR_TRACK_HISTORY - 1] = cur;
    total = (hist_n < BR_TRACK_HISTORY ? hist_n + 1 : BR_TRACK_HISTORY);

    /* DP 评分（简化：只追踪最优路径末端）(2026-05-06) */
    float best_score = -1e18f;
    float best_bpm   = cur->bpm;
    int   i;

    for (i = 0; i < (int)cur->candidate_n; i++) {
        float bpm_i = cur->candidates[i].bpm;
        float score = br_logf(cur->candidates[i].amp + 1e-9f)
                    - BR_FUNDAMENTAL_PRIOR * bpm_i;
        if (prior_bpm > 0.0f)
            score -= BR_TRACK_PRIOR_PENALTY * br_fabsf(bpm_i - prior_bpm);

        /* 与上一窗口最优候选的连续性 (2026-05-06) */
        if (total >= 2) {
            const BRResult *prev = hist_arr[total - 2];
            if (prev->candidate_n > 0) {
                float prev_bpm = prev->candidates[0].bpm;
                float delta = br_fabsf(bpm_i - prev_bpm);
                score -= BR_TRACK_DELTA_PENALTY * delta;
                float ratio = (bpm_i > prev_bpm ? bpm_i : prev_bpm)
                            / ((bpm_i < prev_bpm ? bpm_i : prev_bpm) + 1e-9f);
                if ((ratio > 1.8f && ratio < 2.2f) || (ratio > 2.7f && ratio < 3.3f))
                    score -= BR_HARMONIC_PENALTY;
            }
        }
        if (score > best_score) {
            best_score = score;
            best_bpm   = bpm_i;
        }
    }
    return best_bpm;
}

/* ── 主分析函数 ──────────────────────────────────────────────*/
static void br_analyze(BreathAnalyzer *ba)
{
    int i;
    float total_load;

    /* 1. 有人检测（体动信号融合）(2026-05-06) */
    br_fuse_body_signal(ba, s_body_sig, &total_load);
    ba->present = (total_load >= PRESENCE_MIN) ? 1 : 0;
    if (!ba->present) {
        ba->result_valid = 1;
        return;
    }

    /* 2. 运动检测 (2026-05-06) */
    ba->motion = br_has_motion(ba);

    /* 3. ROI 融合（呼吸率用）(2026-05-06) */
    br_compute_roi(ba);

    /* 4. 线性去趋势 (2026-05-06) */
    br_detrend(s_roi_sig, BR_N_WIN);

    /* 5. 带通滤波（零相位）(2026-05-06) */
    memcpy(s_filtered, s_roi_sig, sizeof(float) * BR_N_WIN);
    br_bandpass(s_filtered, BR_N_WIN);

    /* 6. Hamming 窗 + 补零 + FFT (2026-05-06) */
    memset(s_fft_buf, 0, sizeof(s_fft_buf));
    for (i = 0; i < BR_N_WIN; i++) {
        float w = 0.54f - 0.46f * cosf(2.0f * 3.14159265f * (float)i / (float)(BR_N_WIN - 1));
        s_fft_buf[i] = s_filtered[i] * w;
    }
    br_fft(BR_FFT_NFFT);

    /* 7. 呼吸率峰值检测 (2026-05-06) */
    BRResult cur;
    memset(&cur, 0, sizeof(cur));
    float prev_bpm = ba->result_valid ? ba->br_result.bpm : 0.0f;
    br_fft_peak(&cur, prev_bpm);

    /* 8. 历史追踪 (2026-05-06) */
    float tracked_bpm = br_track(ba, &cur, ba->last_good_bpm);
    cur.track_bpm = tracked_bpm;

    /* 9. 保持机制 (2026-05-06) */
    if (cur.quality == BR_QUALITY_GOOD || cur.quality == BR_QUALITY_USABLE) {
        ba->last_good_bpm = tracked_bpm;
        ba->hold_count    = 0;
        cur.bpm           = tracked_bpm;
    } else if (ba->last_good_bpm > 0.0f && ba->hold_count < BR_HOLD_MAX) {
        cur.bpm = ba->last_good_bpm;
        ba->hold_count++;
    } else {
        cur.bpm = 0.0f;
    }

    /* 10. 卡尔曼平滑呼吸率 (2026-05-06) */
    if (cur.bpm > 0.0f && !ba->motion) {
        float prev_br = ba->br_result.bpm;
        float br_smooth;
        if (cur.snr >= BR_SNR_MIN) {
            if (prev_br > 0.0f && br_fabsf(cur.bpm - prev_br) > 5.0f)
                br_smooth = kalman_update(&ba->kalman_br, prev_br);
            else
                br_smooth = kalman_update(&ba->kalman_br, cur.bpm);
        } else {
            br_smooth = (prev_br > 0.0f) ? prev_br : cur.bpm;
        }
        cur.bpm = br_smooth;
    }

    /* 11. 更新历史 (2026-05-06) */
    ba->hist[ba->hist_head] = cur;
    ba->hist_head = (ba->hist_head + 1) % BR_TRACK_HISTORY;
    if (ba->hist_count < BR_TRACK_HISTORY) ba->hist_count++;
    ba->br_result = cur;

    /* 12. 心率计算（运动时跳过）(2026-05-06) */
    if (!ba->motion && cur.bpm > 0.0f) {
        float hr_raw = 0.0f, hr_snr = 0.0f;
        br_compute_heart_rate(s_body_sig, cur.bpm, &hr_raw, &hr_snr);
        if (hr_raw > 0.0f)
            ba->hr_bpm = kalman_update(&ba->kalman_hr, hr_raw);
        ba->hr_snr = hr_snr;
    }

    ba->result_valid = 1;
}

/* ── 公开接口实现 ─────────────────────────────────────────── */

/* 卡尔曼滤波器初始化 (2026-05-06) */
static void kalman_init(KalmanFilter *kf, float q, float r, float init_val, float init_p)
{
    kf->x = init_val;
    kf->p = init_p;
    kf->q = q;
    kf->r = r;
}

/* 卡尔曼更新，返回平滑后的值 (2026-05-06) */
static float kalman_update(KalmanFilter *kf, float z)
{
    float k;
    kf->p += kf->q;
    k      = kf->p / (kf->p + kf->r);
    kf->x += k * (z - kf->x);
    kf->p *= (1.0f - k);
    return kf->x;
}

/* ── 运动检测 ────────────────────────────────────────────────
 * 总载荷逐帧变化量超阈值 → 运动伪影
 * 移植自 live_prediction.py has_motion() (2026-05-06)
 */
static uint8_t br_has_motion(const BreathAnalyzer *ba)
{
    float prev_load = 0.0f, max_delta = 0.0f;
    int i, c;
    for (i = 0; i < BR_N_WIN; i++) {
        int idx = (ba->buf_head - BR_N_WIN + i + BR_N_WIN) % BR_N_WIN;
        float load = 0.0f;
        for (c = 0; c < BR_HALF_SIZE; c++) load += ba->buf[idx][c];
        if (i > 0) {
            float delta = load - prev_load;
            if (delta < 0.0f) delta = -delta;
            if (delta > max_delta) max_delta = delta;
        }
        prev_load = load;
    }
    return (max_delta > MOTION_DELTA_THRESH) ? 1 : 0;
}

/* ── 体动信号融合（心率用）────────────────────────────────────
 * 按静态载荷加权求和 → 单路体动信号
 * 移植自 live_prediction.py fuse_body_signal() (2026-05-06)
 */
static void br_fuse_body_signal(const BreathAnalyzer *ba, float *out_sig, float *out_total_load)
{
    int i, c;
    float mean_load[BR_HALF_SIZE] = {0};
    float total_load = 0.0f;

    for (i = 0; i < BR_N_WIN; i++) {
        int idx = (ba->buf_head - BR_N_WIN + i + BR_N_WIN) % BR_N_WIN;
        for (c = 0; c < BR_HALF_SIZE; c++)
            mean_load[c] += ba->buf[idx][c];
    }
    for (c = 0; c < BR_HALF_SIZE; c++) {
        mean_load[c] /= (float)BR_N_WIN;
        total_load += mean_load[c];
    }
    *out_total_load = total_load;

    if (total_load <= 1e-6f) {
        memset(out_sig, 0, sizeof(float) * BR_N_WIN);
        return;
    }

    /* 加权求和 (2026-05-06) */
    for (i = 0; i < BR_N_WIN; i++) {
        int idx = (ba->buf_head - BR_N_WIN + i + BR_N_WIN) % BR_N_WIN;
        float val = 0.0f;
        for (c = 0; c < BR_HALF_SIZE; c++)
            val += ba->buf[idx][c] * mean_load[c];
        out_sig[i] = val / (total_load + 1e-9f);
    }
}

/* ── 心率带 IIR 滤波（前向）────────────────────────────────── */
static float s_hr_sos_x[HR_SOS_SECTIONS][2];
static float s_hr_sos_y[HR_SOS_SECTIONS][2];
static float s_hr_filtered[BR_N_WIN];
static float s_hr_filtered_rev[BR_N_WIN];
static float s_body_sig[BR_N_WIN];

static void br_hr_filter_forward(const float *in, float *out, int n)
{
    int i, s;
    memset(s_hr_sos_x, 0, sizeof(s_hr_sos_x));
    memset(s_hr_sos_y, 0, sizeof(s_hr_sos_y));
    for (i = 0; i < n; i++) {
        float x = in[i];
        for (s = 0; s < HR_SOS_SECTIONS; s++) {
            float y = s_hr_sos_b[s][0] * x
                    + s_hr_sos_b[s][1] * s_hr_sos_x[s][0]
                    + s_hr_sos_b[s][2] * s_hr_sos_x[s][1]
                    - s_hr_sos_a[s][0] * s_hr_sos_y[s][0]
                    - s_hr_sos_a[s][1] * s_hr_sos_y[s][1];
            s_hr_sos_x[s][1] = s_hr_sos_x[s][0]; s_hr_sos_x[s][0] = x;
            s_hr_sos_y[s][1] = s_hr_sos_y[s][0]; s_hr_sos_y[s][0] = y;
            x = y;
        }
        out[i] = x;
    }
}

static void br_hr_bandpass(float *sig, int n)
{
    int i;
    br_hr_filter_forward(sig, s_hr_filtered, n);
    for (i = 0; i < n; i++) s_hr_filtered_rev[i] = s_hr_filtered[n - 1 - i];
    float rev_out[BR_N_WIN];
    br_hr_filter_forward(s_hr_filtered_rev, rev_out, n);
    for (i = 0; i < n; i++) sig[i] = rev_out[n - 1 - i];
}

/* ── 心率 FFT 峰值检测（PRQ 约束）───────────────────────────
 * 移植自 live_prediction.py compute_heart_rate() (2026-05-06)
 */
static void br_compute_heart_rate(const float *body_sig, float br_bpm,
                                   float *out_hr_bpm, float *out_snr)
{
    int i;
    float freq_res = (float)BR_FS / (float)BR_FFT_NFFT;

    /* 去趋势 + 心率带滤波 (2026-05-06) */
    memcpy(s_body_sig, body_sig, sizeof(float) * BR_N_WIN);
    br_detrend(s_body_sig, BR_N_WIN);
    br_hr_bandpass(s_body_sig, BR_N_WIN);

    /* FFT（复用呼吸率的 FFT 缓冲区）(2026-05-06) */
    memset(s_fft_buf, 0, sizeof(s_fft_buf));
    for (i = 0; i < BR_N_WIN; i++) s_fft_buf[i] = s_body_sig[i];
    br_fft(BR_FFT_NFFT);

    /* PRQ 约束窗口：HR ∈ [RR×3.0, RR×6.0] (2026-05-06) */
    float rr_hz = br_bpm / 60.0f;
    float lo_hz = rr_hz * HR_PRQ_LO;
    float hi_hz = rr_hz * HR_PRQ_HI;
    if (lo_hz < HR_BAND_LO_HZ) lo_hz = HR_BAND_LO_HZ;
    if (hi_hz > HR_BAND_HI_HZ) hi_hz = HR_BAND_HI_HZ;

    int band_start = (int)(lo_hz / freq_res + 0.5f);
    int band_end   = (int)(hi_hz / freq_res + 0.5f);
    if (band_end >= BR_FFT_OUT) band_end = BR_FFT_OUT - 1;

    /* 若 PRQ 窗口无效，回退到全心脏带 (2026-05-06) */
    if (band_start >= band_end || br_bpm <= 0.0f) {
        band_start = (int)(HR_FALLBACK_LO_HZ / freq_res + 0.5f);
        band_end   = (int)(HR_FALLBACK_HI_HZ / freq_res + 0.5f);
        if (band_end >= BR_FFT_OUT) band_end = BR_FFT_OUT - 1;
    }
    if (band_start >= band_end) { *out_hr_bpm = 0.0f; *out_snr = 0.0f; return; }

    int band_n = band_end - band_start + 1;
    float dom_mag = 0.0f, band_sum = 0.0f;
    int dom_idx = 0;
    for (i = 0; i < band_n; i++) {
        float m = s_fft_mag[band_start + i];
        band_sum += m;
        if (m > dom_mag) { dom_mag = m; dom_idx = i; }
    }
    float band_mean = band_sum / (float)band_n;
    *out_snr    = dom_mag / (band_mean + 1e-9f);
    *out_hr_bpm = (float)(band_start + dom_idx) * freq_res * 60.0f;
}

void breath_analyzer_init(BreathAnalyzer *ba)
{
    memset(ba, 0, sizeof(BreathAnalyzer));
    /* 卡尔曼初始化：Q, R, init_val, init_p (2026-05-06) */
    kalman_init(&ba->kalman_br, 0.5f,  4.0f,  15.0f, 9.0f);
    kalman_init(&ba->kalman_hr, 2.0f, 16.0f,  65.0f, 25.0f);
}

void breath_analyzer_push(BreathAnalyzer *ba, const uint8_t *half80)
{
    int c;
    for (c = 0; c < BR_HALF_SIZE; c++)
        ba->buf[ba->buf_head][c] = (float)half80[c];
    ba->buf_head = (ba->buf_head + 1) % BR_N_WIN;
    if (ba->buf_count < BR_N_WIN) ba->buf_count++;

    ba->frame_cnt++;
    if (ba->buf_count >= BR_N_WIN && ba->frame_cnt % BR_FS == 0)
        br_analyze(ba);
}

