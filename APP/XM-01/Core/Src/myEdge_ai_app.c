/* 传感器处理从260点改为160点，映射规则来自 docs/sensor_mapping.md (2026-05-06) */
#include "myEdge_ai_app.h"
#include "posture_classifier.h"  /* InDevelop随机森林姿态分类器，替代X-CUBE-AI模型 (2026-05-06) */
#include <stdint.h>
#include <math.h>

#define AI_THREAD_STACK_SIZE (1024 * 16)

/* === 自适应基线校准 + 时间投票法参数定义 (2026-05-06) === */
#define PERSON_CALIB_FRAMES    30   /* 启动校准帧数 (约2秒@14Hz，期间床垫应为空) */
#define PERSON_DETECT_MARGIN   4    /* 检测阈值 = 基线均值 + 此余量 (0-255量程) */
#define PERSON_COUNT_THRESHOLD 20   /* 半侧最小激活格数 (80格中约25%) */
#define PERSON_VOTE_WINDOW     7    /* 时间投票窗口帧数 (7帧=500ms@14Hz) */
#define PERSON_VOTE_THRESHOLD  5    /* 判定有人所需最小票数 (5/7多数决) */

/* 睡姿EMA平滑参数 (2026-05-06) */
#define POSTURE_EMA_ALPHA        0.33f  /* 约5帧有效窗口 */
#define POSTURE_STABLE_THRESHOLD 5      /* 5帧连续一致才提交 */

static struct rt_thread ai_thread;
static rt_uint8_t ai_thread_stack[AI_THREAD_STACK_SIZE];

static uint8_t s_person_count;
static uint8_t s_posture_0  = 0xFF;
static uint8_t s_posture_1  = 0xFF;

/* 自适应基线校准状态 (2026-05-06) */
static uint8_t  s_calib_done      = 0;
static uint8_t  s_calib_count     = 0;
static uint32_t s_calib_sum_upper = 0;
static uint32_t s_calib_sum_lower = 0;
static uint16_t s_threshold_upper = 10;
static uint16_t s_threshold_lower = 10;

/* 人数识别时间投票缓冲区 (2026-05-06) */
static uint8_t s_vote_buf_upper[PERSON_VOTE_WINDOW];
static uint8_t s_vote_buf_lower[PERSON_VOTE_WINDOW];
static uint8_t s_vote_index = 0;

/* 睡姿EMA平滑状态 (2026-05-06) */
/* 三分类：ema[0]=仰卧, ema[1]=左侧卧, ema[2]=右侧卧 */
static float   s_posture_ema_0[3]     = {0.0f, 0.0f, 0.0f};
static float   s_posture_ema_1[3]     = {0.0f, 0.0f, 0.0f};
static uint8_t s_posture_pending_0    = 0xFF;
static uint8_t s_posture_pending_1    = 0xFF;
static uint8_t s_posture_stable_cnt_0 = 0;
static uint8_t s_posture_stable_cnt_1 = 0;
static uint8_t s_prev_upper_person    = 0;
static uint8_t s_prev_lower_person    = 0;

uint8_t g_person_count = 0;
uint8_t g_posture_0 = 0xFF;
uint8_t g_posture_1 = 0xFF;

model_t model;

/* 计算160个元素的和 (2026-05-06) */
uint32_t calc_160_sum(const uint8_t *data)
{
    uint32_t sum = 0;
    uint16_t i;
    for (i = 0; i < 160; i++)
        sum += data[i];
    return sum;
}

/* 计算160个元素中非零值的个数 (2026-05-06) */
uint16_t sensor_number_160(const uint8_t *data)
{
    uint16_t count = 0;
    uint16_t i;
    for (i = 0; i < 160; i++)
        if (data[i] != 0) count++;
    return count;
}

/**
 * @brief 分割16x10矩阵为上下两部分(各8x10) (2026-05-06)
 * @description
 *  上半区：output[80:160]（传感器行0-7，经映射后位于索引80-159）
 *  下半区：output[0:80] （传感器行8-15，经映射后位于索引0-79）
 *  分割后各自填充为完整160字节（另一半补零），保持与模型输入格式一致
 */
void split_upper_lower_matrix(const uint8_t *input_data, uint8_t *upper_matrix, uint8_t *lower_matrix)
{
    /* 上半区：索引80-159保留，0-79补零 (2026-05-06) */
    rt_memset(upper_matrix, 0, 160);
    rt_memcpy(upper_matrix + 80, input_data + 80, 80);

    /* 下半区：索引0-79保留，80-159补零 (2026-05-06) */
    rt_memset(lower_matrix, 0, 160);
    rt_memcpy(lower_matrix, input_data, 80);
}

/* 计算上半部分(output[80:160])的压力和 (2026-05-06) */
uint32_t calc_sum_upper_half(const uint8_t *input_data)
{
    uint32_t sum = 0;
    int i;
    for (i = 80; i < 160; i++)
        sum += input_data[i];
    return sum;
}

