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

    size_t needed_pixels = MOTION_PIX_REQ_PERCENT * motion_img->width * motion_img->height;

    // Check if number of pixels is sufficient for image significance
    if (motion_pix_count < needed_pixels)
    {
        ESP_LOGI(CROP_TAG, "Image not motion signficant(<%u)", needed_pixels);
        return false;
    }

    // Calculate the resulting average x/y point then offset it by half the bounding box
    // and clip to inside the image to create a valid bounding box start point
    outPoint->x = ((float)x_sum / (float)motion_pix_count) - (BOUNDING_BOX_EDGE_LEN/2);
    // (+1 is due to the x/y coords ending at width/height -1)
    if ( outPoint->x > (motion_img->width - (1 + BOUNDING_BOX_EDGE_LEN)) )
    {
        outPoint->x = motion_img->width - (1 + BOUNDING_BOX_EDGE_LEN);
    }

    outPoint->y = ((float)y_sum / (float)motion_pix_count) - (BOUNDING_BOX_EDGE_LEN/2);
    if ( outPoint->y > (motion_img->height - (1 + BOUNDING_BOX_EDGE_LEN)) )
    {
        outPoint->y = motion_img->height - (1 + BOUNDING_BOX_EDGE_LEN);
    }

    ESP_LOGI(CROP_TAG, "Image motion signficant (>%u)", needed_pixels);
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

    ESP_LOGI(CROP_TAG, "jpg image cropping started");

    // First, convert whole image to rgb565
    uint8_t* source_rgb = malloc(source_img->height * source_img->width * 2);
    if (jpg2rgb565(source_img->buf, source_img->len, source_rgb, JPG_SCALE_NONE) == false)
    {
        ESP_LOGI(CROP_TAG, "JPG to RGB565 conversion failed");
        free(source_rgb);
        return cropped_jpg;
    }


    rgb565_image_t cropped_rgb;
    // setup crop buffer to extract crop into
    cropped_rgb.len = BOUNDING_BOX_EDGE_LEN * BOUNDING_BOX_EDGE_LEN * 2;
    cropped_rgb.width = BOUNDING_BOX_EDGE_LEN;
    cropped_rgb.height = BOUNDING_BOX_EDGE_LEN;
    cropped_rgb.buf = malloc(cropped_rgb.len);

    point_t crop_end;
    crop_end.x = crop_origin.x + BOUNDING_BOX_EDGE_LEN;
    crop_end.y = crop_origin.y + BOUNDING_BOX_EDGE_LEN;
    size_t crop_idx = 0;
    for (size_t y = crop_origin.y; y < crop_end.y; y++)
    {
        for (size_t x = crop_origin.x; x < crop_end.x; x++)
        {
            point_t cur_p;
            cur_p.x = x;
            cur_p.y = y;
            size_t cur_source_idx = map_pixel_to_bufidx(cur_p, source_img->width, 2);

            cropped_rgb.buf[crop_idx] = source_rgb[cur_source_idx];
            cropped_rgb.buf[crop_idx+1] = source_rgb[cur_source_idx+1];

            crop_idx += 2;
        }
    }

    free(source_rgb);

    if (fmt2jpg(cropped_rgb.buf, cropped_rgb.len, cropped_rgb.width, cropped_rgb.height, PIXFORMAT_RGB565, 240, &cropped_jpg.buf, &cropped_jpg.len) == false)
    {
        ESP_LOGI(CROP_TAG, "RGB565 to JPG conversion failed");
        free(cropped_rgb.buf);
        if (cropped_jpg.buf != NULL)
        {
            free(cropped_jpg.buf);
            cropped_jpg.buf = NULL;
        }
        return cropped_jpg;
    }

    cropped_rgb.height = BOUNDING_BOX_EDGE_LEN;
    cropped_rgb.width = BOUNDING_BOX_EDGE_LEN;
    ESP_LOGI(CROP_TAG, "Cropping done");
    return cropped_jpg;
}