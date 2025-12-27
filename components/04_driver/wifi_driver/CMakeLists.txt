idf_component_register(
    SRCS "wifi_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES wifi_hal esp_wifi esp_netif esp_event freertos t_log event_bus
)
