/// ------------------------------------------
/// @file image_types.h
///
/// @brief Source file for image types used in processing and helper functions
/// ------------------------------------------
#pragma once

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

typedef struct
{
    // Buffer of pixel data
    uint8_t* buf;

    // Length of the buffer
    size_t len;

    // Hieght of the image
    size_t height;

    // Width of the image
    size_t width;
} rgb565_image_t;

typedef struct
{
    // Buffer of pixel data
    uint8_t* buf;

    // Length of the buffer
    size_t len;

    // Hieght of the image
    size_t height;

    // Width of the image
    size_t width;
} rgb888_image_t;

/// @brief Struct that contains jpg image buffer data (NOTE: buf must be individually freed)
typedef struct
{
    // Buffer of image data
    uint8_t* buf;

    // Length of the buffer
    size_t len;

    // Hieght of the image
    size_t height;

    // Width of the image
    size_t width;
} jpg_image_t;

/// @brief Struct that contains two jpg image datasets taken a short time apart (NOTE: both img bufs must be individually freed)
typedef struct
{
    // Flag for data validity
    bool data_valid;

    // First image in the sequence
    jpg_image_t img1;

    // Second image in the sequence
    jpg_image_t img2;

    // ms of end of first capture
    size_t t1;

    // ms of end of second capture
    size_t t2;
} jpg_motion_data_t;

/// @brief Struct to gather data and buffer for a grayscale image
typedef struct
{
    // Buffer of image data
    uint8_t* buf;

    // Length of the buffer
    size_t len;

    // Hieght of the image
    size_t height;

    // Width of the image
    size_t width;
} grayscale_image_t;

/// @brief Struct that contains two grayscale image datasets taken a short time apart (NOTE: both img bufs must be individually freed)
typedef struct
{
    // Flag for data validity
    bool data_valid;

    // First image in the sequence
    grayscale_image_t img1;

    // Second image in the sequence
    grayscale_image_t img2;

    // ms of end of first capture
    size_t t1;

    // ms of end of second capture
    size_t t2;
} grayscale_motion_data_t;

/// ------------------------------------------
/// @brief Frees all buffer data in jpg motion data sturct, checks for null
///
/// @param data struct to free
/// data_valid will be set to false
void free_jpg_motion_data(jpg_motion_data_t* data);

/// ------------------------------------------
/// @brief Frees all buffer data in grayscale motion data sturct, checks for null
///
/// @param data struct to free
/// data_valid will be set to false
void free_grayscale_motion_data(grayscale_motion_data_t* data);