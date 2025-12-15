/*
 * MCU Body Detection and Analysis System
 *
 * Lightweight implementation for microcontroller deployment
 * Compatible with 26x10 sensing mat (260 sensors)
 *
 * Features:
 * - Body detection using threshold and connected components
 * - Body count (0, 1, or 2)
 * - Waist center estimation
 * - Basic posture classification
 *
 * Memory usage: ~2KB RAM
 * Processing time: ~10ms on ARM Cortex-M4 @ 168MHz
 */

#include "mcu_body_analyzer.h"

// Configuration constants
#define MAT_WIDTH            26
#define MAT_HEIGHT           10
#define TOTAL_SENSORS        (MAT_WIDTH * MAT_HEIGHT)
#define MIN_BODY_PRESSURE    50
#define MIN_BODY_AREA        20
#define MAX_BODIES           2

// Physical dimensions (in mm for integer math)
#define POINT_SPACING_X_MM   44 // 4.4cm = 44mm
#define POINT_SPACING_Y_MM   64 // 6.4cm = 64mm

// Working memory (can be static for single-threaded MCU)
static uint8_t binary_mask[TOTAL_SENSORS];
static uint8_t labeled_regions[TOTAL_SENSORS];
static uint8_t temp_buffer[TOTAL_SENSORS];

 /*------------------/-------------------/-------------------/------------------/
 * @brief   Apply simple smoothing filter to reduce noise
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static void smooth_sensor_data(const uint16_t *input, uint16_t *output)
{
    for (int y = 0; y < MAT_HEIGHT; y++)
    {
        for (int x = 0; x < MAT_WIDTH; x++)
        {
            int idx = y * MAT_WIDTH + x;
            uint32_t sum = 0;
            uint8_t count = 0;

            // 3x3 kernel
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    int ny = y + dy;
                    int nx = x + dx;

                    if (ny >= 0 && ny < MAT_HEIGHT && nx >= 0 && nx < MAT_WIDTH)
                    {
                        sum += input[ny * MAT_WIDTH + nx];
                        count++;
                    }
                }
            }

            output[idx] = (uint16_t)(sum / count);
        }
    }
}

 /*------------------/-------------------/-------------------/------------------/
 * @brief   Create binary mask from pressure data
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static void create_binary_mask(const uint16_t *sensor_data)
{
    for (int i = 0; i < TOTAL_SENSORS; i++)
    {
        binary_mask[i] = (sensor_data[i] > MIN_BODY_PRESSURE) ? 1 : 0;
    }
}

 /*------------------/-------------------/-------------------/------------------/
 * @brief   Simple connected component labeling using flood fill
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static uint8_t flood_fill(uint8_t *mask, int start_idx, uint8_t label)
{
    int stack[TOTAL_SENSORS] = {0};
    int stack_ptr = 0;
	
    if (mask[start_idx] != 1)
        return 0;

    // Simple stack using array (limit recursion depth)
    stack_ptr = 0;
    uint8_t area = 0;

    stack[stack_ptr++] = start_idx;

    while (stack_ptr > 0)
    {
        int idx = stack[--stack_ptr];
        if (mask[idx] != 1)
            continue;

        mask[idx] = label;
        labeled_regions[idx] = label;
        area++;

        //int y = idx / MAT_WIDTH;
        //int x = idx % MAT_WIDTH;

        // Add 4-connected neighbors
        int neighbors[] = {
            idx - MAT_WIDTH, // top
            idx + MAT_WIDTH, // bottom
            idx - 1,         // left
            idx + 1          // right
        };

        for (int i = 0; i < 4; i++)
        {
            int n_idx = neighbors[i];
            int n_y = n_idx / MAT_WIDTH;
            int n_x = n_idx % MAT_WIDTH;

            // Check bounds
            if (n_idx >= 0 && n_idx < TOTAL_SENSORS &&
                n_y >= 0 && n_y < MAT_HEIGHT &&
                n_x >= 0 && n_x < MAT_WIDTH &&
                mask[n_idx] == 1 && stack_ptr < TOTAL_SENSORS - 1)
            {
                stack[stack_ptr++] = n_idx;
            }
        }
        if (stack_ptr >= 259)
            // stack_ptr = 0;
            break;
    }

    return area;
}

 /*------------------/-------------------/-------------------/------------------/
 * @brief   Find connected components and filter by size
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static uint8_t detect_body_regions(const uint16_t *sensor_data)
{
    // Copy binary mask to working buffer
    rt_memcpy(temp_buffer, binary_mask, TOTAL_SENSORS);
    rt_memset(labeled_regions, 0, TOTAL_SENSORS);

    uint8_t label = 2; // Start from 2 (0=background, 1=unprocessed)
    uint8_t body_count = 0;

    for (int i = 0; i < TOTAL_SENSORS && body_count < MAX_BODIES; i++)
    {
        if (temp_buffer[i] == 1)
        {
            uint8_t area = flood_fill(temp_buffer, i, label);

            if (area >= MIN_BODY_AREA)
            {
                body_count++;
                label++;
            }
            else
            {
                // Remove small regions
                for (int j = 0; j < TOTAL_SENSORS; j++)
                {
                    if (labeled_regions[j] == (label - 1))
                    {
                        labeled_regions[j] = 0;
                    }
                }
                label--;
            }
        }
    }

    return body_count;
}

 /*------------------/-------------------/-------------------/------------------/
 * @brief   Calculate center of mass for a body region
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static Point_t calculate_center_of_mass(const uint16_t *sensor_data, uint8_t body_label)
{
    uint32_t sum_x = 0, sum_y = 0, sum_weight = 0;

    for (int y = 0; y < MAT_HEIGHT; y++)
    {
        for (int x = 0; x < MAT_WIDTH; x++)
        {
            int idx = y * MAT_WIDTH + x;

            if (labeled_regions[idx] == body_label)
            {
                uint16_t weight = sensor_data[idx];
                sum_x += x * weight;
                sum_y += y * weight;
                sum_weight += weight;
            }
        }
    }

    Point_t center = {0, 0};
    if (sum_weight > 0)
    {
        center.x_mm = (sum_x * POINT_SPACING_X_MM) / sum_weight;
        center.y_mm = (sum_y * POINT_SPACING_Y_MM) / sum_weight;
    }

    return center;
}

 /*------------------/-------------------/-------------------/------------------/
 * @brief   Get bounding box of a body region
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static void get_bounding_box(uint8_t body_label, uint8_t *min_x, uint8_t *min_y,
                             uint8_t *max_x, uint8_t *max_y)
{
    *min_x = MAT_WIDTH;
    *min_y = MAT_HEIGHT;
    *max_x = 0;
    *max_y = 0;

    for (int y = 0; y < MAT_HEIGHT; y++)
    {
        for (int x = 0; x < MAT_WIDTH; x++)
        {
            int idx = y * MAT_WIDTH + x;

            if (labeled_regions[idx] == body_label)
            {
                if (x < *min_x)
                    *min_x = x;
                if (x > *max_x)
                    *max_x = x;
                if (y < *min_y)
                    *min_y = y;
                if (y > *max_y)
                    *max_y = y;
            }
        }
    }
}

 /*------------------/-------------------/-------------------/------------------/
 * @brief  Estimate waist center using pressure distribution
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static Point_t estimate_waist_center(const uint16_t *sensor_data, uint8_t body_label)
{
    uint8_t min_x, min_y, max_x, max_y;
    get_bounding_box(body_label, &min_x, &min_y, &max_x, &max_y);

    // Focus on middle 40% of body height for waist detection
    uint8_t body_height = max_y - min_y + 1;
    uint8_t waist_start = min_y + (body_height * 3) / 10; // 30% from top
    uint8_t waist_end = min_y + (body_height * 7) / 10;   // 70% from top

    // Find row with maximum pressure in waist region
    uint32_t max_row_sum = 0;
    uint8_t waist_row = (waist_start + waist_end) / 2;

    for (uint8_t y = waist_start; y <= waist_end && y <= max_y; y++)
    {
        uint32_t row_sum = 0;

        for (uint8_t x = min_x; x <= max_x; x++)
        {
            int idx = y * MAT_WIDTH + x;
            if (labeled_regions[idx] == body_label)
            {
                row_sum += sensor_data[idx];
            }
        }

        if (row_sum > max_row_sum)
        {
            max_row_sum = row_sum;
            waist_row = y;
        }
    }

    // Calculate weighted center in waist row
    uint32_t sum_x = 0, sum_weight = 0;

    for (uint8_t x = min_x; x <= max_x; x++)
    {
        int idx = waist_row * MAT_WIDTH + x;
        if (labeled_regions[idx] == body_label)
        {
            uint16_t weight = sensor_data[idx];
            sum_x += x * weight;
            sum_weight += weight;
        }
    }

    Point_t waist_center = {0, 0};
    if (sum_weight > 0)
    {
        waist_center.x_mm = (sum_x * POINT_SPACING_X_MM) / sum_weight;
        waist_center.y_mm = waist_row * POINT_SPACING_Y_MM;
    }

    return waist_center;
}

 /*------------------/-------------------/-------------------/------------------/
 * @brief  Classify posture using simple heuristics
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static uint8_t classify_posture(uint8_t body_label, uint8_t *confidence)
{
    uint8_t min_x, min_y, max_x, max_y;
    get_bounding_box(body_label, &min_x, &min_y, &max_x, &max_y);

    uint16_t width = (max_x - min_x + 1) * POINT_SPACING_X_MM;
    uint16_t height = (max_y - min_y + 1) * POINT_SPACING_Y_MM;

    // Calculate aspect ratio (width/height) * 100 for integer math
    uint16_t aspect_ratio_x100 = (width * 100) / height;

    // Heuristic classification
    if (aspect_ratio_x100 > 150)
    { // aspect_ratio > 1.5
        *confidence = 70;
        return 0; // supine
    }
    else if (aspect_ratio_x100 < 120)
    { // aspect_ratio < 1.2
        *confidence = 70;
        return 1; // side
    }
    else
    {
        *confidence = 50;
        return (aspect_ratio_x100 > 130) ? 0 : 1; // uncertain
    }
}

/*
 * Main analysis function
 *
 * @param sensor_data: Array of 260 pressure sensor readings (0-1023)
 * @param result: Pointer to store analysis results
 * @return: true if analysis successful, false otherwise
 */
