/* SD card and FAT filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// This example uses SDMMC peripheral to communicate with SD card.

#include <string.h>
#include <stdint.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "sd_test_io.h"
#if SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

#define EXAMPLE_MAX_CHAR_SIZE    64
#define EXAMPLE_FILE_TEST_ROUNDS 20

static const char *TAG = "example";

#define MOUNT_POINT "/sdcard"

#ifdef CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS
const char* names[] = {"CLK", "CMD", "D0", "D1", "D2", "D3"};
const int pins[] = {CONFIG_EXAMPLE_PIN_CLK,
                    CONFIG_EXAMPLE_PIN_CMD,
                    CONFIG_EXAMPLE_PIN_D0
                    #ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
                    ,CONFIG_EXAMPLE_PIN_D1,
                    CONFIG_EXAMPLE_PIN_D2,
                    CONFIG_EXAMPLE_PIN_D3
                    #endif
                    };

const int pin_count = sizeof(pins)/sizeof(pins[0]);

#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE
const int adc_channels[] = {CONFIG_EXAMPLE_ADC_PIN_CLK,
                            CONFIG_EXAMPLE_ADC_PIN_CMD,
                            CONFIG_EXAMPLE_ADC_PIN_D0
                            #ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
                            ,CONFIG_EXAMPLE_ADC_PIN_D1,
                            CONFIG_EXAMPLE_ADC_PIN_D2,
                            CONFIG_EXAMPLE_ADC_PIN_D3
                            #endif
                            };
#endif //CONFIG_EXAMPLE_ENABLE_ADC_FEATURE

pin_configuration_t config = {
    .names = names,
    .pins = pins,
#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE
    .adc_channels = adc_channels,
#endif
};
#endif //CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS

static esp_err_t s_example_write_file(const char *path, const char *data)
{
    ESP_LOGI(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    size_t data_len = strlen(data);
    size_t written = fwrite(data, 1, data_len, f);
    int close_ret = fclose(f);
    if (written != data_len || close_ret != 0) {
        ESP_LOGE(TAG, "Failed to write file %s", path);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "File written (%u bytes)", (unsigned)written);

    return ESP_OK;
}

static esp_err_t s_example_read_file(const char *path, const char *expected)
{
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char data[EXAMPLE_MAX_CHAR_SIZE];
    size_t read_len = fread(data, 1, sizeof(data) - 1, f);
    int read_error = ferror(f);
    int close_ret = fclose(f);
    if (read_error || close_ret != 0) {
        ESP_LOGE(TAG, "Failed to read file %s", path);
        return ESP_FAIL;
    }

    data[read_len] = '\0';
    if (strcmp(data, expected) != 0) {
        ESP_LOGE(TAG, "File content mismatch for %s: got '%s', expected '%s'",
                 path, data, expected);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Read and verified: '%s'", data);

    return ESP_OK;
}

static esp_err_t s_example_repeat_file_test(sdmmc_card_t *card)
{
    const char *file_hello = MOUNT_POINT "/hello.txt";
    const char *file_foo = MOUNT_POINT "/foo.txt";
    char data[EXAMPLE_MAX_CHAR_SIZE];
    struct stat st;

    ESP_LOGI(TAG, "Starting SD file read/write test (%d rounds)", EXAMPLE_FILE_TEST_ROUNDS);

    for (int round = 1; round <= EXAMPLE_FILE_TEST_ROUNDS; ++round) {
        snprintf(data, sizeof(data), "SDMMC test round %d (%s)\n", round, card->cid.name);

        // Keep every round independent if a previous run was interrupted.
        if (stat(file_hello, &st) == 0) {
            unlink(file_hello);
        }
        if (stat(file_foo, &st) == 0) {
            unlink(file_foo);
        }

        if (s_example_write_file(file_hello, data) != ESP_OK ||
                s_example_read_file(file_hello, data) != ESP_OK) {
            ESP_LOGE(TAG, "SD file test round %d failed during write/read", round);
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Renaming file %s to %s", file_hello, file_foo);
        if (rename(file_hello, file_foo) != 0) {
            ESP_LOGE(TAG, "SD file test round %d rename failed", round);
            return ESP_FAIL;
        }

        if (s_example_read_file(file_foo, data) != ESP_OK) {
            ESP_LOGE(TAG, "SD file test round %d failed after rename", round);
            return ESP_FAIL;
        }

        if (unlink(file_foo) != 0) {
            ESP_LOGE(TAG, "SD file test round %d delete failed", round);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "SD file test round %d/%d passed", round, EXAMPLE_FILE_TEST_ROUNDS);
    }

    ESP_LOGI(TAG, "SD file read/write test passed: %d/%d rounds",
             EXAMPLE_FILE_TEST_ROUNDS, EXAMPLE_FILE_TEST_ROUNDS);
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card = NULL;
    bool mounted = false;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.

    ESP_LOGI(TAG, "Using SDMMC peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    // host.max_freq_khz = 10000;

    // For SoCs where the SD power can be supplied both via an internal or external (e.g. on-board LDO) power supply.
    // When using specific IO pins (which can be used for ultra high-speed SDMMC) to connect to the SD card
    // and the internal LDO power supply, we need to initialize the power supply first.
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_IO_ID,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // Set bus width to use:
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    slot_config.width = 4;
#else
    slot_config.width = 1;
#endif

    // On chips where the GPIOs used for SD card can be configured, set them in
    // the slot_config structure:
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = CONFIG_EXAMPLE_PIN_CLK;
    slot_config.cmd = CONFIG_EXAMPLE_PIN_CMD;
    slot_config.d0 = CONFIG_EXAMPLE_PIN_D0;
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;
    slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;
    slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;
#endif  // CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
#endif  // CONFIG_SOC_SDMMC_USE_GPIO_MATRIX

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
#ifdef CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS
            check_sd_card_pins(&config, pin_count);
#endif
        }
        goto cleanup;
    }
    mounted = true;
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    uint64_t capacity_bytes = (uint64_t)card->csd.capacity * (uint64_t)card->csd.sector_size;
    uint64_t capacity_gb_x100 = capacity_bytes * 100ULL / 1000000000ULL;
    ESP_LOGI(TAG, "SD card capacity: %llu.%02llu GB (%d sectors x %d bytes)",
             (unsigned long long)(capacity_gb_x100 / 100ULL),
             (unsigned long long)(capacity_gb_x100 % 100ULL),
             card->csd.capacity, card->csd.sector_size);

    ret = s_example_repeat_file_test(card);
    if (ret != ESP_OK) {
        goto cleanup;
    }

    const char *file_foo = MOUNT_POINT "/foo.txt";
    char data[EXAMPLE_MAX_CHAR_SIZE];
    struct stat st;

    // Format FATFS
#ifdef CONFIG_EXAMPLE_FORMAT_SD_CARD
    ret = esp_vfs_fat_sdcard_format(mount_point, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format FATFS (%s)", esp_err_to_name(ret));
        goto cleanup;
    }

    if (stat(file_foo, &st) == 0) {
        ESP_LOGI(TAG, "file still exists");
        ret = ESP_FAIL;
        goto cleanup;
    } else {
        ESP_LOGI(TAG, "file doesn't exist, formatting done");
    }
#endif // CONFIG_EXAMPLE_FORMAT_SD_CARD

    const char *file_nihao = MOUNT_POINT"/nihao.txt";
    memset(data, 0, EXAMPLE_MAX_CHAR_SIZE);
    snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Nihao", card->cid.name);
    ret = s_example_write_file(file_nihao, data);
    if (ret != ESP_OK) {
        goto cleanup;
    }

    //Open file for reading
    ret = s_example_read_file(file_nihao, data);
    if (ret != ESP_OK) {
        goto cleanup;
    }

cleanup:
    if (mounted) {
        esp_err_t unmount_ret = esp_vfs_fat_sdcard_unmount(mount_point, card);
        if (unmount_ret == ESP_OK) {
            ESP_LOGI(TAG, "Card unmounted");
        } else {
            ESP_LOGE(TAG, "Card unmount failed: %s", esp_err_to_name(unmount_ret));
            if (ret == ESP_OK) {
                ret = unmount_ret;
            }
        }
    }

    // Deinitialize the power control driver if it was used
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    if (pwr_ctrl_handle != NULL) {
        esp_err_t power_ret = sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_handle);
        if (power_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete the on-chip LDO power control driver: %s",
                     esp_err_to_name(power_ret));
        }
    }
#endif
}
