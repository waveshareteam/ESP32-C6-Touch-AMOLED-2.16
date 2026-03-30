#include "i2c_bsp.h"
#include "sdcard_bsp.h"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

I2cMasterBus I2cMasterBus_(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);
CustomSDPort *CustomSDPort_ = NULL;

static XPowersPMU axp2101;
static I2cMasterBus *i2cbus_ = NULL;
static i2c_master_dev_handle_t i2cPMICdev = NULL;
static uint8_t i2cPMICAddress;

static int AXP2101_SLAVE_Read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len) {
  int ret;
  uint8_t count = 3;
  do {
    ret = (i2cbus_->i2c_read_buff(i2cPMICdev, regAddr, data, len) == ESP_OK) ? 0 : -1;
    if (ret == 0)
      break;
    vTaskDelay(pdMS_TO_TICKS(100));
    count--;
  } while (count);
  return ret;
}

static int AXP2101_SLAVE_Write(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len) {
  int ret;
  uint8_t count = 3;
  do {
    ret = (i2cbus_->i2c_write_buff(i2cPMICdev, regAddr, data, len) == ESP_OK) ? 0 : -1;
    if (ret == 0)
      break;
    vTaskDelay(pdMS_TO_TICKS(100));
    count--;
  } while (count);
  return ret;
}


void Custom_PmicPortInit(I2cMasterBus *i2cbus, uint8_t dev_addr) {
  if (i2cbus_ == NULL) {
    i2cbus_ = i2cbus;
  }
  if (i2cPMICdev == NULL) {
    i2c_master_bus_handle_t BusHandle = i2cbus_->Get_I2cBusHandle();
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.scl_speed_hz = 100000;
    dev_cfg.device_address = dev_addr;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(BusHandle, &dev_cfg, &i2cPMICdev));
    i2cPMICAddress = dev_addr;
  }
  if (axp2101.begin(i2cPMICAddress, AXP2101_SLAVE_Read, AXP2101_SLAVE_Write)) {
    ESP_LOGI(TAG, "Init PMU SUCCESS!");
  } else {
    ESP_LOGE(TAG, "Init PMU FAILED!");
  }
  Custom_PmicRegisterInit();
}

void Custom_PmicRegisterInit(void) {
  axp2101.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_2000MA);

  if (axp2101.getDC1Voltage() != 3300) {
    axp2101.setDC1Voltage(3300);
    ESP_LOGW("axp2101_init_log", "Set DCDC1 to output 3V3");
  }
  if (axp2101.getALDO1Voltage() != 3300) {
    axp2101.setALDO1Voltage(3300);
    ESP_LOGW("axp2101_init_log", "Set ALDO1 to output 3V3");
  }
  if (axp2101.getALDO2Voltage() != 3300) {
    axp2101.setALDO2Voltage(3300);
    ESP_LOGW("axp2101_init_log", "Set ALDO2 to output 3V3");
  }
  if (axp2101.getALDO3Voltage() != 3300) {
    axp2101.setALDO3Voltage(3300);
    ESP_LOGW("axp2101_init_log", "Set ALDO3 to output 3V3");
  }
  if (axp2101.getALDO4Voltage() != 3300) {
    axp2101.setALDO4Voltage(3300);
    ESP_LOGW("axp2101_init_log", "Set ALDO4 to output 3V3");
  }

  axp2101.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
  axp2101.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
  axp2101.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_50MA);
}

void print_sdcard_info(sdmmc_card_t *card) {
  if (!card) {
    Serial.println("Card is null");
    return;
  }
  Serial.println("===== SD Card Info =====");
  if (card->csd.csd_ver == 2) {
    Serial.println("Card Type: SDHC/SDXC");
  } else {
    Serial.println("Card Type: SDSC");
  }
  uint64_t capacity = (uint64_t)card->csd.capacity * card->csd.sector_size;
  Serial.printf("Capacity: %llu MB\n", capacity / (1024 * 1024));
  Serial.printf("Sector Size: %d bytes\n", card->csd.sector_size);
  Serial.printf("Card Name: %s\n", card->cid.name);
  Serial.printf("Manufacturer ID: %d\n", card->cid.mfg_id);
  Serial.printf("OEM ID: 0x%04x\n", card->cid.oem_id);
  Serial.printf("Serial: %lu\n", card->cid.serial);
  uint16_t mdate = card->cid.date;
  int year = ((mdate >> 4) & 0xFF) + 2000;
  int month = mdate & 0x0F;
  Serial.printf("Date: %02d/%d\n", month, year);
  Serial.println("========================");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("start");
  Custom_PmicPortInit(&I2cMasterBus_, 0x34);
  CustomSDPort_ = new CustomSDPort("/sdcard");
  print_sdcard_info(CustomSDPort_->SDPort_GetCardHead());
}

void loop() {
}
