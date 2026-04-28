
#include "myEdge_ai_app.h"
#include "ai_model_wrapper.h"  // X-CUBE-AI睡姿分类模型包装 (2026-03-03)
#include "jq260_hybrid_lr_model.h"  // Hybrid LR person-count + double-posture model (2026-04-28)
#include <stdint.h>
#include <math.h>

#define AI_THREAD_STACK_SIZE (1024 * 16)

/* === 自适应基线校准 + 时间投票法参数定义 (2026-03-23) === */
#define PERSON_CALIB_FRAMES    30   /* 启动校准帧数 (约2秒@14Hz，期间床垫应为空) */
#define PERSON_DETECT_MARGIN   4    /* 检测阈值 = 基线均值 + 此余量 (0-255量程) */
#define PERSON_COUNT_THRESHOLD 20   /* 半侧最小激活格数 (130格中约15%) */
#define PERSON_VOTE_WINDOW     7    /* 时间投票窗口帧数 (7帧=500ms@14Hz) */
#define PERSON_VOTE_THRESHOLD  5    /* 判定有人所需最小票数 (5/7多数决) */
#define SENSOR_HALF_SIZE_260   130  /* 半侧格数 (13×10) */

/* 睡姿EMA平滑参数 - 5帧窗口，行均值变化触发 (2026-04-07) */
#define POSTURE_EMA_ALPHA        0.33f  /* ≈2/(5+1)，5帧有效窗口 (2026-04-07) */
#define POSTURE_STABLE_THRESHOLD 5      /* 5帧连续一致才提交 (2026-04-07) */
#define POSTURE_CHANGE_THRESH    6      /* 行均值变化触发阈值 (0-255量程) (2026-04-07) */
#define CHANGE_EMA_ALPHA         0.1f   /* 基线EMA系数，10帧窗口 (2026-04-07) */

/* Run a few extra frames after a row-delta trigger so 7-frame presence voting can settle. */
#define CHANGE_TRIGGER_HOLD_FRAMES PERSON_VOTE_WINDOW

static struct rt_thread ai_thread;
static rt_uint8_t ai_thread_stack[AI_THREAD_STACK_SIZE];

static uint8_t s_person_count;
static uint8_t s_posture_0  = 0xFF;
static uint8_t s_waist_x_0  = 0xFF;
static uint8_t s_waist_y_0  = 0xFF;
static uint8_t s_posture_1  = 0xFF;
static uint8_t s_waist_x_1  = 0xFF;
static uint8_t s_waist_y_1  = 0xFF;

/* 自适应基线校准状态 (2026-03-23) */
static uint8_t  s_calib_done      = 0;
static uint8_t  s_calib_count     = 0;
static uint32_t s_calib_sum_upper = 0;
static uint32_t s_calib_sum_lower = 0;
static uint16_t s_threshold_upper = 10;  /* 上半均值阈值 (校准前使用保守默认值) */
static uint16_t s_threshold_lower = 10;  /* 下半均值阈值 (校准前使用保守默认值) */
/* 行峰值校准 - 参考JQ704列峰值校准思路，适配上下分割 (2026-04-07) */
static uint32_t s_calib_row_upper = 0;   /* 校准期间上半行峰值累加 */
static uint32_t s_calib_row_lower = 0;   /* 校准期间下半行峰值累加 */
static uint16_t s_threshold_row_upper = 5;  /* 上半行峰值阈值 */
static uint16_t s_threshold_row_lower = 5;  /* 下半行峰值阈值 */

/* 人数识别时间投票缓冲区 (2026-03-23) */
static uint8_t s_vote_buf_upper[PERSON_VOTE_WINDOW];
static uint8_t s_vote_buf_lower[PERSON_VOTE_WINDOW];
static uint8_t s_vote_index = 0;

