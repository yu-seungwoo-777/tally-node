idf_component_register(
    SRCS "ConfigService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash t_log battery_driver
)
