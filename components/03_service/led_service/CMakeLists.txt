idf_component_register(
    SRCS "led_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES ws2812_driver board_led_driver lora_protocol tally_types config_service event_bus
)
