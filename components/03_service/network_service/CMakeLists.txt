idf_component_register(
    SRCS "network_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES config_service wifi_driver ethernet_driver freertos t_log
)
