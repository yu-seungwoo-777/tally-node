idf_component_register(
    SRCS "ethernet_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES ethernet_hal esp_eth esp_netif esp_event freertos t_log event_bus
)
