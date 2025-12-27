idf_component_register(
    SRCS "led_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES ws2812_driver config_service
)
