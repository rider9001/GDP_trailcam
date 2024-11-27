/// ------------------------------------------
/// @file main.c
///
/// @brief Main source of the GDP trailcam project
/// ------------------------------------------

extern "C" {
    #include "freertos/FreeRTOS.h"
    #include "freertos/timers.h"
    #include "freertos/task.h"
    #include "freertos/queue.h"
    #include "driver/gpio.h"

    #include "SDSPI.h"
    #include "Camera.h"
    #include "motion_analysis.h"
    #include "image_cropping.h"
}

#include "image_classifier.h"

uint8_t num = best_float16_tflite[4];

static const char* MAIN_TAG = "main";

SDSPI_connection_t connection;
camera_config_t config;

#define PIR_PIN (gpio_num_t)11

QueueHandle_t cam_queue;
QueueHandle_t motion_proc_queue;

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
    ESP_LOGI(MAIN_TAG, "Starting PIR state latch task");
    while(1)
    {
        // Wait here for ISR to unpause
        vTaskSuspend(NULL);
        ESP_LOGI(MAIN_TAG, "PIR active, running camera");

        // when unpaused that means that the PIR has triggered, run camera task
        xQueueSend(cam_queue, &pin, portMAX_DELAY);

        // Wait this task for a few seconds whilst camera task runs to prevent
        // runnning pushing another image capture for the same incident
        vTaskDelay(pdMS_TO_TICKS(2000));

        PIR_active = 1;
        while (PIR_active)
        {
            // Test if ISR detects PIR being noisy over 500ms
            PIR_active = 0;
            // Connect watchdog ISR
            gpio_isr_handler_add(PIR_PIN, (gpio_isr_t)PIR_hold_state_ISR, 0);
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
        gpio_isr_handler_add(PIR_PIN, (gpio_isr_t)PIR_watchdog_ISR, NULL);

        // Loop should now reset and pause, waiting for next watchdog unpause
    }
}

void motion_processing_task()
{
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

            size_t motion_t1 = jpg_motion_data.t1;

            ESP_LOGI(MAIN_TAG, "Analysing motion on caputre");
            grayscale_image_t sub_img = perform_motion_analysis(&jpg_motion_data);

            if (sub_img.buf != NULL)
            {
                ESP_LOGI(MAIN_TAG, "Writing image subtraction");
                char* sub_filenm = (char*)malloc(32);
                sprintf(sub_filenm, MOUNT_POINT"/%u/sub.bin", motion_t1);

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
                    char* box_filenm = (char*)malloc(32);
                    sprintf(box_filenm, MOUNT_POINT"/%u/box.bin", motion_t1);
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
                        char* box_img_filenm = (char*)malloc(64);
                        sprintf(box_img_filenm, MOUNT_POINT"/%u/box.jpg", jpg_motion_data.t1);

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
    }
}

extern "C" void app_main(void)
{
    // Most SDSPI functions assume that there exists a working connection already
    connect_to_SDSPI(PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS, &connection);
    if (connection.card == NULL)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start SDSPI");
        esp_restart();
        return;
    }

    ESP_LOGI(MAIN_TAG, "Running SD SPI POST...");
    if (SDSPI_POST() != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "POST failed on a SD SPI");
        esp_restart();
        return;
    }
    ESP_LOGI(MAIN_TAG, "SD SPI POST sucsess");


    ESP_LOGI(MAIN_TAG, "Installing ISR service");
    gpio_install_isr_service(0);

    // This should always be run first, sets all power down pins as outputs and shuts down all cameras
    ESP_LOGI(MAIN_TAG, "Setting up cam power pins");
    setup_all_cam_power_down_pins();

    ESP_LOGI(MAIN_TAG, "POSTing all cameras");
    if (POST_all_cams() != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "POST failed on a camera");
        esp_restart();
    }
    ESP_LOGI(MAIN_TAG, "Camera POST sucsess");

    cam_queue = xQueueCreate(5, sizeof(int));
    motion_proc_queue = xQueueCreate(5, sizeof(jpg_motion_data_t));

    xTaskCreate((TaskFunction_t)PIR_state_latch, "PIR_state_latch", 1024 * 8, NULL, 3, &PIR_trig_handle);
    xTaskCreate((TaskFunction_t)motion_processing_task, "Motion processing task", 1024 * 16, &motion_proc_queue, 3, NULL);
    vTaskPrioritySet(NULL, 4);

    esp_rom_gpio_pad_select_gpio(PIR_PIN);
    gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);
    gpio_pulldown_dis(PIR_PIN);
    gpio_pullup_en(PIR_PIN);
    gpio_set_intr_type(PIR_PIN, GPIO_INTR_LOW_LEVEL);

    ESP_LOGI(MAIN_TAG, "Waiting for low");

    // Only engage ISR once all cams tested
    gpio_isr_handler_add(PIR_PIN, (gpio_isr_t)PIR_watchdog_ISR, NULL);

    while(1)
    {
        int cam_pwr_pin = -1;
        if (xQueueReceive(cam_queue, &cam_pwr_pin, portMAX_DELAY))
        {
            if (cam_pwr_pin == -1)
            {
                ESP_LOGW(MAIN_TAG, "Wait timeout, continuing");
                continue;
            }

            ESP_LOGI(MAIN_TAG, "Starting capture on cam_pwr_pin: %i", cam_pwr_pin);

            config = get_default_camera_config(cam_pwr_pin);

            jpg_motion_data_t* motion = get_motion_capture(config);

            ESP_LOGI(MAIN_TAG, "Time between is: %ums", motion->t2 - motion->t1);

            char dir[32];
            sprintf(dir, MOUNT_POINT"/%u", motion->t1);

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

                char* info_text = (char*)malloc(300);
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
            xQueueSend(motion_proc_queue, motion, portMAX_DELAY);
        }
    }
}