uint16_t smoothed_data[TOTAL_SENSORS] = {0};
bool analyze_body_posture(const uint16_t *sensor_data, AnalysisResult_t *result)
{
    if (!sensor_data || !result)
    {
        return false;
    }

    // Initialize result
    rt_memset(result, 0, sizeof(AnalysisResult_t));
    rt_memset(smoothed_data, 0, sizeof(smoothed_data));

    // Apply smoothing filter
    smooth_sensor_data(sensor_data, smoothed_data);

    // Create binary mask
    create_binary_mask(smoothed_data);

    // Detect body regions
    uint8_t num_bodies = detect_body_regions(smoothed_data);

    result->num_bodies = (num_bodies > MAX_BODIES) ? MAX_BODIES : num_bodies;

    // Analyze each detected body
    uint8_t body_idx = 0;
    for (uint8_t label = 2; label < (2 + num_bodies) && body_idx < MAX_BODIES; label++)
    {
        BodyInfo_t *body = &result->bodies[body_idx];

        body->body_id = body_idx + 1;

        // Calculate center of mass
        body->center_of_mass = calculate_center_of_mass(smoothed_data, label);

        // Estimate waist center
        body->waist_center = estimate_waist_center(smoothed_data, label);

        // Get dimensions
        uint8_t min_x, min_y, max_x, max_y;
        get_bounding_box(label, &min_x, &min_y, &max_x, &max_y);
        body->width_mm = (max_x - min_x + 1) * POINT_SPACING_X_MM;
        body->height_mm = (max_y - min_y + 1) * POINT_SPACING_Y_MM;

        // Calculate total pressure
        body->total_pressure = 0;
        for (int i = 0; i < TOTAL_SENSORS; i++)
        {
            if (labeled_regions[i] == label)
            {
                body->total_pressure += smoothed_data[i];
            }
        }

        // Classify posture
        body->posture = classify_posture(label, &body->confidence);

        body_idx++;
    }
    /* */
    return true;
}

