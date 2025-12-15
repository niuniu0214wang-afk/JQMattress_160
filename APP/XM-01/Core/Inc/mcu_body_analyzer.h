/*
 * MCU Body Detection and Analysis System - Header File
 * 
 * Lightweight implementation for microcontroller deployment
 * Compatible with 26x10 sensing mat (260 sensors)
 */

#ifndef _MCU_BODY_ANALYZER_H_
#define _MCU_BODY_ANALYZER_H_

#include "rtthread.h"
#include <stdint.h>
#include <stdbool.h>

#define DEBUG_PRINT
#define MAT_WIDTH           26
#define MAT_HEIGHT          10
#define TOTAL_SENSORS       (MAT_WIDTH * MAT_HEIGHT)
#define MAX_BODIES          2

#define POINT_SPACING_X_MM  44    // 4.4cm = 44mm
#define POINT_SPACING_Y_MM  64    // 6.4cm = 64mm
#define MAT_WIDTH_MM        (MAT_WIDTH * POINT_SPACING_X_MM)     // ~1144mm
#define MAT_HEIGHT_MM       (MAT_HEIGHT * POINT_SPACING_Y_MM)    // ~640mm

// Data structures
typedef struct 
{
    uint16_t x_mm;              // X coordinate in millimeters
    uint16_t y_mm;              // Y coordinate in millimeters
} Point_t;

typedef struct 
{
    uint8_t body_id;            // Body identifier (1 or 2)
    Point_t waist_center;       // Waist center coordinates
    Point_t center_of_mass;     // Center of mass coordinates
    uint16_t width_mm;          // Body width in mm
    uint16_t height_mm;         // Body height in mm
    uint32_t total_pressure;    // Sum of all pressure values
    uint8_t posture;            // 0=supine, 1=side
    uint8_t confidence;         // Confidence score (0-100)
} BodyInfo_t;

typedef struct 
{
    uint8_t num_bodies;         // Number of detected bodies (0-2)
    BodyInfo_t bodies[MAX_BODIES];
} AnalysisResult_t;


/**
 * @brief Main analysis function for body detection and posture classification
 * 
 * @param sensor_data Array of 260 pressure sensor readings (0-1023)
 * @param result Pointer to store analysis results
 * @return true if analysis successful, false otherwise
 * 
 * @note Processing time: ~10ms on ARM Cortex-M4 @ 168MHz
 * @note Memory usage: ~2KB RAM
 */
bool analyze_body_posture(const uint16_t* sensor_data, AnalysisResult_t* result);

/**
 * @brief Get posture string for debugging/display
 * 
 * @param posture Posture code (0=supine, 1=side)
 * @return Constant string pointer ("supine" or "side")
 */
const char* get_posture_string(uint8_t posture);

/**
 * @brief Print analysis results (requires printf implementation)
 * 
 * @param result Pointer to analysis results
 * @note Only available when DEBUG_PRINT is defined
 */
#ifdef DEBUG_PRINT
void print_analysis_results(const AnalysisResult_t* result);
#endif

// Configuration macros (can be adjusted for different hardware)
#ifndef MIN_BODY_PRESSURE
#define MIN_BODY_PRESSURE   50      // Minimum pressure threshold for body detection
#endif

#ifndef MIN_BODY_AREA
#define MIN_BODY_AREA       20      // Minimum number of connected sensors for a body
#endif

// Memory usage information
#define MCU_ANALYZER_RAM_USAGE  (3 * TOTAL_SENSORS + sizeof(AnalysisResult_t))  // ~2KB


#endif /* MCU_BODY_ANALYZER_H */ 


