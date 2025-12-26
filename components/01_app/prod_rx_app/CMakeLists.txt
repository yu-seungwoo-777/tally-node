idf_component_register(
    SRCS "prod_rx_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES t_log event_bus config_service DisplayManager button_poll button_handler lora_service ws2812_driver tally_types freertos
)