/* 睡姿EMA平滑状态 - 跨帧持久化，参考JQ704实现 (2026-04-07) */
/* 二分类：ema[0]=仰卧概率, ema[1]=侧卧概率 */
static float   s_posture_ema_0[2]     = {0.0f, 0.0f};  /* 上半/单人EMA概率 */
static float   s_posture_ema_1[2]     = {0.0f, 0.0f};  /* 下半EMA概率 */
static uint8_t s_posture_pending_0    = 0xFF;  /* 上半待确认姿势argmax */
static uint8_t s_posture_pending_1    = 0xFF;  /* 下半待确认姿势argmax */
static uint8_t s_posture_stable_cnt_0 = 0;     /* 上半连续稳定帧计数 */
static uint8_t s_posture_stable_cnt_1 = 0;     /* 下半连续稳定帧计数 */
static uint8_t s_prev_upper_person    = 0;     /* 上帧上半有人标志 */
static uint8_t s_prev_lower_person    = 0;     /* 上帧下半有人标志 */

/* 行均值EMA基线 - 用于睡姿变化检测 (2026-04-07) */
static float   s_row_ema_upper[13]    = {0};   /* 上半13行均值EMA基线 */
static float   s_row_ema_lower[13]    = {0};   /* 下半13行均值EMA基线 */
static uint8_t s_change_init_cnt      = 0;     /* 基线初始化帧计数 */
static uint8_t s_change_ema_ready     = 0;     /* 基线EMA初始化完成标志 */
static uint8_t s_active_upper         = 0;     /* 未使用，保留 (2026-04-07) */
static uint8_t s_active_lower         = 0;     /* 未使用，保留 (2026-04-07) */

static uint8_t s_active_hold_upper    = 0;
static uint8_t s_active_hold_lower    = 0;

uint8_t g_person_count = 0;
uint8_t g_posture_0 = 0xFF;  /* 初始值0xFF表示无人 (2026-04-07) */
uint8_t g_waist_x_0 = 0xFF;
uint8_t g_waist_y_0 = 0xFF;
uint8_t g_posture_1 = 0xFF;  /* 初始值0xFF表示无人 (2026-04-07) */
uint8_t g_waist_x_1 = 0xFF;
uint8_t g_waist_y_1 = 0xFF;

model_t model;

/* 计算260个元素的和 - 2025-11-04 */
uint32_t calc_260_sum(const uint8_t *data)
{
    uint32_t sum = 0;

    // 求260个元素的和
    for (uint16_t i = 0; i < 260; i++)
    {
        sum += data[i];
    }

    return sum;
}

/* 计算260个元素中非零值的个数 - 2025-11-04 */
uint16_t sensor_number(const uint8_t *data)
{
    uint16_t count = 0;

    // 统计非零元素的个数
    for (uint16_t i = 0; i < 260; i++)
    {
        if (data[i] != 0)
        {
            count++;
        }
    }

    return count;
}

/**
 * @brief 计算质心(Center of Mass) - 基于整数类型的传感器数据
 * @description 使用加权平均法计算26x10矩阵的质心坐标
 * @param matrix 指向260字节传感器数据的指针(uint8_t)
 * @return CoM_t 结构体，包含行列坐标(float)
 */
CoM_t calculate_com(const uint8_t *matrix)
{
    CoM_t com = {0.0f, 0.0f};
    uint32_t total_pressure = 0;
    float weighted_row_sum = 0.0f;
    float weighted_col_sum = 0.0f;

    // 计算加权行列和
    for (int r = 0; r < SENSOR_ROWS; r++)
    {
        for (int c = 0; c < SENSOR_COLS; c++)
        {
            uint8_t pressure = matrix[r * SENSOR_COLS + c];  // 整数类型
            total_pressure += pressure;
            weighted_row_sum += (float)pressure * r;
            weighted_col_sum += (float)pressure * c;
        }
    }

    // 计算质心坐标
    if (total_pressure > 0)
    {
        com.row = weighted_row_sum / (float)total_pressure;
        com.col = weighted_col_sum / (float)total_pressure;
    }
    else
    {
        // 如果没有压力，返回矩阵的中心点，避免除以零
        com.row = 12.5f;  // (SENSOR_ROWS - 1) / 2.0
        com.col = 4.5f;   // (SENSOR_COLS - 1) / 2.0
    }

    return com;
}



