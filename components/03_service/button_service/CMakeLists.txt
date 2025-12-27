idf_component_register(
    SRCS "button_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES t_log event_bus driver esp_timer
)
