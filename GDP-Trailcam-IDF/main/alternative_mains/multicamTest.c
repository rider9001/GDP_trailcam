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

void app_main(void)
{
    // This should always be run first, sets all power down pins as outputs and shuts down all cameras
    setup_all_cam_power_down_pins();
    camera_config_t config_cam_1 = get_default_camera_config(cam_power_down_pins[0]);
    camera_config_t config_cam_2 = get_default_camera_config(cam_power_down_pins[1]);

    ESP_LOGI(MAIN_TAG, "Starting camera 1");
    if (start_camera(config_cam_1) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start Camera");
        return;
    }

    ESP_LOGI(MAIN_TAG, "Grabbing frame buffer");
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(MAIN_TAG, "Frame buffer could not be acquired");
        return;
    }
    ESP_LOGI(MAIN_TAG, "Camera buffer grabbed sucsessfully");
    ESP_LOGI(MAIN_TAG, "Image is %u bytes", fb->len);

    ESP_LOGI(MAIN_TAG, "Starting SDSPI comms.");
    connection = connect_to_SDSPI(PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
    if (connection.card == NULL)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start SDSPI");
        return;
    }

    const char* img_file_1 = MOUNT_POINT"/img1.jpg";

    ESP_LOGI(MAIN_TAG, "Writing frame buffer to SD: %s", img_file_1);
    if(write_fb_to_SD(fb, img_file_1) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to write camera buffer");
    }

    // Return camera buffer
    esp_camera_fb_return(fb);

    ESP_LOGI(MAIN_TAG, "Stopping camera 1");
    if (stop_camera(config_cam_1) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to stop camera");
    }

    ESP_LOGI(MAIN_TAG, "Starting camera 2");
    if (start_camera(config_cam_2) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start Camera");
        return;
    }

    ESP_LOGI(MAIN_TAG, "Grabbing frame buffer");
    fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(MAIN_TAG, "Frame buffer could not be acquired");
        return;
    }
    ESP_LOGI(MAIN_TAG, "Camera buffer grabbed sucsessfully");
    ESP_LOGI(MAIN_TAG, "Image is %u bytes", fb->len);

    ESP_LOGI(MAIN_TAG, "Starting SDSPI comms.");
    connection = connect_to_SDSPI(PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
    if (connection.card == NULL)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start SDSPI");
        return;
    }

    const char* img_file_2 = MOUNT_POINT"/img2.jpg";

    ESP_LOGI(MAIN_TAG, "Writing frame buffer to SD: %s", img_file_2);
    if(write_fb_to_SD(fb, img_file_2) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to write camera buffer");
    }

    // Return camera buffer
    esp_camera_fb_return(fb);

    ESP_LOGI(MAIN_TAG, "Stopping camera 2");
    if (stop_camera(config_cam_2) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to stop camera");
    }

    close_SDSPI_connection(connection);
}