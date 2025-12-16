
#include "myEdge_ai_app.h"
#include "stm32_sleep_posture.h"
#include <stdint.h>

#define AI_THREAD_STACK_SIZE (1024 * 16)

/* === 连通分量检测方法参数定义 (基于连通分量洞察，98.5%准确率) - 2025-11-04 === */
const uint8_t ACTIVITY_THRESHOLD = 10;                      // 活跃传感器阈值
const uint32_t EMPTY_BED_PRESSURE_MIN = 2000U;              // 空床检测压力阈值(整数)
const uint16_t EMPTY_BED_SENSORS_MIN = 10;                  // 空床检测传感器阈值(整数)

/* 压力归一化参数(整数) */
const uint32_t PRESSURE_NORM_BASE = 5000U;                  // 压力归一化基准值(整数)
const uint32_t PRESSURE_NORM_RANGE = 5000U;                 // 压力归一化范围(整数)

/* 传感器归一化参数(整数) */
const uint16_t SENSOR_NORM_BASE = 60U;                      // 传感器归一化基准值(整数)
const uint16_t SENSOR_NORM_RANGE = 100U;                    // 传感器归一化范围(整数)

/* 混合分类权重 */
const float PRESSURE_WEIGHT = 0.3f;                         // 压力特征权重
const float SENSOR_WEIGHT = 0.3f;                           // 传感器特征权重
const float COMPONENT_WEIGHT = 0.4f;                        // 连通分量权重(最关键)
const float CLASSIFICATION_THRESHOLD = 0.5f;                // 分类阈值

/* 连通分量分类参数(整数) */
const int COMPONENT_SINGLE_MAX = 2;                         // <=2个分量：明确单人
const int COMPONENT_GRADIENT_MAX = 4;                       // <=4个分量：渐变区域

/* 前置声明 */
static int count_connected_components(const uint8_t *matrix);
static int hybrid_classification(uint32_t total_pressure, int active_sensors, int connected_components);

static struct rt_thread ai_thread;
static rt_uint8_t ai_thread_stack[AI_THREAD_STACK_SIZE];
//uint8_t Origin_MattressData[260] = {0};

static uint8_t s_person_count;
static uint8_t s_posture_0;
static uint8_t s_waist_x_0;
static uint8_t s_waist_y_0;
static uint8_t s_posture_1;
static uint8_t s_waist_x_1;
static uint8_t s_waist_y_1;

uint8_t g_person_count = 0;
uint8_t g_posture_0 = 0;
uint8_t g_waist_x_0 = 0;
uint8_t g_waist_y_0 = 0;
uint8_t g_posture_1 = 0;
uint8_t g_waist_x_1 = 0;
uint8_t g_waist_y_1 = 0;

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

#if 0
/* === 人数检测 - 基于压力和、活跃传感器、连通分量的混合方法 - 2025-11-04 === */

/**
 * @brief 验证uint8_t传感器数据的有效性并返回和值
 * @param srcBuf 源uint8_t缓冲区(260字节传感器数据)
 * @param len    数据长度 (260)
 * @return >0: 数据有效，返回数据和值; 0: 数据无效（全零）
 */
static uint32_t validate_sensor_data(const uint8_t *srcBuf, int len)
{
    uint32_t data_sum = 0;

    for (int i = 0; i < len; i++)
    {
        data_sum += srcBuf[i];
    }

    // 返回数据和值（0表示全零，非0表示有效数据）
    return data_sum;
}
#endif

/**
 * @brief 连通分量计算函数 - 基于flood-fill算法
 * @description 计算26x10矩阵中的连通分量数，这是区分单人/双人的关键特征
 * @param matrix 指向 26x10 矩阵数据的指针
 * @return 连通分量的数量
 */
