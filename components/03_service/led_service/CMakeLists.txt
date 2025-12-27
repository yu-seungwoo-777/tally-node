idf_component_register(
    SRCS "LedService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES ws2812_driver config_service
)
