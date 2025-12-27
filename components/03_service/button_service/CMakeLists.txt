idf_component_register(
    SRCS "ButtonService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES t_log event_bus driver esp_timer
)
