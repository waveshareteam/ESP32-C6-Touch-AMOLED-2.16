
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>

#include "i2c_bsp.h"
#include "power_bsp.h"
#include "user_config.h"
#include "pcf85063a.h"

I2cMasterBus I2cMasterBus_(BSP_I2C_SCL,BSP_I2C_SDA,BSP_I2C_NUM);
static pcf85063a_dev_t pcf85063; 
static char LvglDataBuff[40] = {""}; 	

void PCF85063_Task(void *arg) {
	while(1) {
        pcf85063a_datetime_t datatime = {};
        pcf85063a_get_time_date(&pcf85063, &datatime);
        pcf85063a_datetime_to_str(LvglDataBuff, datatime);
        snprintf(LvglDataBuff, sizeof(LvglDataBuff), "%04d/%02d/%02d %02d:%02d:%02d",\
        datatime.year, datatime.month,datatime.day, datatime.hour, datatime.min, datatime.sec);
        ESP_LOGW("RTC","%s", LvglDataBuff);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void) {
    Custom_PmicPortInit(&I2cMasterBus_,0x34);
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
	xTaskCreatePinnedToCore(PCF85063_Task, "PCF85063_Task", 3 * 1024, NULL, 3, NULL,0);
}
