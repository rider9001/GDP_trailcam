/// ------------------------------------------
/// @file motion_analysis.c
///
/// @brief Source file for all image motion analysis functions
/// ------------------------------------------

#include "motion_analysis.h"

/// ------------------------------------------
grayscale_image_t convert_jpg_to_grayscale(const jpg_image_t* jpg_image)
{
    grayscale_image_t gray_image;
    gray_image.height = jpg_image->height;
    gray_image.width = jpg_image->width;
    gray_image.len = gray_image.width * gray_image.height;

    ESP_LOGI(MOTION_TAG, "Allocating %u bytes for grayscale, %ux%u", gray_image.len, gray_image.width, gray_image.width);
    gray_image.buf = malloc(gray_image.len);

    ESP_LOGI(MOTION_TAG, "Converting jpg to grayscale");
    if (jpg2grayscale(jpg_image->buf, jpg_image->len, gray_image.buf) == false)
    {
        ESP_LOGE(MOTION_TAG, "Conversion from jpg to grayscale failed!");
        free(gray_image.buf);
        gray_image.buf = NULL;
    }

    return gray_image;
}

/// ------------------------------------------
grayscale_motion_data_t convert_jpg_motion_to_grayscale(const jpg_motion_data_t* jpg_motion)
{
    grayscale_motion_data_t gray_motion;
    gray_motion.t1 = jpg_motion->t1;
    gray_motion.t2 = jpg_motion->t2;

    ESP_LOGI(MOTION_TAG, "Converting img 1");
    grayscale_image_t img1 = convert_jpg_to_grayscale(&jpg_motion->img1);
    if (img1.buf == NULL)
    {
        gray_motion.data_valid = false;
        return gray_motion;
    }

    char * img1_filenm = malloc(sizeof(char) * 64);
    sprintf(img1_filenm, MOUNT_POINT"/gray1.bin");
    if (write_data_SDSPI(img1_filenm, img1.buf, img1.len) != ESP_OK)
    {
        ESP_LOGE(MOTION_TAG, "Writing img 1 failed");
    }
    free(img1_filenm);

    ESP_LOGI(MOTION_TAG, "Converting img 2");
    grayscale_image_t img2 = convert_jpg_to_grayscale(&jpg_motion->img2);
    if (img2.buf == NULL)
    {
        gray_motion.data_valid = false;
        free(img1.buf);
        return gray_motion;
    }

    gray_motion.data_valid = true;
    gray_motion.img1 = img1;
    gray_motion.img2 = img2;
    return gray_motion;
}

/// ------------------------------------------
grayscale_image_t motion_image_subtract(const grayscale_motion_data_t* motion_set)
{
    grayscale_image_t sub_image;
    sub_image.height = motion_set->img1.height;
    sub_image.width = motion_set->img1.width;
    sub_image.len = sub_image.height * sub_image.width;

    sub_image.buf = malloc(sub_image.len);

    for (size_t pix = 0; pix < sub_image.len; pix++)
    {
        sub_image.buf[pix] = abs(motion_set->img1.buf[pix] - motion_set->img2.buf[pix]);
    }

    return sub_image;
}

/// ------------------------------------------
grayscale_image_t perform_motion_analysis(jpg_motion_data_t* motion_set)
{
    grayscale_image_t sub_image;
    sub_image.buf = NULL;

    ESP_LOGI(MOTION_TAG,"Converting motion to grayscale");
    grayscale_motion_data_t gray_motion = convert_jpg_motion_to_grayscale(motion_set);

    ESP_LOGI(MOTION_TAG, "Freeing jpg motion set");
    free_jpg_motion_data(motion_set);

    if (gray_motion.data_valid == false)
    {
        ESP_LOGE(MOTION_TAG, "Grayscale conversion process failed");
        return sub_image;
    }

    ESP_LOGI(MOTION_TAG, "Subtracting images");
    sub_image = motion_image_subtract(&gray_motion);
    free_grayscale_motion_data(&gray_motion);

    ESP_LOGI(MOTION_TAG, "Image subtraction done");
    return sub_image;
}