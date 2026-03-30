#include "i2c_bsp.h"
#include "pcf85063a.h"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

I2cMasterBus I2cMasterBus_(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);

static XPowersPMU axp2101;
static I2cMasterBus *i2cbus_ = NULL;
static i2c_master_dev_handle_t i2cPMICdev = NULL;
static uint8_t i2cPMICAddress;

static pcf85063a_dev_t pcf85063;
static char LvglDataBuff[40] = { "" };

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

void PCF85063_Task(void *arg) {
  while (1) {
    pcf85063a_datetime_t datatime = {};
    pcf85063a_get_time_date(&pcf85063, &datatime);
    pcf85063a_datetime_to_str(LvglDataBuff, datatime);
    snprintf(LvglDataBuff, sizeof(LvglDataBuff), "%04d/%02d/%02d %02d:%02d:%02d",
             datatime.year, datatime.month, datatime.day, datatime.hour, datatime.min, datatime.sec);
    Serial.printf("rtc:%s\n", LvglDataBuff);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("start");
  Custom_PmicPortInit(&I2cMasterBus_, 0x34);
  esp_err_t ret = pcf85063a_init(&pcf85063, I2cMasterBus_.Get_I2cBusHandle(), PCF85063A_ADDRESS);
  if (ret != ESP_OK) {
    ESP_LOGE("pcf85063", "Failed to initialize PCF85063 (error: %d)", ret);
  } else {
    pcf85063a_datetime_t datatime = {
      .year = 2026,
      .month = 1,
      .day = 1,
      .hour = 8,
      .min = 0,
      .sec = 0
    };
    pcf85063a_set_time_date(&pcf85063, datatime);
  }
  xTaskCreatePinnedToCore(PCF85063_Task, "PCF85063_Task", 3 * 1024, NULL, 3, NULL, 0);
}

void loop() {
}
