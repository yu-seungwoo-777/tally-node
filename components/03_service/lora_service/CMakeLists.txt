idf_component_register(
    SRCS "lora_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        lora_driver
        lora_protocol
        event_bus
        tally_types
        t_log
        board_led_driver
)
