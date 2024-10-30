#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sd_test_io.h"
#if SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

#include "SDSPI.h"

static const char *MAIN_TAG = "main";

SDSPI_connection_t connection;

void app_main(void)
{
    long size;
    ESP_LOGI(MAIN_TAG, "Starting SDSPI comms.");

    connection = connect_to_SDSPI(PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
    if (connection.card == NULL)
    {
        ESP_LOGE(MAIN_TAG, "Failed to start SDSPI");
        return;
    }

    const char* text = "Hello123456!\n";
    const char* line2 = "Line2";
    const char* text_file = MOUNT_POINT"/log1234.txt";

    if (write_text_SDSPI(text_file, text) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to write text to SDSPI");
    }

    if (write_text_SDSPI(text_file, line2) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to write text to SDSPI");
    }

    char text_out[100];

    if (read_text_SDSPI(text_file, text_out, strlen(text)) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to read text from SDSPI");
    }
    else
    {
        ESP_LOGI(MAIN_TAG, "Read string is: %s", text_out);
    }

    size = fsize(text_file);
    if (size == -1)
    {
        ESP_LOGE(MAIN_TAG, "Failed to read text SDSPI size");
    }
    else
    {
        ESP_LOGI(MAIN_TAG, "Text size: %ld bytes", size);
    }

    const uint8_t data_write[] = {9,8,7,6,5,4,3,2,1};
    const char* file = MOUNT_POINT"/hello.bin";

    if (write_data_SDSPI(file, data_write, sizeof(data_write)) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to write data to SDSPI");
        return;
    }

    uint8_t data_read[sizeof(data_write)];
    if (read_data_SDSPI(file, data_read, sizeof(data_write)) != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "Failed to read data from SDSPI");
        return;
    }

    ESP_LOGI(MAIN_TAG, "Data read sucsess");

    bool match = true;
    for (size_t i = 0; i < sizeof(data_write); i++)
    {
        if (data_read[i] != data_write[i])
        {
            match = false;
            break;
        }
    }

    if (match)
    {
        ESP_LOGI(MAIN_TAG, "Data matches written");
    }
    else
    {
        ESP_LOGE(MAIN_TAG, "Data mismatch!");
    }

    size = fsize(file);
    if (size == -1)
    {
        ESP_LOGE(MAIN_TAG, "Failed to read SDSPI size");
    }
    else
    {
        ESP_LOGI(MAIN_TAG, "Data size: %ld bytes", size);
    }

    close_SDSPI_connection(connection);
}