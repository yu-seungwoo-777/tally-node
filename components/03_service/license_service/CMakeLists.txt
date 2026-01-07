idf_component_register(
    SRCS "license_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES event_bus nvs_flash t_log license_client
)
