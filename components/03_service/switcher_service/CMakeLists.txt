idf_component_register(
    SRCS "switcher_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES tally_types atem vmix t_log event_bus app_types
)
