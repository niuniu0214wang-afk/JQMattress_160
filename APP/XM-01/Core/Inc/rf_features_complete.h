/**
 * @file rf_features_complete_CORRECTED.h
 * @brief Complete Python-compatible 41-feature extraction for Random Forest - CORRECTED ORDER
 *
 * 完整的Python兼容41特征提取 - 与Python特征工程完全匹配 - 已修正顺序
 * Extracts all 41 selected features in EXACT Python order
 *
 * 生成日期: 2025-11-06
 * 修改日期: 2025-12-16 - 修正特征顺序以匹配Python训练代码 (2025-12-16)
 *
 * CRITICAL FIX (2025-12-16):
 * - Corrected feature extraction order to EXACTLY match Python training code
 * - Reference: retrain_20_trees_improved.py::_extract_all_41_features() lines 154-395
 * - Previous version had completely wrong feature order (only 1/41 matched!)
 * - Now features are extracted in same order as Python for correct RF inference
 *
 * Python Feature Order (retrain_20_trees_improved.py lines 169-393):
 * 0. activation_ratio (171-172)
 * 1. active_sensor_count (174-175)
 * 2. activation_clusters (177-179)
 * 3. max_cluster_size (181-187)
 * 4. activation_spread_x (189-197)
 * 5. activation_spread_y (189-197)
 * 6. coefficient_of_variation (200-203)
 * 7. normalized_pressure_range (205-207)
 * 8. mean_relative_deviation (209-211)
 * 9. median_relative_deviation (213-216)
 * 10. pressure_distribution_skewness (218-220)
 * 11. pressure_distribution_kurtosis (222-224)
 * 12. pressure_entropy (226-230)
 * 13. pressure_gini_coefficient (232-237)
 * 14. mean_gradient_x (240-248)
 * 15. std_gradient_x (240-248)
 * 16. mean_gradient_y (250-258)
 * 17. std_gradient_y (250-258)
 * 18. gradient_directionality (260-264)
 * 19. laplacian_mean (266-268)
 * 20. laplacian_std (269-270)
 * 21. laplacian_of_gaussian (272-276)
 * 22. num_connected_components (279-281)
 * 23. component_size_mean (283-297)
 * 24. component_size_std (283-297)
 * 25. component_size_cv (283-297)
 * 26. isolated_sensors (299-308)
 * 27. skeleton_ratio (310-316)
 * 28. centroid_norm_x (318-325)
 * 29. centroid_norm_y (318-325)
 * 30. left_right_symmetry (327-331)
 * 31. upper_lower_ratio (333-337)
 * 32. aspect_ratio_robust (339-343)
 * 33. mean_local_contrast (346-351)
 * 34. std_local_contrast (346-351)
 * 35. local_contrast_entropy (353-362)
 * 36. quartile_1_ratio (365-393)
 * 37. quartile_2_ratio (365-393)
 * 38. quartile_3_ratio (365-393)
 * 39. quartile_4_ratio (365-393)
 * 40. high_value_clustering (365-393)
 */

#ifndef RF_FEATURES_COMPLETE_CORRECTED_H
#define RF_FEATURES_COMPLETE_CORRECTED_H

#include <stdint.h>
#include <float.h>
#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================

#define SENSOR_ROWS                 26
#define SENSOR_COLS                 10
#define SENSOR_TOTAL                (SENSOR_ROWS * SENSOR_COLS)
#define NUM_EXTRACTED_FEATURES      41
#define HISTOGRAM_BINS              20
#define EPSILON                     1e-6f

#ifndef M_LOG2E
#define M_LOG2E                     1.4426950408889634f
#endif

#ifndef M_PI
#define M_PI                        3.14159265359f
#endif

// =============================================================================
// Global Working Buffers (static to avoid stack overflow)
// =============================================================================

typedef struct {
    float pressure_map[SENSOR_ROWS][SENSOR_COLS];
    uint8_t mask[SENSOR_ROWS][SENSOR_COLS];
    uint8_t labeled[SENSOR_ROWS][SENSOR_COLS];
    float laplacian[SENSOR_ROWS][SENSOR_COLS];
    float gaussian[SENSOR_ROWS][SENSOR_COLS];
    float log_filtered[SENSOR_ROWS][SENSOR_COLS];
    float local_contrast[SENSOR_ROWS][SENSOR_COLS];
    float active_values[SENSOR_TOTAL];
    float gradient_x[SENSOR_ROWS][SENSOR_COLS - 1];
    float gradient_y[SENSOR_ROWS - 1][SENSOR_COLS];
    uint32_t cluster_sizes[SENSOR_TOTAL];
} FeatureWorkspace_t;

// Global workspace
static FeatureWorkspace_t g_workspace;

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Calculate mean with double precision
 * 使用双精度求和以减少舍入误差 (2025-12-16)
 */
static inline float mean_f(const float *vals, int count)
{
    if (count <= 0) return 0.0f;
    double sum = 0.0;
    for (int i = 0; i < count; i++) sum += (double)vals[i];
    return (float)(sum / count);
}

/**
 * @brief Calculate variance with double precision
 * 使用双精度计算平方和 (2025-12-16)
 */
