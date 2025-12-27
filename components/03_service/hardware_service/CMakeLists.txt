idf_component_register(
    SRCS "hardware_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES t_log event_bus battery_driver temperature_driver
)
