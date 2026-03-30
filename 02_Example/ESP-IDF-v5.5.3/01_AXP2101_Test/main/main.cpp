
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>

#include "i2c_bsp.h"
#include "power_bsp.h"
#include "user_config.h"

I2cMasterBus I2cMasterBus_(BSP_I2C_SCL,BSP_I2C_SDA,BSP_I2C_NUM);

extern "C" void app_main(void) {
	Custom_PmicPortInit(&I2cMasterBus_,0x34);
	xTaskCreatePinnedToCore(Axp2101_isChargingTask, "Axp2101_isChargingTask", 3 * 1024, NULL, 3, NULL,0);
}