/**
 * @brief 查找分割行索引 - 将26x10矩阵从中间分割 - 2025-11-04
 * @description 返回用于分割双人矩阵的行索引
 * @return 分割行索引 (当前为12，即将矩阵分为上半部分[0-12]和下半部分[13-25])
 */
int find_split_row(void)
{
    int split_idx = 12;  // 26行的中间位置
    return split_idx;
}

/**
 * @brief 分割矩阵为上下两部分(各13行) - 2026-03-03
 * @description 将260字节的传感器数据(26x10)分割为：
 *              - 上部分(左侧): 前13行保留, 后13行补零
 *              - 下部分(右侧): 后13行向上移动到前13行, 后13行补零
 *              注意：右侧数据需要向上移动，以保持与训练时一致
 * @param input_data 输入的260字节传感器数据 (26x10矩阵)
 * @param upper_matrix 输出的上部分矩阵 (260字节，26x10)
 * @param lower_matrix 输出的下部分矩阵 (260字节，26x10)
 */
void split_dual_matrix(const uint8_t *input_data, uint8_t *upper_matrix, uint8_t *lower_matrix)
{
    // 上部分(左侧)：前13行保留 (0-12)
    rt_memset(upper_matrix, 0, 260);           // 全部初始化为0
    rt_memcpy(upper_matrix, input_data, 13 * SENSOR_COLS);   // 复制前13行

    // 下部分(右侧)：将后13行向上移动到前13行 (2026-03-03)
    rt_memset(lower_matrix, 0, 260);           // 全部初始化为0
    rt_memcpy(lower_matrix,                    // 目标：前13行位置
              input_data + 13 * SENSOR_COLS,   // 源：后13行数据
              13 * SENSOR_COLS);               // 复制后13行到前13行位置
}

/**
 * @brief 计算矩阵中的有效(非零)传感器点数 - 2025-11-06
 * @description 统计260字节矩阵中所有非零值的个数
 * @param matrix 输入的260字节矩阵数据
 * @return 非零传感器点的个数
 */
uint16_t count_nonzero_sensors_in_matrix(const uint8_t *matrix)
{
    uint16_t count = 0;
    for (int i = 0; i < 260; i++)
    {
        if (matrix[i] != 0)
        {
            count++;
        }
    }
    return count;
}

/**
 * @brief 计算矩阵上半部分(前13行)的非零传感器点数 - 2025-11-06
 * @description 统计26x10矩阵前13行的非零值个数
 * @param input_data 输入的260字节矩阵数据
 * @return 上半部分的非零传感器点个数
 */
uint16_t count_nonzero_in_upper_half(const uint8_t *input_data)
{
    uint16_t count = 0;
    for (int i = 0; i < 13 * SENSOR_COLS; i++)
    {
        if (input_data[i] != 0)
        {
            count++;
        }
    }
    return count;
}

/**
 * @brief 计算矩阵下半部分(后13行)的非零传感器点数 - 2025-11-06
 * @description 统计26x10矩阵后13行的非零值个数
 * @param input_data 输入的260字节矩阵数据
 * @return 下半部分的非零传感器点个数
 */
uint16_t count_nonzero_in_lower_half(const uint8_t *input_data)
{
    uint16_t count = 0;
    for (int i = 13 * SENSOR_COLS; i < 26 * SENSOR_COLS; i++)
    {
        if (input_data[i] != 0)
        {
            count++;
        }
    }
    return count;
}

