idf_component_register(
    SRCS "tally_tx_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES switcher_service network_service config_service t_log
)
