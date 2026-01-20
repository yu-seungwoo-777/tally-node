idf_component_register(
    SRCS "test_license_client.cpp"
    INCLUDE_DIRS "."
    REQUIRES unity license_client esp_netif esp_event
    REQUIRES cxx
)
