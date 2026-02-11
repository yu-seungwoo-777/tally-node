idf_component_register(
    SRCS
        "test_license_client.cpp"
        "test_network_service_config.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        unity
        license_client
        network_service
        esp_netif
        esp_event
        app_types
        event_bus
    REQUIRES cxx
)
