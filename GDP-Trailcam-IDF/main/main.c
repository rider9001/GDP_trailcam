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
#include "esp_pm.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"

#include "SDSPI.h"
#include "Camera.h"
#include "motion_analysis.h"
#include "image_cropping.h"
#include "status_led.h"

static const char* MAIN_TAG = "main";

SDSPI_connection_t connection;
camera_config_t config;

#define NVS_CAP_COUNT_KEY "next_cap_num"

#define PIR_PIN 2
#define PIR_TRIG_LEVEL 0

#define MAX_CONT_CAP 5

QueueHandle_t motion_proc_queue;

volatile bool processing_active = false;

void setup_ext0_wakeup()
{
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(PIR_PIN, PIR_TRIG_LEVEL));

    ESP_ERROR_CHECK(rtc_gpio_pullup_en(PIR_PIN));
    ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(PIR_PIN));
}

void enter_deep_sleep()
{
    gpio_deep_sleep_hold_en();

    ESP_LOGI(MAIN_TAG, "Setting pin %i to wakeup on level %i", PIR_PIN, PIR_TRIG_LEVEL);
    setup_ext0_wakeup();

    ESP_LOGI(MAIN_TAG, "Entering deep sleep zzz...");
    esp_deep_sleep_start();
}

void motion_processing_task()
{
    processing_active = true;
    ESP_LOGI(MAIN_TAG, "Starting camera proc task");
    while (1)
    {
        jpg_motion_data_t jpg_motion_data;
        jpg_motion_data.data_valid = false;
        if (xQueueReceive(motion_proc_queue, &jpg_motion_data, portMAX_DELAY))
        {
            if (jpg_motion_data.data_valid  == false)
            {
                ESP_LOGW(MAIN_TAG, "Wait timeout, continuing");
                continue;
            }

            uint32_t capture_count = jpg_motion_data.capture_count;

            ESP_LOGI(MAIN_TAG, "Analysing motion on caputre");
            grayscale_image_t sub_img = perform_motion_analysis(&jpg_motion_data);

            if (sub_img.buf != NULL)
            {
                ESP_LOGI(MAIN_TAG, "Writing image subtraction");
                char* sub_filenm = malloc(32);
                sprintf(sub_filenm, MOUNT_POINT"/CAPTURE%lu/sub.bin", capture_count);

                if (write_data_SDSPI(sub_filenm, sub_img.buf, sub_img.len) != ESP_OK)
                {
                    ESP_LOGE(MAIN_TAG, "Failed to write motion to SD");
                }
                free(sub_filenm);

                point_t bb_origin;
                ESP_LOGI(MAIN_TAG, "Attempting to find centre of motion");
                if (find_motion_centre(&sub_img, &bb_origin))
                {
                    free(sub_img.buf);

                    ESP_LOGI(MAIN_TAG, "Motion bounding box from (%u,%u) to (%u,%u)",
                            bb_origin.x,
                            bb_origin.y,
                            bb_origin.x+BOUNDING_BOX_EDGE_LEN,
                            bb_origin.y+BOUNDING_BOX_EDGE_LEN);

                    draw_motion_box(&sub_img, bb_origin);

                    ESP_LOGI(MAIN_TAG, "Writing box image");
                    char* box_filenm = malloc(sizeof(char) * 32);
                    sprintf(box_filenm, MOUNT_POINT"/CAPTURE%lu/box.bin", capture_count);
                    if (write_data_SDSPI(box_filenm, sub_img.buf, sub_img.len) != ESP_OK)
                    {
                        ESP_LOGE(MAIN_TAG, "Failed to write bounding box to SD");
                    }
                    free(box_filenm);

                    ESP_LOGI(MAIN_TAG, "Cropping jpg image");
                    jpg_image_t box_img = crop_jpg_img(&jpg_motion_data.img1, bb_origin);
                    free_jpg_motion_data(&jpg_motion_data);

                    if (box_img.buf == NULL)
                    {
                        ESP_LOGE(MAIN_TAG, "Image cropping failed");
                    }
                    else
                    {
                        char* box_img_filenm = malloc(64);
                        sprintf(box_img_filenm, MOUNT_POINT"/CAPTURE%lu/box.jpg", capture_count);

                        if (write_data_SDSPI(box_img_filenm, box_img.buf, box_img.len) != ESP_OK)
                        {
                            ESP_LOGE(MAIN_TAG, "Failed to write cropped img to SD");
                        }
                        free(box_img_filenm);

                        free(box_img.buf);
                    }
                }
                else
                {
                    free(sub_img.buf);
                    ESP_LOGI(MAIN_TAG, "Image not motion significant");
                }
            }
            else
            {
                ESP_LOGI(MAIN_TAG, "Analysis failed!");
            }
        }

        if (uxQueueMessagesWaiting(motion_proc_queue) == 0)
        {
            ESP_LOGI(MAIN_TAG, "Queue empty, no longer processing images");

            processing_active = false;
            // Pause this task, if unpaused then a new capture will have been made by the PIR
            vTaskSuspend(NULL);
        }
    }
}

