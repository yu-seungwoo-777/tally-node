idf_component_register(
    SRCS "led_test.cpp"
    INCLUDE_DIRS "include"
    REQUIRES ws2812_driver t_log config_service
)
