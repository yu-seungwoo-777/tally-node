idf_component_register(
    SRCS "battery_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES battery_hal t_log
)
