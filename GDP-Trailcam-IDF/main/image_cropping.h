/// ------------------------------------------
/// @file img_cropping.h
///
/// @brief Header file for all image cropping and prep functions for
/// insertion into the tflite model
/// ------------------------------------------
#pragma once

#include <inttypes.h>
#include <stdlib.h>
#include <math.h>
#include "esp_log.h"
#include "esp_camera.h"

#include "image_types.h"

/// @brief The length in pixels of the created square bounding box
#define BOUNDING_BOX_EDGE_LEN 640

/// @brief Minimum value a pixel must be above the average to be counted as a motion pixel
#define MOTION_PIX_THRES_ABV_AVG 30

/// @brief Number of an images pixels required to be above motion threshold to count as a relevant image
#define MOTION_PIX_REQ 8500

/// @brief Struct for storing a point in an image, assumed origin is (0,0) -> (w-1,h-1)
/// where w is image width and h is image height
typedef struct
{
    // Point x coord
    size_t x;

    // Point y coord
    size_t y;
} point_t;

/// ------------------------------------------
/// @brief Maps a buffer index into a x/y pixel point on the image
///
/// @note No check is made for buffer index being in bounds
///
/// @param bufidx index of the buffer to convert
/// @param img_height the height of the image to map onto
/// @param img_width the width of the image to map onto
/// @param bytes_per_pixel number of bytes per pixel on the image
///
/// @return pixel point that the buffer index maps to
point_t map_bufidx_to_pixel(size_t bufidx, size_t img_width, size_t bytes_per_pixel);

/// ------------------------------------------
/// @brief Maps a pixel x/y point into a buffer index of the image
///
/// @note No check is made for point being in bounds
///
/// @param pixel pixel point to map to a buffer index
/// @param img_height the height of the image to map onto
/// @param img_width the width of the image to map onto
/// @param bytes_per_pixel number of bytes per pixel on the image
///
/// @return buffer index, will refer to the first byte of the pixel
size_t map_pixel_to_bufidx(point_t pixel, size_t img_width, size_t bytes_per_pixel);

/// ------------------------------------------
/// @brief Using the threshold values, determines if a motion image has significant motion
/// and what the x/y origins of a 640x640 bounding box should be
///
/// @note The returned coords will be clipped to not lead to a bounding box outside of the coord space
/// of the input image
///
/// @param motion_img input motion image for evaluation
/// @param[out] outPoint origin for the bounding box, invalid if return is false
///
/// @return does this image contain significant motion?
bool find_motion_centre(const grayscale_image_t* motion_img, point_t* outPoint);