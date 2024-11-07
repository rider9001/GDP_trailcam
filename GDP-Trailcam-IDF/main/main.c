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

#define PIR_PIN 11

QueueHandle_t cam_queue;

TaskHandle_t PIR_trig_handle = NULL;

volatile int PIR_active = 0;

// interrupts on low PIR pin level and detaches itself immediately, then unpauses the PIR handler
static void IRAM_ATTR PIR_watchdog_ISR()
{
    gpio_isr_handler_remove(PIR_PIN);
    vTaskResume(PIR_trig_handle);
}

// interrupts on low PIR level to tell PIR_state_latch that the PIR is still being noisy
static void IRAM_ATTR PIR_hold_state_ISR()
{
    gpio_isr_handler_remove(PIR_PIN);
    PIR_active = 1;
}

void PIR_state_latch()
{
    int pin = cam_power_down_pins[0];

    while(1)
    {
        // Wait here for ISR to unpause
        vTaskSuspend(NULL);
        ESP_LOGI(MAIN_TAG, "PIR active, running camera");

        // when unpaused that means that the PIR has triggered, run camera task
        xQueueSend(cam_queue, &pin, NULL);

        PIR_active = 1;
        while (PIR_active)
        {
            // Test if ISR detects PIR being noisy over 500ms
            PIR_active = 0;
            // Connect watchdog ISR
            gpio_isr_handler_add(PIR_PIN, PIR_hold_state_ISR, NULL);
            vTaskDelay(pdMS_TO_TICKS(500));
            if (PIR_active == 0)
            {
                ESP_LOGI(MAIN_TAG, "PIR quiet, relistening");
            }
            else
            {
                ESP_LOGI(MAIN_TAG, "PIR noisy, waiting");
            }
        }

        // reattach old ISR
        gpio_isr_handler_add(PIR_PIN, PIR_watchdog_ISR, NULL);

        // Loop should now reset and pause, waiting for next watchdog unpause
    }
}

void app_main(void)
{
    esp_rom_gpio_pad_select_gpio(PIR_PIN);
    gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);
    gpio_pulldown_dis(PIR_PIN);
    gpio_pullup_en(PIR_PIN);
    gpio_set_intr_type(PIR_PIN, GPIO_INTR_LOW_LEVEL);

    cam_queue = xQueueCreate(10, sizeof(int));

    xTaskCreate(PIR_state_latch, "PIR_state_latch", 1024 * 16, NULL, 3, &PIR_trig_handle);
    vTaskPrioritySet(NULL, 4);

    gpio_install_isr_service(0);

    // This should always be run first, sets all power down pins as outputs and shuts down all cameras
    setup_all_cam_power_down_pins();
    int cam_pwr_pin = -1;

    ESP_LOGI(MAIN_TAG, "POSTing all cameras");
    if (POST_all_cams() != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "POST failed on a camera");
        return;
    }
    ESP_LOGI(MAIN_TAG, "Camera POST sucsess");

    ESP_LOGI(MAIN_TAG, "Running SD SPI POST...");
    if (SDSPI_POST() != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "POST failed on a SD SPI");
        return;
    }
    ESP_LOGI(MAIN_TAG, "SD SPI POST sucsess");


    ESP_LOGI(MAIN_TAG, "Waiting for low");

    // Only engage ISR once all cams tested
    gpio_isr_handler_add(PIR_PIN, PIR_watchdog_ISR, NULL);

    while(1)
    {
        if (xQueueReceive(cam_queue, &cam_pwr_pin, portMAX_DELAY))
        {
            ESP_LOGI(MAIN_TAG, "Starting capture on cam_pwr_pin: %i", cam_pwr_pin);

            config = get_default_camera_config(cam_pwr_pin);

            ESP_LOGI(MAIN_TAG, "Starting camera");
            if (start_camera(config) != ESP_OK)
            {
                ESP_LOGE(MAIN_TAG, "Failed to start Camera");
                continue;
            }

            default_frame_settings();

            ESP_LOGI(MAIN_TAG, "Grabbing frame buffer");
            camera_fb_t* fb = esp_camera_fb_get();
            if (!fb) {
                ESP_LOGE(MAIN_TAG, "Frame buffer could not be acquired");
                stop_camera(config);
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
                close_SDSPI_connection(connection);
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