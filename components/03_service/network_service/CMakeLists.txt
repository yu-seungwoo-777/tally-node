idf_component_register(
    SRCS "NetworkService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES config_service wifi_driver ethernet_driver freertos t_log
)
