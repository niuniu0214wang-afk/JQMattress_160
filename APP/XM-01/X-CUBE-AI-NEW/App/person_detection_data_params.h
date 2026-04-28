/**
  ******************************************************************************
  * @file    person_detection_data_params.h
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-03-03T19:46:50+0800
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#ifndef PERSON_DETECTION_DATA_PARAMS_H
#define PERSON_DETECTION_DATA_PARAMS_H

#include "ai_platform.h"

/*
#define AI_PERSON_DETECTION_DATA_WEIGHTS_PARAMS \
  (AI_HANDLE_PTR(&ai_person_detection_data_weights_params[1]))
*/

#define AI_PERSON_DETECTION_DATA_CONFIG               (NULL)


#define AI_PERSON_DETECTION_DATA_ACTIVATIONS_SIZES \
  { 768, }
#define AI_PERSON_DETECTION_DATA_ACTIVATIONS_SIZE     (768)
#define AI_PERSON_DETECTION_DATA_ACTIVATIONS_COUNT    (1)
#define AI_PERSON_DETECTION_DATA_ACTIVATION_1_SIZE    (768)



#define AI_PERSON_DETECTION_DATA_WEIGHTS_SIZES \
  { 62600, }
#define AI_PERSON_DETECTION_DATA_WEIGHTS_SIZE         (62600)
#define AI_PERSON_DETECTION_DATA_WEIGHTS_COUNT        (1)
#define AI_PERSON_DETECTION_DATA_WEIGHT_1_SIZE        (62600)



#define AI_PERSON_DETECTION_DATA_ACTIVATIONS_TABLE_GET() \
  (&g_person_detection_activations_table[1])

extern ai_handle g_person_detection_activations_table[1 + 2];



#define AI_PERSON_DETECTION_DATA_WEIGHTS_TABLE_GET() \
  (&g_person_detection_weights_table[1])

extern ai_handle g_person_detection_weights_table[1 + 2];


#endif    /* PERSON_DETECTION_DATA_PARAMS_H */
