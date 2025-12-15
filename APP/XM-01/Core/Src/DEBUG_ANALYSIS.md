# 人数分类调试分析 - Debug Analysis for Person Count Classification
Date: 2025-11-06

## 问题描述 (Problem Statement)

C实现的Model()函数与Python参考实现产生不同结果。需要找出差异原因。

Debug Output:
```
01 06 04 FF FF FF -- FF FF FF FF FF FF -- 02 06 04 02 14 04
```

---

## 第一步: 理解输出格式 (Step 1: Understand Output Format)

需要确认 `analysis_result_t` 的确切结构:

```c
// 在 mcu_body_analyzer.h 中查找:
typedef struct {
    // 可能的字段:
    uint8_t person_count;           // 人数
    // body[0]:
    uint8_t posture_0;              // 睡姿 (0x01 = 仰卧, 0x02 = 侧卧, 0xFF = 无效)
    uint8_t x_0;                    // X坐标或其他
    uint8_t y_0;                    // Y坐标或其他
    // ... 更多字段
    // body[1]:
    uint8_t posture_1;
    // ...
} AnalysisResult_t;
```

**关键问题:** 输出格式到底是什么? 需要查看打印代码。

---

## 第二步: 收集完整的调试数据 (Step 2: Collect Complete Debug Data)

### 输入数据 (Input)
```
// 你的test input 260字节 (16进制格式)
完整的260字节数据
```

### 预期输出 (Expected - from Python)
```python
# 运行 Python test 看输出:
python test_person_classification.py
# 得到: person_count=?, posture=?
```

### 实际输出 (Actual - from C)
```
完整的分析结果 (格式化输出)
```

---

## 第三步: 检查C实现的关键计算 (Step 3: Check C Implementation)

### 需要验证的点:

1. **连通分量计算 (Connected Components)**
   - Python用: `scipy.ndimage.label()` - 使用4-连通法
   - C用: `count_connected_components()` - flood-fill算法
   - ❓ 两者使用相同的连通性定义吗? (4-connected vs 8-connected)
   - ❓ 二值化阈值相同吗? (ACTIVITY_THRESHOLD = 10)

2. **矩阵分割 (Matrix Splitting)**
   - 上部分: 前13行 (行0-12)
   - 下部分: 后13行 (行13-25)
   - ❓ 是否有off-by-one错误?

3. **压力和/传感器数计算**
   - Python: `np.sum()`, `np.count_nonzero()`
   - C: 循环求和
   - ❓ 数据类型溢出? (uint32_t vs uint16_t)

4. **浮点计算精度**
   - Python: Python float (64-bit double)
   - C: float (32-bit)
   - ❓ 是否有precision loss导致不同的比较结果?

5. **阈值判断**
   - Python: `total_pressure >= 7500 and total_sensors >= 120`
   - C: `(total_pressure >= 7500) && (total_sensors >= 120)`
   - ❓ 比较逻辑相同吗?

---

## 第四步: 添加详细的调试输出 (Step 4: Add Debug Output)

在C代码Model()函数中添加:

