idf_component_register(
    SRCS "device_management_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lora_protocol event_bus t_log esp_system
)