void capture_motion_images(uint32_t capture_num)
{
    int cam_pwr_pin = cam_power_down_pins[0];

    ESP_LOGI(MAIN_TAG, "Starting capture on cam_pwr_pin: %i", cam_pwr_pin);

    config = get_default_camera_config(cam_pwr_pin);

    jpg_motion_data_t* motion = get_motion_capture(config);
    motion->capture_count = capture_num;

    ESP_LOGI(MAIN_TAG, "Time between is: %ums", motion->t2 - motion->t1);

    char dir[32];
    sprintf(dir, MOUNT_POINT"/CAPTURE%lu", capture_num);

    if (create_dir_SDSPI(dir) == ESP_OK)
    {
        char filenm1[FILENAME_MAX_SIZE];
        sprintf(filenm1, "%s/img1.jpg", dir);

        if (write_jpg_data_to_SD(filenm1, motion->img1) != ESP_OK)
        {
            ESP_LOGE(MAIN_TAG, "Failed to write %s", filenm1);
        }

        char filenm2[FILENAME_MAX_SIZE];
        sprintf(filenm2, "%s/img2.jpg", dir);

        if (write_jpg_data_to_SD(filenm2, motion->img2) != ESP_OK)
        {
            ESP_LOGE(MAIN_TAG, "Failed to write %s", filenm2);
        }

        char filenm_info[FILENAME_MAX_SIZE];
        sprintf(filenm_info, "%s/info.txt", dir);

        char* info_text = malloc(300 * sizeof(char));
        sprintf(info_text, "Images were taken %ums apart.\nImage 1: %u\nImage 2: %u\n"
                            "Image res is %ux%u", motion->t2 - motion->t1,
                            motion->t1, motion->t2, motion->img1.width, motion->img1.height);
        ESP_LOGI(MAIN_TAG, "%s", info_text);

        if (write_text_SDSPI(filenm_info, info_text) != ESP_OK)
        {
            ESP_LOGE(MAIN_TAG, "Failed to write %s", filenm_info);
        }
        free(info_text);
    }

    // The motion set should be considered transfered to the processing task
    ESP_LOGI(MAIN_TAG, "Sending capture to motion analysis");
    xQueueSend(motion_proc_queue, motion, NULL);
}