static inline float variance_f(const float *vals, int count)
{
    if (count <= 1) return 0.0f;
    double m = (double)mean_f(vals, count);
    double sum_sq = 0.0;
    for (int i = 0; i < count; i++) {
        double d = (double)vals[i] - m;
        sum_sq += d * d;
    }
    return (float)(sum_sq / count);
}

/**
 * @brief Calculate standard deviation
 */
static inline float stdev_f(const float *vals, int count)
{
    return sqrtf(variance_f(vals, count));
}

/**
 * @brief Sort array of floats (bubble sort for small arrays)
 */
static void sort_float_array(float *arr, int n)
{
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                float tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
}

/**
 * @brief Calculate percentile
 */
static float percentile_f(float *vals, int count, float percentile)
{
    if (count <= 0) return 0.0f;
    if (count == 1) return vals[0];

    sort_float_array(vals, count);

    float idx = (percentile / 100.0f) * (count - 1);
    int lower = (int)idx;
    int upper = lower + 1;

    if (upper >= count) return vals[count - 1];

    float frac = idx - lower;
    return vals[lower] * (1.0f - frac) + vals[upper] * frac;
}

/**
 * @brief Calculate median
 */
static float median_f(float *vals, int count)
{
    return percentile_f(vals, count, 50.0f);
}

/**
 * @brief Calculate skewness with double precision
 * skewness = E[(X - mean)^3] / std^3
 * 使用双精度计算中间值 (2025-12-16)
 */
static float skewness_f(const float *vals, int count)
{
    if (count < 3) return 0.0f;

    double m = (double)mean_f(vals, count);
    double s = (double)stdev_f(vals, count);

    if (s < EPSILON) return 0.0f;

    double m3 = 0.0;
    for (int i = 0; i < count; i++) {
        double d = (double)vals[i] - m;
        m3 += d * d * d;
    }
    m3 /= count;

    return (float)(m3 / (s * s * s));
}

/**
 * @brief Calculate kurtosis (excess kurtosis) with double precision
 * kurtosis = E[(X - mean)^4] / std^4 - 3
 * 使用双精度计算中间值 (2025-12-16)
 */
static float kurtosis_f(const float *vals, int count)
{
    if (count < 3) return 0.0f;

    double m = (double)mean_f(vals, count);
    double s = (double)stdev_f(vals, count);

    if (s < EPSILON) return 0.0f;

    double m4 = 0.0;
    for (int i = 0; i < count; i++) {
        double d = (double)vals[i] - m;
        m4 += d * d * d * d;
    }
    m4 /= count;

    return (float)((m4 / (s * s * s * s)) - 3.0);
}

/**
 * @brief Calculate Gini coefficient with double precision
 * gini = (2 * sum(i * sorted_vals)) / (n * sum(vals)) - (n + 1) / n
 * 使用双精度计算加权和 (2025-12-16)
 */
static float gini_f(float *vals, int count)
{
    if (count <= 0) return 0.0f;

    sort_float_array(vals, count);

    double sum = 0.0;
    for (int i = 0; i < count; i++) sum += (double)vals[i];

    if (sum < EPSILON) return 0.0f;

    double weighted_sum = 0.0;
    for (int i = 0; i < count; i++) {
        weighted_sum += (double)(i + 1) * (double)vals[i];
    }

    return (float)((2.0 * weighted_sum) / (count * sum) - (count + 1.0) / count);
}

/**
 * @brief Calculate entropy of histogram
 */
static float entropy_f(const uint32_t *hist, int bins)
{
    uint32_t total = 0;
    for (int i = 0; i < bins; i++) total += hist[i];

    if (total == 0) return 0.0f;

    float entropy = 0.0f;
    for (int i = 0; i < bins; i++) {
        if (hist[i] > 0) {
            float p = (float)hist[i] / total;
            entropy -= p * logf(p) * M_LOG2E;
        }
    }
    return entropy;
}

/**
 * @brief Build histogram of values
 */
static void histogram_f(const float *vals, int count, uint32_t *hist, int bins,
                       float min_val, float max_val)
{
    memset(hist, 0, bins * sizeof(uint32_t));

    if (count <= 0 || max_val <= min_val) return;

    float range = max_val - min_val + EPSILON;

    for (int i = 0; i < count; i++) {
        int bin = (int)((vals[i] - min_val) / range * bins);
        if (bin < 0) bin = 0;
        if (bin >= bins) bin = bins - 1;
        hist[bin]++;
    }
}

// =============================================================================
// Image Processing Filters
// =============================================================================

/**
 * @brief Discrete Laplacian filter for scipy.ndimage.laplace() compatibility
 * Kernel: [0,-1,0; -1,4,-1; 0,-1,0] (4-connectivity)
 * Formula: laplace[i,j] = 4*center - (up + down + left + right)
 * Boundary: Use reflection padding (like scipy)
 * 修正了拉普拉斯滤波器核和边界处理 (2025-12-16)
 */
