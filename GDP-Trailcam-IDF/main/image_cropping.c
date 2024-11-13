/// ------------------------------------------
/// @file img_cropping.c
///
/// @brief Source file for all image cropping and prep functions for
/// insertion into the tflite model
/// ------------------------------------------

#include "image_cropping.h"

/// ------------------------------------------
point_t map_bufidx_to_pixel(size_t bufidx, size_t img_width, size_t bytes_per_pixel)
{
    point_t mapped_point;
    size_t bytes_per_line = img_width * bytes_per_pixel;

    mapped_point.x = (bufidx % bytes_per_line) / bytes_per_pixel;
    mapped_point.y = bufidx / bytes_per_line;

    return mapped_point;
}

/// ------------------------------------------
size_t map_pixel_to_bufidx(point_t pixel, size_t img_width, size_t bytes_per_pixel)
{
    size_t bytes_per_line = img_width * bytes_per_pixel;
    return (bytes_per_line * pixel.y) + (bytes_per_pixel * pixel.x);
}

/// ------------------------------------------
bool find_motion_centre(const grayscale_image_t* motion_img, point_t* outPoint)
{
    uint32_t pixel_avg = 0;
    for (size_t i = 0; i < motion_img->len; i++)
    {
        pixel_avg += motion_img->buf[i];
    }
    pixel_avg /= motion_img->len;

    uint32_t x_sum = 0;
    uint32_t y_sum = 0;
    size_t motion_pix_count = 0;

    // Sum the x/y coords of all pixels which meet threshold
    for (size_t i = 0; i < motion_img->len; i++)
    {
        if (motion_img->buf[i] >= pixel_avg + MOTION_PIX_THRES_ABV_AVG)
        {
            point_t point = map_bufidx_to_pixel(i, motion_img->width, 1);
            x_sum += point.x;
            y_sum += point.y;
            motion_pix_count++;
        }
    }

    ESP_LOGI(CROP_TAG, "Image has %u motion pixels", motion_pix_count);

    // Check if number of pixels is sufficient for image significance
    if (motion_pix_count < MOTION_PIX_REQ)
    {
        ESP_LOGI(CROP_TAG, "Image not motion signficant");
        return false;
    }

    // Calculate the resulting average x/y point then offset it by half the bounding box with
    // and clip to inside the image and at a valid bounding box start
    outPoint->x = (x_sum / motion_pix_count) - (BOUNDING_BOX_EDGE_LEN/2);
    // (-1 is due to the x/y coords starting at 0,0)
    if ( outPoint->x > (motion_img->width - (1 + BOUNDING_BOX_EDGE_LEN)) )
    {
        outPoint->x = motion_img->width - (1 + BOUNDING_BOX_EDGE_LEN);
    }

    outPoint->y = y_sum / motion_pix_count - (BOUNDING_BOX_EDGE_LEN/2);
    if ( outPoint->y > (motion_img->height - (1 + BOUNDING_BOX_EDGE_LEN)) )
    {
        outPoint->y = motion_img->height - (1 + BOUNDING_BOX_EDGE_LEN);
    }

    ESP_LOGI(CROP_TAG, "Image motion signficant");
    return true;
}