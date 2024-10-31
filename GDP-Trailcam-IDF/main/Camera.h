/// ------------------------------------------
/// @file Camera.h
///
/// @brief Header file for Camera interaction using esp_camera API
/// ------------------------------------------
#pragma once

#include <esp_system.h>
#include "esp_camera.h"
#include <esp_psram.h>

/// ------------------------------------------
/// @brief Creates a default camera config sturct to use for initializing a camera
/// Can be modified before use
/// @return camera config structure
camera_config_t get_default_camera_config();

/// ------------------------------------------
/// @brief Attempts to start the camera using the given config
///
/// @param cam_config configuration to use when starting the camera
///
/// @return ESP_OK if sucsessful
esp_err_t start_camera(const camera_config_t cam_config);