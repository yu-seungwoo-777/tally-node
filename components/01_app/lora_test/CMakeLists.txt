idf_component_register(
    SRCS "lora_test_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES button_service lora_service event_bus
)
