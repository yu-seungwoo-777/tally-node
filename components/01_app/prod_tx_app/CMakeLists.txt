idf_component_register(
    SRCS "prod_tx_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES t_log event_bus config_service DisplayManager button_poll button_handler
)
