idf_component_register(
    SRCS "prod_rx_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES t_log event_bus config_service hardware_service DisplayManager button_service lora_service led_service device_management_service tally_types freertos
)
