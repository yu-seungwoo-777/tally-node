# BatteryEmptyPage Component

idf_component_register(
    SRCS "BatteryEmptyPage.cpp"
    INCLUDE_DIRS "include"
    REQUIRES DisplayManager t_log app_types
)
