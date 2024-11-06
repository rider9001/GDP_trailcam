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
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "SDSPI.h"

/// @brief Debugging string tag
static const char *CAM_TAG = "TrailCamera";

/// @brief List of powerdown pins for all attached cameras
static const int cam_power_down_pins[] = {CONFIG_PIN_CAM_PWRDN_1, CONFIG_PIN_CAM_PWRDN_2, CONFIG_PIN_CAM_PWRDN_3, CONFIG_PIN_CAM_PWRDN_4};

/// @brief Logic level for camera power off
#define CAM_POWER_OFF 1

/// @brief Logic level for camera power on
#define CAM_POWER_ON 0

/// @brief Time to allow camera to intialize state in POST test
#define CAM_POST_WAIT_TIME_MS 5000

/// @brief Delay in ms after power down pin is pulled low to init the camera
#define CAM_WAKEUP_DELAY_MS 75

/// @brief Struct that contains image buffer data (NOTE: buf must be individually freed)
typedef struct
{
    uint8_t* buf;
    size_t len;
} image_data_t;

/// ------------------------------------------
/// @brief Creates a default camera config sturct to use for initializing and deinitializing a camera
/// Can be modified before use
///
/// @param power_down_pin pin used for controlling power to the camera
///
/// @return camera config structure
camera_config_t get_default_camera_config(const uint32_t power_down_pin);

/// ------------------------------------------
/// @brief Performs a basic POST to verify that the camera can be polled for an image
///
/// @param config of the camera to test
///
/// @return ESP_OK if tests pass
esp_err_t cam_POST(const camera_config_t config);

/// ------------------------------------------
/// @brief POSTs all cameras from the list of power down pins
///
/// @return ESP_OK if all cams pass
esp_err_t POST_all_cams();

/// ------------------------------------------
/// @brief Attempts to start the camera using the given config
/// Will powerup the camera using the power pin in the config before
/// attempting comms
///
/// @param cam_config configuration to use when starting the camera
///
/// @return ESP_OK if sucsessful
esp_err_t start_camera(const camera_config_t cam_config);

/// ------------------------------------------
/// @brief De-intializes the camera and closes connection
/// Pulls power down pin into OFF state
///
/// @note causes all frame buffers to be freed
///
/// @param cam_config config of the camera to shut down
///
/// @return ESP_OK if camera closes sucsessfully (camera will be powered down reguardless of sucsess)
esp_err_t stop_camera(const camera_config_t cam_config);

/// ------------------------------------------
/// @brief Extracts the image data of the frame buffer into non-camera linked memory
///
/// @param fb frame buffer to extract
///
/// @return structure of image data
image_data_t extract_camera_buffer(const camera_fb_t* fb);

/// ------------------------------------------
/// @brief Writes the given frame buffer to the given path
///
/// @note All frame buffers go null the momment the camera is deinited
/// Copy buffer into other memory if needed
///
/// @param fb frame buffer to write
/// @param save_path path with filename to write buffer to
///
/// @return ESP_OK if sucsessful
esp_err_t write_fb_to_SD(const camera_fb_t* fb, const char* save_path);

/// ------------------------------------------
/// @brief Sets all camera image settings to sensible values
void default_frame_settings();

/// ------------------------------------------
/// @brief Sets up the power down pins given in cam_power_down_pins as a driven outputs
/// and sets all camera power down pins to high (camera off)
void setup_all_cam_power_down_pins();