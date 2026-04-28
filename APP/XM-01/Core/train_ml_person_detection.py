# 机器学习模型训练 - 人数检测
# Machine Learning Model for Person Detection
# 日期: 2026-03-03

import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split, cross_val_score, GridSearchCV
from sklearn.ensemble import RandomForestClassifier, GradientBoostingClassifier
from sklearn.svm import SVC
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import classification_report, confusion_matrix, accuracy_score
import matplotlib.pyplot as plt
import seaborn as sns
import joblib

# 设置中文字体
plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei']
plt.rcParams['axes.unicode_minus'] = False

class PersonDetectionModel:
    """人数检测机器学习模型"""

    def __init__(self):
        self.scaler = StandardScaler()
        self.model = None
        self.feature_names = []

    def extract_features(self, sensor_data_str):
        """
        从传感器数据中提取特征

        特征包括:
        1. 全局统计特征 (总压力、总传感器数、平均值等)
        2. 区域特征 (上下分布、左右分布)
        3. 形状特征 (连通性、分散度)
        4. 压力分布特征 (方差、峰值等)
        """
        bytes_list = sensor_data_str.split()
        if len(bytes_list) < 260:
            return None

        values = np.array([int(b, 16) for b in bytes_list[:260]])
        matrix = values.reshape(26, 10)

        features = {}

        # === 1. 全局统计特征 ===
        threshold = 50
        active_mask = matrix > threshold

        features['total_pressure'] = np.sum(matrix)
        features['total_sensors'] = np.sum(active_mask)
        features['mean_pressure'] = np.mean(matrix[active_mask]) if np.any(active_mask) else 0
        features['max_pressure'] = np.max(matrix)
        features['std_pressure'] = np.std(matrix[active_mask]) if np.any(active_mask) else 0
        features['pressure_variance'] = np.var(matrix)

        # === 2. 上下区域特征 (按行分割) ===
        upper = matrix[:13, :]  # 前13行
        lower = matrix[13:, :]  # 后13行

        upper_active = upper > threshold
        lower_active = lower > threshold

        features['upper_pressure'] = np.sum(upper)
        features['lower_pressure'] = np.sum(lower)
        features['upper_sensors'] = np.sum(upper_active)
        features['lower_sensors'] = np.sum(lower_active)

        # 上下比例
        if features['total_pressure'] > 0:
            features['upper_pressure_ratio'] = features['upper_pressure'] / features['total_pressure']
            features['lower_pressure_ratio'] = features['lower_pressure'] / features['total_pressure']
        else:
            features['upper_pressure_ratio'] = 0
            features['lower_pressure_ratio'] = 0

        if features['total_sensors'] > 0:
            features['upper_sensor_ratio'] = features['upper_sensors'] / features['total_sensors']
            features['lower_sensor_ratio'] = features['lower_sensors'] / features['total_sensors']
        else:
            features['upper_sensor_ratio'] = 0
            features['lower_sensor_ratio'] = 0

        # 上下均衡度
        features['pressure_balance'] = min(features['upper_pressure_ratio'], features['lower_pressure_ratio'])
        features['sensor_balance'] = min(features['upper_sensor_ratio'], features['lower_sensor_ratio'])

        # 上下差异
        features['pressure_diff'] = abs(features['upper_pressure'] - features['lower_pressure'])
        features['sensor_diff'] = abs(features['upper_sensors'] - features['lower_sensors'])

        # === 3. 左右区域特征 (按列分割) ===
        left = matrix[:, :5]   # 前5列
        right = matrix[:, 5:]  # 后5列

        left_active = left > threshold
        right_active = right > threshold

        features['left_pressure'] = np.sum(left)
        features['right_pressure'] = np.sum(right)
        features['left_sensors'] = np.sum(left_active)
        features['right_sensors'] = np.sum(right_active)

        # === 4. 四象限特征 ===
        q1 = matrix[:13, :5]   # 左上
        q2 = matrix[:13, 5:]   # 右上
        q3 = matrix[13:, :5]   # 左下
        q4 = matrix[13:, 5:]   # 右下

        features['q1_pressure'] = np.sum(q1)
        features['q2_pressure'] = np.sum(q2)
        features['q3_pressure'] = np.sum(q3)
        features['q4_pressure'] = np.sum(q4)

        # 四象限活跃数
        features['active_quadrants'] = sum([
            np.sum(q1 > threshold) > 10,
            np.sum(q2 > threshold) > 10,
            np.sum(q3 > threshold) > 10,
            np.sum(q4 > threshold) > 10
        ])

        # === 5. 连通性特征 (简化版) ===
        # 估算连通分量数量
        features['estimated_components'] = self._estimate_components(matrix, threshold)

        # === 6. 形状特征 ===
        if np.any(active_mask):
            # 质心位置
            y_coords, x_coords = np.where(active_mask)
            features['centroid_y'] = np.mean(y_coords)
            features['centroid_x'] = np.mean(x_coords)

            # 分散度
            features['spread_y'] = np.std(y_coords)
            features['spread_x'] = np.std(x_coords)

            # 覆盖范围
            features['range_y'] = np.max(y_coords) - np.min(y_coords)
            features['range_x'] = np.max(x_coords) - np.min(x_coords)
        else:
            features['centroid_y'] = 0
            features['centroid_x'] = 0
            features['spread_y'] = 0
            features['spread_x'] = 0
            features['range_y'] = 0
            features['range_x'] = 0

        # === 7. 峰值特征 ===
        # 找到压力峰值的位置
        if features['max_pressure'] > threshold:
            peak_positions = np.where(matrix == features['max_pressure'])
            features['num_peaks'] = len(peak_positions[0])
            features['peak_y'] = np.mean(peak_positions[0])
            features['peak_x'] = np.mean(peak_positions[1])
        else:
            features['num_peaks'] = 0
            features['peak_y'] = 0
            features['peak_x'] = 0

        # === 8. 密度特征 ===
        if features['total_sensors'] > 0:
            features['density'] = features['total_pressure'] / features['total_sensors']
        else:
            features['density'] = 0

        # 上下密度
        if features['upper_sensors'] > 0:
            features['upper_density'] = features['upper_pressure'] / features['upper_sensors']
        else:
            features['upper_density'] = 0

        if features['lower_sensors'] > 0:
            features['lower_density'] = features['lower_pressure'] / features['lower_sensors']
        else:
            features['lower_density'] = 0

        return features

    def _estimate_components(self, matrix, threshold):
        """简化的连通分量估算"""
        active = matrix > threshold
        if not np.any(active):
            return 0

        # 使用简单的区域计数
        # 检查上下两侧是否都有活跃区域
        upper_active = np.any(active[:13, :])
        lower_active = np.any(active[13:, :])

        if upper_active and lower_active:
            # 检查是否有明显的分离
            middle_row = active[12:14, :]
            if np.sum(middle_row) < 3:  # 中间行几乎没有活跃传感器
                return 2
            else:
                return 1
        else:
            return 1

    def prepare_dataset(self, df):
        """准备训练数据集"""
        print("提取特征...")
        X_list = []
        y_list = []

        for idx, row in df.iterrows():
            features = self.extract_features(row['sensor_data'])
            if features is not None:
                X_list.append(features)
                y_list.append(row['true_num_persons'])

            if (idx + 1) % 100 == 0:
                print(f"  处理进度: {idx+1}/{len(df)}")

        # 转换为DataFrame
        X = pd.DataFrame(X_list)
        y = np.array(y_list)

        self.feature_names = X.columns.tolist()

        print(f"\n特征提取完成!")
        print(f"  特征数量: {len(self.feature_names)}")
        print(f"  样本数量: {len(X)}")

        return X, y

    def train(self, X, y):
        """训练模型"""
        print("\n" + "="*80)
        print("训练模型")
        print("="*80)

        # 划分训练集和测试集
        X_train, X_test, y_train, y_test = train_test_split(
            X, y, test_size=0.2, random_state=42, stratify=y
        )

        print(f"\n训练集: {len(X_train)}样本")
        print(f"测试集: {len(X_test)}样本")

        # 标准化
        X_train_scaled = self.scaler.fit_transform(X_train)
        X_test_scaled = self.scaler.transform(X_test)

        # 测试多个模型
        models = {
            'Random Forest': RandomForestClassifier(n_estimators=100, random_state=42, max_depth=10),
            'Gradient Boosting': GradientBoostingClassifier(n_estimators=100, random_state=42, max_depth=5),
            'SVM': SVC(kernel='rbf', random_state=42, probability=True)
        }

        best_model = None
        best_score = 0
        best_name = None

        for name, model in models.items():
            print(f"\n训练 {name}...")
            model.fit(X_train_scaled, y_train)

            # 交叉验证
            cv_scores = cross_val_score(model, X_train_scaled, y_train, cv=5)
            print(f"  交叉验证准确率: {cv_scores.mean()*100:.2f}% (+/- {cv_scores.std()*100:.2f}%)")

            # 测试集评估
            y_pred = model.predict(X_test_scaled)
            test_acc = accuracy_score(y_test, y_pred)
            print(f"  测试集准确率: {test_acc*100:.2f}%")

            if test_acc > best_score:
                best_score = test_acc
                best_model = model
                best_name = name

        print(f"\n最佳模型: {best_name} (准确率: {best_score*100:.2f}%)")
        self.model = best_model

        # 详细评估
        y_pred = self.model.predict(X_test_scaled)
        print("\n分类报告:")
        print(classification_report(y_test, y_pred, target_names=['1人', '2人']))

        print("\n混淆矩阵:")
        cm = confusion_matrix(y_test, y_pred)
        print("真实\\预测    1人      2人")
        print(f"1人:      {cm[0,0]:6d}   {cm[0,1]:6d}")
        print(f"2人:      {cm[1,0]:6d}   {cm[1,1]:6d}")

        # 特征重要性
        if hasattr(self.model, 'feature_importances_'):
            self.plot_feature_importance(X.columns)

        return X_test, y_test, y_pred

    def plot_feature_importance(self, feature_names):
        """绘制特征重要性"""
        importances = self.model.feature_importances_
        indices = np.argsort(importances)[::-1][:20]  # Top 20

        plt.figure(figsize=(12, 8))
        plt.title('Top 20 特征重要性')
        plt.barh(range(20), importances[indices])
        plt.yticks(range(20), [feature_names[i] for i in indices])
        plt.xlabel('重要性')
        plt.tight_layout()
        plt.savefig('feature_importance.png', dpi=300, bbox_inches='tight')
        print("\n特征重要性图已保存: feature_importance.png")

    def save_model(self, filename='person_detection_model.pkl'):
        """保存模型"""
        joblib.dump({
            'model': self.model,
            'scaler': self.scaler,
            'feature_names': self.feature_names
        }, filename)
        print(f"\n模型已保存: {filename}")

    def load_model(self, filename='person_detection_model.pkl'):
        """加载模型"""
        data = joblib.load(filename)
        self.model = data['model']
        self.scaler = data['scaler']
        self.feature_names = data['feature_names']
        print(f"模型已加载: {filename}")

    def predict(self, sensor_data_str):
        """预测单个样本"""
        features = self.extract_features(sensor_data_str)
        if features is None:
            return None

        X = pd.DataFrame([features])[self.feature_names]
        X_scaled = self.scaler.transform(X)
        prediction = self.model.predict(X_scaled)[0]
        probability = self.model.predict_proba(X_scaled)[0]

        return {
            'prediction': int(prediction),
            'probability': probability,
            'confidence': max(probability)
        }


