idf_component_register(
    SRCS "switcher_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES tally_types atem vmix obs t_log
)