```c
/* 调试输出 - 添加到Model()函数 */
char debug_buf[512];

// 第1行: 总体指标
sprintf(debug_buf, "DEBUG: total_pressure=%lu, total_sensors=%u, total_components=%d\r\n",
        total_pressure, total_sensors, total_components);
HAL_UART_Transmit(&huart3, (uint8_t*)debug_buf, strlen(debug_buf), 100);

// 第2行: 上下部分
sprintf(debug_buf, "DEBUG: upper_p=%lu upper_s=%u upper_c=%d | lower_p=%lu lower_s=%u lower_c=%d\r\n",
        upper_pressure, upper_sensors, upper_components,
        lower_pressure, lower_sensors, lower_components);
HAL_UART_Transmit(&huart3, (uint8_t*)debug_buf, strlen(debug_buf), 100);

// 第3行: 比例
float up_p_ratio = (total_pressure > 0) ? (float)upper_pressure / (float)total_pressure : 0;
float lo_p_ratio = (total_pressure > 0) ? (float)lower_pressure / (float)total_pressure : 0;
float up_s_ratio = (total_sensors > 0) ? (float)upper_sensors / (float)total_sensors : 0;
float lo_s_ratio = (total_sensors > 0) ? (float)lower_sensors / (float)total_sensors : 0;
sprintf(debug_buf, "DEBUG: up_p_ratio=%.3f lo_p_ratio=%.3f up_s_ratio=%.3f lo_s_ratio=%.3f\r\n",
        up_p_ratio, lo_p_ratio, up_s_ratio, lo_s_ratio);
HAL_UART_Transmit(&huart3, (uint8_t*)debug_buf, strlen(debug_buf), 100);

// 第4行: 判断条件
int is_balanced_pressure = (min_pressure_ratio > 0.25f) && (max_pressure_ratio < 0.75f);
int is_balanced_sensors = (min_sensor_ratio > 0.25f) && (max_sensor_ratio < 0.75f);
sprintf(debug_buf, "DEBUG: balanced_p=%d balanced_s=%d components_gte3=%d magnitude=%d\r\n",
        is_balanced_pressure, is_balanced_sensors, total_components >= 3,
        (total_pressure >= 7500) && (total_sensors >= 120));
HAL_UART_Transmit(&huart3, (uint8_t*)debug_buf, strlen(debug_buf), 100);

// 第5行: 决策路径
sprintf(debug_buf, "DEBUG: is_true_dual=%d person_count=%d\r\n", is_true_dual, person_count);
HAL_UART_Transmit(&huart3, (uint8_t*)debug_buf, strlen(debug_buf), 100);

// 第6行: 最终结果
sprintf(debug_buf, "DEBUG: body0_posture=0x%02x body1_posture=0x%02x\r\n",
        analysis_result.bodies[0].posture, analysis_result.bodies[1].posture);
HAL_UART_Transmit(&huart3, (uint8_t*)debug_buf, strlen(debug_buf), 100);
```

---

## 第五步: 对比分析 (Step 5: Comparative Analysis)

运行以下检查清单:

### Python参考实现:
```python
# 在test_person_classification.py中添加:
sample_data = [...]  # 你的test数据
result = detect_person_count(np.array(sample_data))
print(f"Python Result: {result}")
print(f"  total_pressure: {calc_260_sum(sample_data)}")
print(f"  total_sensors: {sensor_number(sample_data)}")
print(f"  total_components: {count_connected_components(sample_data)}")
```

### C实现:
```c
// 运行Model()并收集调试输出
// 与Python结果逐个对比
```

---

## 常见差异来源 (Common Divergence Sources)

| 源头 | Python | C | 可能差异 |
|------|--------|---|---------|
| 连通分量 | scipy 4-connected | flood-fill | 不同的分量数 |
| 浮点精度 | 64-bit double | 32-bit float | 比较结果不同 |
| 整数除法 | `float(a)/float(b)` | `(float)a/(float)b` | Precision loss |
| 二值化 | `> THRESHOLD` | `> THRESHOLD` | 应该相同 |
| 数组索引 | 0-based | 0-based | 应该相同 |
| 整数溢出 | 不会 (Python) | 可能 (C) | 错误的压力和 |

---

## 调试清单 (Debug Checklist)

### [ ] 确认输出格式
- [ ] 查看mcu_body_analyzer.h中的AnalysisResult_t定义
- [ ] 确认打印代码的顺序和格式

### [ ] 收集测试数据
- [ ] 选择一个具体的测试样本
- [ ] 从Python获得期望结果
- [ ] 从C获得实际结果

### [ ] 逐步调试
- [ ] 添加详细调试输出
- [ ] 比较每个中间值
- [ ] 找出第一个不同的地方

### [ ] 验证算法
- [ ] 连通分量计算方式
- [ ] 矩阵分割逻辑
- [ ] 浮点比较精度

### [ ] 修复差异
- [ ] 根据差异原因修改C代码
- [ ] 重新测试
- [ ] 与Python结果对齐

---

## 下一步 (Next Steps)

请提供以下信息:

1. **输入数据** (260字节) - 十六进制或十进制格式
2. **Python输出** - `detect_person_count()` 的返回值和中间计算值
3. **C输出** - 完整的hex dump和中间调试值
4. **mcu_body_analyzer.h** - AnalysisResult_t的结构定义
5. **打印代码** - 如何生成hex output的代码

根据这些信息，我可以精确定位问题所在。
