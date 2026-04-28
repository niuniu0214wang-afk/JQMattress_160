# CNN人数检测模型评估与训练方案
# CNN-Based Person Detection Model Evaluation
# 日期: 2026-03-03

## 一、可行性评估

### 1.1 使用CNN进行人数检测的优势

**优势:**
1. **端到端学习:** 直接从传感器数据学习人数特征，无需手工设计规则
2. **鲁棒性强:** 对噪声和边界情况有更好的泛化能力
3. **统一架构:** 可以同时预测人数和睡姿，简化系统
4. **自适应:** 通过训练数据自动学习最优特征

**劣势:**
1. **需要更多数据:** 当前952个样本可能不够
2. **计算资源:** STM32需要足够的Flash和RAM
3. **推理时间:** 可能比规则方法慢

### 1.2 当前数据集分析

**数据量:**
- 总样本: 952个
- 单人: 592个 (62.18%)
- 双人: 360个 (37.82%)

**数据质量:**
- 覆盖场景: 10个不同场景
- 姿势多样性: 平躺、左侧卧、右侧卧
- 数据不平衡: 单人:双人 = 1.64:1

**结论:** 数据量勉强够用，但建议增加到1500+样本以获得更好效果

---

## 二、CNN模型架构设计

### 2.1 多任务学习架构 (推荐)

```
输入: 26x10 传感器矩阵 (260字节)
  ↓
卷积层1: 16个3x3卷积核 + ReLU + MaxPool(2x2)
  ↓ (13x5x16)
卷积层2: 32个3x3卷积核 + ReLU + MaxPool(2x2)
  ↓ (6x2x32)
展平层: 384维
  ↓
全连接层: 128维 + ReLU + Dropout(0.3)
  ↓
分支1: 人数分类 (3类: 0人/1人/2人)
  ├─ FC: 3维 + Softmax
  └─ 输出: [P(0人), P(1人), P(2人)]

分支2: 左侧睡姿 (3类: 平躺/侧卧/无人)
  ├─ FC: 3维 + Softmax
  └─ 输出: [P(平躺), P(侧卧), P(无人)]

分支3: 右侧睡姿 (3类: 平躺/侧卧/无人)
  ├─ FC: 3维 + Softmax
  └─ 输出: [P(平躺), P(侧卧), P(无人)]
```

**模型参数估算:**
- 卷积层1: 16 * (3*3*1 + 1) = 160参数
- 卷积层2: 32 * (3*3*16 + 1) = 4,640参数
- 全连接层: 384 * 128 + 128 = 49,280参数
- 分支1: 128 * 3 + 3 = 387参数
- 分支2: 128 * 3 + 3 = 387参数
- 分支3: 128 * 3 + 3 = 387参数
- **总计: ~55K参数 (~220KB模型大小)**

**STM32兼容性:** ✓ 可行 (STM32F405有1MB Flash)

### 2.2 简化架构 (仅人数检测)

如果只做人数检测:

```
输入: 26x10 传感器矩阵
  ↓
卷积层1: 8个3x3卷积核 + ReLU + MaxPool
  ↓
卷积层2: 16个3x3卷积核 + ReLU + MaxPool
  ↓
全连接层: 64维 + ReLU
  ↓
输出层: 3维 (0人/1人/2人) + Softmax
```

**模型参数:** ~15K参数 (~60KB)

---

## 三、训练数据准备

### 3.1 使用现有数据

**步骤1: 数据预处理**

创建训练脚本 `prepare_training_data.py`:

```python
import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split

# 加载数据
df = pd.read_csv('parsed_sensor_data.csv')

# 准备输入数据 (sensor_data转为26x10矩阵)
X = []
for idx, row in df.iterrows():
    sensor_bytes = row['sensor_data'].split()
    sensor_values = [int(b, 16) for b in sensor_bytes[:260]]
    matrix = np.array(sensor_values).reshape(26, 10)
    X.append(matrix)

X = np.array(X)

# 准备标签
y_num_persons = df['true_num_persons'].values
y_left_posture = df['true_left_posture'].values
y_right_posture = df['true_right_posture'].values

# 转换right_posture的'FF'为2 (无人类别)
y_right_posture = np.array([2 if x == 'FF' else x for x in y_right_posture])

# 划分训练集和测试集
X_train, X_test, y_num_train, y_num_test = train_test_split(
    X, y_num_persons, test_size=0.2, random_state=42, stratify=y_num_persons
)

print(f"训练集: {len(X_train)}样本")
print(f"测试集: {len(X_test)}样本")
```

### 3.2 数据增强

为了增加数据量，可以使用:

```python
def augment_data(matrix):
    """数据增强"""
    augmented = []

    # 原始数据
    augmented.append(matrix)

    # 水平翻转
    augmented.append(np.fliplr(matrix))

    # 添加小噪声
    noise = np.random.normal(0, 2, matrix.shape)
    augmented.append(np.clip(matrix + noise, 0, 255))

    # 轻微缩放
    scaled = matrix * np.random.uniform(0.95, 1.05)
    augmented.append(np.clip(scaled, 0, 255))

    return augmented

# 应用数据增强
X_train_aug = []
y_num_train_aug = []

for i in range(len(X_train)):
    aug_samples = augment_data(X_train[i])
    X_train_aug.extend(aug_samples)
    y_num_train_aug.extend([y_num_train[i]] * len(aug_samples))

X_train_aug = np.array(X_train_aug)
y_num_train_aug = np.array(y_num_train_aug)

print(f"增强后训练集: {len(X_train_aug)}样本")
```

---

## 四、模型训练

### 4.1 TensorFlow/Keras实现

创建 `train_person_detection_cnn.py`:

