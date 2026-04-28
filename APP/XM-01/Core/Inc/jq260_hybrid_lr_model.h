#ifndef JQ260_HYBRID_LR_MODEL_H
#define JQ260_HYBRID_LR_MODEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JQ260_HYBRID_NUM_FEATURES 260
#define JQ260_REGION_UPPER 0u
#define JQ260_REGION_LOWER 1u

#define JQ260_PERSON_COUNT_ONE 1u
#define JQ260_PERSON_COUNT_TWO 2u
#define JQ260_POSTURE_SUPINE 1u
#define JQ260_POSTURE_SIDE 2u

uint8_t jq260_hybrid_person_count_predict(const uint8_t sensor_data[JQ260_HYBRID_NUM_FEATURES], float *confidence);
uint8_t jq260_hybrid_double_posture_predict(const uint8_t sensor_data[JQ260_HYBRID_NUM_FEATURES], uint8_t region, float *side_probability);

#ifdef __cplusplus
}
#endif

#endif /* JQ260_HYBRID_LR_MODEL_H */
