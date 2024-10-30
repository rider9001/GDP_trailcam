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

/// @brief Stores the needed comms info for a SDSPI reader connection
/// Card should be considered the active indicator,
/// functions will null this to indicate error or to invalidate connections
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
/// @param[out] connection pointer to be filled with connection info, card is set null
/// if connection fails
///
/// @return connection structure, card will be null if connection failed
SDSPI_connection_t connect_to_SDSPI(const int miso,
                                    const int mosi,
                                    const int sclk,
                                    const int cs);

///--------------------------------------------------------
/// @brief Closes the SDSPI connection and nulls the pointer
///
/// @param connection to the SDSPI, card is nulled
void close_SDSPI_connection(SDSPI_connection_t connection);


///--------------------------------------------------------
/// @brief Writes a binary buffer to given filepath
///
/// @param path to file to write data into, root is '/sdcard'
/// @param data buffer of byte data
/// @param len length of buffer to write
///
/// @return ESP_OK if sucsessful
esp_err_t write_data_SDSPI(const char* path, const uint8_t* data, const size_t len);

///--------------------------------------------------------
/// @brief Writes a string to the given filepath, if file exists then text will be
/// appended.
///
/// @param path to file to write data into, root is '/sdcard'
/// @param text string to write
///
/// @return ESP_OK if sucsessful
esp_err_t write_text_SDSPI(const char* path, const char* text);

///--------------------------------------------------------
/// @brief Reads a binary file into a buffer, outbuf is presumed
/// to be pre-sized to at least len.
///
/// @param path to file to read data from, root is '/sdcard'
/// @param out_buf buffer to read data into, should be sized to at least len
/// @param len number of bytes to read into buffer
///
/// @return ESP_OK if sucsessful
esp_err_t read_data_SDSPI(const char* path, uint8_t* out_buf, const size_t len);

///--------------------------------------------------------
/// @brief Reads text from file into string buffer, assumes out_text is
/// sized to at least len
///
/// @param path to file to read data from, root is '/sdcard'
/// @param out_text output string to pipe text into, should be at least of size len
/// @param len number of characters to read into the out string
///
/// @return ESP_OK if sucsessful
esp_err_t read_text_SDSPI(const char* path, char* out_text, const size_t len);

///--------------------------------------------------------
/// @brief Returns the size in bytes of the file at the path
///
/// @param path to file to read size from, root is '/sdcard'
///
/// @return size in bytes, read fail will return negative
long fsize(const char* path);