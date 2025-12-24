idf_component_register(
    SRCS "LoRaService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        lora_driver
        event_bus
        tally_types
        t_log
)
