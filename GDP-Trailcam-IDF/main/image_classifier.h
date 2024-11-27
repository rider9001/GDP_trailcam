/// ------------------------------------------
/// @file image_classifier.h
///
/// @brief Header file for image classifer object
/// ------------------------------------------
#pragma once

#include "model.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void tf_setup_init();
float* tf_get_input_pointer();
void tf_start_inference(void* params);
void tf_stop_inference();
QueueHandle_t tf_get_prediction_queue_handle();