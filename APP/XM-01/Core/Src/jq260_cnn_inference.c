/**
 * @file jq260_cnn_inference.c
 * @brief JQ260 CNN Lite Inference Implementation for STM32F405RG
 *
 * 轻量级CNN推理实现
 * Lightweight CNN inference implementation
 *
 * 生成日期: 2025-12-23 00:05:12
 *
 * 中文注释 (2025-12-22)
 */

#include "jq260_cnn_inference.h"
#include "jq260_cnn_model.h"
#include <string.h>
#include <math.h>

// =============================================================================
// 私有变量 - 中间缓冲区 (2025-12-22)
// =============================================================================

// 修复缓冲区溢出: Conv1输出需要26×10×16=4160字节，原定义只有2080字节 (2025-12-29)
#define MAX_CHANNELS    32
#define MAX_FEATURE_MAP (26 * 10 * MAX_CHANNELS)  // = 8320 bytes，足以容纳Conv1输出

static int8_t g_buffer_a[MAX_FEATURE_MAP];
static int8_t g_buffer_b[MAX_FEATURE_MAP];

// =============================================================================
// 辅助函数 (2025-12-22)
// =============================================================================

static inline int8_t relu_int8(int32_t x)
{
    if (x < 0) return 0;
    if (x > 127) return 127;
    return (int8_t)x;
}

