idf_component_register(
    SRCS "prod_rx_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES t_log event_bus config_service hardware_service DisplayManager button_service lora_service device_manager led_service tally_types freertos
)
