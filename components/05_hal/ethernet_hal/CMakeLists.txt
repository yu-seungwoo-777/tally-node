idf_component_register(
    SRCS "ethernet_hal.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_eth esp_netif esp_event freertos t_log app_types
)