void app_main(void)
{
    setup_onboard_led();
    clear_led();

    // Most SDSPI functions assume that there exists a working connection already
    connect_to_SDSPI(PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS, &connection);

    if (connection.card == NULL)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start SDSPI");
        set_led_colour(255, 0, 0); // error colour
        return;
    }

    esp_sleep_source_t wakeup_reason = esp_sleep_get_wakeup_cause();
    ESP_LOGI(MAIN_TAG, "Wakeup reason: %i", wakeup_reason);

    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED)
    {
        ESP_LOGI(MAIN_TAG, "Power on");

        ESP_LOGI(MAIN_TAG, "Running SD SPI POST...");
        set_led_colour(0, 120, 0); // SD card setup colour (Green)
        if (SDSPI_POST() != ESP_OK)
        {
            ESP_LOGE(MAIN_TAG, "POST failed on a SD SPI");
            set_led_colour(255, 0, 0); // error colour
            return;
        }
        ESP_LOGI(MAIN_TAG, "SD SPI POST sucsess");
        clear_led();

        ESP_LOGI(MAIN_TAG, "NVS erase and test");
        if (nvs_flash_init() != ESP_OK)
        {
            ESP_LOGE(MAIN_TAG, "POST failed on NVS");
            set_led_colour(255, 0, 0); // error colour
            return;
        }
        nvs_flash_erase();
        nvs_flash_init();

        nvs_handle_t my_handle;
        if (nvs_open("storage", NVS_READWRITE, &my_handle) != ESP_OK)
        {
            ESP_LOGE(MAIN_TAG, "POST failed on NVS open");
            set_led_colour(255, 0, 0); // error colour
            return;
        }

        esp_err_t err = nvs_set_u32(my_handle, NVS_CAP_COUNT_KEY, get_next_capture_num());
        if (err != ESP_OK)
        {
            ESP_LOGE(MAIN_TAG, "POST failed on NVS writing, %s", esp_err_to_name(err));
            set_led_colour(255, 0, 0); // error colour
            return;
        }

        set_led_colour(0, 0, 120); // Cam POST indicator (Blue)
        // This should always be run first, sets all power down pins as outputs and shuts down all cameras
        ESP_LOGI(MAIN_TAG, "Setting up cam power pins");
        setup_all_cam_power_down_pins();

        ESP_LOGI(MAIN_TAG, "POSTing all cameras");
        if (POST_all_cams() != ESP_OK)
        {
            ESP_LOGE(MAIN_TAG, "POST failed on a camera");
            set_led_colour(255, 0, 0); // error colour
            return;
        }
        ESP_LOGI(MAIN_TAG, "Camera POST sucsess");
        clear_led();

        ESP_LOGI(MAIN_TAG, "All POSTs successful");

        enter_deep_sleep();
    }
    else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0)
    {
        ESP_LOGI(MAIN_TAG, "Wakeup from PIR trigger");
    }

    setup_all_cam_power_down_pins();

    nvs_handle_t my_handle;
    uint32_t next_capture_count;
    nvs_flash_init();
    if (nvs_open("storage", NVS_READWRITE, &my_handle) != ESP_OK)
    {
        // Guess a value to attempt to write to
        next_capture_count = 1000;
    }
    else
    {
        if (nvs_get_u32(my_handle, NVS_CAP_COUNT_KEY, &next_capture_count) != ESP_OK)
        {
            // Guess a value to attempt to write to
            next_capture_count = 1000;
        }
    }

    motion_proc_queue = xQueueCreate(MAX_CONT_CAP, sizeof(jpg_motion_data_t));

    TaskHandle_t motion_task_handle;
    xTaskCreate(motion_processing_task, "Motion processing task", 1024 * 16, NULL, 4, &motion_task_handle);
    vTaskPrioritySet(NULL, 4);

    esp_rom_gpio_pad_select_gpio(PIR_PIN);
    gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);
    gpio_pulldown_dis(PIR_PIN);
    gpio_pullup_en(PIR_PIN);

    size_t cont_capture_count = 0;
    while(cont_capture_count < MAX_CONT_CAP)
    {
        capture_motion_images(next_capture_count++);
        // Wait 5 seconds to see if motion has stopped
        vTaskDelay(pdMS_TO_TICKS(10000));

        if (gpio_get_level(PIR_PIN) != PIR_TRIG_LEVEL)
        {
            // Motion has stopped, wait for motion processing to end
            break;
        }
        cont_capture_count++;
        vTaskResume(motion_proc_queue);
    }

    if (cont_capture_count < MAX_CONT_CAP)
    {
        ESP_LOGI(MAIN_TAG, "Motion gone quiet, waiting for processing to end.");
    }
    else
    {
        ESP_LOGI(MAIN_TAG, "Continous motion limit hit, %i captures made.", cont_capture_count);
    }

    // Save current capture number for later boot
    nvs_set_u32(my_handle, NVS_CAP_COUNT_KEY, next_capture_count);

    while(1)
    {
        if (processing_active == false)
        {
            ESP_LOGI(MAIN_TAG, "Processing finished");
            enter_deep_sleep();
        }

        // Wait 1 second and check if the camera processing task has reached the end of its queue
        ESP_LOGI(MAIN_TAG, "Processing active, waiting");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}