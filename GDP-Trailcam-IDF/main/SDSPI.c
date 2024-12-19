/// ------------------------------------------
/// @file SDSPI.c
///
/// @brief Source file for SD SPI interaction
/// ------------------------------------------

#include "SDSPI.h"

/// @brief Semaphore that holds a mutex, controlling read/write acsess to the SD card
SemaphoreHandle_t SD_SPI_Mutex;

/// @brief Debugging string tag
static const char *SDSPI_TAG = "SDSPI";

///--------------------------------------------------------
void connect_to_SDSPI(const int miso,
                      const int mosi,
                      const int sclk,
                      const int cs,
                      SDSPI_connection_t* connection)
{
    // Dont ask me what half this stuff does, im just following the example - E

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
        connection->card = NULL;
        return;
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
        connection->card = NULL;
        return;
    }

    ESP_LOGI(SDSPI_TAG, "Filesystem mounted");

    // Connection sucsessful, set connection data
    connection->host = host;
    connection->card = card;

    // setup SD acsess mutex
    SD_SPI_Mutex = xSemaphoreCreateMutex();
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

    // Creating test buffer
    const size_t test_buf_sz = 200;
    uint8_t test_buf[test_buf_sz];

    for (size_t i = 0; i < test_buf_sz; i++)
    {
        test_buf[i] = i;
    }

    // Test filename
    const char* test_filename = MOUNT_POINT"/testdir/test.bin";

    const char* test_dir = MOUNT_POINT"/testdir";

    esp_err_t err;
    err = create_dir_SDSPI(test_dir);
    if (err != ESP_OK)
    {
        if (err != ESP_ERR_INVALID_ARG)
        {
            return ESP_FAIL;
        }
    }

    if (check_file_SDSPI(test_filename) == true)
    {
        if (delete_file_SDSPI(test_filename) != ESP_OK)
        {
            return ESP_FAIL;
        }
    }

    if (write_data_SDSPI(test_filename, test_buf, test_buf_sz) != ESP_OK)
    {
        return ESP_FAIL;
    }

    long readback_size = fsize_SDSPI(test_filename);
    if (readback_size != test_buf_sz)
    {
        ESP_LOGE(SDSPI_TAG, "Filesize discrepancy, %u byte file read back as %ld", test_buf_sz, readback_size);
        return ESP_FAIL;
    }

    uint8_t readback_buf[test_buf_sz];
    if (read_data_SDSPI(test_filename, readback_buf, test_buf_sz) != ESP_OK)
    {
        return ESP_FAIL;
    }

    if (memcmp(test_buf, readback_buf, test_buf_sz) != 0)
    {
        ESP_LOGE(SDSPI_TAG, "Readback data not identical to written!");
        return ESP_FAIL;
    }

    if (delete_file_SDSPI(test_filename) != ESP_OK)
    {
        return ESP_FAIL;
    }

    if (delete_dir_SDSPI(test_dir) != ESP_OK)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}

///--------------------------------------------------------
esp_err_t write_data_SDSPI(const char* path, const void* data, const size_t len)
{
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
        return ESP_FAIL;
    }

    ESP_LOGI(SDSPI_TAG, "Opening file %s", path);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        xSemaphoreGive(SD_SPI_Mutex);
        ESP_LOGE(SDSPI_TAG, "Failed to open file for writing, errno: %d", errno);
        return ESP_FAIL;
    }

    size_t write_bytes = fwrite(data, 1, len, f);
    fclose(f);

    xSemaphoreGive(SD_SPI_Mutex);

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
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
        return ESP_FAIL;
    }

    ESP_LOGI(SDSPI_TAG, "Opening file %s", path);
    // Append file if existing, create if not
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        xSemaphoreGive(SD_SPI_Mutex);
        ESP_LOGE(SDSPI_TAG, "Failed to open file for writing, errno: %d", errno);
        return ESP_FAIL;
    }

    ESP_LOGI(SDSPI_TAG, "String is %u chars", strlen(text));
    size_t write_chars = fwrite(text, sizeof(char), strlen(text), f);
    fclose(f);

    xSemaphoreGive(SD_SPI_Mutex);

    if (write_chars != 0)
    {
        ESP_LOGI(SDSPI_TAG, "File written, %u chars", write_chars);
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(SDSPI_TAG, "Zero chars written to file, errno: %d", errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

///--------------------------------------------------------
esp_err_t read_data_SDSPI(const char* path, void* out_buf, const size_t len)
{
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
        return ESP_FAIL;
    }

    ESP_LOGI(SDSPI_TAG, "Opening file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        xSemaphoreGive(SD_SPI_Mutex);
        ESP_LOGE(SDSPI_TAG, "Failed to open file for reading, errno: %d", errno);
        return ESP_FAIL;
    }

    size_t read_bytes = fread(out_buf, 1, len, f);
    fclose(f);

    xSemaphoreGive(SD_SPI_Mutex);

    ESP_LOGI(SDSPI_TAG, "File read, %u bytes", read_bytes);
    return ESP_OK;
}

///--------------------------------------------------------
esp_err_t read_text_SDSPI(const char* path, char* out_text, const size_t len)
{
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
        return ESP_FAIL;
    }

    ESP_LOGI(SDSPI_TAG, "Opening file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        xSemaphoreGive(SD_SPI_Mutex);
        ESP_LOGE(SDSPI_TAG, "Failed to open file for reading, errno: %d", errno);
        return ESP_FAIL;
    }

    size_t read_chars = fread(out_text, sizeof(char), len, f);
    if (read_chars != len)
    {
        ESP_LOGW(SDSPI_TAG, "Read %u chars not %u", read_chars, len);
    }

    fclose(f);

    xSemaphoreGive(SD_SPI_Mutex);

    ESP_LOGI(SDSPI_TAG, "File read, %u chars", len);
    return ESP_OK;
}

