/// ------------------------------------------
/// @file Camera.c
///
/// @brief Source file for Camera interaction using esp_camera API
/// ------------------------------------------

#include "Camera.h"

/// @brief Debugging string tag
static const char* CAM_TAG = "TrailCamera";

/// ------------------------------------------
camera_config_t get_default_camera_config(const uint32_t power_down_pin)
{
    camera_config_t camera_default_config = {
        .pin_pwdn       = power_down_pin,
        .pin_reset      = CONFIG_PIN_CAM_RESET,
        // If pin < 0 then clock is disabled
        .pin_xclk       = CONFIG_PIN_CAM_XCLK,
        // I2C SDA and SCL
        .pin_sccb_sda   = CONFIG_PIN_CAM_I2C_SDA,
        .pin_sccb_scl   = CONFIG_PIN_CAM_I2C_SCL,

        // Data pins, for some reason breakout labels these D9-2
        .pin_d7         = CONFIG_PIN_CAM_D9,
        .pin_d6         = CONFIG_PIN_CAM_D8,
        .pin_d5         = CONFIG_PIN_CAM_D7,
        .pin_d4         = CONFIG_PIN_CAM_D6,
        .pin_d3         = CONFIG_PIN_CAM_D5,
        .pin_d2         = CONFIG_PIN_CAM_D4,
        .pin_d1         = CONFIG_PIN_CAM_D3,
        .pin_d0         = CONFIG_PIN_CAM_D2,

        // Vertical and horizontal sync, high when a new frame and row of data begins
        .pin_vsync      = CONFIG_PIN_CAM_VSYNC,
        .pin_href       = CONFIG_PIN_CAM_HREF,

        // Pixel clock, used to indicate when new data is available
        .pin_pclk       = CONFIG_PIN_CAM_PCLK,

        // Clock freq used by camera, 8MHz tested to be best, except for images above FHD resolution
        .xclk_freq_hz   = CONFIG_PIN_CAM_XCLK_FREQ * 1000000,

        // LEDC timers used to gen camera clock signal, unused here but should be filled with
        // these enums to prevent compiler complaining
        .ledc_timer     = LEDC_TIMER_0,
        .ledc_channel   = LEDC_CHANNEL_0,

        // Format and framesize settings, not sure if jpeg quality has any effect outside of JPEG mode
        .pixel_format   = PIXFORMAT_JPEG,
        .frame_size     = FRAMESIZE_FHD,
        .jpeg_quality   = 5, // seems to not work below 5

        // Two frame buffers, one for each frame of the motion capture
        .fb_count       = 2,

        .fb_location    = CAMERA_FB_IN_PSRAM,
        /* 'When buffers should be filled' */
        .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
    };

    return camera_default_config;
}

/// ------------------------------------------
esp_err_t cam_POST(const camera_config_t config)
{
    ESP_LOGI(CAM_TAG, "POSTing cam for pin %i", config.pin_pwdn);
    // Start the tested camera
    if (start_camera(config) != ESP_OK)
    {
        ESP_LOGE(CAM_TAG, "Camera startup for pwr_dwn pin %i failed", config.pin_pwdn);
        return ESP_FAIL;
    }

    // Wait 0.05 seconds to allow the camera to exit any anomalous state
    vTaskDelay(pdMS_TO_TICKS(50));

    // Shutdown camera
    ESP_LOGI(CAM_TAG, "Stopping camera");
    if (stop_camera(config) != ESP_OK)
    {
        ESP_LOGE(CAM_TAG, "Failed to stop camera");
        return ESP_FAIL;
    }

    // Wait half a second
    vTaskDelay(pdMS_TO_TICKS(500));

    // Restart camera
    if (start_camera(config) != ESP_OK)
    {
        ESP_LOGE(CAM_TAG, "Camera startup for pwr_dwn pin %i failed", config.pin_pwdn);
        return ESP_FAIL;
    }

    // Setup frame settings
    default_frame_settings(TEMP_GLOBAL_IMAGE_SET);

    // Take an image
    ESP_LOGI(CAM_TAG, "Grabbing frame buffer");
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(CAM_TAG, "Frame buffer could not be acquired");
        stop_camera(config);
        return ESP_FAIL;
    }
    ESP_LOGI(CAM_TAG, "Camera buffer grabbed sucsessfully");
    ESP_LOGI(CAM_TAG, "Image is %u bytes", fb->len);

    // check image buffer is non empty
    if (fb->len == 0)
    {
        stop_camera(config);
        ESP_LOGE(CAM_TAG, "Frame buffer zero length");
        return ESP_FAIL;
    }

    // Return camera buffer
    esp_camera_fb_return(fb);

    ESP_LOGI(CAM_TAG, "Stopping camera");
    if (stop_camera(config) != ESP_OK)
    {
        ESP_LOGE(CAM_TAG, "Failed to stop camera");
        return ESP_FAIL;
    }

    ESP_LOGI(CAM_TAG, "Camera POST for pwr_dwn pin %i sucsess", config.pin_pwdn);
    return ESP_OK;
}