static int count_connected_components(const uint8_t *matrix)
{
    // 创建二值化矩阵
    uint8_t binary_matrix[SENSOR_ROWS][SENSOR_COLS] = {{0}};
    uint8_t visited[SENSOR_ROWS][SENSOR_COLS] = {{0}};

    // 二值化：大于阈值的区域标记为1
    for (int i = 0; i < SENSOR_ROWS; i++)
    {
        for (int j = 0; j < SENSOR_COLS; j++)
        {
            binary_matrix[i][j] = (matrix[i * SENSOR_COLS + j] > ACTIVITY_THRESHOLD) ? 1 : 0;
        }
    }

    int components = 0;

    // 简化栈结构用于flood-fill
    typedef struct { uint8_t row, col; } Point;
    Point stack[260]; // 最大可能的栈大小(260)

    // 扫描每个像素点
    for (int i = 0; i < SENSOR_ROWS; i++)
    {
        for (int j = 0; j < SENSOR_COLS; j++)
        {
            if (binary_matrix[i][j] == 1 && !visited[i][j])
            {
                components++;

                // 使用栈进行flood-fill
                int stack_top = 0;
                stack[stack_top].row = i;
                stack[stack_top].col = j;
                stack_top++;

                while (stack_top > 0)
                {
                    stack_top--;
                    Point p = stack[stack_top];

                    // 边界检查和访问检查
                    if (p.row >= SENSOR_ROWS || p.col >= SENSOR_COLS ||
                        visited[p.row][p.col] ||
                        binary_matrix[p.row][p.col] == 0)
                    {
                        continue;
                    }

                    visited[p.row][p.col] = 1;

                    // 添加4连通的邻居 (防止栈溢出)
                    if (stack_top < 256)
                    {
                        if (p.row > 0) {
                            stack[stack_top].row = p.row - 1;
                            stack[stack_top].col = p.col;
                            stack_top++;
                        }
                        if (p.row < SENSOR_ROWS - 1) {
                            stack[stack_top].row = p.row + 1;
                            stack[stack_top].col = p.col;
                            stack_top++;
                        }
                        if (p.col > 0) {
                            stack[stack_top].row = p.row;
                            stack[stack_top].col = p.col - 1;
                            stack_top++;
                        }
                        if (p.col < SENSOR_COLS - 1) {
                            stack[stack_top].row = p.row;
                            stack[stack_top].col = p.col + 1;
                            stack_top++;
                        }
                    }
                }
            }
        }
    }

    return components;
}

/**
 * @brief 混合分类器 - 结合统计特征和连通分量的核心逻辑
 * @description 使用整数计算压力和传感器特征，浮点计算仅用于最后的加权组合
 * @param total_pressure 总压力值(整数uint32_t)
 * @param active_sensors 活跃传感器数量(整数)
 * @param connected_components 连通分量数量
 * @return 1 代表单人, 2 代表双人
 */
static int hybrid_classification(uint32_t total_pressure, int active_sensors, int connected_components)
{
    // 特征标准化评分(0-1范围)

    // 1. 压力评分 - 基于整数uint32_t计算
    float pressure_score;
    if (total_pressure >= (PRESSURE_NORM_BASE + PRESSURE_NORM_RANGE))
    {
        pressure_score = 1.0f;
    }
    else if (total_pressure <= PRESSURE_NORM_BASE)
    {
        pressure_score = 0.0f;
    }
    else
    {
        pressure_score = (float)(total_pressure - PRESSURE_NORM_BASE) / (float)PRESSURE_NORM_RANGE;
    }

    // 2. 传感器评分 - 基于整数uint16_t计算
    float sensor_score;
    if (active_sensors >= (int)(SENSOR_NORM_BASE + SENSOR_NORM_RANGE))
    {
        sensor_score = 1.0f;
    }
    else if (active_sensors <= (int)SENSOR_NORM_BASE)
    {
        sensor_score = 0.0f;
    }
    else
    {
        sensor_score = (float)(active_sensors - (int)SENSOR_NORM_BASE) / (float)SENSOR_NORM_RANGE;
    }

    // 3. 连通分量评分(核心特征) - 完全基于整数判断
    float component_score = 0.0f;
    if (connected_components <= COMPONENT_SINGLE_MAX)
    {
        component_score = 0.0f;  // 明确单人
    }
    else if (connected_components <= COMPONENT_GRADIENT_MAX)
    {
        component_score = (float)(connected_components - COMPONENT_SINGLE_MAX) /
                         (float)(COMPONENT_GRADIENT_MAX - COMPONENT_SINGLE_MAX);
    }
    else
    {
        component_score = 1.0f;  // 明确双人
    }

    // 4. 加权组合 - 最后阶段才用浮点数
    float combined_score = PRESSURE_WEIGHT * pressure_score +
                          SENSOR_WEIGHT * sensor_score +
                          COMPONENT_WEIGHT * component_score;

    // 5. 分类决策
    return (combined_score >= CLASSIFICATION_THRESHOLD) ? 2 : 1;
}

