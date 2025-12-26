# Button Handler Component

idf_component_register(
    SRCS "button_handler.c"
    INCLUDE_DIRS "include"
    REQUIRES button_poll DisplayManager TxPage RxPage t_log
)