/*
 * Get posture string for debugging
 */
const char *get_posture_string(uint8_t posture)
{
    return (posture == 0) ? "supine" : "side";
}

/*
 * Print analysis results (for debugging)
 * Note: Requires printf implementation on MCU
 */
#ifdef DEBUG_PRINT
#include <stdio.h>
/*------------------/-------------------/-------------------/------------------/
 * @brief   结果打印
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
void print_analysis_results(const AnalysisResult_t *result)
{
    rt_kprintf("=== Body Analysis Results ===\n");
    rt_kprintf("Bodies detected: %d\n", result->num_bodies);

    for (int i = 0; i < result->num_bodies; i++)
    {
        const BodyInfo_t *body = &result->bodies[i];

        rt_kprintf("\nBody %d:\n", body->body_id);
        rt_kprintf("  Posture: %s (confidence: %d%%)\n",
                   get_posture_string(body->posture), body->confidence);
        rt_kprintf("  Waist center: (%d, %d) mm\n",
                   body->waist_center.x_mm, body->waist_center.y_mm);
        rt_kprintf("  Center of mass: (%d, %d) mm\n",
                   body->center_of_mass.x_mm, body->center_of_mass.y_mm);
        rt_kprintf("  Dimensions: %d x %d mm\n",
                   body->width_mm, body->height_mm);
        rt_kprintf("  Total pressure: %lu\n", body->total_pressure);
    }
}
#endif
