
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "i2c_bsp.h"
#include "power_bsp.h"
#include "user_config.h"

#define APP_NAME "WIFI STA & AP"

I2cMasterBus I2cMasterBus_(BSP_I2C_SCL,BSP_I2C_SDA,BSP_I2C_NUM);
esp_event_handler_instance_t wifi_event_instance;
esp_event_handler_instance_t ip_event_instance;

static char wifi_ip[16] = {0}; 
static char wifi_mac[6] = {0}; 

void fac_wifi_default_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
	esp_netif_init();                                // Initialize TCP/IP stack
    esp_event_loop_create_default();                 // Create default event loop
    esp_netif_create_default_wifi_sta();             // STA 
    esp_netif_create_default_wifi_ap();              // AP 
}

void FactoryWifiTestStartCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        auto *got_ip_event = (ip_event_got_ip_t *) event_data;
        uint32_t ip_addr = got_ip_event->ip_info.ip.addr;
        snprintf(wifi_ip, sizeof(wifi_ip), "%u.%u.%u.%u",
                (uint8_t)(ip_addr),
                (uint8_t)(ip_addr >> 8),
                (uint8_t)(ip_addr >> 16),
                (uint8_t)(ip_addr >> 24));
        ESP_LOGW(APP_NAME,"STA IP:%s",wifi_ip);
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_ip[0] = '\0';
        ESP_LOGW(APP_NAME,"STA Mode Device disconnected");
    } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        for (uint8_t i = 0; i < 6; i++)
        wifi_mac[i] = event->mac[i];
		ESP_LOGW(APP_NAME,"AP MAC:%02X:%02X:%02X:%02X:%02X:%02X",wifi_mac[0],wifi_mac[1],wifi_mac[2],wifi_mac[3],wifi_mac[4],wifi_mac[5]);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGW(APP_NAME,"AP Mode Device disconnected");
    }
}

void fac_wifi_mode_init(bool sta_mode) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &FactoryWifiTestStartCallback, NULL, &wifi_event_instance);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &FactoryWifiTestStartCallback, NULL, &ip_event_instance);
    if(sta_mode) {
        wifi_config_t sta_config = {};
        strcpy((char*)sta_config.sta.ssid, "ESP32_STA");
        strcpy((char*)sta_config.sta.password, "12345678");
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        esp_wifi_start();
    } else {
        wifi_config_t ap_config = {};
        strcpy((char*)ap_config.ap.ssid, "ESP32_AP");
        strcpy((char*)ap_config.ap.password, "12345678");
        ap_config.ap.max_connection = 4;
        ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        esp_wifi_set_mode(WIFI_MODE_AP);
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        esp_wifi_start();
        wifi_ip[0] = '\0'; 
    }
}

extern "C" void app_main(void) {
	Custom_PmicPortInit(&I2cMasterBus_,0x34);
	fac_wifi_default_init();
	fac_wifi_mode_init(1);
}
