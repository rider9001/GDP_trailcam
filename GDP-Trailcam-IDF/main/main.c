/// ------------------------------------------
/// @file main.c
///
/// @brief Main source of the GDP trailcam project
/// ------------------------------------------

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "SDSPI.h"
#include "Camera.h"

static const char *MAIN_TAG = "main";

SDSPI_connection_t connection;
camera_config_t config;

#define TRIGGER_PIN 11
#define SIGNAL_PIN 10

QueueHandle_t cam_queue;

TaskHandle_t cam_task_handle = NULL;

void get_frame_task(void* params)
{
    // This should always be run first, sets all power down pins as outputs and shuts down all cameras
    setup_all_cam_power_down_pins();
    int cam_pwr_pin = -1;

    while(1)
    {
        if (xQueueReceive(cam_queue, &cam_pwr_pin, portMAX_DELAY))
        {
            config = get_default_camera_config(cam_pwr_pin);

            ESP_LOGI(MAIN_TAG, "Starting camera");
            if (start_camera(config) != ESP_OK)
            {
                ESP_LOGE(MAIN_TAG, "Failed to start Camera");
                continue;
            }

            ESP_LOGI(MAIN_TAG, "Grabbing frame buffer");
            camera_fb_t* fb = esp_camera_fb_get();
            if (!fb) {
                ESP_LOGE(MAIN_TAG, "Frame buffer could not be acquired");
                continue;
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
                continue;
            }

            char img_file[FILENAME_MAX_SIZE];
            sprintf(img_file, MOUNT_POINT"/img%u.jpg", (size_t)esp_log_timestamp());

            ESP_LOGI(MAIN_TAG, "Writing frame buffer to SD: %s", img_file);
            if(write_data_SDSPI(img_file, jpg_data.buf, jpg_data.len) != ESP_OK)
            {
                ESP_LOGE(MAIN_TAG, "Failed to write camera buffer");
            }

            free(jpg_data.buf);

            close_SDSPI_connection(connection);
        }
    }
}

static void IRAM_ATTR cam_trigger_ISR(void* args)
{
    int pin = 0;
    xQueueSendFromISR(cam_queue, &pin, NULL);
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
    gpio_isr_handler_add(TRIGGER_PIN, cam_trigger_ISR, NULL);

    esp_rom_gpio_pad_select_gpio(SIGNAL_PIN);
    gpio_set_direction(SIGNAL_PIN, GPIO_MODE_OUTPUT);

    while(1)
    {
        ESP_LOGI(MAIN_TAG, "Sending negedge in 5 sec");
        gpio_set_level(SIGNAL_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(5000));
        gpio_set_level(SIGNAL_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}