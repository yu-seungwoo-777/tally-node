idf_component_register(
    SRCS "wifi_hal.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_wifi esp_netif esp_event freertos t_log
)
