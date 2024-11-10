/// ------------------------------------------
/// @file image_types.c
///
/// @brief Source file for image types used in processing and helper functions
/// ------------------------------------------

#include "image_types.h"

/// ------------------------------------------
void free_jpg_motion_data(jpg_motion_data_t* data)
{
    if (data->img1.buf != NULL)
    {
        free(data->img1.buf);
        data->img1.buf = NULL;
    }

    if (data->img2.buf != NULL)
    {
        free(data->img2.buf);
        data->img2.buf = NULL;
    }

    data->data_valid = false;
}

/// ------------------------------------------
void free_grayscale_motion_data(grayscale_motion_data_t* data)
{
    if (data->img1.buf != NULL)
    {
        free(data->img1.buf);
        data->img1.buf = NULL;
    }

    if (data->img2.buf != NULL)
    {
        free(data->img2.buf);
        data->img2.buf = NULL;
    }

    data->data_valid = false;
}