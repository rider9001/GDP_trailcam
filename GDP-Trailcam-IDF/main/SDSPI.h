/// ------------------------------------------
/// @file SDSPI.h
///
/// @brief Header file for SD SPI interaction, handles filesystem read/writes and
/// directory scanning.
///
/// @note README: For some reason, the SD SPI seems to refuse any filenames that contain anything other than
/// alphabetical, numierical and '.' characters (needs testing). Lower and uppercase can be used, but all filenames
/// will be written in capitals only. Spaces break filenames
/// ------------------------------------------
#pragma once

#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_mac.h"
#include "esp_log.h"

/// @brief MISO pin definition
#define PIN_NUM_MISO  CONFIG_PIN_SPI_MISO

/// @brief MOSI pin definition
#define PIN_NUM_MOSI  CONFIG_PIN_SPI_MOSI

/// @brief SCLK pin definition
#define PIN_NUM_CLK   CONFIG_PIN_SPI_CLK

/// @brief CS pin definition
#define PIN_NUM_CS    CONFIG_PIN_SPI_CS

/// @brief Mount point for the sdcard, must be prefixed to all filepaths
#define MOUNT_POINT "/sdcard"


/// README: Any static buffer above this value tends to result in a range of errors, use malloc if needed
/// @brief Max length allowed for a filename under unix
#define FILENAME_MAX_SIZE 256

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
void connect_to_SDSPI(const int miso,
                      const int mosi,
                      const int sclk,
                      const int cs,
                      SDSPI_connection_t* connection);

///--------------------------------------------------------
/// @brief Closes the SDSPI connection and nulls the pointer
///
/// @param connection to the SDSPI, card is nulled
void close_SDSPI_connection(SDSPI_connection_t connection);

///--------------------------------------------------------
/// @brief Performs a POST on the SDSPI module
///
/// @note Assumes a working SDSPI connection has already been made
///
/// @return ESP_OK if sucsessful
esp_err_t SDSPI_POST();

///--------------------------------------------------------
/// @brief Writes a binary buffer to given filepath
///
/// @param path to file to write data into, root is '/sdcard'
/// @param data buffer of byte data
/// @param len length of buffer to write
///
/// @return ESP_OK if sucsessful
esp_err_t write_data_SDSPI(const char* path, const void* data, const size_t len);

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
esp_err_t read_data_SDSPI(const char* path, void* out_buf, const size_t len);

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
long fsize_SDSPI(const char* path);

///--------------------------------------------------------
/// @brief Deletes the file at the given path
///
/// @param path of file to delete
///
/// @return ESP_OK if sucsessful
esp_err_t delete_file_SDSPI(const char* path);

///--------------------------------------------------------
/// @brief Checks the existence of the directory in path
///
/// @param path to check
///
/// @return true if dir exists and is readable, false if not
bool check_dir_SDSPI(const char* path);

///--------------------------------------------------------
/// @brief Creates directory at the specified path
///
/// @note Can only create directories one level at a time
///
/// @param path to create new directory at
///
/// @return ESP_OK if sucsessful, ESP_ERR_INVALID_ARG if directory already exists, ESP_FAIL if creation fails
esp_err_t create_dir_SDSPI(const char* path);

///--------------------------------------------------------
/// @brief Deletes the directory at specified path
///
/// @param path to directory to delete
///
/// @return ESP_OK if sucsessful ESP_ERR_INVALID_ARG if directory already exists, ESP_FAIL if creation fails
esp_err_t delete_dir_SDSPI(const char* path);

///--------------------------------------------------------
/// @brief Get the name of the nth file in a directory
/// Returns null if there is no nth file, count starts at 0
///
/// @note "." and ".." are filtered and are not counted when finding nth file
///
/// @param path to the directory
/// @param dir_num the number of the file to find
/// @param name_out output ptr to place the name into, should be sized to FILENAME_MAX_SIZE
void get_filenm_in_dir_SDSPI(const char* path, const size_t dir_num, char* name_out);

///--------------------------------------------------------
/// @brief Check if a file exists
///
/// @param path to the file
///
/// @return does the file exist in this path?
bool check_file_SDSPI(const char* path);

///--------------------------------------------------------
/// @brief Prints the contents of a dir in info logs
///
/// @param path to print contents
void print_dir_content_in_info_SDSPI(const char* path);