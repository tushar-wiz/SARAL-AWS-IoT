#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"

#include "core2forAWS.h"

#include "wifi.h"
#include "ui.h"

static const char *TAG = "MAIN";

lv_obj_t* homeScreen;

QueueHandle_t xQueueMsgPtrs;


#define STARTING_FALL_DETECTION false
#define STARTINGG_REPORT_SCREEN false
#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 300

/* CA Root certificate */
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");

/* Default MQTT HOST URL is pulled from the aws_iot_config.h */
char HostAddress[255] = AWS_IOT_MQTT_HOST;

/* Default MQTT port is pulled from the aws_iot_config.h */
uint32_t port = AWS_IOT_MQTT_PORT;

void disconnect_callback_handler(AWS_IoT_Client *pClient, void *data) {
    ESP_LOGW(TAG, "MQTT Disconnect");

    IoT_Error_t rc = FAILURE;

    if(NULL == pClient) {
        return;
    }

    if(aws_iot_is_autoreconnect_enabled(pClient)) {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } else {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc) {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        } else {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

static bool shadowUpdateInProgress;

void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
                                const char *pReceivedJsonDocument, void *pContextData) {
    IOT_UNUSED(pThingName);
    IOT_UNUSED(action);
    IOT_UNUSED(pReceivedJsonDocument);
    IOT_UNUSED(pContextData);

    shadowUpdateInProgress = false;

    if(SHADOW_ACK_TIMEOUT == status) {
        ESP_LOGE(TAG, "Update timed out");
    } else if(SHADOW_ACK_REJECTED == status) {
        ESP_LOGE(TAG, "Update rejected");
    } else if(SHADOW_ACK_ACCEPTED == status) {
        ESP_LOGI(TAG, "Update accepted");
    }
}


//**********************************Fall Detection Task**********************************
bool fall_detect = STARTING_FALL_DETECTION;

SemaphoreHandle_t xAccelData;
SemaphoreHandle_t xHealthParams;
// This Variable is Protected by Semaphore
bool b_fallDetected = false;

void fall_detection(void *param){
    MPU6886_Init();
    float accel_x = 0.0;
    float accel_y = 0.0;
    float accel_z = 0.0;
    float accel_data = 0.0;
    while(1){
        MPU6886_GetAccelData(&accel_x, &accel_y, &accel_z);
        
        accel_data = sqrt((accel_x*accel_x)+(accel_y*accel_y)+(accel_z*accel_z));

        if(accel_data < 0.40){
            xSemaphoreTake(xAccelData, portMAX_DELAY);
            b_fallDetected = true;
            xSemaphoreGive(xAccelData);

            Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0xff0000);
            Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0xff0000);
            Core2ForAWS_Sk6812_Show();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else if(b_fallDetected == true && accel_data >= 0.40){
            xSemaphoreTake(xAccelData, portMAX_DELAY);
            b_fallDetected = false;
            xSemaphoreGive(xAccelData);

            Core2ForAWS_Sk6812_Clear();
            Core2ForAWS_Sk6812_Show();
        }

        //ESP_LOGI(TAG, "%.2f", accel_data);
        vTaskDelay(250 / portTICK_PERIOD_MS);       
    }

    ESP_LOGE(TAG, "Error in Blink Task");
    abort();
}


//************************************Stagnancy Task*************************************


void not_moving(void *param){
    float accel_x = 0.0;
    float accel_y = 0.0;
    float accel_z = 0.0;
    float accel_data = 0.0;
    rtc_date_t last_minute;
    rtc_date_t current_minute;
    BM8563_GetTime(&last_minute);
    for(;;){
        MPU6886_GetAccelData(&accel_x, &accel_y, &accel_z);
        accel_data = sqrt((accel_x*accel_x)+(accel_y*accel_y)+(accel_z*accel_z));
        BM8563_GetTime(&current_minute);

        if(abs(1.1 - accel_data) < 0.4){
            if(current_minute.minute - last_minute.minute >= 1){
                Core2ForAWS_Motor_SetStrength(60);
                Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0xff0000);
                Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0xff0000);
                Core2ForAWS_Sk6812_Show();
                vTaskDelay(pdMS_TO_TICKS(1000));
                Core2ForAWS_Motor_SetStrength(0);
                Core2ForAWS_Sk6812_Clear();
                Core2ForAWS_Sk6812_Show();
                last_minute = current_minute;
            }
        }
        else{
            last_minute = current_minute;
        }

        vTaskDelay(pdMS_TO_TICKS(10000));

    }
    ESP_LOGE(TAG, "Error in Not Moving Task");
    abort();
}
//********************************Heart Rate and SPO2 Task*******************************


uint8_t heartRateVal;
uint8_t spo2Val;

