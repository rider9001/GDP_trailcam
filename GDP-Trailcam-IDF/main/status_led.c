/// ------------------------------------------
/// @file status_led.c
///
/// @brief Source file for Camera interaction using esp_camera API
/// ------------------------------------------
#include "status_led.h"

static led_strip_handle_t led_strip;

static char* LED_TAG = "Status_LED";

/// ------------------------------------------
void setup_led(void)
{
    ESP_LOGI(LED_TAG, "Setting up status LED");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = 38,
        .max_leds = 1, // at least one LED on board
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    /* Set all LED off to clear all pixels */
    clear_led();
}

/// ------------------------------------------
void set_led_colour(uint8_t R, uint8_t G, uint8_t B)
{
    ESP_LOGI(LED_TAG, "Setting status led colour: %u, %u, %u", R, G, B);
    led_strip_set_pixel(led_strip, 0, R, G, B);
    led_strip_refresh(led_strip);
}

/// ------------------------------------------
void clear_led()
{
    ESP_LOGI(LED_TAG, "Clearing status LED");
    led_strip_clear(led_strip);
}