static void laplacian_filter(float pressure[SENSOR_ROWS][SENSOR_COLS],
                            float output[SENSOR_ROWS][SENSOR_COLS])
{
    for (int i = 0; i < SENSOR_ROWS; i++) {
        for (int j = 0; j < SENSOR_COLS; j++) {
            double result = 4.0 * (double)pressure[i][j];

            if (i > 0) {
                result -= (double)pressure[i-1][j];
            } else {
                result -= (double)pressure[i][j];
            }

            if (i < SENSOR_ROWS - 1) {
                result -= (double)pressure[i+1][j];
            } else {
                result -= (double)pressure[i][j];
            }

            if (j > 0) {
                result -= (double)pressure[i][j-1];
            } else {
                result -= (double)pressure[i][j];
            }

            if (j < SENSOR_COLS - 1) {
                result -= (double)pressure[i][j+1];
            } else {
                result -= (double)pressure[i][j];
            }

            output[i][j] = (float)result;
        }
    }
}

/**
 * @brief Gaussian filter (3x3, sigma=1.0)
 * Kernel: [1,2,1; 2,4,2; 1,2,1] / 16
 */
static void gaussian_filter(float pressure[SENSOR_ROWS][SENSOR_COLS],
                           float output[SENSOR_ROWS][SENSOR_COLS])
{
    for (int i = 0; i < SENSOR_ROWS; i++) {
        for (int j = 0; j < SENSOR_COLS; j++) {
            float sum = pressure[i][j] * 4.0f;

            if (i > 0) sum += pressure[i-1][j] * 2.0f;
            if (i < SENSOR_ROWS-1) sum += pressure[i+1][j] * 2.0f;
            if (j > 0) sum += pressure[i][j-1] * 2.0f;
            if (j < SENSOR_COLS-1) sum += pressure[i][j+1] * 2.0f;

            if (i > 0 && j > 0) sum += pressure[i-1][j-1];
            if (i > 0 && j < SENSOR_COLS-1) sum += pressure[i-1][j+1];
            if (i < SENSOR_ROWS-1 && j > 0) sum += pressure[i+1][j-1];
            if (i < SENSOR_ROWS-1 && j < SENSOR_COLS-1) sum += pressure[i+1][j+1];

            output[i][j] = sum / 16.0f;
        }
    }
}

/**
 * @brief Uniform filter (3x3 box)
 */
static void uniform_filter(float pressure[SENSOR_ROWS][SENSOR_COLS],
                          float output[SENSOR_ROWS][SENSOR_COLS])
{
    for (int i = 0; i < SENSOR_ROWS; i++) {
        for (int j = 0; j < SENSOR_COLS; j++) {
            float sum = 0.0f;
            int count = 0;

            for (int di = -1; di <= 1; di++) {
                for (int dj = -1; dj <= 1; dj++) {
                    int ni = i + di;
                    int nj = j + dj;
                    if (ni >= 0 && ni < SENSOR_ROWS && nj >= 0 && nj < SENSOR_COLS) {
                        sum += pressure[ni][nj];
                        count++;
                    }
                }
            }

            output[i][j] = count > 0 ? sum / count : 0.0f;
        }
    }
}

/**
 * @brief Binary erosion (3x3)
 */
static void binary_erosion(const uint8_t input[SENSOR_ROWS][SENSOR_COLS],
                          uint8_t output[SENSOR_ROWS][SENSOR_COLS])
{
    memset(output, 0, SENSOR_ROWS * SENSOR_COLS);

    for (int i = 1; i < SENSOR_ROWS - 1; i++) {
        for (int j = 1; j < SENSOR_COLS - 1; j++) {
            if (input[i][j] == 0) continue;

            int all_neighbors = 1;
            for (int di = -1; di <= 1; di++) {
                for (int dj = -1; dj <= 1; dj++) {
                    if (input[i+di][j+dj] == 0) {
                        all_neighbors = 0;
                        break;
                    }
                }
                if (!all_neighbors) break;
            }

            if (all_neighbors) output[i][j] = 1;
        }
    }
}

// =============================================================================
// Connected Components Labeling
// =============================================================================

/**
 * @brief Label connected components (8-connected)
 * Returns number of components found
 */
static uint8_t label_components(const uint8_t mask[SENSOR_ROWS][SENSOR_COLS],
                               uint8_t labeled[SENSOR_ROWS][SENSOR_COLS],
                               uint32_t component_sizes[SENSOR_TOTAL])
{
    memset(labeled, 0, SENSOR_ROWS * SENSOR_COLS);
    memset(component_sizes, 0, SENSOR_TOTAL * sizeof(uint32_t));

    uint8_t next_label = 1;

    for (int i = 0; i < SENSOR_ROWS; i++) {
        for (int j = 0; j < SENSOR_COLS; j++) {
            if (mask[i][j] == 0 || labeled[i][j] != 0) continue;

            // BFS flood fill
            uint16_t queue[SENSOR_TOTAL];
            int qhead = 0, qtail = 0;

            queue[qtail++] = i * SENSOR_COLS + j;
            labeled[i][j] = next_label;

            while (qhead < qtail) {
                uint16_t idx = queue[qhead++];
                int ci = idx / SENSOR_COLS;
                int cj = idx % SENSOR_COLS;

                // 8 neighbors
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        if (di == 0 && dj == 0) continue;
                        int ni = ci + di;
                        int nj = cj + dj;

                        if (ni >= 0 && ni < SENSOR_ROWS && nj >= 0 && nj < SENSOR_COLS) {
                            if (mask[ni][nj] != 0 && labeled[ni][nj] == 0) {
                                labeled[ni][nj] = next_label;
                                queue[qtail++] = ni * SENSOR_COLS + nj;
                            }
                        }
                    }
                }
            }

            component_sizes[next_label] = qtail;
            next_label++;
        }
    }

    return next_label - 1;
}

