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

    gpio_set_pull_mode(mosi, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(miso, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(sclk, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(cs, GPIO_PULLUP_ONLY);

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
    if (connection.card != NULL)
    {
        // All done, unmount partition and disable SPI peripheral
        // (Note that this frees the card ptr)
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, connection.card);
        ESP_LOGI(SDSPI_TAG, "Card unmounted");
    }

    //deinitialize the bus after all devices are removed
    spi_bus_free(connection.host.slot);
}

///--------------------------------------------------------
esp_err_t SDSPI_POST()
{
    ESP_LOGI(SDSPI_TAG, "Starting POST for SDSPI");
    SDSPI_connection_t POST_connection = connect_to_SDSPI(PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);

    if (POST_connection.card == NULL)
    {
        ESP_LOGE(SDSPI_TAG, "Failed to start SDSPI");
        close_SDSPI_connection(POST_connection);
        return ESP_FAIL;
    }

    // Creating test buffer
    const size_t test_buf_sz = 200;
    uint8_t test_buf[test_buf_sz];

    for (size_t i = 0; i < test_buf_sz; i++)
    {
        test_buf[i] = i;
    }

    // Test filename
    const char* test_filename = MOUNT_POINT"/test.bin";

    if (write_data_SDSPI(test_filename, test_buf, test_buf_sz) != ESP_OK)
    {
        close_SDSPI_connection(POST_connection);
        return ESP_FAIL;
    }

    long readback_size = fsize_SDSPI(test_filename);
    if (readback_size != test_buf_sz)
    {
        ESP_LOGE(SDSPI_TAG, "Filesize discrepancy, %u byte file read back as %ld", test_buf_sz, readback_size);
        close_SDSPI_connection(POST_connection);
        return ESP_FAIL;
    }

    uint8_t readback_buf[test_buf_sz];
    if (read_data_SDSPI(test_filename, readback_buf, test_buf_sz) != ESP_OK)
    {
        close_SDSPI_connection(POST_connection);
        return ESP_FAIL;
    }

    if (memcmp(test_buf, readback_buf, test_buf_sz) != 0)
    {
        ESP_LOGE(SDSPI_TAG, "Readback data not identical to written!");
        close_SDSPI_connection(POST_connection);
        return ESP_FAIL;
    }

    if (delete_file_SDSPI(test_filename) != ESP_OK)
    {
        close_SDSPI_connection(POST_connection);
        return ESP_FAIL;
    }

    return ESP_OK;
}

///--------------------------------------------------------
esp_err_t write_data_SDSPI(const char* path, const uint8_t* data, const size_t len)
{
    ESP_LOGI(SDSPI_TAG, "Opening file %s", path);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        ESP_LOGE(SDSPI_TAG, "Failed to open file for writing, errno: %d", errno);
        return ESP_FAIL;
    }

    size_t write_bytes = fwrite(data, sizeof(uint8_t), len, f);
    fclose(f);
    if (write_bytes != 0)
    {
        ESP_LOGI(SDSPI_TAG, "File written, %u bytes", write_bytes);
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(SDSPI_TAG, "Zero bytes written to file, errno: %d", errno);
        return ESP_FAIL;
    }
}

///--------------------------------------------------------
esp_err_t write_text_SDSPI(const char* path, const char* text)
{
    ESP_LOGI(SDSPI_TAG, "Opening file %s", path);
    // Append file if existing, create if not
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        ESP_LOGE(SDSPI_TAG, "Failed to open file for writing, errno: %d", errno);
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
        ESP_LOGE(SDSPI_TAG, "Failed to open file for reading, errno: %d", errno);
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
        ESP_LOGE(SDSPI_TAG, "Failed to open file for reading, errno: %d", errno);
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
long fsize_SDSPI(const char* path)
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

///--------------------------------------------------------
esp_err_t delete_file_SDSPI(const char* path)
{
    ESP_LOGI(SDSPI_TAG, "Deleting file %s", path);

    if (remove(path) == 0)
    {
        ESP_LOGI(SDSPI_TAG, "File deleted");
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(SDSPI_TAG, "Failed to delete file: errno: %d", errno);
        return ESP_FAIL;
    }
}

///--------------------------------------------------------
bool check_dir_SDSPI(const char* path)
{
    ESP_LOGI(SDSPI_TAG, "Checking existence of dir %s", path);
    DIR* dir = opendir(path);
    bool readable = dir != NULL;
    closedir(dir);

    if (readable)
    {
        ESP_LOGI(SDSPI_TAG, "Directory exists and can be opened");
    }
    else
    {
        ESP_LOGW(SDSPI_TAG, "Directory cannot be opened");
    }

    return readable;
}

///--------------------------------------------------------
esp_err_t create_dir_SDSPI(const char* path)
{
    ESP_LOGI(SDSPI_TAG, "Creating directory %s", path);
    errno = 0;
    if (mkdir(path, S_IRWXU) == 0)
    {
        ESP_LOGI(SDSPI_TAG, "Directory created sucsessfully");
        return ESP_OK;
    }
    else
    {
        if(errno == EEXIST)
        {
            ESP_LOGE(SDSPI_TAG, "Directory already exists");
            return ESP_ERR_INVALID_ARG;
        }
        else
        {
            ESP_LOGE(SDSPI_TAG, "Failed to create directory, errno: %i", errno);
            return ESP_FAIL;
        }
    }
}

///--------------------------------------------------------
void get_filenm_in_dir_SDSPI(const char* path, const size_t dir_num, char* name_out)
{
    ESP_LOGI(SDSPI_TAG, "Getting file %u from dir %s", dir_num, path);

    DIR* dir = opendir(path);
    if (dir == NULL)
    {
        ESP_LOGE(SDSPI_TAG, "Failed to open directory, errno: %d", errno);
        name_out = NULL;
        return;
    }

    struct dirent* entry;
    struct dirent* nth_entry = NULL;
    size_t num = 0;
    while((entry = readdir(dir)) != NULL)
    {
        // Filter "." and "..", strcmp returns 0 on equal
        bool not_filelinks = strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0;
        if (num == dir_num && not_filelinks)
        {
            nth_entry = entry;
            break;
        }

        if (not_filelinks)
        {
            num++;
        }
    }

    closedir(dir);
    if (nth_entry == NULL)
    {
        ESP_LOGI(SDSPI_TAG, "File %u not found", dir_num);
        name_out = NULL;
    }
    else
    {
        ESP_LOGI(SDSPI_TAG, "File %u found: %s", dir_num, nth_entry->d_name);
        strcpy(name_out, nth_entry->d_name);
    }
}

///--------------------------------------------------------
void print_dir_content_in_info_SDSPI(const char* path)
{
    ESP_LOGI(SDSPI_TAG, "Reading dir contents %s", path);

    DIR* dir = opendir(path);
    if (dir == NULL)
    {
        ESP_LOGE(SDSPI_TAG, "Failed to open directory, errno: %d", errno);
        return;
    }

    struct dirent* entry;
    size_t count = 0;
    while((entry = readdir(dir)) != NULL)
    {
        ESP_LOGI(SDSPI_TAG, "%u: %s", count++, entry->d_name);
    }

    closedir(dir);
}

///--------------------------------------------------------
bool check_file_SDSPI(const char* path)
{
    ESP_LOGI(SDSPI_TAG, "Checking %s", path);

    FILE* f = fopen(path, "r");
    if (f == NULL)
    {
        ESP_LOGW(SDSPI_TAG, "Failed to open %s, errno: %d", path, errno);
        return false;
    }

    ESP_LOGI(SDSPI_TAG, "Found %s", path);
    fclose(f);
    return true;
}