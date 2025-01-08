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
#include "SDSPI.h"

/// @brief The length in pixels of the created square bounding box
#define BOUNDING_BOX_EDGE_LEN 640

/// @brief Minimum value a pixel must be above the average to be counted as a motion pixel
#define MOTION_PIX_THRES_ABV_AVG 30

/// @brief Percent of an images pixels required to be above motion threshold to count as a relevant image
#define MOTION_PIX_REQ_PERCENT 0.005

/// @brief Struct for storing a point in an image, origin is at top left and coord space runs (0,0) -> (w-1,h-1)
/// where w is image width and h is image height
typedef struct
{
    // Point x coord
    int x;

    // Point y coord
    int y;
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
/// and what the x/y origins of the BOUNDING_BOX_EDGE_LEN x BOUNDING_BOX_EDGE_LEN bounding box should be
///
/// @note The returned coords will be clipped to not lead to a bounding box outside of the coord space
/// of the input image
///
/// @param motion_img input motion image for evaluation
/// @param[out] outPoint origin for the bounding box, invalid if return is false
///
/// @return does this image contain significant motion?
bool find_motion_centre(grayscale_image_t* motion_img, point_t* outPoint);

/// ------------------------------------------
/// @brief Draw the square bounding box onto the grayscale image using white pixels
///
/// @note Draws a box from box_origin -> box_origin + BOUNDING_BOX_EDGE_LEN
///
/// @param motion_img grayscale image to draw bounding box onto
/// @param box_origin point origin of the BOUNDING_BOX_EDGE_LEN square
void draw_motion_box(grayscale_image_t* motion_img, point_t box_origin);

/// ------------------------------------------
/// @brief Quantizes the input image based on wether a given pixel passes motion tests
///
/// @param motion_img input image
void quantize_motion_img(grayscale_image_t* motion_img);

/// ------------------------------------------
/// @brief Extracts a square frame from the source image of size BOUNDING_BOX_EDGE_LEN
/// at the origin crop_origin
///
/// @param source_img source image to extract crop from
/// @param crop_origin the origin of the square to extract
///
/// @return output cropped frame, buf is null if conversion fails
jpg_image_t crop_jpg_img(const jpg_image_t* source_img, point_t crop_origin);