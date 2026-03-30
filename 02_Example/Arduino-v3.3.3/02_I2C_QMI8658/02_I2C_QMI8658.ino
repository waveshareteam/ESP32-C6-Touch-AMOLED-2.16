#include "i2c_bsp.h"
#include "qmi8658.h"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

I2cMasterBus I2cMasterBus_(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);

static XPowersPMU axp2101;
static I2cMasterBus *i2cbus_ = NULL;
static i2c_master_dev_handle_t i2cPMICdev = NULL;
static uint8_t i2cPMICAddress;

static qmi8658_dev_t qmi8658;       	
static char LvglDataBuff[40] = {""}; 

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

void QMI8658_Task(void *arg) {
  while (1) {
    bool ready;
    int ret = qmi8658_is_data_ready(&qmi8658, &ready);
    if (ret == ESP_OK && ready) {
      qmi8658_data_t qmidata = {};
      ret = qmi8658_read_sensor_data(&qmi8658, &qmidata);
      if (ret == ESP_OK) {
        snprintf(LvglDataBuff, sizeof(LvglDataBuff), "Acc(m/s2):%.2f,%.2f,%.2f",
                 qmidata.accelX, qmidata.accelY, qmidata.accelZ);
        Serial.printf("acc:%s\n", LvglDataBuff);
        snprintf(LvglDataBuff, sizeof(LvglDataBuff), "Gyro(rad/s):%.2f,%.2f,%.2f",
                 qmidata.gyroX, qmidata.gyroY, qmidata.gyroZ);
        Serial.printf("gyro:%s\n", LvglDataBuff);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("start");
  Custom_PmicPortInit(&I2cMasterBus_, 0x34);
  esp_err_t ret = qmi8658_init(&qmi8658, I2cMasterBus_.Get_I2cBusHandle(), QMI8658_ADDRESS_HIGH);
  if (ret != ESP_OK) {
    Serial.printf("Failed to initialize QMI8658 (error: %d)\n", ret);
  } else {
    qmi8658_set_accel_range(&qmi8658, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(&qmi8658, QMI8658_ACCEL_ODR_1000HZ);
    qmi8658_set_gyro_range(&qmi8658, QMI8658_GYRO_RANGE_512DPS);
    qmi8658_set_gyro_odr(&qmi8658, QMI8658_GYRO_ODR_1000HZ);
    qmi8658_set_accel_unit_mps2(&qmi8658, true);
    qmi8658_set_gyro_unit_rads(&qmi8658, true);
    qmi8658_set_display_precision(&qmi8658, 4);
  }
  xTaskCreatePinnedToCore(QMI8658_Task, "QMI8658_Task", 3 * 1024, NULL, 3, NULL, 0);
}

void loop() {
}
