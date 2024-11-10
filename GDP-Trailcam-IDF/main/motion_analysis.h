/// ------------------------------------------
/// @file motion_analysis.h
///
/// @brief Header file for all image motion analysis functions
/// ------------------------------------------
#pragma once

#include <inttypes.h>
#include <stdlib.h>
#include <math.h>
#include "esp_log.h"
#include "esp_camera.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "Camera.h"
#include "image_types.h"

static const char* MOTION_TAG = "motion_analysis";

/// ------------------------------------------
/// @brief Generates a grayscale image from an input jpg
///
/// @param jpg_image input jpg image
///
/// @return grayscale image struct, buf is null if conversion fails
grayscale_image_t convert_jpg_to_grayscale(const jpg_image_t* jpg_image);

/// ------------------------------------------
/// @brief Converts a motion set from jpg to grayscale format
///
/// @param jpg_motion input motion set
///
/// @return grayscale motion set, check data_valid for validity
grayscale_motion_data_t convert_jpg_motion_to_grayscale(const jpg_motion_data_t* jpg_motion);

/// ------------------------------------------
/// @brief Performs image subtraction on a motion set and outputs the resulting image
///
/// @param motion_set motion set to perform subtraction on
///
/// @return grayscale motion subtracted image
grayscale_image_t motion_image_subtract(const grayscale_motion_data_t* motion_set);

/// ------------------------------------------
/// @brief Ingests a jpg motion set and generates a subtracted image
///
/// @note Frees motion set data as it processes, including input
///
/// @param motion_set input jpg motion set
///
/// @return subtracted grayscale image
grayscale_image_t perform_motion_analysis(jpg_motion_data_t* motion_set);