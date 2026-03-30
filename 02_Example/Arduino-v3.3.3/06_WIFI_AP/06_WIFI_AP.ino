#include "i2c_bsp.h"
#include <esp_wifi.h>
#include <nvs_flash.h>
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

I2cMasterBus I2cMasterBus_(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);

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

/*wifi api*/
esp_event_handler_instance_t wifi_event_instance;
esp_event_handler_instance_t ip_event_instance;
static char wifi_ip[16] = { 0 };
static char wifi_mac[6] = { 0 };

void fac_wifi_default_init(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  esp_netif_init();                     // Initialize TCP/IP stack
  esp_event_loop_create_default();      // Create default event loop
  esp_netif_create_default_wifi_sta();  // STA
  esp_netif_create_default_wifi_ap();   // AP
}

void FactoryWifiTestStartCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_id == IP_EVENT_STA_GOT_IP) {
    auto *got_ip_event = (ip_event_got_ip_t *)event_data;
    uint32_t ip_addr = got_ip_event->ip_info.ip.addr;
    snprintf(wifi_ip, sizeof(wifi_ip), "%u.%u.%u.%u",
             (uint8_t)(ip_addr),
             (uint8_t)(ip_addr >> 8),
             (uint8_t)(ip_addr >> 16),
             (uint8_t)(ip_addr >> 24));
    Serial.printf("STA IP:%s\n", wifi_ip);
  } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_ip[0] = '\0';
    Serial.printf("STA Mode Device disconnected\n");
  } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
    for (uint8_t i = 0; i < 6; i++)
      wifi_mac[i] = event->mac[i];
    Serial.printf("AP MAC:%02X:%02X:%02X:%02X:%02X:%02X\n", wifi_mac[0], wifi_mac[1], wifi_mac[2], wifi_mac[3], wifi_mac[4], wifi_mac[5]);
  } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    Serial.printf("AP Mode Device disconnected\n");
  }
}

void fac_wifi_mode_init(bool sta_mode) {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &FactoryWifiTestStartCallback, NULL, &wifi_event_instance);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &FactoryWifiTestStartCallback, NULL, &ip_event_instance);
  if (sta_mode) {
    wifi_config_t sta_config = {};
    strcpy((char *)sta_config.sta.ssid, "ESP32_STA");
    strcpy((char *)sta_config.sta.password, "12345678");
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    esp_wifi_start();
  } else {
    wifi_config_t ap_config = {};
    strcpy((char *)ap_config.ap.ssid, "ESP32_AP");
    strcpy((char *)ap_config.ap.password, "12345678");
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
    wifi_ip[0] = '\0';
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("start");
  Custom_PmicPortInit(&I2cMasterBus_, 0x34);
  /*wifi*/
  fac_wifi_default_init();
	fac_wifi_mode_init(0);
}

void loop() {
}
