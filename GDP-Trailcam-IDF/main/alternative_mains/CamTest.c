/// ------------------------------------------
/// @file main.c
///
/// @brief Main source of the GDP trailcam project
/// ------------------------------------------

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "SDSPI.h"
#include "Camera.h"

static const char *MAIN_TAG = "main";

SDSPI_connection_t connection;

const uint32_t cam_pwr_pin = CONFIG_PIN_CAM_PWRDN_1;

void app_main(void)
{
    // This should always be run first, sets all power down pins as outputs and shuts down all cameras
    setup_all_cam_power_down_pins();
    camera_config_t config = get_default_camera_config(cam_pwr_pin);

    ESP_LOGI(MAIN_TAG, "Starting camera");
    if (start_camera(config) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start Camera");
        return;
    }

    ESP_LOGI(MAIN_TAG, "Testing powerdown");

    stop_camera(config);
    ESP_LOGI(MAIN_TAG, "Waiting 5 seconds...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGI(MAIN_TAG, "Restarting camera");

    if (start_camera(config) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start Camera");
        return;
    }

    default_frame_settings();

    ESP_LOGI(MAIN_TAG, "Grabbing frame buffer");
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(MAIN_TAG, "Frame buffer could not be acquired");
        return;
    }
    ESP_LOGI(MAIN_TAG, "Camera buffer grabbed sucsessfully");
    ESP_LOGI(MAIN_TAG, "Image is %u bytes", fb->len);

    image_data_t jpg_data = extract_camera_buffer(fb);

    // Return camera buffer
    esp_camera_fb_return(fb);

    ESP_LOGI(MAIN_TAG, "Stopping camera");
    if (stop_camera(config) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to stop camera");
    }

    ESP_LOGI(MAIN_TAG, "Starting SDSPI comms.");
    connection = connect_to_SDSPI(PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
    if (connection.card == NULL)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start SDSPI");
        return;
    }

    const char* img_file = MOUNT_POINT"/img.jpg";

    ESP_LOGI(MAIN_TAG, "Writing frame buffer to SD: %s", img_file);
    if(write_data_SDSPI(img_file, jpg_data.buf, jpg_data.len) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to write camera buffer");
    }

    free(jpg_data.buf);

    close_SDSPI_connection(connection);
}