def main():
    # 加载数据
    print("加载数据...")
    df = pd.read_csv(r'C:\Users\Lenovo\Desktop\0225JQ_T12\parsed_sensor_data.csv', encoding='utf-8-sig')
    print(f"总样本数: {len(df)}")
    print(f"单人样本: {len(df[df['true_num_persons']==1])}")
    print(f"双人样本: {len(df[df['true_num_persons']==2])}")

    # 创建模型
    model = PersonDetectionModel()

    # 准备数据
    X, y = model.prepare_dataset(df)

    # 训练模型
    X_test, y_test, y_pred = model.train(X, y)

    # 保存模型
    model.save_model()

    # 与当前MCU对比
    print("\n" + "="*80)
    print("与当前MCU对比")
    print("="*80)

    mcu_acc = accuracy_score(df['true_num_persons'], df['pred_num_persons'])
    ml_acc = accuracy_score(y_test, y_pred)

    print(f"\n当前MCU准确率: {mcu_acc*100:.2f}%")
    print(f"机器学习模型准确率: {ml_acc*100:.2f}%")
    print(f"提升: {(ml_acc - mcu_acc)*100:.2f}个百分点")

    # 测试几个样本
    print("\n" + "="*80)
    print("样本预测测试")
    print("="*80)

    for i in range(5):
        sample = df.iloc[i]
        result = model.predict(sample['sensor_data'])
        print(f"\n样本 {i+1}:")
        print(f"  真实: {sample['true_num_persons']}人")
        print(f"  MCU预测: {sample['pred_num_persons']}人")
        print(f"  ML预测: {result['prediction']}人 (置信度: {result['confidence']*100:.1f}%)")

    print("\n训练完成!")


if __name__ == '__main__':
    main()
