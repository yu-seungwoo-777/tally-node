idf_component_register(
    SRCS "SwitcherService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES tally_types atem vmix obs wifi_driver ethernet_driver t_log
)
