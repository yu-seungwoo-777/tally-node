idf_component_register(
    SRCS "prod_tx_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES t_log event_bus config_service hardware_service DisplayManager button_service lora_service switcher_service network_service device_management_service tally_types web_server esp_netif esp_event freertos
)
