#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include "sdcard_bsp.h"

//CustomSDPort::CustomSDPort(const char *SdName,DisplayPort &display,int cs) :
//SdName_(SdName)
//{
//    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
//    mount_config.format_if_mount_failed = false;        //If the hook fails, create a partition table and format the SD card
//    mount_config.max_files = 2;                         //Maximum number of open files
//    mount_config.allocation_unit_size = 512;            //Similar to sector size
//
//    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
//    slot_config.gpio_cs = (gpio_num_t)cs;
//    slot_config.host_id = display.Get_SpiHost();
//    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
//    host.slot = display.Get_SpiHost();
//    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_vfs_fat_sdspi_mount(SdName_, &host, &slot_config, &mount_config, &sdCardHead));
//
//    if(sdCardHead != NULL) {
//        sdmmc_card_print_info(stdout, sdCardHead);          //Print out the card information
//        //printf("practical_size:%.2fG\n",sdcard_GetValue()); //g
//        is_SdcardInitOK = 1;
//    } else {
//        ESP_LOGE(TAG, "SD card mount failed");
//        is_SdcardInitOK = 0;
//    }
//}

CustomSDPort::CustomSDPort(const char *SdName, int clk, int mosi, int miso, int cs, spi_host_device_t spihost)
  : SdName_(SdName) {
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
  mount_config.format_if_mount_failed = false;  //If the hook fails, create a partition table and format the SD card
  mount_config.max_files = 5;                   //Maximum number of open files
  mount_config.allocation_unit_size = 512;      //Similar to sector size

  spi_bus_config_t bus_cfg = {};
  bus_cfg.mosi_io_num = mosi;
  bus_cfg.miso_io_num = miso;
  bus_cfg.sclk_io_num = clk;
  bus_cfg.quadwp_io_num = -1;
  bus_cfg.quadhd_io_num = -1;
  bus_cfg.max_transfer_sz = 4000;
  ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_initialize(spihost, &bus_cfg, SDSPI_DEFAULT_DMA));


  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = (gpio_num_t)cs;
  slot_config.host_id = spihost;
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = spihost;
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_vfs_fat_sdspi_mount(SdName_, &host, &slot_config, &mount_config, &sdCardHead));

  if (sdCardHead != NULL) {
    sdmmc_card_print_info(stdout, sdCardHead);  //Print out the card information
    //printf("practical_size:%.2fG\n",sdcard_GetValue()); //g
    is_SdcardInitOK = 1;
  } else {
    ESP_LOGE(TAG, "SD card mount failed");
    is_SdcardInitOK = 0;
  }
}

CustomSDPort::~CustomSDPort() {
}

int CustomSDPort::SDPort_WriteFile(const char *path, const void *data, size_t data_len) {
  if (sdCardHead == NULL) {
    ESP_LOGE(TAG, "SD card not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (sdmmc_get_status(sdCardHead) != ESP_OK) {
    ESP_LOGE(TAG, "SD card not ready");
    return ESP_FAIL;
  }

  FILE *f = fopen(path, "wb");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
    return ESP_ERR_NOT_FOUND;
  }

  size_t written = fwrite(data, 1, data_len, f);
  fclose(f);

  if (written != data_len) {
    ESP_LOGE(TAG, "Write failed (%zu/%zu bytes)", written, data_len);
    return ESP_FAIL;
  }
  return ESP_OK;
}

int CustomSDPort::SDPort_ReadFile(const char *path, uint8_t *buffer, size_t *outLen) {
  if (sdCardHead == NULL) {
    ESP_LOGE(TAG, "SD card not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (sdmmc_get_status(sdCardHead) != ESP_OK) {
    ESP_LOGE(TAG, "SD card not ready");
    return ESP_FAIL;
  }

  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file: %s", path);
    return ESP_ERR_NOT_FOUND;
  }

  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  if (file_size <= 0) {
    ESP_LOGE(TAG, "Invalid file size");
    fclose(f);
    return ESP_FAIL;
  }
  fseek(f, 0, SEEK_SET);

  size_t bytes_read = fread(buffer, 1, file_size, f);
  fclose(f);

  if (outLen) *outLen = bytes_read;
  return (bytes_read > 0) ? ESP_OK : ESP_FAIL;
}

int CustomSDPort::SDPort_ReadOffset(const char *path, void *buffer, size_t len, size_t offset) {
  if (sdCardHead == NULL) {
    ESP_LOGE(TAG, "SD card not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (sdmmc_get_status(sdCardHead) != ESP_OK) {
    ESP_LOGE(TAG, "SD card not ready");
    return ESP_FAIL;
  }

  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file: %s", path);
    return ESP_ERR_NOT_FOUND;
  }

  fseek(f, offset, SEEK_SET);
  size_t bytes_read = fread(buffer, 1, len, f);
  fclose(f);
  return bytes_read;
}

int CustomSDPort::SDPort_WriteOffset(const char *path, const void *data, size_t len, bool append) {
  if (sdCardHead == NULL) {
    ESP_LOGE(TAG, "SD card not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (sdmmc_get_status(sdCardHead) != ESP_OK) {
    ESP_LOGE(TAG, "SD card not ready");
    return ESP_FAIL;
  }

  const char *mode = append ? "ab" : "wb";
  FILE *f = fopen(path, mode);
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file: %s", path);
    return ESP_ERR_NOT_FOUND;
  }

  size_t bytes_written = fwrite(data, 1, len, f);
  fclose(f);

  if (!append && len == 0) {
    ESP_LOGI(TAG, "File cleared: %s", path);
    return ESP_OK;
  }
  return bytes_written;
}