/* 计算下半部分(output[0:80])的压力和 (2026-05-06) */
uint32_t calc_sum_lower_half(const uint8_t *input_data)
{
    uint32_t sum = 0;
    int i;
    for (i = 0; i < 80; i++)
        sum += input_data[i];
    return sum;
}

/* 计算上半部分的非零传感器点数 (2026-05-06) */
uint16_t count_nonzero_in_upper_half(const uint8_t *input_data)
{
    uint16_t count = 0;
    int i;
    for (i = 80; i < 160; i++)
        if (input_data[i] != 0) count++;
    return count;
}

/* 计算下半部分的非零传感器点数 (2026-05-06) */
uint16_t count_nonzero_in_lower_half(const uint8_t *input_data)
{
    uint16_t count = 0;
    int i;
    for (i = 0; i < 80; i++)
        if (input_data[i] != 0) count++;
    return count;
}

/* ========== 睡姿EMA平滑辅助函数 (2026-05-06) ========== */

static void reset_posture_ema(float *ema, uint8_t *pending, uint8_t *stable_cnt)
{
    ema[0] = ema[1] = ema[2] = 0.0f;
    *pending    = 0xFF;
    *stable_cnt = 0;
}

/**
 * @brief 用新推理结果更新EMA，返回经迟滞确认后的姿势值 (2026-05-06)
 * @param label      posture_classify() 返回的 PostureLabel（1/2/3）
 * @param ema        EMA状态数组 [3]（持久化，跨帧）
 * @param pending    待确认姿势（持久化）
 * @param stable_cnt 稳定帧计数（持久化）
 * @param cur_posture 当前已提交姿势（未达阈值时原样返回）
 * @return 更新后的姿势值（1=仰卧, 2=左侧卧, 3=右侧卧）
 */
static uint8_t update_posture_ema(int8_t label, float *ema,
                                   uint8_t *pending, uint8_t *stable_cnt,
                                   uint8_t cur_posture)
{
    int i;
    uint8_t argmax = 0;
    float one_hot[3] = {0.0f, 0.0f, 0.0f};

    /* 将 PostureLabel 转为 one-hot，POSTURE_UNKNOWN 不更新 (2026-05-06) */
    if (label >= 1 && label <= 3)
        one_hot[label - 1] = 1.0f;
    else
        return cur_posture;

    for (i = 0; i < 3; i++)
        ema[i] = POSTURE_EMA_ALPHA * one_hot[i] + (1.0f - POSTURE_EMA_ALPHA) * ema[i];

    /* argmax (2026-05-06) */
    for (i = 1; i < 3; i++)
        if (ema[i] > ema[argmax]) argmax = (uint8_t)i;

    if (argmax == *pending) {
        if (*stable_cnt < 0xFF) (*stable_cnt)++;
    } else {
        *pending    = argmax;
        *stable_cnt = 1;
    }

    if (*stable_cnt >= POSTURE_STABLE_THRESHOLD)
        return (uint8_t)(argmax + 1);  /* 1=仰卧, 2=左侧卧, 3=右侧卧 */

    return cur_posture;
}

/* ========== Model处理函数 (2026-05-06) ========== */