/// ------------------------------------------
esp_err_t POST_all_cams()
{
    size_t cam_list_sz = sizeof(cam_power_down_pins) / sizeof(cam_power_down_pins[0]);
    for (size_t i = 0; i <  cam_list_sz; i++)
    {
        if (!(cam_power_down_pins[i] < 0))
        {
            camera_config_t config = get_default_camera_config(cam_power_down_pins[i]);
            if (cam_POST(config) != ESP_OK)
            {
                return ESP_FAIL;
            }
        }
    }

    return ESP_OK;
}

/// ------------------------------------------
esp_err_t start_camera(const camera_config_t cam_config)
{
    ESP_LOGI(CAM_TAG, "Powering up camera");
    gpio_set_level(cam_config.pin_pwdn, CAM_POWER_ON);

    // Must wait whilst the camera goes through powerup sequence
    vTaskDelay(pdMS_TO_TICKS(CAM_WAKEUP_DELAY_MS));

    ESP_LOGI(CAM_TAG, "Initialising camera");
    esp_err_t err = esp_camera_init(&cam_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(CAM_TAG, "Camera Init Failed, err: %s", esp_err_to_name(err));
        return err;
    }

    esp_camera_return_all();

    ESP_LOGI(CAM_TAG, "Camera Init Success");
    return ESP_OK;
}

/// ------------------------------------------
esp_err_t stop_camera(const camera_config_t cam_config)
{
    ESP_LOGI(CAM_TAG, "De-initialising camera");
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_STATE)
        {
            ESP_LOGE(CAM_TAG, "Camera never initialized");
        }
        else
        {
            ESP_LOGE(CAM_TAG, "Unexpected error: %s", esp_err_to_name(err));
        }
    }
    else
    {
        ESP_LOGI(CAM_TAG, "Camera De-init Success");
    }

    ESP_LOGI(CAM_TAG, "Powering down camera");
    gpio_set_level(cam_config.pin_pwdn, CAM_POWER_OFF);

    return err;
}

/// ------------------------------------------
jpg_image_t extract_camera_buffer(const camera_fb_t* fb)
{
    jpg_image_t img_data;
    img_data.buf = (uint8_t*) malloc(fb->len);
    memcpy(img_data.buf, fb->buf, fb->len);
    img_data.len = fb->len;
    img_data.height = fb->height;
    img_data.width = fb->width;

    return img_data;
}

/// ------------------------------------------
esp_err_t write_fb_to_SD(const char* save_path, const camera_fb_t* fb)
{
    return write_data_SDSPI(save_path, fb->buf, fb->len);
}

/// ------------------------------------------
esp_err_t write_jpg_data_to_SD(const char* path, const jpg_image_t jpg_data)
{
    return write_data_SDSPI(path, jpg_data.buf, jpg_data.len);
}

/// ------------------------------------------
void default_frame_settings(Camera_image_preset_t camera_setting)
{
    sensor_t* s = esp_camera_sensor_get();
    if (s == NULL)
    {
        ESP_LOGE(CAM_TAG, "Failed to grab sensor ptr");
        return;
    }

    if (camera_setting == LOW_LIGHT)
    {
        s->set_brightness(s, 2);     // -2 to 2
        s->set_contrast(s, -2);       // -2 to 2
        s->set_saturation(s, -2);     // -2 to 2
        s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
        s->set_whitebal(s, 1);       // 0 = disable , 1 = ednable
        s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
        s->set_wb_mode(s, 2);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
        s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
        s->set_aec2(s, 1);           // 0 = disable , 1 = enabled
        s->set_ae_level(s, 2);       // -2 to 2
        s->set_aec_value(s, 300);    // 0 to 1200
        s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
        s->set_agc_gain(s, 1);       // 0 to 30
        s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
        s->set_bpc(s, 0);            // 0 = disable , 1 = enable
        s->set_wpc(s, 1);            // 0 = disable , 1 = enable
        s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
        s->set_lenc(s, 1);           // 0 = disable , 1 = enable
        s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
        s->set_vflip(s, 0);          // 0 = disable , 1 = enable
        s->set_dcw(s, 1);            // 0 = disable , 1 = enable
        s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
        s->set_denoise(s, 10);

        ESP_LOGI(CAM_TAG, "Setup imaging for low light");
    }
    else if (camera_setting == DAYLIGHT)
    {
        s->set_brightness(s, 0);     // -2 to 2
        s->set_contrast(s, 0);       // -2 to 2
        s->set_saturation(s, 0);     // -2 to 2
        s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
        s->set_whitebal(s, 1);       // 0 = disable , 1 = ednable
        s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
        s->set_wb_mode(s, 2);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
        s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
        s->set_aec2(s, 1);           // 0 = disable , 1 = enabled
        s->set_ae_level(s, 2);       // -2 to 2
        s->set_aec_value(s, 300);    // 0 to 1200
        s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
        s->set_agc_gain(s, 1);       // 0 to 30
        s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
        s->set_bpc(s, 0);            // 0 = disable , 1 = enable
        s->set_wpc(s, 1);            // 0 = disable , 1 = enable
        s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
        s->set_lenc(s, 1);           // 0 = disable , 1 = enable
        s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
        s->set_vflip(s, 0);          // 0 = disable , 1 = enable
        s->set_dcw(s, 1);            // 0 = disable , 1 = enable
        s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
        s->set_denoise(s, 10);

        ESP_LOGI(CAM_TAG, "Setup imaging for daylight");
    }
    else
    {
        ESP_LOGI(CAM_TAG, "Ivalid imaging preset num, no change made.");
    }
}

