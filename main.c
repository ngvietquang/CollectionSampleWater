#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <math.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "mqtt_client.h"
#define TDS_ADC_UNIT       ADC_UNIT_1
#define TDS_ADC_CHANNEL    ADC_CHANNEL_0 
#define VREF               3300.0        
#define TEMPERATURE 25

#define URL ""
#define WIFI_SSID "Xtech789"
#define WIFI_PASS "xtech789"

#define MQTT_BROKER_URL "mqtt://mqtt3.thingspeak.com"
#define THINGSPEAK_CHANNEL_ID "3048761"
#define THINGSPEAK_API_KEY    "R2TRX2TRPJL21TVP"
#define THINGSPEAK_CLIENT_ID  "CSo3Dg00CCQxMTEVGi4NAjU"
#define THINGSPEAK_PASS "0BJTm/sd/xbkneiFgp/Nbsdo"

esp_mqtt_client_handle_t mqtt_client = NULL;

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect(); 
        printf("Dang ket noi lai Wi-Fi...\n");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        printf("Da ket noi Wi-Fi! IP: " IPSTR "\n", IP2STR(&((ip_event_got_ip_t*)event_data)->ip_info.ip));
    }
}

void wifi_init() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}
void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
        .credentials.client_id = THINGSPEAK_CLIENT_ID,
        .credentials.username = THINGSPEAK_CLIENT_ID,
        .credentials.authentication.password = THINGSPEAK_PASS,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtt_client);
}


void send_to_private_php(float tds_value) {
    const char *url = URL;

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);


    char post_data[100];
    snprintf(post_data, sizeof(post_data), "{\"tds\": %.2f, \"unit\": \"ppm\"}", tds_value);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("HTTP POST Status = %d\n", esp_http_client_get_status_code(client));
        printf("Da gui\n");
    } else {
        printf("HTTP POST Failed: %s\n", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}
float read_tds_logic(adc_oneshot_unit_handle_t handle) {
    int adc_raw;
    uint32_t sum = 0;
    int sample = 20;
    for (int i = 0; i < sample; i++) {
        adc_oneshot_read(handle, TDS_ADC_CHANNEL, &adc_raw); // Day tu 0 - 4095
        sum += adc_raw;
        vTaskDelay(pdMS_TO_TICKS(10));
    } //Tinh trung binh 
    float avg_raw = (float)sum / sample;

    float voltage = (avg_raw * VREF / 4095.0) / 1000.0; 

    // Công thức tính TDS (ppm)
    float compensationCoefficient = 1.0+0.02*(TEMPERATURE-25.0);
    float compensationVoltage = voltage / compensationCoefficient;
    float tdsValue = (133.42 * pow(compensationVoltage, 3) - 255.86 * pow(compensationVoltage, 2) + 857.39 * compensationVoltage) * 0.5;

    return tdsValue;
}

void tds_task(void *pvParameters) {

    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = TDS_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, 
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, TDS_ADC_CHANNEL, &config));
   
    while(1) {
        float tds = read_tds_logic(adc1_handle);
        
        send_to_private_php(tds);

        printf("Chat luong nuoc (TDS): %.2f ppm\n", tds);
            
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    printf("Dang khoi tao Wi-Fi...\n");
    wifi_init();
    printf("Cho Wi-Fi on dinh...\n");
    vTaskDelay(pdMS_TO_TICKS(10000));
    xTaskCreate(tds_task, "Read_TDS", 4096, NULL, 1, NULL);
}
