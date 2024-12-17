/// ------------------------------------------
/// @file status_led.h
///
/// @brief Header file for interacting with the ES32-S3 onboard led
/// ------------------------------------------
#pragma once

#include "led_strip.h"
#include "esp_log.h"

/// ------------------------------------------
/// @brief Configures the status LED for startup
void setup_onboard_led();

/// ------------------------------------------
/// @brief Set the led to the desired colour, all input values 0-255
/// @param R Red to set led to
/// @param G Green to set led to
/// @param B Blue to set led to
void set_led_colour(uint8_t R, uint8_t G, uint8_t B);

/// @brief Clears and turns the LED off
void clear_led();