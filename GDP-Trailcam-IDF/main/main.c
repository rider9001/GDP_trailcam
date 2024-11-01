/// ------------------------------------------
/// @file main.c
///
/// @brief Main source of the GDP trailcam project
/// ------------------------------------------

#include "SDSPI.h"
#include "Camera.h"

static const char *MAIN_TAG = "main";

SDSPI_connection_t connection;

void app_main(void)
{
    camera_config_t config = get_default_camera_config();

    ESP_LOGI(MAIN_TAG, "Starting camera");
    if (start_camera(config) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start Camera");
        return;
    }

    default_camera_settings();

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

    const char* img_file = MOUNT_POINT"/img.jpg";

    ESP_LOGI(MAIN_TAG, "Writing frame buffer to SD: %s", img_file);
    if(write_fb_to_SD(fb, img_file) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to write camera buffer");
    }

    // Return camera buffer
    esp_camera_fb_return(fb);

    ESP_LOGI(MAIN_TAG, "Stopping camera");
    if (stop_camera() != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to stop camera");
    }

    close_SDSPI_connection(connection);
}