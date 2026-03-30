#pragma once

#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>

//#include "display_bsp.h"

class CustomSDPort {
private:
  const char *TAG = "SDPort";
  const char *SdName_;
  int is_SdcardInitOK = 0;
  sdmmc_card_t *sdCardHead = NULL;
public:
  //CustomSDPort(const char *SdName,DisplayPort &display,int cs = 6);
  CustomSDPort(const char *SdName, int clk = 0, int mosi = 1, int miso = 2, int cs = 6, spi_host_device_t spihost = SPI2_HOST);
  ~CustomSDPort();

  int SDPort_GetStatus() {
    return is_SdcardInitOK;
  }
  int SDPort_WriteFile(const char *path, const void *data, size_t data_len);
  int SDPort_ReadFile(const char *path, uint8_t *buffer, size_t *outLen);
  int SDPort_ReadOffset(const char *path, void *buffer, size_t len, size_t offset);
  int SDPort_WriteOffset(const char *path, const void *data, size_t len, bool append);
  sdmmc_card_t *SDPort_GetCardHead() {
    if (is_SdcardInitOK) {
      return sdCardHead;
    }
    return NULL;
  }
};