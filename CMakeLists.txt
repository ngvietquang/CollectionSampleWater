idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES 
                        esp_wifi 
                        esp_event 
                        nvs_flash 
                        esp_http_client 
                        esp_https_ota 
                        bootloader_support 
                        app_update 
                        esp_timer
                        esp_adc
                        ) 