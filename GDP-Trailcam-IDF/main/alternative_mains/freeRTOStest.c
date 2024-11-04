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

const uint32_t cam_pwr_pin = CONFIG_PIN_CAM_PWRDN;
camera_config_t config;

#define TRIGGER_PIN -1
QueueHandle_t cam_queue;

TaskHandle_t cam_task_handle = NULL;

void get_frame_task()
{
    // This should always be run first, sets all power down pins as outputs and shuts down all cameras
    setup_all_cam_power_down_pins();
    config = get_default_camera_config(cam_pwr_pin);

    while(1)
    {
        vTaskSuspend(NULL);

        ESP_LOGI(MAIN_TAG, "Starting camera");
        if (start_camera(config) != ESP_OK)
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

        const char* img_file = MOUNT_POINT"/img.jpg";

        ESP_LOGI(MAIN_TAG, "Writing frame buffer to SD: %s", img_file);
        if(write_fb_to_SD(fb, img_file) != ESP_OK)
        {
            ESP_LOGE(MAIN_TAG, "Failed to write camera buffer");
        }

        // Return camera buffer
        esp_camera_fb_return(fb);

        ESP_LOGI(MAIN_TAG, "Stopping camera");
        if (stop_camera(config) != ESP_OK)
        {
            ESP_LOGE(MAIN_TAG, "Failed to stop camera");
        }

        close_SDSPI_connection(connection);
    }
}

static void IRAM_ATTR ISR_resumer()
{
    xTaskResumeFromISR(cam_task_handle);
}

void app_main(void)
{
    esp_rom_gpio_pad_select_gpio(TRIGGER_PIN);
    gpio_set_direction(TRIGGER_PIN, GPIO_MODE_INPUT);
    gpio_pulldown_dis(TRIGGER_PIN);
    gpio_pullup_en(TRIGGER_PIN);
    gpio_set_intr_type(TRIGGER_PIN, GPIO_INTR_NEGEDGE);

    cam_queue = xQueueCreate(10, sizeof(int));

    xTaskCreate(get_frame_task, "get_frame_task", 4096, NULL, 1, &cam_task_handle);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(TRIGGER_PIN, ISR_resumer, NULL);
}