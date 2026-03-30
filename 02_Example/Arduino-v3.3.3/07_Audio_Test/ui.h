#pragma once

#include "lvgl.h"

typedef struct
{
	lv_obj_t *screen;
	bool screen_del;
	lv_obj_t *screen_btn_4;
	lv_obj_t *screen_btn_4_label;
	lv_obj_t *screen_label_1;
	lv_obj_t *screen_label_2;
	lv_obj_t *screen_btn_5;
	lv_obj_t *screen_btn_5_label;
	lv_obj_t *screen_btn_6;
	lv_obj_t *screen_btn_6_label;
	lv_obj_t *screen_btn_7;
	lv_obj_t *screen_btn_7_label;
}lv_audio_ui;

void setup_audio_ui(lv_audio_ui *ui);

LV_FONT_DECLARE(lv_font_montserratMedium_18)
LV_FONT_DECLARE(lv_font_montserratMedium_16)
LV_FONT_DECLARE(lv_font_montserratMedium_12)