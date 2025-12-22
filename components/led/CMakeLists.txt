# LED 컴포넌트

idf_component_register(
    SRCS
        "core/src/WS2812Core.c"
    INCLUDE_DIRS
        "core/include"
    REQUIRES
        driver
        simple_log
)
