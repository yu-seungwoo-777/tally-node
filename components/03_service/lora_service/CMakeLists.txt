idf_component_register(
    SRCS "src/LoRaService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        lora_driver
        event_bus
        simple_log
)
