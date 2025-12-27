idf_component_register(
    SRCS "temperature_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES temperature_hal t_log
)
