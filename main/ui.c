#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"

#include "esp_log.h"
#include "core2forAWS.h"
#include "ui.h"

#define MAX_TEXTAREA_LENGTH 1024

LV_IMG_DECLARE(heart_icon);
LV_IMG_DECLARE(report_icon);
LV_IMG_DECLARE(logs_icon);
LV_IMG_DECLARE(fun_icon);
LV_IMG_DECLARE(qr_code);

static const char* TAG = "UI";

lv_obj_t* homeScreen;
TaskHandle_t batteryTaskHandle;

lv_style_t homeScreenStyle;
lv_style_t buttonStyle;

bool reportScreenBool = false;

void ui_init(lv_obj_t* globScreen)
{
    homeScreen = lv_obj_create(NULL,NULL);

    lv_style_set_bg_color(&homeScreenStyle,LV_STATE_DEFAULT,LV_COLOR_MAKE(108, 99, 255));
    lv_obj_add_style(homeScreen,0,&homeScreenStyle);
    
    lv_obj_t* label;

    lv_style_init(&buttonStyle);
    lv_style_set_radius(&buttonStyle, LV_STATE_DEFAULT, 20);
    lv_style_set_bg_color(&buttonStyle, LV_STATE_DEFAULT, LV_COLOR_MAKE(255,255,255));
    lv_style_set_bg_color(&buttonStyle, LV_STATE_PRESSED, LV_COLOR_MAKE(200,200,200));
    lv_style_set_border_color(&buttonStyle,LV_STATE_DEFAULT,LV_COLOR_MAKE(255,255,255));
    lv_style_set_border_color(&buttonStyle,LV_STATE_PRESSED, LV_COLOR_MAKE(200,200,200));
    lv_style_set_border_color(&buttonStyle,LV_BTN_STATE_PRESSED, LV_COLOR_MAKE(200,200,200));

    lv_obj_t* heartRateButton = lv_btn_create(homeScreen,NULL);
    lv_obj_set_event_cb(heartRateButton,heartRateScreen);
    lv_obj_set_height(heartRateButton, 90);
    lv_obj_align(heartRateButton, NULL, LV_ALIGN_CENTER, -70, -40);
    label = lv_label_create(heartRateButton, NULL);
    lv_label_set_text(label, "Heart Rate");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER,0,5);
    lv_obj_align(label,heartRateButton,LV_ALIGN_CENTER,0,20);
    lv_obj_add_style(heartRateButton,LV_BTN_PART_MAIN,&buttonStyle);
    lv_obj_t* heartIcon = lv_img_create(heartRateButton,NULL);
    lv_img_set_src(heartIcon, &heart_icon);
    lv_obj_align(heartIcon, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_local_bg_color(heartIcon,LV_IMG_PART_MAIN,LV_STATE_DEFAULT, LV_COLOR_MAKE(255,255,255));

    lv_obj_t* qrCodeButton = lv_btn_create(homeScreen,NULL);
    lv_obj_set_event_cb(qrCodeButton,reportScreen);
    lv_obj_set_height(qrCodeButton, 90);
    lv_obj_align(qrCodeButton, NULL, LV_ALIGN_CENTER, 70, -40);
    label = lv_label_create(qrCodeButton, NULL);
    lv_label_set_text(label, "Reports");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER,0,5);
    lv_obj_add_style(qrCodeButton,0,&buttonStyle);
    lv_obj_t* reportIcon = lv_img_create(qrCodeButton,NULL);
    lv_img_set_src(reportIcon, &report_icon);
    lv_obj_align(reportIcon, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_local_bg_color(reportIcon,LV_IMG_PART_MAIN,LV_STATE_DEFAULT, LV_COLOR_MAKE(255,255,255));

    lv_obj_t* logButton = lv_btn_create(homeScreen,NULL);
    lv_obj_set_event_cb(logButton,logScreen);
    lv_obj_set_height(logButton, 90);
    lv_obj_align(logButton, NULL, LV_ALIGN_CENTER, -70, 60);
    label = lv_label_create(logButton, NULL);
    lv_label_set_text(label, "Logs");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER,0,5);
    lv_obj_add_style(logButton,0,&buttonStyle);
    lv_obj_t* logsIcon = lv_img_create(logButton,NULL);
    lv_img_set_src(logsIcon, &logs_icon);
    lv_obj_align(logsIcon, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_local_bg_color(logsIcon,LV_IMG_PART_MAIN,LV_STATE_DEFAULT, LV_COLOR_MAKE(255,255,255));

    lv_obj_t* tempButton = lv_btn_create(homeScreen,NULL);
    lv_obj_set_event_cb(tempButton,tempScreen);
    lv_obj_set_height(tempButton, 90);
    lv_obj_align(tempButton, NULL, LV_ALIGN_CENTER, 70, 60);
    label = lv_label_create(tempButton, NULL);
    lv_label_set_text(label, "Fun");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER,0,5);
    lv_obj_add_style(tempButton,0,&buttonStyle);
    lv_obj_t* funIcon = lv_img_create(tempButton,NULL);
    lv_img_set_src(funIcon, &fun_icon);
    lv_obj_align(funIcon, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_local_bg_color(funIcon,LV_IMG_PART_MAIN,LV_STATE_DEFAULT, LV_COLOR_MAKE(255,255,255));


    if(globScreen != homeScreen) globScreen = homeScreen;
    lv_scr_load(homeScreen);
}
void goBack(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) 
    {
        vTaskDelete(batteryTaskHandle);
        lv_obj_del(lv_obj_get_parent(obj));
        ui_init(homeScreen);
        xTaskCreate(header, "Header", 4096 * 2, (void *)homeScreen, 1, &batteryTaskHandle);
    }
}
void reportScreen(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED)
    {
        lv_obj_t* hrScreen = lv_obj_create(NULL,NULL);

        lv_obj_add_style(hrScreen,0,&homeScreenStyle);

        lv_obj_t* qrCode = lv_img_create(hrScreen,NULL);
        lv_img_set_src(qrCode, &qr_code);
        lv_obj_align(qrCode, NULL, LV_ALIGN_CENTER, 0, 0);

        lv_scr_load(hrScreen);
    }
}

