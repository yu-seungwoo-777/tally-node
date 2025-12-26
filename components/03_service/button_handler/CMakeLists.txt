# Button Handler Component

idf_component_register(
    SRCS "button_handler.c"
    INCLUDE_DIRS "include"
    REQUIRES button_poll event_bus t_log
)