/**
 * @brief 计算矩阵上半部分(前13行)的压力和 - 2025-11-06
 * @description 计算26x10矩阵前13行所有元素的和
 * @param input_data 输入的260字节矩阵数据
 * @return 上半部分的压力和
 */
uint32_t calc_sum_upper_half(const uint8_t *input_data)
{
    uint32_t sum = 0;
    for (int i = 0; i < 13 * SENSOR_COLS; i++)
    {
        sum += input_data[i];
    }
    return sum;
}

/**
 * @brief 计算矩阵下半部分(后13行)的压力和 - 2025-11-06
 * @description 计算26x10矩阵后13行所有元素的和
 * @param input_data 输入的260字节矩阵数据
 * @return 下半部分的压力和
 */
uint32_t calc_sum_lower_half(const uint8_t *input_data)
{
    uint32_t sum = 0;
    for (int i = 13 * SENSOR_COLS; i < 26 * SENSOR_COLS; i++)
    {
        sum += input_data[i];
    }
    return sum;
}

/**
 * @brief 计算上半部分(前13行)各行压力和的最大值 (2026-04-07)
 * @description 参考JQ704 calc_max_column_left/right，适配JQ260上下行分割。
 *              行峰值比均值更能反映人体集中压力区域，提升有人/无人判断鲁棒性。
 * @param input_data 输入的260字节矩阵数据
 * @return 上半部分各行压力和的最大值
 */
uint16_t calc_max_row_upper(const uint8_t *input_data)
{
    uint32_t max_val = 0;
    int row, col;
    for (row = 0; row < 13; row++) {
        uint32_t row_sum = 0;
        for (col = 0; col < SENSOR_COLS; col++) {
            row_sum += input_data[row * SENSOR_COLS + col];
        }
        if (row_sum > max_val) max_val = row_sum;
    }
    return (max_val > 0xFFFF) ? 0xFFFF : (uint16_t)max_val;
}

/**
 * @brief 计算下半部分(后13行)各行压力和的最大值 (2026-04-07)
 * @description 参考JQ704 calc_max_column_right，适配JQ260上下行分割。
 * @param input_data 输入的260字节矩阵数据
 * @return 下半部分各行压力和的最大值
 */
uint16_t calc_max_row_lower(const uint8_t *input_data)
{
    uint32_t max_val = 0;
    int row, col;
    for (row = 13; row < 26; row++) {
        uint32_t row_sum = 0;
        for (col = 0; col < SENSOR_COLS; col++) {
            row_sum += input_data[row * SENSOR_COLS + col];
        }
        if (row_sum > max_val) max_val = row_sum;
    }
    return (max_val > 0xFFFF) ? 0xFFFF : (uint16_t)max_val;
}

/* ========== 睡姿EMA平滑辅助函数 - 参考JQ704实现 (2026-04-07) ========== */

/* 重置单个人员槽的EMA状态（人员离开或到达时调用） (2026-04-07) */
static void reset_posture_ema(float *ema, uint8_t *pending, uint8_t *stable_cnt)
{
    ema[0] = ema[1] = 0.0f;
    *pending    = 0xFF;
    *stable_cnt = 0;
}

/**
 * @brief 用新推理概率更新EMA，返回经迟滞确认后的姿势值 (2026-04-07)
 *
 * 流程：
 *   1. EMA更新：ema[i] = alpha * prob[i] + (1-alpha) * ema[i]
 *   2. argmax(ema) 得到候选姿势
 *   3. 若候选与上次相同则稳定计数+1，否则重置为1
 *   4. 稳定计数达到阈值才提交新姿势，否则保持当前值
 *
 * @param probs         推理输出概率数组 [2]（[0]=仰卧, [1]=侧卧）
 * @param ema           EMA状态数组 [2]（持久化，跨帧）
 * @param pending       待确认姿势argmax（持久化）
 * @param stable_cnt    稳定帧计数（持久化）
 * @param cur_posture   当前已提交姿势（未达阈值时原样返回）
 * @return 更新后的姿势值（1=仰卧, 2=侧卧）
 */
