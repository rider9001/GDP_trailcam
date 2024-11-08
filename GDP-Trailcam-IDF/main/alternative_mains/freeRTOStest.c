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

        // Wait this task for a few seconds whilst camera task runs to prevent interference
        vTaskDelay(pdMS_TO_TICKS(10000));

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
    connection = connect_to_SDSPI(PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
    if (connection.card == NULL)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start SDSPI");
        return;
    }

    gpio_install_isr_service(0);

    // This should always be run first, sets all power down pins as outputs and shuts down all cameras
    setup_all_cam_power_down_pins();

    // ESP_LOGI(MAIN_TAG, "POSTing all cameras");
    // if (POST_all_cams() != ESP_OK)
    // {
    //     ESP_LOGE(MAIN_TAG, "POST failed on a camera");
    //     return;d
    // }
    // ESP_LOGI(MAIN_TAG, "Camera POST sucsess");

    // ESP_LOGI(MAIN_TAG, "Running SD SPI POST...");
    // if (SDSPI_POST() != ESP_OK)
    // {
    //     ESP_LOGE(MAIN_TAG, "POST failed on a SD SPI");
    //     return;
    // }
    // ESP_LOGI(MAIN_TAG, "SD SPI POST sucsess");

    esp_rom_gpio_pad_select_gpio(PIR_PIN);
    gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);
    gpio_pulldown_dis(PIR_PIN);
    gpio_pullup_en(PIR_PIN);
    gpio_set_intr_type(PIR_PIN, GPIO_INTR_LOW_LEVEL);

    cam_queue = xQueueCreate(10, sizeof(int));

    xTaskCreate(PIR_state_latch, "PIR_state_latch", 1024 * 16, NULL, 3, &PIR_trig_handle);
    vTaskPrioritySet(NULL, 4);

    ESP_LOGI(MAIN_TAG, "Waiting for low");

    // Only engage ISR once all cams tested
    gpio_isr_handler_add(PIR_PIN, PIR_watchdog_ISR, NULL);

    while(1)
    {
        int cam_pwr_pin = -1;
        if (xQueueReceive(cam_queue, &cam_pwr_pin, portMAX_DELAY))
        {
            ESP_LOGI(MAIN_TAG, "Starting capture on cam_pwr_pin: %i", cam_pwr_pin);

            config = get_default_camera_config(cam_pwr_pin);

            jpg_motion_data_t motion_data = get_motion_capture(config);

            if (motion_data.capture_sucsess == true)
            {
                if (connection.card == NULL)
                {
                    ESP_LOGE(MAIN_TAG, "Failed to start SDSPI");
                    free_motion_data(motion_data);
                    continue;
                }

                char dir[FILENAME_MAX_SIZE];
                sprintf(dir, MOUNT_POINT"/%u", (size_t) esp_log_timestamp());
                if (create_dir_SDSPI(dir) != ESP_OK)
                {
                    free_motion_data(motion_data);
                    continue;
                }

                char motion_filenm1[FILENAME_MAX_SIZE];
                sprintf(motion_filenm1, "%.16s/img1.jpg", dir);

                if (write_data_SDSPI(motion_filenm1, motion_data.img1.buf, motion_data.img1.len) != ESP_OK)
                {
                    free_motion_data(motion_data);
                    continue;
                }

                char motion_filenm2[FILENAME_MAX_SIZE];
                sprintf(motion_filenm2, "%.16s/img2.jpg", dir);

                if (write_data_SDSPI(motion_filenm2, motion_data.img2.buf, motion_data.img2.len) != ESP_OK)
                {
                    free_motion_data(motion_data);
                    continue;
                }

                char info_filenm[FILENAME_MAX_SIZE];
                sprintf(motion_filenm2, "%.16s/info.txt", dir);

                const char info[FILENAME_MAX_SIZE];
                sprintf(info, "Images were taken %ums apart", motion_data.ms_between);

                if (write_text_SDSPI(info_filenm, info))
                {
                    free_motion_data(motion_data);
                    continue;
                }

                free_motion_data(motion_data);
            }
            else
            {
                free_motion_data(motion_data);
            }
        }
    }
}