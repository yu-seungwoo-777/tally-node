idf_component_register(
    SRCS "SystemMonitor.cpp" "system_monitor_c.cpp" "DeviceIdManager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_common nvs_flash esp_wifi esp_netif esp_event
)