static uint8_t update_posture_ema(const float *probs, float *ema,
                                   uint8_t *pending, uint8_t *stable_cnt,
                                   uint8_t cur_posture)
{
    int i;
    uint8_t argmax = 0;

    /* 步骤1：更新EMA (2026-04-07) */
    for (i = 0; i < 2; i++) {
        ema[i] = POSTURE_EMA_ALPHA * probs[i] + (1.0f - POSTURE_EMA_ALPHA) * ema[i];
    }

    /* 步骤2：求argmax (2026-04-07) */
    if (ema[1] > ema[0]) argmax = 1;

    /* 步骤3：迟滞稳定计数 (2026-04-07) */
    if (argmax == *pending) {
        if (*stable_cnt < 0xFF) (*stable_cnt)++;
    } else {
        *pending    = argmax;
        *stable_cnt = 1;
    }

    /* 步骤4：达到稳定阈值才提交 (2026-04-07) */
    if (*stable_cnt >= POSTURE_STABLE_THRESHOLD) {
        return (uint8_t)(argmax + 1);  /* 转为1-based: 1=仰卧, 2=侧卧 */
    }

    return cur_posture;  /* 未达阈值，保持当前姿势 */
}

/**
 * @brief 检测半区行均值是否有显著变化，同时更新EMA基线 (2026-04-07)
 * @param half_data  指向130字节半区数据（13行×10列）
 * @param row_ema    该半区13行均值EMA基线数组（持久化）
 * @return 1=检测到显著变化，0=无变化
 */
static uint8_t detect_row_change(const uint8_t *half_data, float *row_ema)
{
    uint8_t changed = 0;
    int r, c;
    for (r = 0; r < 13; r++) {
        float avg = 0.0f;
        for (c = 0; c < 10; c++) avg += half_data[r * 10 + c];
        avg /= 10.0f;
        if (s_change_ema_ready && fabsf(avg - row_ema[r]) > POSTURE_CHANGE_THRESH)
            changed = 1;
        row_ema[r] = CHANGE_EMA_ALPHA * avg + (1.0f - CHANGE_EMA_ALPHA) * row_ema[r];
    }
    return changed;
}

rt_bool_t ai_should_schedule_model(const uint8_t *input)
{
    uint8_t upper_changed;
    uint8_t lower_changed;

    if (!input) return RT_FALSE;

    upper_changed = detect_row_change(input,       s_row_ema_upper);
    lower_changed = detect_row_change(input + 130, s_row_ema_lower);

    if (!s_change_ema_ready && (++s_change_init_cnt >= 10)) {
        s_change_ema_ready = 1;
    }

    if (upper_changed) {
        if (!s_active_upper) {
            reset_posture_ema(s_posture_ema_1, &s_posture_pending_1, &s_posture_stable_cnt_1);
        }
        s_active_upper = 1;
        s_active_hold_upper = CHANGE_TRIGGER_HOLD_FRAMES;
    }

    if (lower_changed) {
        if (!s_active_lower) {
            reset_posture_ema(s_posture_ema_0, &s_posture_pending_0, &s_posture_stable_cnt_0);
        }
        s_active_lower = 1;
        s_active_hold_lower = CHANGE_TRIGGER_HOLD_FRAMES;
    }

    if (!s_calib_done) return RT_TRUE;

    return (upper_changed || lower_changed ||
            s_active_upper || s_active_lower ||
            s_active_hold_upper || s_active_hold_lower) ? RT_TRUE : RT_FALSE;
}

/* Model处理函数 - 行均值变化重置EMA + 人员检测门控AI推理 + 5帧EMA睡姿分类 (2026-04-07) */
static void decay_trigger_holds(void)
{
    if (s_active_hold_upper > 0) s_active_hold_upper--;
    if (s_active_hold_lower > 0) s_active_hold_lower--;
}

