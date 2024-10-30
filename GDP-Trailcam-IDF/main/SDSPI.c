/// ------------------------------------------
/// @file SDSPI.c
///
/// @brief Source file for SD SPI interaction
/// ------------------------------------------

#include "SDSPI.h"

static const char *SDSPI_TAG = "SDSPI";

///--------------------------------------------------------
SDSPI_connection_t connect_to_SDSPI(const int miso,
                                    const int mosi,
                                    const int sclk,
                                    const int cs)
{
    // Dont ask me what half this stuff does, im just following the example - E

    // Set card to null, this will be overwritten if the connection succeeds
    SDSPI_connection_t new_connection;
    new_connection.card = NULL;

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(SDSPI_TAG, "Initializing SD card");

    ESP_LOGI(SDSPI_TAG, "Using SPI peripheral");
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(SDSPI_TAG, "Failed to initialize bus.");
        return new_connection;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = cs;
    slot_config.host_id = host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(SDSPI_TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        ESP_LOGE(SDSPI_TAG, "Failed to mount filesystem.");
        return new_connection;
    }

    ESP_LOGI(SDSPI_TAG, "Filesystem mounted");

    // Connection sucsessful, set connection data
    new_connection.host = host;
    new_connection.card = card;

    return new_connection;
}

///--------------------------------------------------------
void close_SDSPI_connection(SDSPI_connection_t connection)
{
    if (connection.card == NULL)
    {
        // preventing free null error
        return;
    }

    // All done, unmount partition and disable SPI peripheral
    // (Note that this frees the card ptr)
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, connection.card);
    ESP_LOGI(SDSPI_TAG, "Card unmounted");

    //deinitialize the bus after all devices are removed
    spi_bus_free(connection.host.slot);

    connection.card = NULL;
}

///--------------------------------------------------------
esp_err_t write_data_SDSPI(const char* path, const uint8_t* data, const size_t len)
{
    ESP_LOGI(SDSPI_TAG, "Opening file %s", path);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        ESP_LOGE(SDSPI_TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    size_t write_bytes = fwrite(data, sizeof(uint8_t), len, f);
    fclose(f);
    ESP_LOGI(SDSPI_TAG, "File written, %u bytes", write_bytes);

    return ESP_OK;
}

///--------------------------------------------------------
esp_err_t write_text_SDSPI(const char* path, const char* text)
{
    ESP_LOGI(SDSPI_TAG, "Opening file %s", path);
    // Append file if existing, create if not
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        ESP_LOGE(SDSPI_TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    size_t write_chars = fwrite(text, sizeof(char), strlen(text), f);
    fclose(f);
    ESP_LOGI(SDSPI_TAG, "File written, %u chars", write_chars - 1);

    return ESP_OK;
}

///--------------------------------------------------------
esp_err_t read_data_SDSPI(const char* path, uint8_t* out_buf, const size_t len)
{
    ESP_LOGI(SDSPI_TAG, "Opening file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(SDSPI_TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    size_t read_bytes = fread(out_buf, sizeof(uint8_t), len, f);
    fclose(f);
    ESP_LOGI(SDSPI_TAG, "File read, %u bytes", read_bytes);

    return ESP_OK;
}

///--------------------------------------------------------
esp_err_t read_text_SDSPI(const char* path, char* out_text, const size_t len)
{
    ESP_LOGI(SDSPI_TAG, "Opening file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(SDSPI_TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    size_t read_chars = fread(out_text, sizeof(char), len, f);
    if (read_chars != len)
    {
        ESP_LOGW(SDSPI_TAG, "Read %u chars not %u", read_chars, len);
    }

    fclose(f);
    ESP_LOGI(SDSPI_TAG, "File read, %u chars", len);
    return ESP_OK;
}

///--------------------------------------------------------
long fsize(const char* path)
{
    struct stat sb;

    if (stat(path, &sb) == 0) {
        ESP_LOGI(SDSPI_TAG, "Size of %s is %ld bytes", path, sb.st_size);
        return sb.st_size;
    } else {
        ESP_LOGE(SDSPI_TAG, "Failed to stat %s", path);
        return -1;
    }
}