/// ------------------------------------------
void setup_all_cam_power_down_pins()
{
    size_t cam_list_sz = sizeof(cam_power_down_pins) / sizeof(cam_power_down_pins[0]);
    for (size_t i = 0; i < cam_list_sz; i++)
    {
        if (cam_power_down_pins[i] < 0)
        {
            ESP_LOGW(CAM_TAG, "Camera %i PWR_DWN is -1, no connection made", i+1);
            continue;
        }

        esp_rom_gpio_pad_select_gpio((uint32_t)cam_power_down_pins[i]);
        gpio_set_direction((uint32_t)cam_power_down_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level((uint32_t)cam_power_down_pins[i], CAM_POWER_OFF);
    }
}

/// ------------------------------------------
jpg_motion_data_t* get_motion_capture(camera_config_t config)
{
    jpg_motion_data_t* motion = malloc(sizeof(jpg_motion_data_t));
    motion->data_valid = false;
    motion->img1.buf = NULL;
    motion->img2.buf = NULL;

    ESP_LOGI(CAM_TAG, "Starting camera");
    if (start_camera(config) != ESP_OK)
    {
        ESP_LOGE(CAM_TAG, "Failed to start Camera");
        return motion;
    }

    default_frame_settings(TEMP_GLOBAL_IMAGE_SET);

    ESP_LOGI(CAM_TAG, "Grabbing frame buffer");
    size_t capture1_milli = esp_log_timestamp();
    camera_fb_t* frame1 = esp_camera_fb_get();
    if (!frame1) {
        ESP_LOGE(CAM_TAG, "Frame buffer could not be acquired");
        stop_camera(config);
        free(motion);
        return NULL;
    }
    ESP_LOGI(CAM_TAG, "Camera buffer grabbed sucsessfully");
    ESP_LOGI(CAM_TAG, "Image is %u bytes", frame1->len);

    ESP_LOGI(CAM_TAG, "Grabbing frame buffer");
    size_t capture2_milli = esp_log_timestamp();
    camera_fb_t* frame2 = esp_camera_fb_get();
    if (!frame2) {
        ESP_LOGE(CAM_TAG, "Frame buffer could not be acquired");
        stop_camera(config);
        free(motion);
        return NULL;
    }
    ESP_LOGI(CAM_TAG, "Camera buffer grabbed sucsessfully");
    ESP_LOGI(CAM_TAG, "Image is %u bytes", frame2->len);

    ESP_LOGI(CAM_TAG, "Frame diff is %ums", capture2_milli - capture1_milli);

    jpg_image_t img1 = extract_camera_buffer(frame1);
    esp_camera_fb_return(frame1);
    jpg_image_t img2 = extract_camera_buffer(frame2);
    esp_camera_fb_return(frame2);

    ESP_LOGI(CAM_TAG, "Stopping camera");
    if (stop_camera(config) != ESP_OK)
    {
        ESP_LOGE(CAM_TAG, "Failed to stop camera");
    }

    motion->img1 = img1;
    motion->img2 = img2;
    motion->t1 = capture1_milli;
    motion->t2 = capture2_milli;

    ESP_LOGI(CAM_TAG, "Motion capture image grab sucsess");
    motion->data_valid = true;
    return motion;
}