int Model(const uint8_t *input)
{
    s_person_count = 0;
    /* 姿势与腰部坐标为持久值，仅在人员离开时清除 (2026-04-07) */

    if (!input) return 0;

    /* === 步骤1：始终追踪行均值变化，显著变化时重置EMA（每帧必跑）(2026-04-07) ===
     * 仅重置EMA，不激活AI — 人员检测仍是AI推理的唯一门控条件 */

    /* === 步骤2：人员检测（始终运行，轻量级）(2026-04-07) === */
    uint32_t upper_pressure = calc_sum_upper_half(input);
    uint32_t lower_pressure = calc_sum_lower_half(input);
    uint16_t upper_sensors  = count_nonzero_in_upper_half(input);
    uint16_t lower_sensors  = count_nonzero_in_lower_half(input);
    uint16_t upper_avg      = (uint16_t)(upper_pressure / SENSOR_HALF_SIZE_260);
    uint16_t lower_avg      = (uint16_t)(lower_pressure / SENSOR_HALF_SIZE_260);
    uint16_t upper_row_max  = calc_max_row_upper(input);
    uint16_t lower_row_max  = calc_max_row_lower(input);

    /* 校准阶段：建立人员检测阈值基线 (2026-03-23) */
    if (!s_calib_done) {
        s_calib_sum_upper += upper_avg;
        s_calib_sum_lower += lower_avg;
        s_calib_row_upper += upper_row_max;
        s_calib_row_lower += lower_row_max;
        s_calib_count++;
        if (s_calib_count >= PERSON_CALIB_FRAMES) {
            s_threshold_upper     = (uint16_t)(s_calib_sum_upper / PERSON_CALIB_FRAMES) + PERSON_DETECT_MARGIN;
            s_threshold_lower     = (uint16_t)(s_calib_sum_lower / PERSON_CALIB_FRAMES) + PERSON_DETECT_MARGIN;
            s_threshold_row_upper = (uint16_t)(s_calib_row_upper / PERSON_CALIB_FRAMES) + PERSON_DETECT_MARGIN * 10;
            s_threshold_row_lower = (uint16_t)(s_calib_row_lower / PERSON_CALIB_FRAMES) + PERSON_DETECT_MARGIN * 10;
            s_calib_done = 1;
        }
        return 0;
    }

    s_vote_buf_upper[s_vote_index] = (upper_avg     > s_threshold_upper     &&
                                      upper_row_max > s_threshold_row_upper &&
                                      upper_sensors > PERSON_COUNT_THRESHOLD) ? 1 : 0;
    s_vote_buf_lower[s_vote_index] = (lower_avg     > s_threshold_lower     &&
                                      lower_row_max > s_threshold_row_lower &&
                                      lower_sensors > PERSON_COUNT_THRESHOLD) ? 1 : 0;
    s_vote_index = (s_vote_index + 1) % PERSON_VOTE_WINDOW;

    uint8_t upper_votes = 0, lower_votes = 0, vi;
    for (vi = 0; vi < PERSON_VOTE_WINDOW; vi++) {
        upper_votes += s_vote_buf_upper[vi];
        lower_votes += s_vote_buf_lower[vi];
    }

    int upper_has_person = (upper_votes >= PERSON_VOTE_THRESHOLD) ? 1 : 0;
    int lower_has_person = (lower_votes >= PERSON_VOTE_THRESHOLD) ? 1 : 0;

    /* 人员离开时清除姿势与腰部坐标 (2026-04-07) */
    if ((uint8_t)upper_has_person != s_prev_upper_person && !upper_has_person) {
        s_posture_1 = 0xFF;
        s_waist_x_1 = 0xFF; s_waist_y_1 = 0xFF;
        reset_posture_ema(s_posture_ema_1, &s_posture_pending_1, &s_posture_stable_cnt_1);
        s_active_upper = 0;
        s_active_hold_upper = 0;
    }
    if ((uint8_t)lower_has_person != s_prev_lower_person && !lower_has_person) {
        s_posture_0 = 0xFF;
        s_waist_x_0 = 0xFF; s_waist_y_0 = 0xFF;
        reset_posture_ema(s_posture_ema_0, &s_posture_pending_0, &s_posture_stable_cnt_0);
        s_active_lower = 0;
        s_active_hold_lower = 0;
    }
    if (!upper_has_person && s_active_upper && (s_active_hold_upper == 0)) {
        s_active_upper = 0;
        reset_posture_ema(s_posture_ema_1, &s_posture_pending_1, &s_posture_stable_cnt_1);
    }
    if (!lower_has_person && s_active_lower && (s_active_hold_lower == 0)) {
        s_active_lower = 0;
        reset_posture_ema(s_posture_ema_0, &s_posture_pending_0, &s_posture_stable_cnt_0);
    }
    s_prev_upper_person = (uint8_t)upper_has_person;
    s_prev_lower_person = (uint8_t)lower_has_person;

    /* Hybrid stage 1: use LR to distinguish one-person vs two-person after empty-bed gate. */
    if (!upper_has_person && !lower_has_person) {
        s_person_count = 0;
        decay_trigger_holds();
        return 0;  /* Empty bed: skip posture inference. */
    }

    float person_confidence = 0.0f;
    s_person_count = jq260_hybrid_person_count_predict(input, &person_confidence);

    if (s_person_count == JQ260_PERSON_COUNT_TWO) {
        upper_has_person = 1;
        lower_has_person = 1;
        s_active_upper = 1;
        s_active_lower = 1;
    } else {
        /* If legacy presence voting sees both halves but LR says one person, keep the stronger half. */
        if (upper_has_person && lower_has_person) {
            if (upper_pressure >= lower_pressure) {
                lower_has_person = 0;
            } else {
                upper_has_person = 0;
            }
        }
    }

    /* === 步骤3：人员检测确认后运行AI推理 + 5帧EMA稳定 (2026-04-07) === */
    if (s_person_count == JQ260_PERSON_COUNT_TWO) {
        /* 双人：使用分割矩阵分别推理 */
        uint8_t upper_matrix[260], lower_matrix[260];
        split_dual_matrix(input, upper_matrix, lower_matrix);

        float prob; uint8_t raw;

        if (s_active_upper) {
            raw = jq260_hybrid_double_posture_predict(input, JQ260_REGION_UPPER, &prob);
            if (raw != 0) {
                float probs[2] = {1.0f - prob, prob};
                s_posture_1 = update_posture_ema(probs, s_posture_ema_1,
                                                 &s_posture_pending_1, &s_posture_stable_cnt_1,
                                                 s_posture_1);
            }
        /* 腰部坐标与姿势同步：仅在EMA稳定后更新，避免posture=FF但waist有值 (2026-04-07) */
        }
        if (s_active_upper && (s_posture_stable_cnt_1 >= POSTURE_STABLE_THRESHOLD)) {
            CoM_t com = calculate_com(upper_matrix);
            int xv = (int)com.row, yv = (int)com.col;
            if (xv >= 0 && xv <= 255) s_waist_x_1 = (uint8_t)xv;
            if (yv >= 0 && yv <= 255) s_waist_y_1 = (uint8_t)yv;
            s_active_upper = 0;
        }

        if (s_active_lower) {
            raw = jq260_hybrid_double_posture_predict(input, JQ260_REGION_LOWER, &prob);
            if (raw != 0) {
                float probs[2] = {1.0f - prob, prob};
                s_posture_0 = update_posture_ema(probs, s_posture_ema_0,
                                                 &s_posture_pending_0, &s_posture_stable_cnt_0,
                                                 s_posture_0);
            }
        }
        if (s_active_lower && (s_posture_stable_cnt_0 >= POSTURE_STABLE_THRESHOLD)) {
            CoM_t com0 = calculate_com(lower_matrix);
            int xv0 = (int)com0.row, yv0 = (int)com0.col;
            if (xv0 >= 0 && xv0 <= 255) s_waist_x_0 = (uint8_t)xv0;
            if (yv0 >= 0 && yv0 <= 255) s_waist_y_0 = (uint8_t)yv0;
            s_active_lower = 0;
        }

    } else {
        /* 单人：使用完整数据，分配到有人的半区槽位 */
        int is_upper = upper_has_person;
        uint8_t *s_posture = is_upper ? &s_posture_1   : &s_posture_0;
        float   *s_ema     = is_upper ? s_posture_ema_1 : s_posture_ema_0;
        uint8_t *s_pending = is_upper ? &s_posture_pending_1   : &s_posture_pending_0;
        uint8_t *s_stab    = is_upper ? &s_posture_stable_cnt_1 : &s_posture_stable_cnt_0;
        uint8_t *s_wx      = is_upper ? &s_waist_x_1   : &s_waist_x_0;
        uint8_t *s_wy      = is_upper ? &s_waist_y_1   : &s_waist_y_0;
        uint8_t *s_active  = is_upper ? &s_active_upper : &s_active_lower;

        if (*s_active) {
            float prob; uint8_t raw = ai_model_classify(input, &prob);
            if (raw != 0) {
                float probs[2] = {1.0f - prob, prob};
                *s_posture = update_posture_ema(probs, s_ema, s_pending, s_stab, *s_posture);
            }
        }
        /* 腰部坐标与姿势同步：仅在EMA稳定后更新 (2026-04-07) */
        if (*s_active && (*s_stab >= POSTURE_STABLE_THRESHOLD)) {
            CoM_t com = calculate_com(input);
            int xv = (int)com.row, yv = (int)com.col;
            if (xv >= 0 && xv <= 255) *s_wx = (uint8_t)xv;
            if (yv >= 0 && yv <= 255) *s_wy = (uint8_t)yv;
            *s_active = 0;
        }
    }

    decay_trigger_holds();
    return s_person_count;
}


