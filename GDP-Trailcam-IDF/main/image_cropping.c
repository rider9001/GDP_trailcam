/// ------------------------------------------
/// @file img_cropping.c
///
/// @brief Source file for all image cropping and prep functions for
/// insertion into the tflite model
/// ------------------------------------------

#include "image_cropping.h"

/// @brief Logging tag
const static char* CROP_TAG = "image_cropping";

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
bool find_motion_centre(grayscale_image_t* motion_img, point_t* outPoint)
{
    quantize_motion_img(motion_img);

    uint32_t x_sum = 0;
    uint32_t y_sum = 0;
    size_t motion_pix_count = 0;

    // Sum the x/y coords of all pixels which meet threshold
    for (size_t i = 0; i < motion_img->len; i++)
    {
        if (motion_img->buf[i] == 0xff)
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

    // Calculate the resulting average x/y point then offset it by half the bounding box
    // and clip to inside the image to create a valid bounding box start point
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

/// ------------------------------------------
void quantize_motion_img(grayscale_image_t* motion_img)
{
    uint32_t pixel_avg = 0;
    for (size_t i = 0; i < motion_img->len; i++)
    {
        pixel_avg += motion_img->buf[i];
    }
    pixel_avg /= motion_img->len;

    for (size_t i = 0; i < motion_img->len; i++)
    {
        if (motion_img->buf[i] >= pixel_avg + MOTION_PIX_THRES_ABV_AVG)
        {
            motion_img->buf[i] = 0xff;
        }
        else
        {
            motion_img->buf[i] = 0;
        }
    }
}

/// ------------------------------------------
void draw_motion_box(grayscale_image_t* motion_img, point_t box_origin)
{
    size_t y = box_origin.y;
    for (size_t x = box_origin.x; x <= box_origin.x + BOUNDING_BOX_EDGE_LEN; x++)
    {
        point_t point;
        point.x = x;
        point.y = y;
        motion_img->buf[map_pixel_to_bufidx(point, motion_img->width, 1)] = 0xff;
    }

    // Bottom edge
    y = box_origin.y + BOUNDING_BOX_EDGE_LEN;
    for (size_t x = box_origin.x; x <= box_origin.x + BOUNDING_BOX_EDGE_LEN; x++)
    {
        point_t point;
        point.x = x;
        point.y = y;
        motion_img->buf[map_pixel_to_bufidx(point, motion_img->width, 1)] = 0xff;
    }

    // Left edge
    size_t x = box_origin.x;
    for (y = box_origin.y; y <= box_origin.y + BOUNDING_BOX_EDGE_LEN; y++)
    {
        point_t point;
        point.x = x;
        point.y = y;
        motion_img->buf[map_pixel_to_bufidx(point, motion_img->width, 1)] = 0xff;
    }

    // Right edge
    x = box_origin.x + BOUNDING_BOX_EDGE_LEN;
    for (y = box_origin.y; y <= box_origin.y + BOUNDING_BOX_EDGE_LEN; y++)
    {
        point_t point;
        point.x = x;
        point.y = y;
        motion_img->buf[map_pixel_to_bufidx(point, motion_img->width, 1)] = 0xff;
    }
}

/// ------------------------------------------
jpg_image_t crop_jpg_img(const jpg_image_t* source_img, point_t crop_origin)
{
    jpg_image_t cropped_jpg;
    cropped_jpg.buf = NULL;

    size_t crop_rgb_len = BOUNDING_BOX_EDGE_LEN * BOUNDING_BOX_EDGE_LEN * 3;
    uint8_t* cropped_rgb = malloc(crop_rgb_len);

    ESP_LOGI(CROP_TAG, "jpg image cropping started");
    if (jpg2rgb888cropped(source_img->buf, source_img->len, cropped_rgb, JPG_SCALE_NONE, crop_origin.x, crop_origin.y, BOUNDING_BOX_EDGE_LEN) == false)
    {
        ESP_LOGE(CROP_TAG, "jpg image cropping failed!");
        free(cropped_rgb);
        return cropped_jpg;
    }

    ESP_LOGI(CROP_TAG, "converting cropped rgb to jpg");
    if (fmt2jpg(cropped_rgb, crop_rgb_len, BOUNDING_BOX_EDGE_LEN, BOUNDING_BOX_EDGE_LEN, PIXFORMAT_RGB888, 1, &cropped_jpg.buf, &cropped_jpg.len) == false)
    {
        ESP_LOGE(CROP_TAG, "rgb->jpg image conversion failed!");
        free(cropped_rgb);
        free(cropped_jpg.buf);
        cropped_jpg.buf = NULL;
        return cropped_jpg;
    }

    free(cropped_rgb);

    cropped_jpg.height = BOUNDING_BOX_EDGE_LEN;
    cropped_jpg.width = BOUNDING_BOX_EDGE_LEN;
    return cropped_jpg;
}