// =============================================================================
// Main Feature Extraction - CORRECTED ORDER
// =============================================================================

/**
 * @brief Extract all 41 features in EXACT Python order
 * @param sensor_data: Raw 260-byte sensor array
 * @param features: Output array of 41 floats
 * @return 0 on success, -1 on error
 *
 * CRITICAL: This function extracts features in the EXACT same order as
 * retrain_20_trees_improved.py::_extract_all_41_features() (lines 154-395)
 *
 * 关键修正：此函数按照与Python训练代码完全相同的顺序提取特征 (2025-12-16)
 */
static int extract_41_python_features(const uint8_t *sensor_data, float *features)
{
    if (!sensor_data || !features) return -1;

    memset(features, 0, NUM_EXTRACTED_FEATURES * sizeof(float));

    // =========================================================================
    // PREPROCESSING: Convert to pressure map and apply filters
    // =========================================================================

    for (int i = 0; i < SENSOR_ROWS; i++) {
        for (int j = 0; j < SENSOR_COLS; j++) {
            uint8_t val = sensor_data[i * SENSOR_COLS + j];
            g_workspace.pressure_map[i][j] = (float)val;
            g_workspace.mask[i][j] = (val > 0) ? 1 : 0;
        }
    }

    // Apply filters (same as Python: laplacian, gaussian, LoG, uniform)
    laplacian_filter(g_workspace.pressure_map, g_workspace.laplacian);
    gaussian_filter(g_workspace.pressure_map, g_workspace.gaussian);
    laplacian_filter(g_workspace.gaussian, g_workspace.log_filtered);
    uniform_filter(g_workspace.pressure_map, g_workspace.local_contrast);

    // Calculate local contrast
    for (int i = 0; i < SENSOR_ROWS; i++) {
        for (int j = 0; j < SENSOR_COLS; j++) {
            g_workspace.local_contrast[i][j] = fabsf(g_workspace.pressure_map[i][j] -
                                                    g_workspace.local_contrast[i][j]);
        }
    }

    // Calculate gradients
    for (int i = 0; i < SENSOR_ROWS; i++) {
        for (int j = 0; j < SENSOR_COLS - 1; j++) {
            g_workspace.gradient_x[i][j] = fabsf(g_workspace.pressure_map[i][j+1] -
                                                g_workspace.pressure_map[i][j]);
        }
    }

    for (int i = 0; i < SENSOR_ROWS - 1; i++) {
        for (int j = 0; j < SENSOR_COLS; j++) {
            g_workspace.gradient_y[i][j] = fabsf(g_workspace.pressure_map[i+1][j] -
                                                g_workspace.pressure_map[i][j]);
        }
    }

    // Collect active values and coordinates
    int active_count = 0;
    float active_x_coords[SENSOR_TOTAL];
    float active_y_coords[SENSOR_TOTAL];
    float active_x_min = SENSOR_COLS, active_x_max = 0;
    float active_y_min = SENSOR_ROWS, active_y_max = 0;

    for (int i = 0; i < SENSOR_ROWS; i++) {
        for (int j = 0; j < SENSOR_COLS; j++) {
            if (g_workspace.mask[i][j] != 0) {
                g_workspace.active_values[active_count] = g_workspace.pressure_map[i][j];
                active_x_coords[active_count] = (float)j;
                active_y_coords[active_count] = (float)i;

                if (j < active_x_min) active_x_min = j;
                if (j > active_x_max) active_x_max = j;
                if (i < active_y_min) active_y_min = i;
                if (i > active_y_max) active_y_max = i;

                active_count++;
            }
        }
    }

    // Label connected components
    uint8_t num_components = label_components((const uint8_t(*)[SENSOR_COLS])g_workspace.mask,
                                             (uint8_t(*)[SENSOR_COLS])g_workspace.labeled,
                                             g_workspace.cluster_sizes);

    // Find max cluster size
    uint32_t max_cluster = 0;
    for (int i = 1; i <= num_components; i++) {
        if (g_workspace.cluster_sizes[i] > max_cluster) {
            max_cluster = g_workspace.cluster_sizes[i];
        }
    }

    // Handle edge case: no active sensors
    if (active_count == 0) {
        memset(features, 0, NUM_EXTRACTED_FEATURES * sizeof(float));
        return 0;
    }

    // =========================================================================
    // FEATURE EXTRACTION IN EXACT PYTHON ORDER
    // =========================================================================

    int feat_idx = 0;

    // Create copies for sorting operations
    float sorted_active[SENSOR_TOTAL];
    memcpy(sorted_active, g_workspace.active_values, active_count * sizeof(float));

    // Feature 0: activation_ratio (Python line 171-172)
    // np.sum(nonzero_mask_float) / pressure_map.size
    features[feat_idx++] = (float)active_count / SENSOR_TOTAL;

    // Feature 1: active_sensor_count (Python line 174-175)
    // np.sum(nonzero_mask_float)
    features[feat_idx++] = (float)active_count;

    // Feature 2: activation_clusters (Python line 177-179)
    // ndimage.label(nonzero_mask_float) -> num_clusters
    features[feat_idx++] = (float)num_components;

    // Feature 3: max_cluster_size (Python line 181-187)
    // np.max(cluster_sizes)
    features[feat_idx++] = (float)max_cluster;

    // Feature 4: activation_spread_x (Python line 189-197)
    // np.std(x_coords) if len(x_coords) > 1 else 0
    features[feat_idx++] = active_count > 1 ? stdev_f(active_x_coords, active_count) : 0.0f;

    // Feature 5: activation_spread_y (Python line 189-197)
    // np.std(y_coords) if len(y_coords) > 1 else 0
    features[feat_idx++] = active_count > 1 ? stdev_f(active_y_coords, active_count) : 0.0f;

    // Feature 6: coefficient_of_variation (Python line 200-203)
    // cv = np.std(active_values) / (mean_pressure + 1e-6)
    {
        float m = mean_f(g_workspace.active_values, active_count);
        float s = stdev_f(g_workspace.active_values, active_count);
        features[feat_idx++] = s / (m + EPSILON);
    }

    // Feature 7: normalized_pressure_range (Python line 205-207)
    // (np.max(active_values) - np.min(active_values)) / (mean_pressure + 1e-6)
    {
        sort_float_array(sorted_active, active_count);
        float m = mean_f(g_workspace.active_values, active_count);
        float min_v = sorted_active[0];
        float max_v = sorted_active[active_count - 1];
        features[feat_idx++] = (max_v - min_v) / (m + EPSILON);
    }

    // Feature 8: mean_relative_deviation (Python line 209-211)
    // np.mean(np.abs(active_values - mean_pressure)) / (mean_pressure + 1e-6)
    {
        float m = mean_f(g_workspace.active_values, active_count);
        float mean_dev = 0.0f;
        for (int i = 0; i < active_count; i++) {
            mean_dev += fabsf(g_workspace.active_values[i] - m);
        }
        mean_dev /= active_count;
        features[feat_idx++] = mean_dev / (m + EPSILON);
    }

    // Feature 9: median_relative_deviation (Python line 213-216)
    // np.mean(np.abs(active_values - median_pressure)) / (median_pressure + 1e-6)
    {
        memcpy(sorted_active, g_workspace.active_values, active_count * sizeof(float));
        float median = median_f(sorted_active, active_count);
        float mean_dev = 0.0f;
        for (int i = 0; i < active_count; i++) {
            mean_dev += fabsf(g_workspace.active_values[i] - median);
        }
        mean_dev /= active_count;
        features[feat_idx++] = mean_dev / (median + EPSILON);
    }

    // Feature 10: pressure_distribution_skewness (Python line 218-220)
    // stats.skew(active_values)
    features[feat_idx++] = skewness_f(g_workspace.active_values, active_count);

    // Feature 11: pressure_distribution_kurtosis (Python line 222-224)
    // stats.kurtosis(active_values)
    features[feat_idx++] = kurtosis_f(g_workspace.active_values, active_count);

    // Feature 12: pressure_entropy (Python line 226-230)
    // -np.sum(hist_norm * np.log2(hist_norm))
    {
        uint32_t hist[HISTOGRAM_BINS];
        memcpy(sorted_active, g_workspace.active_values, active_count * sizeof(float));
        sort_float_array(sorted_active, active_count);
        histogram_f(g_workspace.active_values, active_count, hist, HISTOGRAM_BINS,
                   sorted_active[0], sorted_active[active_count - 1] + EPSILON);
        features[feat_idx++] = entropy_f(hist, HISTOGRAM_BINS);
    }

    // Feature 13: pressure_gini_coefficient (Python line 232-237)
    // (2 * np.sum(index * sorted_vals)) / (n * np.sum(sorted_vals)) - (n + 1) / n
    {
        memcpy(sorted_active, g_workspace.active_values, active_count * sizeof(float));
        features[feat_idx++] = gini_f(sorted_active, active_count);
    }

    // Feature 14: mean_gradient_x (Python line 240-248)
    // np.mean(grad_x)
    {
        float grad_x_sum = 0.0f;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS - 1; j++) {
                grad_x_sum += g_workspace.gradient_x[i][j];
            }
        }
        int grad_count = SENSOR_ROWS * (SENSOR_COLS - 1);
        features[feat_idx++] = grad_count > 0 ? grad_x_sum / grad_count : 0.0f;
    }

    // Feature 15: std_gradient_x (Python line 240-248)
    // np.std(grad_x)
    {
        float grad_x_sum = 0.0f;
        int grad_count = SENSOR_ROWS * (SENSOR_COLS - 1);

        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS - 1; j++) {
                grad_x_sum += g_workspace.gradient_x[i][j];
            }
        }
        float grad_mean = grad_count > 0 ? grad_x_sum / grad_count : 0.0f;

        float grad_var = 0.0f;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS - 1; j++) {
                float d = g_workspace.gradient_x[i][j] - grad_mean;
                grad_var += d * d;
            }
        }
        features[feat_idx++] = sqrtf(grad_var / grad_count);
    }

    // Feature 16: mean_gradient_y (Python line 250-258)
    // np.mean(grad_y)
    {
        float grad_y_sum = 0.0f;
        for (int i = 0; i < SENSOR_ROWS - 1; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                grad_y_sum += g_workspace.gradient_y[i][j];
            }
        }
        int grad_count = (SENSOR_ROWS - 1) * SENSOR_COLS;
        features[feat_idx++] = grad_count > 0 ? grad_y_sum / grad_count : 0.0f;
    }

    // Feature 17: std_gradient_y (Python line 250-258)
    // np.std(grad_y)
    {
        float grad_y_sum = 0.0f;
        int grad_count = (SENSOR_ROWS - 1) * SENSOR_COLS;

        for (int i = 0; i < SENSOR_ROWS - 1; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                grad_y_sum += g_workspace.gradient_y[i][j];
            }
        }
        float grad_mean = grad_count > 0 ? grad_y_sum / grad_count : 0.0f;

        float grad_var = 0.0f;
        for (int i = 0; i < SENSOR_ROWS - 1; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                float d = g_workspace.gradient_y[i][j] - grad_mean;
                grad_var += d * d;
            }
        }
        features[feat_idx++] = sqrtf(grad_var / grad_count);
    }

    // Feature 18: gradient_directionality (Python line 260-264)
    // (total_grad_x - total_grad_y) / (total_grad_x + total_grad_y + 1e-6)
    {
        float total_grad_x = 0.0f, total_grad_y = 0.0f;

        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS - 1; j++) {
                total_grad_x += g_workspace.gradient_x[i][j];
            }
        }
        for (int i = 0; i < SENSOR_ROWS - 1; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                total_grad_y += g_workspace.gradient_y[i][j];
            }
        }
        float sum_grad = total_grad_x + total_grad_y;
        features[feat_idx++] = sum_grad > EPSILON ? (total_grad_x - total_grad_y) / sum_grad : 0.0f;
    }

    // Feature 19: laplacian_mean (Python line 266-268)
    // np.mean(np.abs(laplacian))
    {
        float lap_sum = 0.0f;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                lap_sum += fabsf(g_workspace.laplacian[i][j]);
            }
        }
        features[feat_idx++] = lap_sum / SENSOR_TOTAL;
    }

    // Feature 20: laplacian_std (Python line 269-270)
    // np.std(laplacian)
    {
        float lap_sum = 0.0f;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                lap_sum += g_workspace.laplacian[i][j];
            }
        }
        float lap_mean = lap_sum / SENSOR_TOTAL;

        float lap_var = 0.0f;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                float d = g_workspace.laplacian[i][j] - lap_mean;
                lap_var += d * d;
            }
        }
        features[feat_idx++] = sqrtf(lap_var / SENSOR_TOTAL);
    }

    // Feature 21: laplacian_of_gaussian (Python line 272-276)
    // np.mean(np.abs(log[nonzero_mask]))
    {
        float log_sum = 0.0f;
        int log_count = 0;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                if (g_workspace.mask[i][j] != 0) {
                    log_sum += fabsf(g_workspace.log_filtered[i][j]);
                    log_count++;
                }
            }
        }
        features[feat_idx++] = log_count > 0 ? log_sum / log_count : 0.0f;
    }

    // Feature 22: num_connected_components (Python line 279-281)
    // ndimage.label() -> num_components
    features[feat_idx++] = (float)num_components;

    // Feature 23: component_size_mean (Python line 283-297)
    // np.mean(component_sizes)
    {
        if (num_components > 0) {
            float sizes[SENSOR_TOTAL];
            for (int i = 1; i <= num_components; i++) {
                sizes[i-1] = (float)g_workspace.cluster_sizes[i];
            }
            features[feat_idx++] = mean_f(sizes, num_components);
        } else {
            features[feat_idx++] = 0.0f;
        }
    }

    // Feature 24: component_size_std (Python line 283-297)
    // np.std(component_sizes)
    {
        if (num_components > 0) {
            float sizes[SENSOR_TOTAL];
            for (int i = 1; i <= num_components; i++) {
                sizes[i-1] = (float)g_workspace.cluster_sizes[i];
            }
            features[feat_idx++] = stdev_f(sizes, num_components);
        } else {
            features[feat_idx++] = 0.0f;
        }
    }

    // Feature 25: component_size_cv (Python line 283-297)
    // np.std(component_sizes) / np.mean(component_sizes)
    {
        if (num_components > 0) {
            float sizes[SENSOR_TOTAL];
            for (int i = 1; i <= num_components; i++) {
                sizes[i-1] = (float)g_workspace.cluster_sizes[i];
            }
            float size_mean = mean_f(sizes, num_components);
            float size_std = stdev_f(sizes, num_components);
            features[feat_idx++] = size_mean > EPSILON ? size_std / size_mean : 0.0f;
        } else {
            features[feat_idx++] = 0.0f;
        }
    }

    // Feature 26: isolated_sensors (Python line 299-308)
    // Count sensors with no active neighbors
    {
        int isolated = 0;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                if (g_workspace.mask[i][j] == 0) continue;

                int neighbor_count = 0;
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        if (di == 0 && dj == 0) continue;
                        int ni = i + di;
                        int nj = j + dj;
                        if (ni >= 0 && ni < SENSOR_ROWS && nj >= 0 && nj < SENSOR_COLS) {
                            if (g_workspace.mask[ni][nj] != 0) {
                                neighbor_count++;
                            }
                        }
                    }
                }

                if (neighbor_count == 0) isolated++;
            }
        }
        features[feat_idx++] = (float)isolated;
    }

    // Feature 27: skeleton_ratio (Python line 310-316)
    // np.sum(skeleton) / (np.sum(nonzero_mask) + 1e-6)
    {
        uint8_t skeleton[SENSOR_ROWS][SENSOR_COLS];
        binary_erosion((const uint8_t(*)[SENSOR_COLS])g_workspace.mask,
                      (uint8_t(*)[SENSOR_COLS])skeleton);

        int skeleton_count = 0;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                if (skeleton[i][j] != 0) skeleton_count++;
            }
        }

        features[feat_idx++] = active_count > 0 ? (float)skeleton_count / active_count : 0.0f;
    }

    // Feature 28: centroid_norm_x (Python line 319-325)
    // np.mean(x_coords) / pressure_map.shape[1]
    {
        float cx = active_count > 0 ? mean_f(active_x_coords, active_count) : 0.0f;
        features[feat_idx++] = cx / SENSOR_COLS;
    }

    // Feature 29: centroid_norm_y (Python line 319-325)
    // np.mean(y_coords) / pressure_map.shape[0]
    {
        float cy = active_count > 0 ? mean_f(active_y_coords, active_count) : 0.0f;
        features[feat_idx++] = cy / SENSOR_ROWS;
    }

    // Feature 30: left_right_symmetry (Python line 327-331)
    // min(left_active, right_active) / (max(left_active, right_active) + 1e-6)
    {
        int left_active = 0, right_active = 0;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS / 2; j++) {
                if (g_workspace.mask[i][j] != 0) left_active++;
            }
            for (int j = SENSOR_COLS / 2; j < SENSOR_COLS; j++) {
                if (g_workspace.mask[i][j] != 0) right_active++;
            }
        }
        int max_lr = (left_active > right_active) ? left_active : right_active;
        int min_lr = (left_active < right_active) ? left_active : right_active;
        features[feat_idx++] = max_lr > 0 ? (float)min_lr / max_lr : 0.0f;
    }

    // Feature 31: upper_lower_ratio (Python line 333-337)
    // upper_active / (lower_active + 1e-6)
    {
        int upper_active = 0, lower_active = 0;
        for (int i = 0; i < SENSOR_ROWS / 2; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                if (g_workspace.mask[i][j] != 0) upper_active++;
            }
        }
        for (int i = SENSOR_ROWS / 2; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                if (g_workspace.mask[i][j] != 0) lower_active++;
            }
        }
        features[feat_idx++] = lower_active > 0 ? (float)upper_active / lower_active : 0.0f;
    }

    // Feature 32: aspect_ratio_robust (Python line 339-343)
    // width / (height + 1e-6)
    {
        float width = active_x_max - active_x_min + 1;
        float height = active_y_max - active_y_min + 1;
        features[feat_idx++] = height > 0 ? width / height : 0.0f;
    }

    // Feature 33: mean_local_contrast (Python line 346-351)
    // np.mean(local_contrast)
    {
        float contrast_sum = 0.0f;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                contrast_sum += g_workspace.local_contrast[i][j];
            }
        }
        features[feat_idx++] = contrast_sum / SENSOR_TOTAL;
    }

    // Feature 34: std_local_contrast (Python line 346-351)
    // np.std(local_contrast)
    {
        float contrast_mean = 0.0f;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                contrast_mean += g_workspace.local_contrast[i][j];
            }
        }
        contrast_mean /= SENSOR_TOTAL;

        float contrast_var = 0.0f;
        for (int i = 0; i < SENSOR_ROWS; i++) {
            for (int j = 0; j < SENSOR_COLS; j++) {
                float d = g_workspace.local_contrast[i][j] - contrast_mean;
                contrast_var += d * d;
            }
        }
        features[feat_idx++] = sqrtf(contrast_var / SENSOR_TOTAL);
    }

    // Feature 35: local_contrast_entropy (Python line 353-362)
    // Entropy of local_contrast histogram (only active sensors)
    {
        if (active_count > 0) {
            float active_contrast[SENSOR_TOTAL];
            int ac = 0;
            float min_c = 0.0f, max_c = 0.0f;

            for (int i = 0; i < SENSOR_ROWS; i++) {
                for (int j = 0; j < SENSOR_COLS; j++) {
                    if (g_workspace.mask[i][j] != 0) {
                        active_contrast[ac] = g_workspace.local_contrast[i][j];
                        if (ac == 0 || active_contrast[ac] < min_c) min_c = active_contrast[ac];
                        if (ac == 0 || active_contrast[ac] > max_c) max_c = active_contrast[ac];
                        ac++;
                    }
                }
            }

            uint32_t hist[HISTOGRAM_BINS];
            histogram_f(active_contrast, ac, hist, HISTOGRAM_BINS, min_c, max_c + EPSILON);
            features[feat_idx++] = entropy_f(hist, HISTOGRAM_BINS);
        } else {
            features[feat_idx++] = 0.0f;
        }
    }

    // Feature 36: quartile_1_ratio (Python line 365-393)
    // np.sum(active_values <= q1) / len(active_values)
    {
        if (active_count >= 3) {
            memcpy(sorted_active, g_workspace.active_values, active_count * sizeof(float));
            float q1 = percentile_f(sorted_active, active_count, 25.0f);

            int q1_count = 0;
            for (int i = 0; i < active_count; i++) {
                if (g_workspace.active_values[i] <= q1) {
                    q1_count++;
                }
            }
            features[feat_idx++] = (float)q1_count / active_count;
        } else {
            features[feat_idx++] = 0.0f;
        }
    }

    // Feature 37: quartile_2_ratio (Python line 365-393)
    // np.sum((active_values > q1) & (active_values <= q2)) / len(active_values)
    {
        if (active_count >= 3) {
            memcpy(sorted_active, g_workspace.active_values, active_count * sizeof(float));
            float q1 = percentile_f(sorted_active, active_count, 25.0f);
            float q2 = percentile_f(sorted_active, active_count, 50.0f);

            int q2_count = 0;
            for (int i = 0; i < active_count; i++) {
                if (g_workspace.active_values[i] > q1 && g_workspace.active_values[i] <= q2) {
                    q2_count++;
                }
            }
            features[feat_idx++] = (float)q2_count / active_count;
        } else {
            features[feat_idx++] = 0.0f;
        }
    }

    // Feature 38: quartile_3_ratio (Python line 365-393)
    // np.sum((active_values > q2) & (active_values <= q3)) / len(active_values)
    {
        if (active_count >= 3) {
            memcpy(sorted_active, g_workspace.active_values, active_count * sizeof(float));
            float q2 = percentile_f(sorted_active, active_count, 50.0f);
            float q3 = percentile_f(sorted_active, active_count, 75.0f);

            int q3_count = 0;
            for (int i = 0; i < active_count; i++) {
                if (g_workspace.active_values[i] > q2 && g_workspace.active_values[i] <= q3) {
                    q3_count++;
                }
            }
            features[feat_idx++] = (float)q3_count / active_count;
        } else {
            features[feat_idx++] = 0.0f;
        }
    }

    // Feature 39: quartile_4_ratio (Python line 365-393)
    // np.sum(active_values > q3) / len(active_values)
    {
        if (active_count >= 3) {
            memcpy(sorted_active, g_workspace.active_values, active_count * sizeof(float));
            float q3 = percentile_f(sorted_active, active_count, 75.0f);

            int q4_count = 0;
            for (int i = 0; i < active_count; i++) {
                if (g_workspace.active_values[i] > q3) {
                    q4_count++;
                }
            }
            features[feat_idx++] = (float)q4_count / active_count;
        } else {
            features[feat_idx++] = 0.0f;
        }
    }

    // Feature 40: high_value_clustering (Python line 365-393)
    // (y_std + x_std) / (pressure_map.shape[0] + pressure_map.shape[1])
    {
        if (active_count >= 3) {
            memcpy(sorted_active, g_workspace.active_values, active_count * sizeof(float));
            float q3 = percentile_f(sorted_active, active_count, 75.0f);

            float top_x_coords[SENSOR_TOTAL];
            float top_y_coords[SENSOR_TOTAL];
            int top_count = 0;

            for (int i = 0; i < SENSOR_ROWS; i++) {
                for (int j = 0; j < SENSOR_COLS; j++) {
                    if (g_workspace.pressure_map[i][j] >= q3) {
                        top_x_coords[top_count] = (float)j;
                        top_y_coords[top_count] = (float)i;
                        top_count++;
                    }
                }
            }

            if (top_count > 1) {
                float x_std = stdev_f(top_x_coords, top_count);
                float y_std = stdev_f(top_y_coords, top_count);
                features[feat_idx++] = (y_std + x_std) / (SENSOR_ROWS + SENSOR_COLS);
            } else {
                features[feat_idx++] = 0.0f;
            }
        } else {
            features[feat_idx++] = 0.0f;
        }
    }

    // Sanity check: Ensure we extracted exactly 41 features
    if (feat_idx != NUM_EXTRACTED_FEATURES) {
        return -1;  // ERROR: Feature count mismatch!
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif // RF_FEATURES_COMPLETE_CORRECTED_H