/* 线程入口函数 */
static void ai_thread_entry(void *parameter)
{
    rt_uint32_t e;

    while (1)
    {
        if (rt_event_recv(&data_ready_event, 0x01, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &e) == RT_EOK)
        {
            // 调用Model处理，返回人数(0=空床, 1=一人, 2=两人)
            // Model()函数会更新全局的analysis_result结构体，包含检测结果
            (void)Model(Origin_MattressData);

            rt_enter_critical();
            //内存数据拷贝
            g_person_count = s_person_count;
            g_posture_0 = s_posture_0;
            g_posture_1 = s_posture_1;
            g_waist_x_0 = s_waist_x_0;
            g_waist_y_0 = s_waist_y_0;
            g_waist_x_1 = s_waist_x_1;
            g_waist_y_1 = s_waist_y_1;
            rt_exit_critical();
            #if DEBUG_MODE
            rt_kprintf("[DBG] Model done: person_count=%d, posture_0=0x%02X, posture_1=0x%02X\n",
                g_person_count, g_posture_0, g_posture_1);
            #endif
        }
    }
}

/* 初始化函数，使用静态线程启动 AI 运算 */
int ai_thread_init(void)
{
    rt_thread_init(&ai_thread,              // 线程控制块
                   "ai_task",               // 名称
                   ai_thread_entry,         // 入口函数
                   RT_NULL,                 // 参数
                   &ai_thread_stack[0],     // 栈起始地址
                   sizeof(ai_thread_stack), // 栈大小
                   20,                      // 优先级
                   10);                     // 时间片
    rt_thread_startup(&ai_thread);          // 启动线程
    return 0;
}
INIT_APP_EXPORT(ai_thread_init);