/**
 * @brief 人数检测主函数 - 综合检测逻辑
 * @param matrix 指向Origin_MattressData数组的指针(260字节)
 * @param total_pressure 总压力和(来自calc_260_sum)
 * @param active_sensors 活跃传感器数量(来自sensor_number)
 * @return 0: 空床, 1: 一个人, 2: 两个人
 */
int detect_person_count(const uint8_t *matrix, uint32_t total_pressure, uint16_t active_sensors)
{
    if (!matrix)
    {
        return 0;
    }

    // 步骤1: 空床检测（快速筛选）
    // 修复：改用 OR (||) 而不是 AND (&&)
    // 理由：如果压力低 OR 活跃传感器少，都应该判断为空床
    if (total_pressure < EMPTY_BED_PRESSURE_MIN || active_sensors < EMPTY_BED_SENSORS_MIN)
    {
        return 0; // 空床
    }

    // 步骤2: 计算连通分量数 (核心特征)
    int connected_components = count_connected_components(matrix);

    // 步骤3: 混合特征评分和分类
    return hybrid_classification(total_pressure, active_sensors, connected_components);
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
 * @brief 分割矩阵为上下两部分(各13行) - 2025-11-06
 * @description 将260字节的传感器数据(26x10)分割为：
 *              - 上部分: 前13行, 后13行补零
 *              - 下部分: 后13行, 前13行补零
 * @param input_data 输入的260字节传感器数据 (26x10矩阵)
 * @param upper_matrix 输出的上部分矩阵 (260字节，26x10)
 * @param lower_matrix 输出的下部分矩阵 (260字节，26x10)
 */
void split_dual_matrix(const uint8_t *input_data, uint8_t *upper_matrix, uint8_t *lower_matrix)
{
    // 上部分：前13行 (0-12)
    rt_memset(upper_matrix, 0, 260);           // 全部初始化为0
    rt_memcpy(upper_matrix, input_data, 13 * SENSOR_COLS);   // 复制前13行

    // 下部分：后13行 (13-25)
    rt_memset(lower_matrix, 0, 260);           // 全部初始化为0
    rt_memcpy(lower_matrix + 13 * SENSOR_COLS,
              input_data + 13 * SENSOR_COLS,
              13 * SENSOR_COLS);                              // 复制后13行到下部分的后13行
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
 * @brief 改进的混合分类器 - 使用相对指标而非绝对值 - 2025-11-06
 * @description 使用相对压力比、相对传感器比和连通分量进行分类，不依赖绝对阈值
 * @param relative_pressure 相对压力(0-1，该部分压力/总压力)
 * @param relative_sensors 相对传感器数(0-1，该部分传感器/总传感器)
 * @param connected_components 该部分的连通分量数
 * @return 0表示该部分无效, 1表示有效单人, 2表示有效双人
 */
static int hybrid_classification_relative(float relative_pressure, float relative_sensors, int connected_components)
{
    // 处理无效情况：如果没有连通分量，则该部分无效
    if (connected_components == 0)
    {
        return 0;  // 无效
    }

    // 特征标准化评分 (0-1范围)

    // 1. 相对压力评分 - 基于压力比例(0-1)
    // 通常单人占总体的30-70%，过低(<10%)或过高(>90%)视为异常
    float pressure_score;
    if (relative_pressure < 0.1f)
    {
        pressure_score = 0.0f;  // 压力太低，可能无效
    }
    else if (relative_pressure > 0.9f)
    {
        pressure_score = 0.5f;  // 压力过高，可能是该部分是双人
    }
    else
    {
        // 在10%-90%范围内，压力越高评分越高
        pressure_score = (relative_pressure - 0.1f) / 0.8f;
    }

    // 2. 相对传感器评分 - 基于传感器比例(0-1)
    float sensor_score;
    if (relative_sensors < 0.1f)
    {
        sensor_score = 0.0f;  // 传感器太少，可能无效
    }
    else if (relative_sensors > 0.9f)
    {
        sensor_score = 0.5f;  // 传感器过多，可能是该部分是双人
    }
    else
    {
        // 在10%-90%范围内，传感器越多评分越高
        sensor_score = (relative_sensors - 0.1f) / 0.8f;
    }

    // 3. 连通分量评分(核心特征) - 完全基于整数判断
    float component_score = 0.0f;
    if (connected_components <= COMPONENT_SINGLE_MAX)
    {
        component_score = 0.0f;  // 明确单人或无效
    }
    else if (connected_components <= COMPONENT_GRADIENT_MAX)
    {
        component_score = (float)(connected_components - COMPONENT_SINGLE_MAX) /
                         (float)(COMPONENT_GRADIENT_MAX - COMPONENT_SINGLE_MAX);
    }
    else
    {
        component_score = 1.0f;  // 明确双人
    }

    // 4. 加权组合 - 连通分量权重最高(40%)
    float combined_score = PRESSURE_WEIGHT * pressure_score +
                          SENSOR_WEIGHT * sensor_score +
                          COMPONENT_WEIGHT * component_score;

    // 5. 分类决策 - 如果组合分数>=阈值则判为有效且为双人特征，否则为单人
    return (combined_score >= CLASSIFICATION_THRESHOLD) ? 2 : 1;
}

/* Model处理函数 - 超细化智能检测逻辑(改进版v2) - 直接变量输出 - 2025-11-06 */
int Model(const uint8_t *input)
{
    // 步骤1: 计算全局指标
    uint32_t total_pressure = calc_260_sum(input);
    uint16_t total_sensors = sensor_number(input);

    // 步骤2: 分割矩阵为上下两部分(各13行)
    uint8_t upper_matrix[260];
    uint8_t lower_matrix[260];
    split_dual_matrix(input, upper_matrix, lower_matrix);

    // 步骤3: 计算上下部分的压力和传感器数
    uint16_t upper_sensors = count_nonzero_in_upper_half(input);
    uint16_t lower_sensors = count_nonzero_in_lower_half(input);
    uint32_t upper_pressure = calc_sum_upper_half(input);
    uint32_t lower_pressure = calc_sum_lower_half(input);

    // 步骤4: 计算连通分量
    int total_components = count_connected_components(input);
    int upper_components = count_connected_components(upper_matrix);
    int lower_components = count_connected_components(lower_matrix);

    // 步骤5: 初始化所有输出变量为无效 - 直接变量方式 - 2025-11-06
    s_person_count = 0;
    s_posture_0 = 0xFF;
    s_waist_x_0 = 0xFF;
    s_waist_y_0 = 0xFF;
    s_posture_1 = 0xFF;
    s_waist_x_1 = 0xFF;
    s_waist_y_1 = 0xFF;

    // 步骤6: 空床检测
    if (total_components == 0 || total_pressure < EMPTY_BED_PRESSURE_MIN || total_sensors < EMPTY_BED_SENSORS_MIN)
    {
        return 0;  // 空床
    }

    // 步骤7: 计算分布均衡度指标 - 2025-11-06
    float min_pressure_ratio = (total_pressure > 0) ?
        ((upper_pressure < lower_pressure) ? (float)upper_pressure / (float)total_pressure : (float)lower_pressure / (float)total_pressure) : 0.0f;
    float max_pressure_ratio = (total_pressure > 0) ?
        ((upper_pressure > lower_pressure) ? (float)upper_pressure / (float)total_pressure : (float)lower_pressure / (float)total_pressure) : 0.0f;

    float min_sensor_ratio = (total_sensors > 0) ?
        ((upper_sensors < lower_sensors) ? (float)upper_sensors / (float)total_sensors : (float)lower_sensors / (float)total_sensors) : 0.0f;
    float max_sensor_ratio = (total_sensors > 0) ?
        ((upper_sensors > lower_sensors) ? (float)upper_sensors / (float)total_sensors : (float)lower_sensors / (float)total_sensors) : 0.0f;

    // 判断分布是否均衡
    int is_balanced_pressure = (min_pressure_ratio > 0.25f) && (max_pressure_ratio < 0.75f);
    int is_balanced_sensors = (min_sensor_ratio > 0.25f) && (max_sensor_ratio < 0.75f);

    int person_count = 0;

    // ======== 超细化决策逻辑 v2 - 2025-11-06 ========
    // 核心原则: 双人检测必须基于多个强信号的组合，而非仅凭分布均衡
    // 关键洞察: 单人可能有2-3个连通分量+相对均衡的分布(对角线躺卧)
    //          但双人必须同时满足: 两侧都有人体信号 + 足够的总数据量

    // ===== 双人检测 - ULTRA STRICT 条件 (ALL must be satisfied) =====
    // 核心要求: 同时满足以下ALL条件才判为双人 (AND logic, not OR)
    // 1. 两侧都有独立的连通分量 - 这是最重要的信号
    // 2. 总连通分量 >= 3 (not just >= 2, because single can have 2-3)
    // 3. 两侧压力/传感器都要充分均衡 (>25%各侧)
    // 4. 绝对值足够大: 总压力 > 7500, 总传感器 > 120
    //    (这排除了只是"噪声均衡分布"的单人样本)

    // ===== 双人检测 - MCU-Safe严格条件 - 2025-11-06 =====
    // 使用整数比较避免浮点精度问题
    // 条件1: 两侧都有独立的连通分量
    int is_dual_by_components = (upper_components > 0) && (lower_components > 0);

    // 条件2: 总连通分量 >= 3
    int is_dual_by_total = (total_components >= 3);

    // 条件3: 绝对值足够大 (使用整数比较，避免浮点误差)
    // 双人特征: 总压力 >= 7500, 总传感器 >= 120
    int is_dual_by_magnitude = (total_pressure >= 7500) && (total_sensors >= 120);

    // 条件4: 分布均衡 (已计算的浮点值)
    // is_balanced_pressure && is_balanced_sensors

    // 综合判断 - 使用显式逻辑避免编译器优化问题
    int is_true_dual = 0;
    if (is_dual_by_components && is_dual_by_total && is_balanced_pressure && is_balanced_sensors && is_dual_by_magnitude)
    {
        is_true_dual = 1;
    }

    if (is_true_dual)
    {
        // ===== 情况B: 双人处理 - 直接变量输出 - 2025-11-06 =====
        // 处理上部分
        if (upper_components > 0 && upper_pressure > 0)
        {
            float relative_pressure = (float)upper_pressure / (float)total_pressure;
            float relative_sensors = (total_sensors > 0) ? (float)upper_sensors / (float)total_sensors : 0.0f;

            // 双人时范围更严格: 20-80% (不是15-85%)
            // 使用容差值(epsilon)来处理浮点比较
            // 0.20 - epsilon < ratio < 0.80 + epsilon
            float epsilon = 0.001f;  // 0.1% 容差
            int is_valid_range = (relative_pressure > (0.20f - epsilon)) && (relative_pressure < (0.80f + epsilon)) &&
                                 (relative_sensors > (0.20f - epsilon)) && (relative_sensors < (0.80f + epsilon));

            if (is_valid_range)
            {
                int classification = hybrid_classification_relative(relative_pressure, relative_sensors, upper_components);
                if (classification > 0)
                {
                    CoM_t com_upper = calculate_com(upper_matrix);
                    uint32_t posture_upper = feature_engineering_model_run((void *)upper_matrix, NULL);

                    // 直接分配到输出变量 - 避免struct对齐问题
                    int x_val = (int)com_upper.row;
                    int y_val = (int)com_upper.col;
                    if (x_val >= 0 && x_val <= 255) s_waist_x_1 = (uint8_t)x_val;
                    if (y_val >= 0 && y_val <= 255) s_waist_y_1 = (uint8_t)y_val;
                    s_posture_1 = (uint8_t)posture_upper;
                    person_count++;
                }
            }
        }

        // 处理下部分
        if (lower_components > 0 && lower_pressure > 0)
        {
            float relative_pressure = (float)lower_pressure / (float)total_pressure;
            float relative_sensors = (total_sensors > 0) ? (float)lower_sensors / (float)total_sensors : 0.0f;

            // 同上，使用容差值处理浮点比较
            float epsilon = 0.001f;
            int is_valid_range = (relative_pressure > (0.20f - epsilon)) && (relative_pressure < (0.80f + epsilon)) &&
                                 (relative_sensors > (0.20f - epsilon)) && (relative_sensors < (0.80f + epsilon));

            if (is_valid_range)
            {
                int classification = hybrid_classification_relative(relative_pressure, relative_sensors, lower_components);
                if (classification > 0)
                {
                    CoM_t com_lower = calculate_com(lower_matrix);
                    uint32_t posture_lower = feature_engineering_model_run((void *)lower_matrix, NULL);

                    // 直接分配到输出变量 - 避免struct对齐问题
                    int x_val = (int)com_lower.row;
                    int y_val = (int)com_lower.col;
                    if (x_val >= 0 && x_val <= 255) s_waist_x_0 = (uint8_t)x_val;
                    if (y_val >= 0 && y_val <= 255) s_waist_y_0 = (uint8_t)y_val;
                    s_posture_0 = (uint8_t)posture_lower;
                    person_count++;
                }
            }
        }
    }
    else
    {
        // ===== 情况A: 单人检测 (默认fallback) - 直接变量输出 - 2025-11-06 =====
        // 任何不满足"严格双人"条件的，都尝试作为单人处理
        // 这包括: 单侧人+噪声, 低幅度信号, 不够均衡等

        // 确定单人位置：找出数据较多的那一侧
        CoM_t com_original = calculate_com(input);

        if (com_original.row < 13)
        {
            // 人更可能在上半部分
            if (upper_components > 0 && upper_sensors > 20)
            {
                float relative_pressure = (total_pressure > 0) ? (float)upper_pressure / (float)total_pressure : 0.0f;
                float relative_sensors = (total_sensors > 0) ? (float)upper_sensors / (float)total_sensors : 0.0f;

                int classification = hybrid_classification_relative(relative_pressure, relative_sensors, upper_components);
                if (classification > 0)
                {
                    CoM_t com_upper = calculate_com(upper_matrix);
                    uint32_t posture_upper = feature_engineering_model_run((void *)input, NULL);

                    // 直接分配到输出变量 - 避免struct对齐问题
                    int x_val = (int)com_upper.row;
                    int y_val = (int)com_upper.col;
                    if (x_val >= 0 && x_val <= 255) s_waist_x_1 = (uint8_t)x_val;
                    if (y_val >= 0 && y_val <= 255) s_waist_y_1 = (uint8_t)y_val;
                    s_posture_1 = (uint8_t)posture_upper;
                    person_count = 1;
                }
            }
        }
        else
        {
            // 人更可能在下半部分
            if (lower_components > 0 && lower_sensors > 20)
            {
                float relative_pressure = (total_pressure > 0) ? (float)lower_pressure / (float)total_pressure : 0.0f;
                float relative_sensors = (total_sensors > 0) ? (float)lower_sensors / (float)total_sensors : 0.0f;

                int classification = hybrid_classification_relative(relative_pressure, relative_sensors, lower_components);
                if (classification > 0)
                {
                    CoM_t com_lower = calculate_com(lower_matrix);
                    uint32_t posture_lower = feature_engineering_model_run((void *)input, NULL);

                    // 直接分配到输出变量 - 避免struct对齐问题
                    int x_val = (int)com_lower.row;
                    int y_val = (int)com_lower.col;
                    if (x_val >= 0 && x_val <= 255) s_waist_x_0 = (uint8_t)x_val;
                    if (y_val >= 0 && y_val <= 255) s_waist_y_0 = (uint8_t)y_val;
                    s_posture_0 = (uint8_t)posture_lower;
                    person_count = 1;
                }
            }
        }
    }

    // 更新全局person_count变量
    s_person_count = (uint8_t)person_count;

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
