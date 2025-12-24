idf_component_register(
    SRCS "WiFiHal.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_wifi esp_netif esp_event freertos t_log
)
