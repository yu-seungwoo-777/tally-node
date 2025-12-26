idf_component_register(
    SRCS "TemperatureDriver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES temperature_hal t_log
)
