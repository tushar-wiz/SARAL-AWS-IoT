#pragma once
#include "lvgl/lvgl.h"

extern TaskHandle_t batteryTaskHandle;
extern bool reportScreenBool; 

void ui_init(lv_obj_t* globScreen);
void reportScreen(lv_obj_t * obj, lv_event_t event);
void heartRateScreen(lv_obj_t * obj, lv_event_t event);
void logScreen(lv_obj_t * obj, lv_event_t event);
void tempScreen(lv_obj_t * obj, lv_event_t event);
void header();