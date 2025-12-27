idf_component_register(
    SRCS "tally_rx_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lora_service led_service config_service button_poll t_log event_bus tally_types
)
