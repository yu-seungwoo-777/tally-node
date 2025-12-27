idf_component_register(
    SRCS "device_management_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lora_protocol lora_service event_bus t_log nvs_flash esp_system
)
