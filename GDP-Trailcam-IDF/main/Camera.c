/// ------------------------------------------
/// @file Camera.c
///
/// @brief Source file for Camera interaction using esp_camera API
/// ------------------------------------------

#include "Camera.h"

static const char *CAM_TAG = "Camera";

/// ------------------------------------------
camera_config_t get_default_camera_config()
{
    camera_config_t camera_default_config = {
        .pin_pwdn       = CONFIG_PIN_CAM_PWRDN,
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
        .frame_size     = FRAMESIZE_QSXGA,
        .jpeg_quality   = 5,

        .fb_count       = 1,

        .fb_location    = CAMERA_FB_IN_PSRAM,
        /* 'When buffers should be filled' <- Not sure what this means */
        .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
    };

    return camera_default_config;
}

/// ------------------------------------------
esp_err_t start_camera(const camera_config_t cam_config)
{
    ESP_LOGI(CAM_TAG, "Initialising camera");
    esp_err_t err = esp_camera_init(&cam_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(CAM_TAG, "Camera Init Failed, err: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(CAM_TAG, "Camera Init Success");
    return ESP_OK;
}

/// ------------------------------------------
esp_err_t stop_camera()
{
    ESP_LOGI(CAM_TAG, "De-initialising camera");
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_STATE)
        {
            ESP_LOGE(CAM_TAG, "Camera never initialized");
            return ESP_ERR_INVALID_STATE;
        }
        else
        {
            ESP_LOGE(CAM_TAG, "Unexpected error: %s", esp_err_to_name(err));
            return err;
        }
    }
    else
    {
        ESP_LOGI(CAM_TAG, "Camera De-init Success");
        return ESP_OK;
    }
}

/// ------------------------------------------
esp_err_t write_fb_to_SD(const camera_fb_t* fb, const char* save_path)
{
    return write_data_SDSPI(save_path, fb->buf, fb->len);
}

/// ------------------------------------------
void default_camera_settings()
{
    sensor_t* s = esp_camera_sensor_get();
    if (s == NULL)
    {
        ESP_LOGE(CAM_TAG, "Failed to grab sensor ptr");
        return;
    }

    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
    s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
    s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
    s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
    s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
    s->set_aec2(s, 0);           // 0 = disable , 1 = enable
    s->set_ae_level(s, 0);       // -2 to 2
    s->set_aec_value(s, 300);    // 0 to 1200
    s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
    s->set_agc_gain(s, 0);       // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
    s->set_bpc(s, 0);            // 0 = disable , 1 = enable
    s->set_wpc(s, 1);            // 0 = disable , 1 = enable
    s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
    s->set_lenc(s, 1);           // 0 = disable , 1 = enable
    s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
    s->set_vflip(s, 0);          // 0 = disable , 1 = enable
    s->set_dcw(s, 1);            // 0 = disable , 1 = enable
    s->set_colorbar(s, 0);       // 0 = disable , 1 = enable

    ESP_LOGI(CAM_TAG, "Defaulted camera settings");
}