///--------------------------------------------------------
long fsize_SDSPI(const char* path)
{
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
        return -1;
    }

    struct stat sb;

    int ret = stat(path, &sb);
    xSemaphoreGive(SD_SPI_Mutex);

    if (ret == 0) {
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
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
        return ESP_FAIL;
    }

    ESP_LOGI(SDSPI_TAG, "Deleting file %s", path);

    int ret = remove(path);
    xSemaphoreGive(SD_SPI_Mutex);

    if (ret == 0)
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
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
        return ESP_FAIL;
    }

    ESP_LOGI(SDSPI_TAG, "Checking existence of dir %s", path);
    DIR* dir = opendir(path);
    bool readable = dir != NULL;
    closedir(dir);

    xSemaphoreGive(SD_SPI_Mutex);

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
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
        return ESP_FAIL;
    }


    ESP_LOGI(SDSPI_TAG, "Creating directory %s", path);
    errno = 0;
    int ret = mkdir(path, S_IRWXU);
    xSemaphoreGive(SD_SPI_Mutex);

    if (ret == 0)
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
esp_err_t delete_dir_SDSPI(const char* path)
{
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
        return ESP_FAIL;
    }

    ESP_LOGI(SDSPI_TAG, "Deleting directory %s", path);
    int ret = rmdir(path);
    xSemaphoreGive(SD_SPI_Mutex);

    if (ret != 0)
    {
        if (errno == ENOTDIR)
        {
            ESP_LOGW(SDSPI_TAG, "Directory does not exist");
            return ESP_ERR_INVALID_ARG;
        }
        else
        {
            ESP_LOGE(SDSPI_TAG, "Directory deletion failed");
            return ESP_FAIL;
        }
    }

    ESP_LOGI(SDSPI_TAG, "Directory deleted sucsessfully");
    return ESP_OK;
}

///--------------------------------------------------------
void get_filenm_in_dir_SDSPI(const char* path, const size_t dir_num, char* name_out)
{
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
    }

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
    xSemaphoreGive(SD_SPI_Mutex);

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
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
    }

    ESP_LOGI(SDSPI_TAG, "Reading dir contents %s", path);

    DIR* dir = opendir(path);
    if (dir == NULL)
    {
        xSemaphoreGive(SD_SPI_Mutex);
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
    xSemaphoreGive(SD_SPI_Mutex);
}

///--------------------------------------------------------
bool check_file_SDSPI(const char* path)
{
    if (!xSemaphoreTake(SD_SPI_Mutex, pdMS_TO_TICKS(MAX_SD_WAIT_MS)))
    {
        ESP_LOGE(SDSPI_TAG, "Unable to grab SD mutex!");
        return ESP_FAIL;
    }

    ESP_LOGI(SDSPI_TAG, "Checking %s", path);

    FILE* f = fopen(path, "r");
    if (f == NULL)
    {
        xSemaphoreGive(SD_SPI_Mutex);
        ESP_LOGW(SDSPI_TAG, "Failed to open %s, errno: %d", path, errno);
        return false;
    }


    fclose(f);
    xSemaphoreGive(SD_SPI_Mutex);

    ESP_LOGI(SDSPI_TAG, "Found %s", path);
    return true;
}

///--------------------------------------------------------
size_t get_next_capture_num()
{
    char capture_dir[16];

    bool dir_exists = true;
    size_t num = 1;
    while(dir_exists)
    {
        sprintf(capture_dir, CAPTURE_DIR_PREFIX"%u", num);
        dir_exists = check_dir_SDSPI(capture_dir);
    }

    return num;
}