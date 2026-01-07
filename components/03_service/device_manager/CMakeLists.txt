idf_component_register(
    SRCS "device_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        lora_protocol
        event_bus
        t_log
)
