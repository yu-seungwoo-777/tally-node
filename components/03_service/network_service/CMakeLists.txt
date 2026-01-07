idf_component_register(
    SRCS "network_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES wifi_driver ethernet_driver esp_netif freertos t_log app_types event_bus
)
