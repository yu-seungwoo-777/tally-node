idf_component_register(
    SRCS "button_poll.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer t_log
)