static inline float sigmoid_f32(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

static void conv2d_int8(
    const int8_t* input,
    int8_t* output,
    const int8_t* weights,
    const int32_t* bias,
    int in_h, int in_w,
    int out_ch, int in_ch,
    int ker_h, int ker_w,
    int stride,
    int padding,
    float input_scale,
    float weight_scale,
    float output_scale)
{
    int out_h, out_w;
    int pad_h = 0, pad_w = 0;

    if (padding == 1) {
        out_h = (in_h + stride - 1) / stride;
        out_w = (in_w + stride - 1) / stride;
        pad_h = ((out_h - 1) * stride + ker_h - in_h) / 2;
        pad_w = ((out_w - 1) * stride + ker_w - in_w) / 2;
    } else {
        out_h = (in_h - ker_h) / stride + 1;
        out_w = (in_w - ker_w) / stride + 1;
    }

    float acc_scale = input_scale * weight_scale / output_scale;

    for (int oc = 0; oc < out_ch; oc++) {
        for (int oh = 0; oh < out_h; oh++) {
            for (int ow = 0; ow < out_w; ow++) {
                int32_t acc = bias[oc];

                for (int kh = 0; kh < ker_h; kh++) {
                    for (int kw = 0; kw < ker_w; kw++) {
                        int ih = oh * stride + kh - pad_h;
                        int iw = ow * stride + kw - pad_w;

                        if (ih >= 0 && ih < in_h && iw >= 0 && iw < in_w) {
                            for (int ic = 0; ic < in_ch; ic++) {
                                int input_idx = (ih * in_w + iw) * in_ch + ic;
                                int weight_idx = ((oc * ker_h + kh) * ker_w + kw) * in_ch + ic;
                                acc += (int32_t)input[input_idx] * (int32_t)weights[weight_idx];
                            }
                        }
                    }
                }

                float out_f = (float)acc * acc_scale;
                int32_t out_q = (int32_t)roundf(out_f);
                output[(oh * out_w + ow) * out_ch + oc] = relu_int8(out_q);
            }
        }
    }
}

static void maxpool2d_int8(
    const int8_t* input,
    int8_t* output,
    int in_h, int in_w, int channels)
{
    int out_h = in_h / 2;
    int out_w = in_w / 2;

    for (int oh = 0; oh < out_h; oh++) {
        for (int ow = 0; ow < out_w; ow++) {
            for (int c = 0; c < channels; c++) {
                int ih = oh * 2;
                int iw = ow * 2;

                int8_t max_val = -128;
                for (int kh = 0; kh < 2; kh++) {
                    for (int kw = 0; kw < 2; kw++) {
                        int idx = ((ih + kh) * in_w + (iw + kw)) * channels + c;
                        if (input[idx] > max_val) {
                            max_val = input[idx];
                        }
                    }
                }
                output[(oh * out_w + ow) * channels + c] = max_val;
            }
        }
    }
}

static void global_avgpool_int8_to_f32(
    const int8_t* input,
    float* output,
    int in_h, int in_w, int channels,
    float input_scale)
{
    int total_pixels = in_h * in_w;

    for (int c = 0; c < channels; c++) {
        int32_t sum = 0;
        for (int h = 0; h < in_h; h++) {
            for (int w = 0; w < in_w; w++) {
                sum += input[(h * in_w + w) * channels + c];
            }
        }
        output[c] = (float)sum / (float)total_pixels * input_scale;
    }
}

static void dense_f32(
    const float* input,
    float* output,
    const int8_t* weights,
    const int32_t* bias,
    int in_dim, int out_dim,
    float weight_scale,
    float bias_scale,
    int apply_relu)
{
    for (int o = 0; o < out_dim; o++) {
        float acc = (float)bias[o] * bias_scale;

        for (int i = 0; i < in_dim; i++) {
            float w = (float)weights[o * in_dim + i] * weight_scale;
            acc += input[i] * w;
        }

        if (apply_relu && acc < 0) {
            acc = 0;
        }
        output[o] = acc;
    }
}

// =============================================================================
// 主推理函数 (2025-12-22)
// =============================================================================

int cnn_posture_init(void)
{
    memset(g_buffer_a, 0, sizeof(g_buffer_a));
    memset(g_buffer_b, 0, sizeof(g_buffer_b));
    return 0;
}

uint8_t cnn_posture_classify(const uint8_t* sensor_data, float* probability)
{
    // 输入预处理 - 修复: 匹配Python的[0,1]归一化 (2025-12-29)
    // Fix: Match Python's [0,1] normalization
    // Python: X / 255.0 → range [0, 1]
    // C: (X / 2) * (2/255) = X / 255 → range [0, 1]
    int8_t* input = g_buffer_a;
    for (int i = 0; i < CNN_INPUT_SIZE; i++) {
        // 除以2使其适应int8正数范围[0,127]，scale补偿回来 (2025-12-29)
        input[i] = (int8_t)(sensor_data[i] >> 1);  // 等效于 /2，范围[0,127]
    }

    // scale = 2/255，与/2抵消后等效于原始值/255，匹配Python (2025-12-29)
    float input_scale = 2.0f / 255.0f;

    // Conv1 + ReLU + MaxPool (2025-12-22)
    int8_t* conv1_out = g_buffer_b;
    conv2d_int8(
        input, conv1_out,
        conv1_weights, conv1_bias,
        26, 10,
        CONV1_OUT_CH, CONV1_IN_CH,
        CONV1_KER_H, CONV1_KER_W,
        1, 1,
        input_scale, CONV1_SCALE,
        1.0f / 127.0f
    );

    int8_t* pool1_out = g_buffer_a;
    maxpool2d_int8(conv1_out, pool1_out, 26, 10, CONV1_OUT_CH);
    // 13x5xCONV1_OUT_CH

    // Conv2 + ReLU + MaxPool (2025-12-22)
    int8_t* conv2_out = g_buffer_b;
    conv2d_int8(
        pool1_out, conv2_out,
        conv2_weights, conv2_bias,
        13, 5,
        CONV2_OUT_CH, CONV2_IN_CH,
        CONV2_KER_H, CONV2_KER_W,
        1, 1,
        1.0f / 127.0f, CONV2_SCALE,
        1.0f / 127.0f
    );

    int8_t* pool2_out = g_buffer_a;
    maxpool2d_int8(conv2_out, pool2_out, 13, 5, CONV2_OUT_CH);
    // 6x2xCONV2_OUT_CH

    // Conv3 + ReLU (2025-12-22)
    int8_t* conv3_out = g_buffer_b;
    conv2d_int8(
        pool2_out, conv3_out,
        conv3_weights, conv3_bias,
        6, 2,
        CONV3_OUT_CH, CONV3_IN_CH,
        CONV3_KER_H, CONV3_KER_W,
        1, 1,
        1.0f / 127.0f, CONV3_SCALE,
        1.0f / 127.0f
    );
    // 6x2xCONV3_OUT_CH

    // Global Average Pooling (2025-12-22)
    float gap_out[CONV3_OUT_CH];
    global_avgpool_int8_to_f32(conv3_out, gap_out, 6, 2, CONV3_OUT_CH, 1.0f / 127.0f);

    // Dense1 + ReLU (2025-12-22)
    float fc1_out[FC1_OUT_DIM];
    dense_f32(
        gap_out, fc1_out,
        fc1_weights, fc1_bias,
        FC1_IN_DIM, FC1_OUT_DIM,
        FC1_SCALE, FC1_BIAS_SCALE,
        1
    );

    // Dense2 + ReLU (2025-12-22)
    float fc2_out[FC2_OUT_DIM];
    dense_f32(
        fc1_out, fc2_out,
        fc2_weights, fc2_bias,
        FC2_IN_DIM, FC2_OUT_DIM,
        FC2_SCALE, FC2_BIAS_SCALE,
        1
    );

    // Output + Sigmoid (2025-12-22)
    float logit;
    dense_f32(
        fc2_out, &logit,
        output_weights, output_bias,
        OUTPUT_IN_DIM, OUTPUT_OUT_DIM,
        OUTPUT_SCALE, OUTPUT_BIAS_SCALE,
        0
    );

    float prob = sigmoid_f32(logit);

    if (probability != NULL) {
        *probability = prob;
    }

    return (prob >= CNN_OPTIMAL_THRESHOLD) ? CNN_POSTURE_SIDE : CNN_POSTURE_SUPINE;
}

void cnn_posture_deinit(void)
{
    // 无需释放 (2025-12-22)
}
