idf_component_register(
    SRCS "system_wdt.c"
    INCLUDE_DIRS "."
    REQUIRES
        freertos
        t_log
)