```python
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import numpy as np

# 构建模型
def build_multitask_model(input_shape=(26, 10, 1)):
    inputs = keras.Input(shape=input_shape)

    # 卷积层
    x = layers.Conv2D(16, (3, 3), activation='relu', padding='same')(inputs)
    x = layers.MaxPooling2D((2, 2))(x)

    x = layers.Conv2D(32, (3, 3), activation='relu', padding='same')(x)
    x = layers.MaxPooling2D((2, 2))(x)

    # 展平
    x = layers.Flatten()(x)
    x = layers.Dense(128, activation='relu')(x)
    x = layers.Dropout(0.3)(x)

    # 分支1: 人数检测
    num_persons = layers.Dense(3, activation='softmax', name='num_persons')(x)

    # 分支2: 左侧睡姿
    left_posture = layers.Dense(3, activation='softmax', name='left_posture')(x)

    # 分支3: 右侧睡姿
    right_posture = layers.Dense(3, activation='softmax', name='right_posture')(x)

    model = keras.Model(inputs=inputs,
                       outputs=[num_persons, left_posture, right_posture])

    return model

# 创建模型
model = build_multitask_model()

# 编译模型
model.compile(
    optimizer='adam',
    loss={
        'num_persons': 'sparse_categorical_crossentropy',
        'left_posture': 'sparse_categorical_crossentropy',
        'right_posture': 'sparse_categorical_crossentropy'
    },
    loss_weights={
        'num_persons': 2.0,  # 人数检测权重更高
        'left_posture': 1.0,
        'right_posture': 1.0
    },
    metrics=['accuracy']
)

model.summary()

# 准备数据
X_train_norm = X_train_aug.reshape(-1, 26, 10, 1) / 255.0
X_test_norm = X_test.reshape(-1, 26, 10, 1) / 255.0

# 训练模型
history = model.fit(
    X_train_norm,
    {
        'num_persons': y_num_train_aug,
        'left_posture': y_left_train_aug,
        'right_posture': y_right_train_aug
    },
    validation_data=(
        X_test_norm,
        {
            'num_persons': y_num_test,
            'left_posture': y_left_test,
            'right_posture': y_right_test
        }
    ),
    epochs=50,
    batch_size=32,
    callbacks=[
        keras.callbacks.EarlyStopping(patience=10, restore_best_weights=True),
        keras.callbacks.ReduceLROnPlateau(patience=5, factor=0.5)
    ]
)

# 评估模型
results = model.evaluate(X_test_norm, {
    'num_persons': y_num_test,
    'left_posture': y_left_test,
    'right_posture': y_right_test
})

print("\n测试集结果:")
print(f"人数检测准确率: {results[4]*100:.2f}%")
print(f"左侧睡姿准确率: {results[5]*100:.2f}%")
print(f"右侧睡姿准确率: {results[6]*100:.2f}%")

# 保存模型
model.save('person_detection_multitask.h5')
```

### 4.2 转换为X-CUBE-AI格式

训练完成后，转换为STM32可用格式:

```bash
# 使用X-CUBE-AI工具转换
# 1. 在STM32CubeMX中打开X-CUBE-AI
# 2. 导入 person_detection_multitask.h5
# 3. 验证模型
# 4. 生成C代码
```

---

## 五、预期效果评估

### 5.1 性能预测

基于当前数据量和模型复杂度:

| 指标 | 规则方法(优化后) | CNN方法(预期) |
|------|-----------------|--------------|
| 人数检测准确率 | 75-80% | 85-90% |
| 单人检测准确率 | 85% | 90-95% |
| 双人检测准确率 | 95% | 90-95% |
| 推理时间 | ~5ms | ~15ms |
| Flash占用 | ~50KB | ~250KB |
| RAM占用 | ~10KB | ~30KB |

### 5.2 优缺点对比

**规则方法优势:**
- 推理速度快
- 资源占用少
- 可解释性强
- 易于调试

**CNN方法优势:**
- 准确率更高
- 泛化能力强
- 自动学习特征
- 对边界情况鲁棒

---

## 六、推荐方案

### 方案A: 混合方案 (推荐)

1. **第一阶段:** 使用优化后的规则方法 (当前方案)
   - 快速部署，立即见效
   - 准确率提升到75-80%

2. **第二阶段:** 收集更多数据 (目标1500+样本)
   - 补充单人场景数据
   - 平衡数据集

3. **第三阶段:** 训练CNN模型
   - 使用多任务学习
   - 达到90%+准确率

### 方案B: 纯CNN方案

如果你有足够的数据和时间:
1. 立即开始训练CNN
2. 使用数据增强扩充数据集
3. 部署到STM32

---

## 七、实施建议

### 当前最佳策略:

1. **立即执行:** 按照 `STEP_BY_STEP_OPTIMIZATION.md` 优化规则方法
2. **并行准备:** 开始准备CNN训练数据和脚本
3. **数据收集:** 在优化规则方法的同时，收集更多训练数据
4. **模型训练:** 数据量达到1500+后，训练CNN模型
5. **A/B测试:** 对比规则方法和CNN方法的效果
6. **最终部署:** 选择效果最好的方案

### 时间规划:

- Week 1: 优化规则方法 + 开始数据收集
- Week 2-3: 继续数据收集 + 准备训练脚本
- Week 4: 训练CNN模型
- Week 5: 测试和对比
- Week 6: 部署最优方案

---

## 八、结论

**CNN人数检测是可行的，但建议分阶段实施:**

1. ✅ **短期 (1周):** 优化规则方法 → 75-80%准确率
2. ✅ **中期 (1个月):** 收集数据 + 训练CNN → 85-90%准确率
3. ✅ **长期 (持续):** 持续优化和迭代 → 95%+准确率

**当前行动:** 先执行规则优化，同时准备CNN训练环境。

---

文档创建日期: 2026-03-03