void heartRateScreen(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED)
    {
        lv_obj_t* hrScreen = lv_obj_create(NULL,NULL);

        lv_obj_add_style(hrScreen,0,&homeScreenStyle);

        lv_obj_t* backButton = lv_btn_create(hrScreen, NULL);
        lv_obj_align(backButton,hrScreen,LV_ALIGN_CENTER,-70,-70);
        lv_obj_set_width(backButton,60);
        lv_obj_set_height(backButton,40);
        lv_obj_t* homeSymbol = lv_label_create(backButton,NULL);
        lv_label_set_text(homeSymbol,LV_SYMBOL_HOME);
        lv_obj_add_style(backButton,LV_BTN_PART_MAIN,&buttonStyle);
        lv_obj_set_event_cb(backButton,goBack);

        lv_obj_t* label1 = lv_label_create(hrScreen,NULL);
        lv_label_set_recolor(label1, true);
        lv_label_set_text_fmt(label1, "#0000ff Heart Rate : #");
        lv_obj_align(label1, hrScreen,LV_ALIGN_CENTER,-50,-40);
        lv_obj_set_width(label1, 150); lv_obj_set_height(label1, 150);

        lv_obj_t* label2 = lv_label_create(hrScreen,NULL);
        lv_label_set_recolor(label2, true);
        lv_label_set_text_fmt(label2, "#0000ff Oxygen Level : #");
        lv_obj_align(label2, hrScreen,LV_ALIGN_CENTER,-50,50);
        lv_obj_set_width(label2, 150); lv_obj_set_height(label2, 150);

        lv_scr_load(hrScreen);
    }
}
void logScreen(lv_obj_t * obj, lv_event_t event)
{
}
void tempScreen(lv_obj_t * obj, lv_event_t event)
{

}

void header()
{   
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    lv_obj_t* battery_label = lv_label_create(homeScreen, NULL);
    ESP_LOGI(TAG,"Battery Init");
    lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL);
    lv_label_set_recolor(battery_label, true);
    lv_label_set_align(battery_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_align(battery_label, homeScreen, LV_ALIGN_IN_TOP_RIGHT, -20, 10);
    lv_obj_t* charge_label = lv_label_create(battery_label, NULL);
    lv_label_set_recolor(charge_label, true);
    lv_label_set_text(charge_label, "");
    lv_obj_align(charge_label, battery_label, LV_ALIGN_CENTER, -4, 0);
    xSemaphoreGive(xGuiSemaphore);
    ESP_LOGI(TAG,"After Battery Init");
    for(;;){
        xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
        float battery_voltage = Core2ForAWS_PMU_GetBatVolt();
        if(battery_voltage >= 4.100){
            lv_label_set_text(battery_label, "#0ab300 " LV_SYMBOL_BATTERY_FULL "#");
        } else if(battery_voltage >= 3.95){
            lv_label_set_text(battery_label, "#0ab300 " LV_SYMBOL_BATTERY_3 "#");
        } else if(battery_voltage >= 3.80){
            lv_label_set_text(battery_label, "#ff9900 " LV_SYMBOL_BATTERY_2 "#");
        } else if(battery_voltage >= 3.25){
            lv_label_set_text(battery_label, "#ff0000 " LV_SYMBOL_BATTERY_1 "#");
        } else{
            lv_label_set_text(battery_label, "#ff0000 " LV_SYMBOL_BATTERY_EMPTY "#");
        }

        if(Core2ForAWS_PMU_GetBatCurrent() >= 0.00){
            lv_label_set_text(charge_label, "#0000cc " LV_SYMBOL_CHARGE "#");
        } else{
            lv_label_set_text(charge_label, "");
        }
        xSemaphoreGive(xGuiSemaphore);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    vTaskDelete(NULL); // Should never get to here..
}
