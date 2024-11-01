/// ------------------------------------------
/// @file Camera.h
///
/// @brief Header file for Camera interaction using esp_camera API
/// ------------------------------------------
#pragma once

#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_system.h>
#include "esp_camera.h"
#include <esp_psram.h>
#include "esp_mac.h"
#include "esp_log.h"

#include "SDSPI.h"

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

/// ------------------------------------------
/// @brief De-intializes the camera and closes connection
///
/// @return ESP_OK if camera closes sucsessfully
esp_err_t stop_camera();

/// ------------------------------------------
/// @brief Writes the given frame buffer to the given path
///
/// @note filename should include file extension
///
/// @param fb frame buffer to write
/// @param save_path path with filename to write buffer to
///
/// @return ESP_OK if sucsessful
esp_err_t write_fb_to_SD(const camera_fb_t* fb, const char* save_path);

/// ------------------------------------------
/// @brief Sets all camera image settings to somewhat sensible values
void default_camera_settings();