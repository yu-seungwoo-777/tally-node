# DisplayManager Component

idf_component_register(
    SRCS "DisplayManager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES display_driver t_log freertos BootPage TxPage RxPage event_bus tally_types
)
