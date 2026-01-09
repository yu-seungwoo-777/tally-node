idf_component_register(
    SRCS "prod_tx_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES t_log event_bus config_service hardware_service license_service DisplayManager button_service lora_service device_manager switcher_service network_service tally_test_service tally_types web_server esp_netif esp_event freertos
)
