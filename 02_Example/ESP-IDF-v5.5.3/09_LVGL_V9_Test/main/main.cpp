
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_spiffs.h>

#include "lvgl_bsp.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "power_bsp.h"

#define TAG "main"

#define Brightness_Test_EN  1

I2cMasterBus user_i2cbus(7,8,0); //scl,sda,i2c_port
DisplayPort *user_display = NULL;

#if (Brightness_Test_EN == 1) 

void Brightness_Test(void *arg) {
    uint8_t back = 100;
    while(1) {
        if(Lvgl_lock(-1) == ESP_OK) {
		    user_display->Set_Backlight(back);
            Lvgl_unlock();
  	    }
        vTaskDelay(pdMS_TO_TICKS(2000));
        if(back > 0) {
            back = back - 20;
        } else {
            back = 100;
        }
    }
}

#endif

extern "C" void app_main(void)
{
    Custom_PmicPortInit(&user_i2cbus,0x34);
    user_display = new DisplayPort(user_i2cbus,480,480);
    user_display->DisplayPort_TouchInit();
    Lvgl_PortInit(*user_display);
    if(Lvgl_lock(-1) == ESP_OK) {
		lv_demo_widgets();
    	Lvgl_unlock();
  	}
#if (Brightness_Test_EN == 1) 
    xTaskCreatePinnedToCore(Brightness_Test, "Brightness_Test", 3 * 1024, NULL, 3, NULL,0);
#endif
}
