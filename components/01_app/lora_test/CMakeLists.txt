idf_component_register(
    SRCS "lora_test_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES button_poll lora_service event_bus tally_types t_log
)
