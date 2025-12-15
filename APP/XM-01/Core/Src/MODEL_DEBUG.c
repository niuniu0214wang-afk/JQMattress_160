/*
 * MODEL_DEBUG.c - 诊断用调试代码
 * 添加到Model()函数中以诊断问题
 * 日期: 2025-11-06
 */

// ===== ADD THIS DEBUG CODE AT THE START OF Model() FUNCTION =====

#include <stdio.h>
#include <string.h>

/* 调试输出helper函数 */
static void debug_uart_print(const char *format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    // 发送到UART (替换成你的实际UART发送函数)
    HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), 100);
}

/* 在Model()函数开始处添加这些调试代码 */

// 步骤1: 打印输入数据统计
debug_uart_print("\n\n========== MODEL() DEBUG START ==========\r\n");
debug_uart_print("INPUT: 260 bytes of sensor data\r\n");

// 步骤2: 打印全局指标
debug_uart_print("\r\n[STEP 1] GLOBAL METRICS:\r\n");
debug_uart_print("  total_pressure = %lu\r\n", total_pressure);
debug_uart_print("  total_sensors = %u\r\n", total_sensors);
debug_uart_print("  total_components = %d\r\n", total_components);

// 步骤3: 打印上下部分数据
debug_uart_print("\r\n[STEP 2] UPPER/LOWER SPLIT:\r\n");
debug_uart_print("  UPPER: pressure=%lu, sensors=%u, components=%d\r\n",
                upper_pressure, upper_sensors, upper_components);
debug_uart_print("  LOWER: pressure=%lu, sensors=%u, components=%d\r\n",
                lower_pressure, lower_sensors, lower_components);

// 步骤4: 打印比例计算
float min_p = (upper_pressure < lower_pressure) ? (float)upper_pressure/(float)total_pressure : (float)lower_pressure/(float)total_pressure;
float max_p = (upper_pressure > lower_pressure) ? (float)upper_pressure/(float)total_pressure : (float)lower_pressure/(float)total_pressure;
float min_s = (upper_sensors < lower_sensors) ? (float)upper_sensors/(float)total_sensors : (float)lower_sensors/(float)total_sensors;
float max_s = (upper_sensors > lower_sensors) ? (float)upper_sensors/(float)total_sensors : (float)lower_sensors/(float)total_sensors;

debug_uart_print("\r\n[STEP 3] DISTRIBUTION BALANCE:\r\n");
debug_uart_print("  Pressure: min=%.3f, max=%.3f (need 0.25 < min AND max < 0.75)\r\n", min_p, max_p);
debug_uart_print("  Sensors:  min=%.3f, max=%.3f (need 0.25 < min AND max < 0.75)\r\n", min_s, max_s);

int is_balanced_p = (min_p > 0.25f) && (max_p < 0.75f);
int is_balanced_s = (min_s > 0.25f) && (max_s < 0.75f);
debug_uart_print("  is_balanced_pressure = %d\r\n", is_balanced_p);
debug_uart_print("  is_balanced_sensors = %d\r\n", is_balanced_s);

// 步骤5: 打印双人检测条件
debug_uart_print("\r\n[STEP 4] DUAL-PERSON DETECTION CONDITIONS:\r\n");
int cond1 = (upper_components > 0) && (lower_components > 0);
int cond2 = (total_components >= 3);
int cond3 = (total_pressure >= 7500) && (total_sensors >= 120);

debug_uart_print("  1. Both sides have components: %d (upper=%d, lower=%d)\r\n",
                cond1, upper_components, lower_components);
debug_uart_print("  2. Total components >= 3: %d (actual=%d)\r\n",
                cond2, total_components);
debug_uart_print("  3. Magnitude sufficient: %d (pressure=%lu >= 7500, sensors=%u >= 120)\r\n",
                cond3, total_pressure, total_sensors);
debug_uart_print("  4. Pressure balanced: %d\r\n", is_balanced_p);
debug_uart_print("  5. Sensors balanced: %d\r\n", is_balanced_s);

int is_true_dual_check = cond1 && cond2 && is_balanced_p && is_balanced_s && cond3;
debug_uart_print("\r\n  COMBINED: is_true_dual = %d\r\n", is_true_dual_check);

// 步骤6: 打印决策路径
debug_uart_print("\r\n[STEP 5] DECISION PATH:\r\n");
if (is_true_dual_check) {
    debug_uart_print("  → DUAL-PERSON PATH (情况B)\r\n");
} else {
    debug_uart_print("  → SINGLE-PERSON PATH (情况A)\r\n");
}

// 步骤7: 最终结果 (在函数末尾添加)
debug_uart_print("\r\n[FINAL] RESULT:\r\n");
debug_uart_print("  person_count = %d\r\n", person_count);
debug_uart_print("  body[0].posture = 0x%02X\r\n", analysis_result.bodies[0].posture);
debug_uart_print("  body[1].posture = 0x%02X\r\n", analysis_result.bodies[1].posture);
debug_uart_print("========== MODEL() DEBUG END ==========\r\n\n");

// ===== END DEBUG CODE =====