int Model(const uint8_t *input)
{
    s_person_count = 0;

    if (!input) return 0;

    /* 步骤1：人员检测（始终运行）(2026-05-06) */
    uint32_t upper_pressure = calc_sum_upper_half(input);
    uint32_t lower_pressure = calc_sum_lower_half(input);
    uint16_t upper_sensors  = count_nonzero_in_upper_half(input);
    uint16_t lower_sensors  = count_nonzero_in_lower_half(input);
    uint16_t upper_avg      = (uint16_t)(upper_pressure / SENSOR_HALF_SIZE);
    uint16_t lower_avg      = (uint16_t)(lower_pressure / SENSOR_HALF_SIZE);

    /* 校准阶段：建立人员检测阈值基线 (2026-05-06) */
    if (!s_calib_done) {
        s_calib_sum_upper += upper_avg;
        s_calib_sum_lower += lower_avg;
        s_calib_count++;
        if (s_calib_count >= PERSON_CALIB_FRAMES) {
            s_threshold_upper = (uint16_t)(s_calib_sum_upper / PERSON_CALIB_FRAMES) + PERSON_DETECT_MARGIN;
            s_threshold_lower = (uint16_t)(s_calib_sum_lower / PERSON_CALIB_FRAMES) + PERSON_DETECT_MARGIN;
            s_calib_done = 1;
            SEGGER_RTT_printf(0, "[CALIB] done. thr_upper:%d thr_lower:%d\r\n",
                              s_threshold_upper, s_threshold_lower);
        }
        return 0;
    }

    /* 步骤2：时间投票判断有人/无人 (2026-05-06) */
    s_vote_buf_upper[s_vote_index] = (upper_avg > s_threshold_upper &&
                                      upper_sensors > PERSON_COUNT_THRESHOLD) ? 1 : 0;
    s_vote_buf_lower[s_vote_index] = (lower_avg > s_threshold_lower &&
                                      lower_sensors > PERSON_COUNT_THRESHOLD) ? 1 : 0;
    s_vote_index = (s_vote_index + 1) % PERSON_VOTE_WINDOW;

    uint8_t upper_votes = 0, lower_votes = 0, vi;
    for (vi = 0; vi < PERSON_VOTE_WINDOW; vi++) {
        upper_votes += s_vote_buf_upper[vi];
        lower_votes += s_vote_buf_lower[vi];
    }

    int upper_has_person = (upper_votes >= PERSON_VOTE_THRESHOLD) ? 1 : 0;
    int lower_has_person = (lower_votes >= PERSON_VOTE_THRESHOLD) ? 1 : 0;

    SEGGER_RTT_printf(0, "upper_avg:%d(thr:%d) lower_avg:%d(thr:%d)\r\n",
                      upper_avg, s_threshold_upper, lower_avg, s_threshold_lower);

    /* 人员离开时清除姿势并重置EMA (2026-05-06) */
    if ((uint8_t)upper_has_person != s_prev_upper_person && !upper_has_person) {
        s_posture_1 = 0xFF;
        reset_posture_ema(s_posture_ema_1, &s_posture_pending_1, &s_posture_stable_cnt_1);
    }
    if ((uint8_t)lower_has_person != s_prev_lower_person && !lower_has_person) {
        s_posture_0 = 0xFF;
        reset_posture_ema(s_posture_ema_0, &s_posture_pending_0, &s_posture_stable_cnt_0);
    }
    s_prev_upper_person = (uint8_t)upper_has_person;
    s_prev_lower_person = (uint8_t)lower_has_person;

    if (!upper_has_person && !lower_has_person)
        return 0;

    /* 步骤3：AI推理 + EMA稳定 (2026-05-06) */
    if (upper_has_person && lower_has_person) {
        /* 双人：分割矩阵分别推理 */
        uint8_t upper_matrix[160], lower_matrix[160];
        float   half_float[80];
        int8_t  label;
        int     k;
        split_upper_lower_matrix(input, upper_matrix, lower_matrix);
        s_person_count = 2;

        /* 上半区推理：取 upper_matrix[80:160]（8×10）转 float (2026-05-06) */
        for (k = 0; k < 80; k++) half_float[k] = (float)upper_matrix[80 + k];
        label = posture_classify(half_float);
        s_posture_1 = update_posture_ema(label, s_posture_ema_1,
                                         &s_posture_pending_1, &s_posture_stable_cnt_1,
                                         s_posture_1);

        /* 下半区推理：取 lower_matrix[0:80]（8×10）转 float (2026-05-06) */
        for (k = 0; k < 80; k++) half_float[k] = (float)lower_matrix[k];
        label = posture_classify(half_float);
        s_posture_0 = update_posture_ema(label, s_posture_ema_0,
                                         &s_posture_pending_0, &s_posture_stable_cnt_0,
                                         s_posture_0);
    } else {
        /* 单人：提取有人的半区80点推理 */
        s_person_count = 1;
        int is_upper = upper_has_person;
        uint8_t *s_posture = is_upper ? &s_posture_1   : &s_posture_0;
        float   *s_ema     = is_upper ? s_posture_ema_1 : s_posture_ema_0;
        uint8_t *s_pending = is_upper ? &s_posture_pending_1   : &s_posture_pending_0;
        uint8_t *s_stab    = is_upper ? &s_posture_stable_cnt_1 : &s_posture_stable_cnt_0;

        float half_float[80];
        int k;
        /* 上半区在 input[80:160]，下半区在 input[0:80] (2026-05-06) */
        if (is_upper)
            for (k = 0; k < 80; k++) half_float[k] = (float)input[80 + k];
        else
            for (k = 0; k < 80; k++) half_float[k] = (float)input[k];

        int8_t label = posture_classify(half_float);
        *s_posture = update_posture_ema(label, s_ema, s_pending, s_stab, *s_posture);
        SEGGER_RTT_printf(0, "posture_%d:%d (label=%d)\r\n", is_upper ? 1 : 0, *s_posture, label);
    }

    return s_person_count;
}

/* 线程入口函数 (2026-05-06) */
static void ai_thread_entry(void *parameter)
{
    rt_uint32_t e;

    while (1)
    {
        if (rt_event_recv(&data_ready_event, 0x01, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &e) == RT_EOK)
        {
            (void)Model(Origin_MattressData);

            rt_enter_critical();
            g_person_count = s_person_count;
            g_posture_0    = s_posture_0;
            g_posture_1    = s_posture_1;
            rt_exit_critical();
        }
    }
}

/* 初始化函数，使用静态线程启动 AI 运算 (2026-05-06) */
int ai_thread_init(void)
{
    rt_thread_init(&ai_thread,
                   "ai_task",
                   ai_thread_entry,
                   RT_NULL,
                   &ai_thread_stack[0],
                   sizeof(ai_thread_stack),
                   20,
                   10);
    rt_thread_startup(&ai_thread);
    return 0;
}
INIT_APP_EXPORT(ai_thread_init);
