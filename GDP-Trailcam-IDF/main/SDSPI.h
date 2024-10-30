/// ------------------------------------------
/// @file SDSPI.h
///
/// @brief Header file for SD SPI interaction, handles filesystem read/writes and
/// directory scanning.
/// ------------------------------------------
#pragma once

#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_mac.h"

#define PIN_NUM_MISO  CONFIG_EXAMPLE_PIN_MISO
#define PIN_NUM_MOSI  CONFIG_EXAMPLE_PIN_MOSI
#define PIN_NUM_CLK   CONFIG_EXAMPLE_PIN_CLK
#define PIN_NUM_CS    CONFIG_EXAMPLE_PIN_CS

#define MOUNT_POINT "/sdcard"

static const char *SDSPI_TAG = "SDSPI";

/// @brief Stores the needed comms info for a SDSPI reader connection
typedef struct {
    // pointer to SD/MMC card information structure
    sdmmc_card_t* card;

    // SD/MMC Host description
    sdmmc_host_t host;
} SDSPI_connection_t;

///--------------------------------------------------------
/// @brief Creates a new connection to the SD SPI card module using
/// the given GPIO pins. Once complete the read/write functions should
/// be operational.
///
/// @param miso pin no
/// @param mosi pin no
/// @param sclk pin no
/// @param cs pin no
///
/// @returns struct with connection info, null if connection failed
SDSPI_connection_t* connect_to_SDSPI(const int miso,
                                    const int mosi,
                                    const int sclk,
                                    const int cs);

///--------------------------------------------------------
/// @brief Closes the SDSPI connection and nulls the pointer
///
/// @param connection to the SDSPI, will be nulled
void close_SDSPI_connection(SDSPI_connection_t* connection);


///--------------------------------------------------------
/// @brief Writes a binary buffer to given filepath
///
/// @param path to file to write data into, root is '/'
/// @param data buffer of byte data
/// @param len length of buffer to write
///
/// @return ESP_OK if sucsessful
esp_err_t write_data_SDSPI(const char* path, const uint8_t* data, const size_t len);

///--------------------------------------------------------
/// @brief Reads a binary file into a buffer, outbuf is presumed
/// to be pre-sized to at least len.
///
/// @param path to file to read data from, root is '/'
/// @param out_buf buffer to read data into, should be sized to at least len
/// @param len number of bytes to read into buffer
///
/// @return ESP_OK if sucsessful
esp_err_t read_data_SDSPI(const char* path, uint8_t* out_buf, const size_t len);