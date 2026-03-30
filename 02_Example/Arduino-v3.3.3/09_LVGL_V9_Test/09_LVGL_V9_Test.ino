#include <Arduino.h>
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "bsp_lvgl_port.h"
#include "./src/port_bsp/i2c_bsp.h"
#include "./src/port_bsp/axp2101_bsp.h"

I2cMasterBus I2cMasterBus_(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);

#define Brightness_Test_EN  1

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("start");
  Custom_PmicPortInit(&I2cMasterBus_, 0x34);

  bsp_lvgl_init(I2cMasterBus_);
  if (bsp_lvgl_lock(0)) {
    Serial.println("start ui");
    lv_demo_widgets();
    bsp_lvgl_unlock();
  }
}

uint8_t back = 100;

void loop() {
#if (Brightness_Test_EN == 1) 
  if (bsp_lvgl_lock(0)) {
    Lcd_SetBacklight(back);
    bsp_lvgl_unlock();
  }
  vTaskDelay(pdMS_TO_TICKS(2000));
  if (back > 0) {
    back = back - 20;
  } else {
    back = 100;
  }
#endif
}
