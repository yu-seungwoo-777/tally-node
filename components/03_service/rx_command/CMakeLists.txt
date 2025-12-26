idf_component_register(
    SRCS "rx_command.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lora_protocol lora_service event_bus t_log esp_system
)
