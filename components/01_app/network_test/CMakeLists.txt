idf_component_register(
    SRCS "network_test_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES network_service config_service
)
