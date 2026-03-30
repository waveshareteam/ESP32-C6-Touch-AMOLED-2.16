
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>

#include "i2c_bsp.h"
#include "power_bsp.h"
#include "user_config.h"
#include "qmi8658.h"

I2cMasterBus I2cMasterBus_(BSP_I2C_SCL,BSP_I2C_SDA,BSP_I2C_NUM);
static qmi8658_dev_t qmi8658;       	
static char LvglDataBuff[40] = {""}; 	

void QMI8658_Task(void *arg) {
	while(1) {
        bool ready;
        int ret = qmi8658_is_data_ready(&qmi8658, &ready);
        if(ret == ESP_OK && ready) {
            qmi8658_data_t qmidata = {};
            ret = qmi8658_read_sensor_data(&qmi8658, &qmidata);
            if(ret == ESP_OK) {
                snprintf(LvglDataBuff, sizeof(LvglDataBuff), "Acc(m/s2):%.2f,%.2f,%.2f",\
                qmidata.accelX, qmidata.accelY, qmidata.accelZ);
                ESP_LOGW("acc","%s", LvglDataBuff);
                snprintf(LvglDataBuff, sizeof(LvglDataBuff), "Gyro(rad/s):%.2f,%.2f,%.2f",\
                qmidata.gyroX, qmidata.gyroY, qmidata.gyroZ);
                ESP_LOGW("gyro","%s", LvglDataBuff);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
	}
}

extern "C" void app_main(void) {
    Custom_PmicPortInit(&I2cMasterBus_,0x34);
    esp_err_t ret = qmi8658_init(&qmi8658, I2cMasterBus_.Get_I2cBusHandle(), QMI8658_ADDRESS_HIGH);
    if (ret != ESP_OK) {
        ESP_LOGE("qmi8658", "Failed to initialize QMI8658 (error: %d)", ret);
    } else {
        qmi8658_set_accel_range(&qmi8658, QMI8658_ACCEL_RANGE_8G);
        qmi8658_set_accel_odr(&qmi8658, QMI8658_ACCEL_ODR_1000HZ);
        qmi8658_set_gyro_range(&qmi8658, QMI8658_GYRO_RANGE_512DPS);
        qmi8658_set_gyro_odr(&qmi8658, QMI8658_GYRO_ODR_1000HZ);
        qmi8658_set_accel_unit_mps2(&qmi8658, true); 
        qmi8658_set_gyro_unit_rads(&qmi8658, true);  
        qmi8658_set_display_precision(&qmi8658, 4);  
    }
	xTaskCreatePinnedToCore(QMI8658_Task, "QMI8658_Task", 3 * 1024, NULL, 3, NULL,0);
}