void heartRateSensor(void *param){
    for(;;){
        /* Insert Heart Rate Sensor Code Here */

        //Place Holder Stuff 
        xSemaphoreTake(xHealthParams, portMAX_DELAY);
        heartRateVal = 78;
        spo2Val = 95;
        xSemaphoreGive(xHealthParams);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGE(TAG, "Error in Heart Rate Sensor");
    abort();
}


void aws_iot_task(void *param) {
    IoT_Error_t rc = FAILURE;

    char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
    size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);


    jsonStruct_t deviceid;
    deviceid.cb = NULL;
    deviceid.pKey = "deviceid";
    deviceid.pData = "0123a1b61c51cfc601";
    deviceid.type = SHADOW_JSON_STRING;
    deviceid.dataLength = sizeof("0123a1b61c51cfc601")+1;
    
    jsonStruct_t fallHandler;
    fallHandler.cb = NULL;
    fallHandler.pKey = "FallDetect";
    fallHandler.pData = &fall_detect;
    fallHandler.type = SHADOW_JSON_BOOL;
    fallHandler.dataLength = sizeof(bool); 

    jsonStruct_t heartRate;
    heartRate.cb = NULL;
    heartRate.pKey = "heartRate";
    heartRate.pData = &heartRateVal;
    heartRate.type = SHADOW_JSON_INT8;
    heartRate.dataLength = sizeof(uint8_t); 

    jsonStruct_t spo2;
    spo2.cb = NULL;
    spo2.pKey = "spo2";
    spo2.pData = &spo2Val;
    spo2.type = SHADOW_JSON_INT8;
    spo2.dataLength = sizeof(uint8_t); 
    

    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

    // initialize the mqtt client
    AWS_IoT_Client iotCoreClient;

    ShadowInitParameters_t sp = ShadowInitParametersDefault;
    sp.pHost = HostAddress;
    sp.port = port;
    sp.enableAutoReconnect = false;
    sp.disconnectHandler = disconnect_callback_handler;

    sp.pRootCA = (const char *)aws_root_ca_pem_start;
    sp.pClientCRT = "#";
    sp.pClientKey = "#0";
    
    #define CLIENT_ID_LEN (ATCA_SERIAL_NUM_SIZE * 2)
    char *client_id = malloc(CLIENT_ID_LEN + 1);
    ATCA_STATUS ret = Atecc608_GetSerialString(client_id);
    if (ret != ATCA_SUCCESS){
        ESP_LOGE(TAG, "Failed to get device serial from secure element. Error: %i", ret);
        abort();
    }


    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Shadow Init");

    rc = aws_iot_shadow_init(&iotCoreClient, &sp);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_shadow_init returned error %d, aborting...", rc);
        abort();
    }

    ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
    scp.pMyThingName = client_id;
    scp.pMqttClientId = client_id;
    scp.mqttClientIdLen = CLIENT_ID_LEN;

    ESP_LOGI(TAG, "Shadow Connect");
    rc = aws_iot_shadow_connect(&iotCoreClient, &scp);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_shadow_connect returned error %d, aborting...", rc);
        abort();
    }
    
    
    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    rc = aws_iot_shadow_set_autoreconnect_status(&iotCoreClient, true);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d, aborting...", rc);
        abort();
    }

    // loop and publish changes
    while(NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc) {
        rc = aws_iot_shadow_yield(&iotCoreClient, 200);
        if(NETWORK_ATTEMPTING_RECONNECT == rc || shadowUpdateInProgress) {
            rc = aws_iot_shadow_yield(&iotCoreClient, 1000);
            // If the client is attempting to reconnect, or already waiting on a shadow update,
            // we will skip the rest of the loop.
            continue;
        }

       xSemaphoreTake(xAccelData, portMAX_DELAY);
       fall_detect = b_fallDetected;
       xSemaphoreGive(xAccelData);


        ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "On Device: Fall Detect %d", fall_detect);

        rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
        if(SUCCESS == rc) {
            rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 4, &deviceid, &fallHandler, &heartRate, &spo2);
            if(SUCCESS == rc) {
                rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
                if(SUCCESS == rc) {
                    ESP_LOGI(TAG, "Update Shadow: %s", JsonDocumentBuffer);
                    rc = aws_iot_shadow_update(&iotCoreClient, client_id, JsonDocumentBuffer,
                                               ShadowUpdateStatusCallback, NULL, 4, true);
                    shadowUpdateInProgress = true;
                }
            }
        }
        ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "An error occurred in the loop %d", rc);
    }

    ESP_LOGI(TAG, "Disconnecting");
    rc = aws_iot_shadow_disconnect(&iotCoreClient);

    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Disconnect error %d", rc);
    }

    vTaskDelete(NULL);
}

void app_main()
{   
    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(60);
    Core2ForAWS_LED_Enable(1);

    xAccelData = xSemaphoreCreateMutex();
    xHealthParams = xSemaphoreCreateMutex();
    if(xAccelData == NULL || xHealthParams == NULL){
        ESP_LOGE(TAG, "Error in Mutex Creation");
        abort();
    }


    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    ui_init(homeScreen);
    xSemaphoreGive(xGuiSemaphore);

    initialise_wifi();
    
    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 4096*2, NULL, 5, NULL, 1);
    xTaskCreate(&header, "Header", 4096 * 2, (void *)homeScreen, 1, NULL);
    xTaskCreate(&fall_detection, "Fall Task", 4096 * 2, NULL, 1, NULL);
    xTaskCreate(&not_moving, "Not Moving", 4096, NULL, 4, NULL);
}